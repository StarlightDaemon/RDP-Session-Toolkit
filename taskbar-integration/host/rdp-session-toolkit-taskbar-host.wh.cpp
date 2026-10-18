// ==WindhawkMod==
// @id              rdp-session-toolkit-taskbar-host
// @name            RDP Session Toolkit — Taskbar Host
// @description     Host-side taskbar integration for RDP Session Toolkit. Injects a native XAML widget into the Windows 11 taskbar of the RDP session host showing the connecting client's name and the session's connection state, with disconnect, minimize, fullscreen-toggle and restart-clipboard buttons. Minimize and the fullscreen toggle signal the client over an RDP Dynamic Virtual Channel, which also carries the client's fullscreen state back so the toggle labels its real direction. Restart clipboard kills and relaunches the session's own rdpclip.exe to repair broken copy-and-paste.
// @version         0.5.1
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -lruntimeobject -luser32 -lwindowsapp -lwtsapi32
// ==/WindhawkMod==

// Provenance: new source, written for the RDP Session Toolkit as the host-side
// sibling of rdp-session-toolkit-taskbar-client. The taskbar XAML injection
// technique — TaskListButton::UpdateVisualStates symbol hook, walk up the
// visual tree to Taskbar.TaskbarFrame, inject into Grid#RootGrid, plus the
// Taskbar.View.dll polling and WM_SIZE initial-scan trigger — is adapted from
// the sibling Native Taskbar Media Controller mod
// (https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller,
// v1.5.0). The WTS session logic is new.

// ==WindhawkModReadme==
/*
# RDP Session Toolkit — Taskbar Host

**Reliability candidate:** relay controls require matching client 0.11.1 and
relay 0.3.1. Missing, ambiguous or incompatible peers disable only those controls.
The status line and tooltip report action results. Local Disconnect and Restart
Clipboard remain available without a relay. Host jobs and taskbar callbacks are
owned through unload; failed status polls repaint the unavailable state.

The host-side (`explorer.exe`) taskbar-integration component of the
**RDP Session Toolkit**. Runs on the machine being connected *to* — the RDP
session host — and puts a small widget in the taskbar of the remote session
itself.

The widget is inserted directly into the taskbar's own XAML tree
(`Grid#RootGrid` under `Taskbar.TaskbarFrame`) — no overlay window, no GDI
paint loop. It inherits z-ordering, auto-hide handling, and DPI scaling from
the taskbar itself.

## What it shows

- **Connecting client name** — the `WTSClientName` of the machine currently
  connected to this session (your local machine's name, when you are RDP'd
  in). Shows `Console` when the session is attached to the physical console
  rather than an RDP client.
- **Connection state** — the live `WTSConnectState` of the session
  (Active, Disconnected, …), refreshed on session change notifications and
  on a periodic safety-net timer.

## Disconnect

The widget's disconnect button calls
`WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION)` —
the same clean disconnect as the Start-menu Disconnect option: the session
keeps running and the client is detached.

The button is enabled only while the session is attached over RDP
(`WTSClientProtocolType` == RDP). On a console session it is dimmed and does
nothing, so the mod cannot accidentally kick the locally-signed-in user to
the lock screen.

## Minimize the client

The widget's minimize button does not act locally. It opens the toolkit's
RDP Dynamic Virtual Channel (`dvc::taskbar::relay`), writes a versioned,
correlated request bound to the client process lifetime, which the client-side DVC relay plugin forwards to the
client mod's relay receiver — minimizing the remote `mstsc` window on your
local machine — and then **reads the relay's small status reply back before
closing**. The reply doubles as delivery confirmation: the original
write-then-immediately-close shape could tear the channel down before the
queued byte reached the client, which is why the button used to drop clicks.
The open-write-read-close runs on the owned, bounded host worker so the
taskbar's UI thread never blocks on channel latency. Missing/incompatible peers
and uncertain results appear in the status line and tooltip; uncertain actions
are never retried automatically.

Like Disconnect, the button is enabled only over RDP (a minimize signal is
meaningless on the physical console).

## Fullscreen / windowed toggle for the client

A fourth button switches the connecting computer's Remote Desktop window
between fullscreen and windowed, over the same relay channel (the client mod
forwards it to Remote Desktop's own Ctrl+Alt+Break toggle — nothing is
reimplemented). The button is not a blind toggle: every relay reply — and a
lightweight status query the widget sends every few seconds — carries the
client's current fullscreen state back, so the button's glyph and tooltip
always name the actual direction a click will take, or say plainly that the
state is unknown when the client has not reported it. Enabled only over RDP,
re-checked at click time, like every other button here.

## Restart the clipboard helper

`rdpclip.exe` is the per-session helper that carries copy-and-paste between
your local machine and the session. When it wedges — a common, well-documented
RDP failure with no in-box fix — the only remedy is to kill and relaunch it
from inside the session by hand. The widget's **Restart Clipboard** button does
exactly that, in one click.

It finds the `rdpclip.exe` processes belonging to *this* session (matched
against this `explorer.exe`'s own session id, not the active console session),
verifies each one really is `%SystemRoot%\System32\rdpclip.exe` by its full
image path before touching it, terminates every verified match, waits for it to
actually exit, and then launches exactly one replacement from that same
verified path. If none was running, it simply starts one.

**Restarting the helper discards whatever is currently on the shared
clipboard** — the button's tooltip says so. That is acceptable in the case the
button exists for (the clipboard is already broken), but it is not a no-op.

Like Disconnect and Minimize, the button is enabled only over RDP, re-checked
at the moment of the click.

## Clipboard status line

When the widget can see that *no* verified `rdpclip.exe` is running in this
session, it shows a **clipboard helper not running** line. When one is running
it shows nothing at all about the clipboard — deliberately. A helper that is
running but wedged is a real possibility this check cannot distinguish, so the
widget never claims the clipboard is fine; it only reports the one thing it can
prove. The check rides the same 30 s session-state refresh the rest of the
widget uses, and adds no timer of its own.

## Toolkit context

This mod is one component of the RDP Session Toolkit. The minimize button is
the send side of the toolkit's cross-machine pipe: host mod → DVC relay
plugin (client) → client mod. The underlying DVC activation + delivery pipe
was proven by the `dvc-plugin` probe's two-machine test; the full
end-to-end feature (a host minimize click actually minimizing the client's
`mstsc` window) still needs its own live hardware test.

## Requirements

- Windows 11 (22H2 or later recommended) on the session host
- [Windhawk](https://windhawk.net) mod loader on the session host
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- WidgetPosition: Right
  $name: Widget position
  $description: "Where on the taskbar the widget appears."
  $options:
  - Right: Right — next to clock & tray
  - Left: Left — taskbar far left
  - Center: Center — middle of taskbar
- OffsetX: 8
  $name: Position offset (px)
  $description: "Fine-tune placement. Right: gap from the system tray. Left: gap from the left edge. Center: nudge from center (positive = shift right)."
- PanelWidth: 300
  $name: Widget width (px)
- FontSize: 11
  $name: Font size
*/
// ==/WindhawkModSettings==

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <windows.h>
#include <wtsapi32.h>
#include <pchannel.h>

// winbase.h defines GetCurrentTime() as a macro wrapping GetTickCount().
// winrt XAML headers declare a virtual GetCurrentTime(int64_t*) method.
// Undefine the macro before pulling in WinRT to avoid the collision.
#undef GetCurrentTime

#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.UI.Xaml.Media.h>

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

// WH_CATCH logs hresult, std::exception, and unknown exceptions with a context label.
// Usage: try { ... } WH_CATCH(L"context")
#define WH_CATCH(ctx)                                                          \
    catch (winrt::hresult_error const& e) {                                    \
        Wh_Log(L"[" ctx L"] hresult 0x%08X: %s", (unsigned)e.code().value,    \
               e.message().c_str());                                           \
    }                                                                          \
    catch (std::exception const& e) {                                          \
        Wh_Log(L"[" ctx L"] std::exception (see debug log)"); (void)e;         \
    }                                                                          \
    catch (...) {                                                              \
        Wh_Log(L"[" ctx L"] unknown exception");                               \
    }

// BEGIN TOOLKIT RUNTIME CONTRACT
// Mirrored verbatim into the standalone Windhawk sources. The runtime tests
// enforce equality; users can still install either mod by pasting one file.
#include <windows.h>
#include <tlhelp32.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace rdpstk {
enum class Result : DWORD {
    Unknown, Accepted, Observed, NoReceiver, Ambiguous, TargetChanged,
    Unavailable, Rejected, TimedOut, Unsupported, Busy
};
inline PCWSTR ResultText(Result r) {
    switch (r) {
    case Result::Accepted: return L"Accepted; awaiting state";
    case Result::Observed: return L"Requested window state observed";
    case Result::NoReceiver: return L"Client mod unavailable";
    case Result::Ambiguous: return L"Multiple Remote Desktop clients; controls paused";
    case Result::TargetChanged: return L"Client changed; refresh before trying again";
    case Result::Unavailable: return L"Client state unavailable";
    case Result::Rejected: return L"Command rejected";
    case Result::TimedOut: return L"Timed out; outcome unknown";
    case Result::Unsupported: return L"Component upgrade required";
    case Result::Busy: return L"Another action is in progress";
    default: return L"Outcome unknown";
    }
}
inline bool ValidResult(DWORD value) { return value <= (DWORD)Result::Busy; }
inline Result CommandReply(DWORD_PTR value) {
    // Zero can mean an uncertain action or an unhandled message; neither is
    // proof of rejection. Preserve uncertainty without ever claiming success.
    return value <= (DWORD)Result::Busy ? (Result)value : Result::Unsupported;
}
inline ULONGLONG FileTimeValue(FILETIME t) {
    return ((ULONGLONG)t.dwHighDateTime << 32) | t.dwLowDateTime;
}
inline bool ProcessBirth(DWORD pid, ULONGLONG* birth) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    if (!p) return false;
    FILETIME created{}, exit{}, kernel{}, user{};
    bool ok = WaitForSingleObject(p, 0) == WAIT_TIMEOUT &&
              GetProcessTimes(p, &created, &exit, &kernel, &user);
    if (ok) *birth = FileTimeValue(created);
    CloseHandle(p);
    return ok;
}
struct Target {
    Result result = Result::Unavailable;
    HWND window = nullptr;
    DWORD pid = 0;
    ULONGLONG birth = 0;
};
struct FrameScan { bool complete = false; unsigned count = 0; HWND window = nullptr; };
inline FrameScan ScanFrames(DWORD targetPid) {
    struct Context { DWORD pid; FrameScan frames; } context{targetPid, {}};
    context.frames.complete = EnumWindows([](HWND window, LPARAM data) -> BOOL {
        auto& context = *reinterpret_cast<Context*>(data);
        DWORD pid = 0; GetWindowThreadProcessId(window, &pid);
        if (pid == context.pid) {
            wchar_t cls[64]{};
            if (!GetClassNameW(window, cls, ARRAYSIZE(cls))) return FALSE;
            if (_wcsicmp(cls, L"TscShellContainerClass") == 0) {
                ++context.frames.count; context.frames.window = window;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&context)) != FALSE;
    return context.frames;
}
// Count every mstsc in this Windows session, including unmodified connection
// dialogs. This intentionally favors a conservative refusal over guessing.
// processName is injectable only for isolated fixture-process tests.
inline Target FindTarget(PCWSTR windowClass, DWORD expectedPid = 0,
                         ULONGLONG expectedBirth = 0,
                         PCWSTR processName = L"mstsc.exe") {
    Target t;
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) return t;
    HANDLE scan = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (scan == INVALID_HANDLE_VALUE) return t;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    BOOL more = Process32FirstW(scan, &entry);
    unsigned count = 0;
    bool uncertain = !more;
    while (more) {
        if (_wcsicmp(entry.szExeFile, processName) == 0) {
            DWORD candidateSession = 0;
            if (!ProcessIdToSessionId(entry.th32ProcessID, &candidateSession)) uncertain = true;
            else if (candidateSession == session) { ++count; t.pid = entry.th32ProcessID; }
        }
        more = Process32NextW(scan, &entry);
    }
    if (GetLastError() != ERROR_NO_MORE_FILES) uncertain = true;
    CloseHandle(scan);
    if (count > 1) { t.result = Result::Ambiguous; return t; }
    if (uncertain) return t;
    if (!count) { t.result = Result::NoReceiver; return t; }
    if (!ProcessBirth(t.pid, &t.birth)) return t;
    if ((expectedPid && expectedPid != t.pid) ||
        (expectedBirth && expectedBirth != t.birth)) {
        t.result = Result::TargetChanged;
        return t;
    }
    // PID uniqueness alone does not identify a frame when a client hosts more
    // than one top-level RDP window. Do not let cached frame selection guess.
    auto frames = ScanFrames(t.pid);
    if (!frames.complete) return t;
    if (frames.count > 1) { t.result = Result::Ambiguous; return t; }
    unsigned receivers = 0;
    HWND w = nullptr;
    while ((w = FindWindowExW(HWND_MESSAGE, w, windowClass, nullptr))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(w, &pid);
        // Any extra receiver on this desktop is ambiguous, even a foreign one.
        if (++receivers > 1 || pid != t.pid) { t.result = Result::Ambiguous; return t; }
        t.window = w;
    }
    t.result = t.window ? Result::Observed : Result::NoReceiver;
    return t;
}
inline bool Fresh(ULONGLONG tick, ULONGLONG now, ULONGLONG limit = 4000) {
    return tick <= now && now - tick <= limit;
}

constexpr DWORD kBoundMagic = 0x33435452; // 'RTC3'; low byte is never a legacy command
constexpr DWORD kProtocol = 3;
constexpr DWORD kCapabilities = 3; // bound targets + explicit results
constexpr BYTE kRequestMarker = 0x7F; // old relays forward an unknown, harmless command
constexpr DWORD kReplyMagic = 0x33525352; // distinct from the v1 status reply
#pragma pack(push, 1)
struct BoundCommand {
    DWORD magic = kBoundMagic;
    DWORD version = kProtocol;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
    ULONGLONG requestId = 0;
    BYTE command = 0;
};
struct Request {
    BYTE marker = kRequestMarker;
    BYTE command = 0;
    WORD size = sizeof(Request);
    DWORD magic = kBoundMagic;
    DWORD version = kProtocol;
    ULONGLONG requestId = 0;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
};
struct Reply {
    DWORD magic = kReplyMagic;
    DWORD version = kProtocol;
    ULONGLONG requestId = 0;
    DWORD capabilities = kCapabilities;
    DWORD result = (DWORD)Result::Unknown;
    DWORD targetPid = 0;
    ULONGLONG targetBirth = 0;
    ULONGLONG snapshotTick = 0;
    BYTE command = 0;
    BYTE known = 0;
    BYTE active = 0;
    BYTE fullscreen = 0;
    BYTE iconic = 0;
};
#pragma pack(pop)
static_assert(sizeof(Request) == 32 && sizeof(BoundCommand) == 29 && sizeof(Reply) == 49,
              "Wire contract drift");
inline bool ValidRequest(const Request& r) {
    return r.marker == kRequestMarker && r.size == sizeof(r) &&
           r.magic == kBoundMagic && r.version == kProtocol && r.requestId &&
           r.command >= 1 && r.command <= 3 &&
           (r.command == 3 || (r.targetPid && r.targetBirth));
}
inline bool ValidReply(const Reply& r, const Request& q) {
    return r.magic == kReplyMagic && r.version == kProtocol &&
           (r.capabilities & kCapabilities) == kCapabilities &&
           r.requestId == q.requestId && r.command == q.command &&
           ValidResult(r.result) && r.known <= 1 && r.active <= 1 &&
           r.fullscreen <= 1 && r.iconic <= 1 &&
           (r.known || (!r.active && !r.fullscreen && !r.iconic)) &&
           (!r.known || r.result == (DWORD)Result::Observed || r.result == (DWORD)Result::Accepted) &&
           (!r.known || (r.targetPid && r.targetBirth && r.snapshotTick)) &&
           ((r.result != (DWORD)Result::Accepted && r.result != (DWORD)Result::Observed) ||
            (r.known && (q.command == 3 || r.active))) &&
           (q.command == 3 || !r.known ||
            (r.targetPid == q.targetPid && r.targetBirth == q.targetBirth));
}
inline bool ParseReply(const BYTE* bytes, DWORD length, const Request& request, Reply* out) {
    if (!bytes || !out) return false;
    for (DWORD offset : {0ul, 8ul}) {
        if (length != offset + sizeof(Reply)) continue;
        if (offset) {
            DWORD size = 0, flags = 0;
            memcpy(&size, bytes, sizeof(size)); memcpy(&flags, bytes + 4, sizeof(flags));
            // A v3 reply fits in a single DVC chunk. Ignore unrelated flag bits,
            // but require both FIRST and LAST and the exact declared length.
            if (size != sizeof(Reply) || (flags & 3) != 3) continue;
        }
        Reply reply;
        memcpy(&reply, bytes + offset, sizeof(reply));
        if (ValidReply(reply, request)) { *out = reply; return true; }
    }
    return false;
}
// command uses the local control numbering (1 minimize, 2 restore, 3 fullscreen).
// The caller must establish identity and sample ordering before using this test.
inline bool WindowOutcome(BYTE command, bool wasFullscreen, bool fullscreen, bool iconic) {
    return (command == 1 && iconic) || (command == 2 && !iconic) ||
           (command == 3 && fullscreen != wasFullscreen);
}
template<class Snapshot> inline bool ValidSnapshotState(const Snapshot& s) {
    auto boolean = [](BOOL value) { return value == FALSE || value == TRUE; };
    return s.writerPid && s.writerBirth && s.writeTick && boolean(s.sessionActive) &&
        boolean(s.qualityAvailable) && boolean(s.hung) && boolean(s.iconic) && boolean(s.fullscreen) &&
        (s.sessionActive || (!s.iconic && !s.fullscreen));
}
inline bool PublishRecord(PCWSTR path, const void* data, DWORD size) {
    try {
        static std::atomic<ULONGLONG> sequence{0};
        std::wstring temporary = std::wstring(path) + L"." + std::to_wstring(GetCurrentProcessId()) +
            L"." + std::to_wstring(GetCurrentThreadId()) + L"." +
            std::to_wstring(sequence.fetch_add(1) + 1) + L".tmp";
        HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        BOOL ok = WriteFile(file, data, size, &written, nullptr);
        CloseHandle(file);
        // Publish a closed, complete record on the same volume. Readers allow
        // FILE_SHARE_DELETE and therefore keep an immutable old or new handle.
        if (ok && written == size && MoveFileExW(temporary.c_str(), path, MOVEFILE_REPLACE_EXISTING)) return true;
        DeleteFileW(temporary.c_str());
    } catch (...) {}
    return false;
}

// One owned worker and a bounded queue. Stop rejects future jobs, discards
// queued jobs and joins the active job before DLL unload. No detached work.
class WorkQueue {
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::function<void()>> jobs_;
    std::thread thread_;
    bool stopping_ = true;
public:
    bool Start() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_.joinable()) return false;
        stopping_ = false;
        try {
            thread_ = std::thread([this] {
                for (;;) {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        wake_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
                        if (stopping_) return;
                        job = std::move(jobs_.front()); jobs_.pop_front();
                    }
                    try { job(); } catch (...) { /* job-specific feedback belongs to its caller */ }
                }
            });
        } catch (...) { stopping_ = true; return false; }
        return true;
    }
    bool Post(std::function<void()> job) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ || jobs_.size() >= 4) return false;
        try { jobs_.push_back(std::move(job)); } catch (...) { return false; }
        wake_.notify_one(); return true;
    }
    void Stop() {
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; jobs_.clear(); }
        wake_.notify_all();
        if (thread_.joinable()) thread_.join();
    }
    ~WorkQueue() { Stop(); }
};
} // namespace rdpstk
// END TOOLKIT RUNTIME CONTRACT

// BEGIN TRACKED TASKBAR DISPATCH
// Own every queued delegate until completion. A timeout is not proof that a
// callback has stopped using this DLL. Windhawk unload runs off the XAML thread.
class TrackedDispatch {
    std::mutex mutex_;
    std::atomic<bool> stopping_{false};
    struct Entry {
        winrt::Windows::Foundation::IAsyncAction action{nullptr};
        winrt::Windows::UI::Core::CoreDispatcher dispatcher{nullptr};
    };
    std::vector<Entry> actions_;
    void Queue(winrt::Windows::UI::Xaml::FrameworkElement const& element,
               winrt::Windows::UI::Core::CoreDispatcherPriority priority,
               winrt::Windows::UI::Core::DispatchedHandler const& callback, bool cleanup) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_ && !cleanup) return;
        actions_.erase(std::remove_if(actions_.begin(), actions_.end(), [](auto const& action) {
            return action.action.Status() != winrt::Windows::Foundation::AsyncStatus::Started;
        }), actions_.end());
        // Bound retained delegates while an Explorer dispatcher is stalled. All
        // callers already catch dispatch errors (including scan-latch reset).
        if (!cleanup && actions_.size() >= 128) winrt::throw_hresult(E_OUTOFMEMORY);
        // Reserve BEFORE queueing: allocation failure must not orphan a delegate.
        actions_.reserve(actions_.size() + 1);
        auto dispatcher = element.Dispatcher();
        actions_.push_back({dispatcher.RunAsync(priority, [this, callback, cleanup] {
            if (cleanup || !stopping_.load()) callback();
        }), dispatcher});
    }
public:
    void Post(winrt::Windows::UI::Xaml::FrameworkElement const& element,
              winrt::Windows::UI::Core::CoreDispatcherPriority priority,
              winrt::Windows::UI::Core::DispatchedHandler const& callback) {
        Queue(element, priority, callback, false);
    }
    void PostCleanup(winrt::Windows::UI::Xaml::FrameworkElement const& element,
                     winrt::Windows::UI::Core::CoreDispatcherPriority priority,
                     winrt::Windows::UI::Core::DispatchedHandler const& callback) {
        try { Queue(element, priority, callback, true); }
        catch (...) { RetainCode(); } // live XAML handlers may still own this code
    }
    void Drain() {
        std::vector<Entry> pending;
        { std::lock_guard<std::mutex> lock(mutex_); stopping_ = true; pending.swap(actions_); }
        ULONGLONG deadline = GetTickCount64() + 2000;
        bool pinned = false;
        for (auto const& entry : pending) {
            try {
                auto started = winrt::Windows::Foundation::AsyncStatus::Started;
                bool retained = false;
                while (entry.action.Status() == started) {
                    if (entry.dispatcher.HasThreadAccess() || GetTickCount64() >= deadline) {
                        if (!pinned) { RetainCode(); pinned = true; }
                        retained = true; break;
                    }
                    Sleep(10);
                }
                if (!retained) entry.action.GetResults();
            } catch (...) { if (!pinned) { RetainCode(); pinned = true; } }
        }
    }

public:
    static void PinAddress() {}
    static void RetainCode() {
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<PCWSTR>(&PinAddress), &pinned)) {
            // Without a module reference we cannot safely return from unload.
            for (;;) Sleep(1000);
        }
        Wh_Log(L"Taskbar callback completion unavailable; code retained until Explorer exits");
    }
};
// END TRACKED TASKBAR DISPATCH

namespace {
TrackedDispatch g_Dispatch;
rdpstk::WorkQueue g_HostWork;
void RefreshWidgetUI();
std::mutex g_NoticeMutex;
std::wstring g_Notice;
ULONGLONG g_NoticeTick = 0;
void HostNotice(const std::wstring& text) {
    { std::lock_guard<std::mutex> lock(g_NoticeMutex); g_Notice = text; g_NoticeTick = GetTickCount64(); }
    RefreshWidgetUI();
}
std::wstring HostNoticeText() {
    std::lock_guard<std::mutex> lock(g_NoticeMutex);
    return GetTickCount64() - g_NoticeTick < 10000 ? g_Notice : L"";
}
bool IsRdpNow() {
    LPWSTR buffer = nullptr; DWORD bytes = 0;
    bool rdp = WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION,
        WTSClientProtocolType, &buffer, &bytes) && buffer && bytes >= sizeof(USHORT) &&
        *reinterpret_cast<USHORT*>(buffer) == 2;
    if (buffer) WTSFreeMemory(buffer);
    return rdp;
}

// ── Settings ──────────────────────────────────────────────────────────────

enum class WidgetPosition { Right, Left, Center };

struct ModSettings {
    // 300 since v0.4.0: a fourth button (the fullscreen toggle) does not fit
    // legibly at 260. Existing saved settings are untouched.
    int panelWidth = 300;
    int fontSize = 11;
    int offsetX = 8;
    WidgetPosition widgetPosition = WidgetPosition::Right;
} g_Settings;

void LoadSettings() {
    g_Settings.panelWidth = Wh_GetIntSetting(L"PanelWidth");
    g_Settings.fontSize   = Wh_GetIntSetting(L"FontSize");
    g_Settings.offsetX    = Wh_GetIntSetting(L"OffsetX");
    if (g_Settings.panelWidth <= 0) g_Settings.panelWidth = 300;
    if (g_Settings.fontSize   <= 0) g_Settings.fontSize = 11;
    if (g_Settings.offsetX    <  0) g_Settings.offsetX = 8;
    PCWSTR pos = Wh_GetStringSetting(L"WidgetPosition");
    if (wcscmp(pos, L"Left") == 0)
        g_Settings.widgetPosition = WidgetPosition::Left;
    else if (wcscmp(pos, L"Center") == 0)
        g_Settings.widgetPosition = WidgetPosition::Center;
    else
        g_Settings.widgetPosition = WidgetPosition::Right;
    Wh_FreeStringSetting(pos);
}

// ── Clipboard helper (rdpclip.exe) — find, verify, restart ────────────────
//
// Design basis: EXPANSION_RESEARCH.md §4.1 (the restart action) and §4.2 (the
// presence line), built from three patterns this toolkit already established
// rather than anything new — D-8's click-time console-vs-RDP gate, D-11's
// exact-image-path identity check, and D-12's detached one-shot thread with an
// in-flight counter drained in Wh_ModUninit.
//
// rdpclip.exe is the per-session clipboard-redirection helper. When it wedges,
// copy-and-paste between the client and the session stops working and the only
// in-box remedy is to kill and relaunch it by hand from inside the session.
// This is that, as one button.

// The image this mod will terminate or launch, and nothing else.
constexpr wchar_t kRdpClipImageName[]  = L"rdpclip.exe";
constexpr wchar_t kRdpClipPathSuffix[] = L"\\System32\\rdpclip.exe";

// Bounded wait on a terminated rdpclip's process handle, plus a short settle
// after it, before the replacement is launched.
//
// Why wait at all: rdpclip does not merely occupy a slot in the process list.
// It binds itself to the session clipboard in two ways, both keyed to its
// windows — as a clipboard format listener (AddClipboardFormatListener; the
// older SetClipboardViewer chain on down-level paths), and as the clipboard
// *owner* publishing delayed-render formats on the remote side's behalf. Both
// bindings are released when its windows are destroyed. A replacement that
// registers while the dying instance's window still exists can end up sitting
// behind a stale delayed-render owner — which is precisely the wedged
// clipboard this button exists to clear.
//
// Why a settle on top of the handle wait: WaitForSingleObject on a process
// handle is satisfied once the process's last thread has terminated. The USER
// half of the teardown — destroying the windows, and with them the listener
// registration and any clipboard ownership — is completed by the window
// manager during process rundown, which is not ordered before that signal.
// The settle covers the gap. 250 ms is comfortably past what that teardown
// takes and is invisible in practice: the whole sequence runs on a background
// thread, so no UI is waiting on it.
constexpr DWORD kRdpClipExitWaitMs = 2000;
constexpr DWORD kRdpClipSettleMs   = 250;

// Tri-state on purpose. "Could not tell" must never render as "not running"
// — the same never-invent-a-value rule D-16 set for connection quality.
// Running is never *shown* either: an rdpclip that exists but is wedged is a
// real failure this check cannot distinguish, so the widget must not claim the
// clipboard is fine (EXPANSION_RESEARCH §4.2 is explicit about this).
enum class ClipboardHelper { Unknown, Running, NotRunning };

PCWSTR ClipboardHelperName(ClipboardHelper c) {
    switch (c) {
        case ClipboardHelper::Running:    return L"running";
        case ClipboardHelper::NotRunning: return L"not-running";
        case ClipboardHelper::Unknown:    break;
    }
    return L"unknown";
}

// Forward — the restart worker refreshes the widget's clipboard line through
// the same function the 30 s safety-net timer calls, so there is exactly one
// path that computes it.
void QueryWtsState();

// Builds "<shared Windows directory>\System32\rdpclip.exe".
//
// GetSystemWindowsDirectoryW rather than GetWindowsDirectoryW deliberately: on
// a Terminal Services host — which is exactly what this mod runs on — the two
// can differ. GetWindowsDirectoryW may return a per-user private Windows
// directory for an application running in a session; GetSystemWindowsDirectoryW
// always returns the shared one, which is where the real System32\rdpclip.exe
// lives. Getting this wrong would make every verification fail closed, so the
// TS-correct call is the one to use.
bool GetRdpClipImagePath(wchar_t* out, DWORD cch) {
    if (!out || cch == 0) return false;
    out[0] = L'\0';
    wchar_t win[MAX_PATH] = {};
    UINT n = GetSystemWindowsDirectoryW(win, ARRAYSIZE(win));
    if (n == 0 || n >= ARRAYSIZE(win)) {
        Wh_Log(L"Clipboard: GetSystemWindowsDirectoryW failed GLE=%d",
               GetLastError());
        return false;
    }
    // Only a drive root ("X:\") normally carries a trailing separator; strip it
    // so the fixed suffix can never produce a doubled backslash.
    if (n > 0 && win[n - 1] == L'\\') win[n - 1] = L'\0';
    if (wcslen(win) + wcslen(kRdpClipPathSuffix) + 1 > cch) return false;
    wsprintfW(out, L"%s%s", win, kRdpClipPathSuffix);
    return true;
}

// Resolves a process's full image path through a limited-rights handle —
// the same technique the client mod uses to verify the relay sender (D-11).
bool GetProcessImagePath(DWORD pid, wchar_t* out, DWORD cch) {
    if (!out || cch == 0) return false;
    out[0] = L'\0';
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD n = cch;
    BOOL ok = QueryFullProcessImageNameW(h, 0, out, &n);
    CloseHandle(h);
    return ok && n > 0;
}

// What one enumerate-and-verify pass found.
struct RdpClipScan {
    std::vector<DWORD> pids;   // verified rdpclip.exe pids in THIS session
    bool enumerated = false;   // WTSEnumerateProcessesW itself succeeded
};

// The single source of truth for "which rdpclip.exe processes belong to this
// session and are really the System32 binary". Both the restart action and the
// status line call this one function, so the two can never disagree about what
// counts as present.
//
// Session scoping is mandatory, not cosmetic: a multi-session host runs one
// rdpclip per session, and touching another session's is both wrong and,
// unelevated, not permitted anyway. The session is this process's own, via
// ProcessIdToSessionId(GetCurrentProcessId()) — deliberately NOT
// WTSGetActiveConsoleSessionId, which is a different session entirely and
// would be the wrong answer on every RDP host that also has someone at the
// physical console.
//
// Identity is the exact image path, never the name alone: a name match would
// accept anything a user could drop in their own profile and call rdpclip.exe.
// The comparison is CompareStringOrdinal with ignore-case, matching the style
// D-11 established for the relay sender's path check.
//
// `logSkips` controls per-candidate rejection logging — on for the restart
// action, where "why was that one not killed" has to be answerable from the
// log; off for the 30 s status poll, which would otherwise repeat the same
// line forever.
RdpClipScan ScanForRdpClipInSession(const wchar_t* expectedPath, bool logSkips) {
    RdpClipScan scan;
    if (!expectedPath || !*expectedPath) return scan;

    DWORD ourSession = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &ourSession)) {
        Wh_Log(L"Clipboard: ProcessIdToSessionId failed GLE=%d", GetLastError());
        return scan;
    }

    PWTS_PROCESS_INFOW procs = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &procs, &count)) {
        Wh_Log(L"Clipboard: WTSEnumerateProcessesW failed GLE=%d", GetLastError());
        return scan;   // enumerated stays false — the caller must not conclude
                       // "not running" from a failed enumeration
    }
    scan.enumerated = true;
    struct FreeProcesses { PWTS_PROCESS_INFOW value; ~FreeProcesses() { WTSFreeMemory(value); } } freeProcesses{procs};

    for (DWORD i = 0; i < count; ++i) {
        const WTS_PROCESS_INFOW& p = procs[i];
        if (!p.pProcessName) continue;
        if (CompareStringOrdinal(p.pProcessName, -1, kRdpClipImageName, -1, TRUE)
                != CSTR_EQUAL)
            continue;                                 // not named rdpclip.exe
        if (p.SessionId != ourSession) continue;      // belongs to another session
        if (p.ProcessId == 0) continue;

        wchar_t actual[MAX_PATH] = {};
        if (!GetProcessImagePath(p.ProcessId, actual, ARRAYSIZE(actual))) {
            scan.enumerated = false; // an inaccessible match is not evidence of absence
            if (logSkips)
                Wh_Log(L"Clipboard: pid=%lu is named rdpclip.exe but its image path "
                       L"could not be resolved (GLE=%d) — skipped, not touched",
                       p.ProcessId, GetLastError());
            continue;
        }
        if (CompareStringOrdinal(actual, -1, expectedPath, -1, TRUE) != CSTR_EQUAL) {
            if (logSkips)
                Wh_Log(L"Clipboard: pid=%lu image '%s' is not '%s' — skipped; a name "
                       L"match alone is not enough to terminate anything",
                       p.ProcessId, actual, expectedPath);
            continue;
        }
        scan.pids.push_back(p.ProcessId);
    }

    return scan;
}

// The owned host work queue serializes clipboard and DVC operations and joins
// active work at unload. A busy latch prevents repeated clipboard clicks.
std::atomic<bool> g_ClipboardRestartBusy{ false };

// The whole find → verify → terminate → wait → launch sequence.
// Background thread only (see RestartRdpClipAsync).
bool RestartRdpClipNow() {
    if (!IsRdpNow()) return false;
    wchar_t expectedPath[MAX_PATH] = {};
    if (!GetRdpClipImagePath(expectedPath, ARRAYSIZE(expectedPath))) {
        Wh_Log(L"Restart clipboard: could not resolve System32\\rdpclip.exe — "
               L"aborting without terminating or launching anything");
        return false;
    }

    RdpClipScan scan = ScanForRdpClipInSession(expectedPath, /*logSkips=*/true);
    if (!scan.enumerated) {
        Wh_Log(L"Restart clipboard: process enumeration failed — aborting without "
               L"terminating or launching anything");
        return false;
    }

    // Zero verified matches is neither an error nor a reason to stop: there is
    // simply nothing to terminate, so this becomes a fresh start rather than a
    // restart. More than one is not a reason to pick a favourite either —
    // every verified match is terminated, and exactly one replacement follows.
    if (scan.pids.empty()) {
        Wh_Log(L"Restart clipboard: no verified rdpclip.exe in this session — "
               L"nothing to terminate; starting a fresh instance");
    } else {
        Wh_Log(L"Restart clipboard: %u verified rdpclip.exe instance(s) in this "
               L"session — terminating every one of them",
               (unsigned)scan.pids.size());
    }

    bool terminatedAny = false;
    for (DWORD pid : scan.pids) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
        if (!h) {
            Wh_Log(L"Restart clipboard: OpenProcess(pid=%lu) failed GLE=%d — skipped",
                   pid, GetLastError());
            return false;
        }
        // Reverify on the actual handle being terminated; a PID from the scan
        // may have exited/reused before OpenProcess. Holding this handle pins identity.
        wchar_t image[MAX_PATH]{}; DWORD imageChars = ARRAYSIZE(image);
        DWORD currentSession = 0, candidateSession = 0;
        if (!IsRdpNow() || !QueryFullProcessImageNameW(h, 0, image, &imageChars) ||
            CompareStringOrdinal(image, -1, expectedPath, -1, TRUE) != CSTR_EQUAL ||
            !ProcessIdToSessionId(GetCurrentProcessId(), &currentSession) ||
            !ProcessIdToSessionId(pid, &candidateSession) || currentSession != candidateSession) {
            CloseHandle(h); return false;
        }
        if (!TerminateProcess(h, 0)) {
            Wh_Log(L"Restart clipboard: TerminateProcess(pid=%lu) failed GLE=%d",
                   pid, GetLastError());
            CloseHandle(h);
            return false;
        }
        terminatedAny = true;
        // Bounded wait on the handle — never an assumption that termination is
        // instantaneous.
        DWORD w = WaitForSingleObject(h, kRdpClipExitWaitMs);
        if (w == WAIT_OBJECT_0)
            Wh_Log(L"Restart clipboard: pid=%lu terminated and exited", pid);
        else
            Wh_Log(L"Restart clipboard: pid=%lu terminated but did not signal exit "
                   L"within %lu ms (wait=%lu) — restart unconfirmed",
                   pid, kRdpClipExitWaitMs, w);
        CloseHandle(h);
        if (w != WAIT_OBJECT_0) return false;
    }

    // Let the window manager finish the USER-side rundown (see the constants'
    // comment) before a new instance tries to register.
    if (terminatedAny) Sleep(kRdpClipSettleMs);

    // Re-check before launching (EXPANSION_RESEARCH §4.1): some builds respawn
    // rdpclip themselves once it dies, and launching on top of that would leave
    // two running. If one is already back, leave it alone. This only closes the
    // race up to the settle above — a respawn slower than that would still land
    // beside the replacement, which is why it is logged either way.
    RdpClipScan after = ScanForRdpClipInSession(expectedPath, /*logSkips=*/false);
    if (!after.enumerated) return false;
    if (!after.pids.empty()) {
        Wh_Log(L"Restart clipboard: rdpclip.exe is already running again (pid=%lu) "
               L"— not launching a second one", after.pids.front());
        QueryWtsState();
        return true;
    }

    // CreateProcessW on the resolved System32 path. The replacement inherits
    // this process's session and token automatically — no impersonation, no
    // session plumbing, and it lands in the right session by construction.
    if (!IsRdpNow()) return false;
    wchar_t cmdLine[MAX_PATH + 4] = {};
    wsprintfW(cmdLine, L"\"%s\"", expectedPath);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(expectedPath, cmdLine, nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        Wh_Log(L"Restart clipboard: CreateProcessW('%s') failed GLE=%d",
               expectedPath, GetLastError());
        QueryWtsState();
        return false;
    }
    Wh_Log(L"Restart clipboard: launched '%s' pid=%lu",
           expectedPath, pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Refresh the widget's clipboard line now rather than up to 30 s later.
    // This goes through QueryWtsState — the same function the safety-net timer
    // calls — so no new timer and no second status code path.
    QueryWtsState();
    return true;
}

// Queue the operation off the XAML UI thread. The worker is owned, bounded,
// and stopped before taskbar delegates are drained at unload.
void RestartRdpClipAsync() {
    bool expected = false;
    if (!g_ClipboardRestartBusy.compare_exchange_strong(expected, true)) {
        HostNotice(L"Clipboard restart already in progress"); return;
    }
    HostNotice(L"Clipboard restart queued...");
    if (!g_HostWork.Post([] {
        bool ok = false;
        try { ok = RestartRdpClipNow(); } WH_CATCH(L"RestartRdpClip")
        g_ClipboardRestartBusy.store(false);
        HostNotice(ok ? L"Clipboard restart accepted; copy/paste not verified"
                      : L"Clipboard restart failed; see Windhawk log");
    })) {
        g_ClipboardRestartBusy.store(false);
        HostNotice(L"Clipboard restart unavailable during shutdown or busy queue");
    }
}

// ── WTS session state ─────────────────────────────────────────────────────
// The three session facts come from WTSQuerySessionInformation against
// WTS_CURRENT_SERVER_HANDLE / WTS_CURRENT_SESSION — i.e. the session this
// explorer.exe instance runs in, on the local (host) machine. The fourth
// field, the clipboard helper's presence, comes from the process scan above
// and rides the same refresh (D-35).

// WTSClientProtocolType value for an RDP connection (0 = console, 1 = legacy
// ICA, 2 = RDP — per the WTSQuerySessionInformation documentation).
constexpr USHORT kProtocolRdp = 2;

// Production DVC channel this mod opens to signal the client (see DECISIONS.md
// D-10). MUST match the relay plugin's RELAY_CHANNEL_NAME
// (dvc-plugin/relay/common/RelayIds.h). ANSI, because WTSVirtualChannelOpenEx
// takes an LPSTR channel name.
constexpr char RELAY_DVC_CHANNEL[] = "dvc::taskbar::relay";
// Command bytes the relay leg carries, one per send. 0x01 and 0x02 are
// forwarded by the relay plugin to the client mod's CitadelRdpTaskbarRelay
// receiver (which interprets them); 0x03 is answered by the relay plugin
// itself and never forwarded — it exists purely so the host can read the
// client's current state with no side effect on the client. Mirrored copies
// live in dvc-plugin/relay/common/RelayIds.h and the client mod's
// RelayCommand enum; DECISIONS.md D-10 keeps the copies in sync.
constexpr BYTE RELAY_CMD_MINIMIZE_BYTE          = 0x01;
constexpr BYTE RELAY_CMD_FULLSCREEN_TOGGLE_BYTE = 0x02;
constexpr BYTE RELAY_CMD_QUERY_STATUS_BYTE      = 0x03;

// ── Relay status reply — the client-to-host half of the pipe ──────────────
//
// After ANY command byte lands (a real command or the status query), the
// relay plugin reads the client mod's status snapshot and writes this small
// fixed reply back on the same still-open IWTSVirtualChannel before the host
// closes it. Same magic+version wire-contract discipline as the client mod's
// LocalWidgetStatus and LocalWidgetCommandPayload — never a bare byte.
// Mirrored copy in dvc-plugin/relay/common/RelayIds.h (D-10: separate build
// systems keep their own copies; the decision log reconciles them).
struct WtsState {
    std::wstring clientName;                              // WTSClientName; empty when none reported
    WTS_CONNECTSTATE_CLASS connectState = WTSDisconnected; // WTSConnectState
    USHORT protocol = 0;                                   // WTSClientProtocolType
    // Presence of a verified rdpclip.exe in this session. Part of the state
    // struct (rather than a separate variable) so the existing change
    // detection drives the widget refresh for it too — no extra plumbing.
    ClipboardHelper clipboard = ClipboardHelper::Unknown;

    bool operator==(const WtsState& o) const {
        return clientName == o.clientName &&
               connectState == o.connectState &&
               protocol == o.protocol &&
               clipboard == o.clipboard;
    }
};

std::mutex g_WtsMutex;
WtsState g_WtsState;

PCWSTR ConnectStateName(WTS_CONNECTSTATE_CLASS s) {
    switch (s) {
        case WTSActive:       return L"Active";
        case WTSConnected:    return L"Connected";
        case WTSConnectQuery: return L"Connecting";
        case WTSShadow:       return L"Shadowed";
        case WTSDisconnected: return L"Disconnected";
        case WTSIdle:         return L"Idle";
        case WTSListen:       return L"Listening";
        case WTSReset:        return L"Reset";
        case WTSDown:         return L"Down";
        case WTSInit:         return L"Initializing";
    }
    return L"Unknown";
}

// Forward
void RefreshWidgetUI();
void RefreshThemeUI();  // live theme change — defined with the widget section

// Re-query all displayed session facts and refresh the widget if anything
// changed. Called from the WTS notification thread (session change events and
// the safety-net timer) and once at thread start.
void QueryWtsState() {
    WtsState s;
    LPWSTR buf = nullptr;
    DWORD len = 0;

    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION, WTSClientName,
                                    &buf, &len)) {
        if (buf) s.clientName = buf;
        WTSFreeMemory(buf);
    } else {
        Wh_Log(L"WTS: WTSClientName query failed GLE=%d", GetLastError());
    }

    buf = nullptr;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION, WTSConnectState,
                                    &buf, &len)) {
        if (buf && len >= sizeof(WTS_CONNECTSTATE_CLASS))
            s.connectState = *reinterpret_cast<WTS_CONNECTSTATE_CLASS*>(buf);
        WTSFreeMemory(buf);
    } else {
        Wh_Log(L"WTS: WTSConnectState query failed GLE=%d", GetLastError());
    }

    buf = nullptr;
    if (WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE,
                                    WTS_CURRENT_SESSION, WTSClientProtocolType,
                                    &buf, &len)) {
        if (buf && len >= sizeof(USHORT))
            s.protocol = *reinterpret_cast<USHORT*>(buf);
        WTSFreeMemory(buf);
    } else {
        Wh_Log(L"WTS: WTSClientProtocolType query failed GLE=%d", GetLastError());
    }

    // Clipboard-helper presence (EXPANSION_RESEARCH §4.2). It rides this
    // function, which the 30 s WTS safety-net timer already calls on every
    // tick (and every WM_WTSSESSION_CHANGE besides) — so the check adds no
    // timer of its own, per D-22's no-new-timer discipline. Only meaningful
    // over RDP: a console session has no rdpclip and never should, so the
    // state stays Unknown there and the widget says nothing about it.
    if (s.protocol == kProtocolRdp) {
        wchar_t expectedPath[MAX_PATH] = {};
        if (GetRdpClipImagePath(expectedPath, ARRAYSIZE(expectedPath))) {
            RdpClipScan scan = ScanForRdpClipInSession(expectedPath,
                                                       /*logSkips=*/false);
            // A failed enumeration stays Unknown — never rendered as
            // "not running".
            if (scan.enumerated)
                s.clipboard = scan.pids.empty() ? ClipboardHelper::NotRunning
                                                : ClipboardHelper::Running;
        }
    }

    bool changed;
    {
        std::lock_guard<std::mutex> g(g_WtsMutex);
        changed = !(s == g_WtsState);
        g_WtsState = std::move(s);
    }
    if (changed) {
        {
            std::lock_guard<std::mutex> g(g_WtsMutex);
            Wh_Log(L"WTS: state → client=\"%s\" state=%s protocol=%u clipboard=%s",
                   g_WtsState.clientName.c_str(),
                   ConnectStateName(g_WtsState.connectState),
                   g_WtsState.protocol,
                   ClipboardHelperName(g_WtsState.clipboard));
        }
        RefreshWidgetUI();
    }
}

// ── WTS session-change notification thread ────────────────────────────────
// A dedicated thread owns a hidden top-level window registered for
// WM_WTSSESSION_CHANGE via WTSRegisterSessionNotification. A hidden normal
// window is used rather than a message-only (HWND_MESSAGE) window because
// session-change delivery to message-only windows is not reliable. A 30 s
// timer re-queries as a safety net (and retries registration if it failed —
// e.g. when the mod loads before the Terminal Services service is ready).

constexpr auto WTS_WND_CLASS = L"WH_RdpstkHostWtsNotify";
constexpr UINT_PTR WTS_REFRESH_TIMER_ID  = 1;
constexpr UINT     WTS_REFRESH_INTERVAL_MS = 30000;
// Client-state poll (see "Periodic client-state poll" below): rides this
// thread's timer, queues the poll on the owned worker. 3 s sits in the
// requested 2–4 s range: responsive against the view's staleness window
// without chattering the channel.
constexpr UINT_PTR RELAY_POLL_TIMER_ID = 2;
constexpr UINT     RELAY_POLL_INTERVAL_MS = 3000;
void PollClientStateAsync();  // defined with the relay send section below

// WTS-thread-only, except g_wtsThreadId which the uninit path reads.
HWND               g_hWtsWnd = nullptr;
bool               g_wtsRegistered = false;
HANDLE             g_hWtsThread = nullptr;
std::atomic<DWORD> g_wtsThreadId{ 0 };
// Manual-reset, signaled once the WTS thread's message queue exists, so
// StopWtsThread can never post WM_QUIT before the thread can receive it
// (same pattern as the client mod's relay thread).
HANDLE             g_hWtsThreadReady = nullptr;

void TryRegisterSessionNotification(HWND hwnd) {
    if (g_wtsRegistered) return;
    if (WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION)) {
        g_wtsRegistered = true;
        Wh_Log(L"WTS: session notifications registered for HWND=%p", hwnd);
    } else {
        Wh_Log(L"WTS: WTSRegisterSessionNotification failed GLE=%d — "
               L"will retry on timer", GetLastError());
    }
}

LRESULT CALLBACK WtsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_WTSSESSION_CHANGE:
        // wParam is the WTS_* event code (WTS_REMOTE_CONNECT, …); rather than
        // interpreting each code, any event triggers a full re-query.
        Wh_Log(L"WTS: session change event 0x%02X", (unsigned)wParam);
        QueryWtsState();
        return 0;

    case WM_SETTINGCHANGE:
        // A system light/dark flip broadcasts WM_SETTINGCHANGE with
        // "ImmersiveColorSet" to every top-level window — this hidden window
        // is one, which is what lets the widget re-tone live instead of only
        // at injection time.
        if (lParam && wcscmp(reinterpret_cast<PCWSTR>(lParam),
                             L"ImmersiveColorSet") == 0)
            RefreshThemeUI();
        break;

    case WM_TIMER:
        if (wParam == WTS_REFRESH_TIMER_ID) {
            TryRegisterSessionNotification(hwnd);
            QueryWtsState();
            return 0;
        }
        if (wParam == RELAY_POLL_TIMER_ID) {
            // Queue onto the owned worker; channel I/O never blocks this timer.
            PollClientStateAsync();
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, WTS_REFRESH_TIMER_ID);
        KillTimer(hwnd, RELAY_POLL_TIMER_ID);
        if (g_wtsRegistered) {
            WTSUnRegisterSessionNotification(hwnd);
            g_wtsRegistered = false;
        }
        g_hWtsWnd = nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

DWORD WINAPI WtsThread(LPVOID) {
    // Force this thread's message queue into existence, then signal readiness
    // immediately — before class/window creation, so the signal fires no
    // matter what happens after, including an early return on failure.
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hWtsThreadReady)
        SetEvent(g_hWtsThreadReady);

    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = WtsWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = WTS_WND_CLASS;
    if (!RegisterClassExW(&wc)) {
        Wh_Log(L"WTS: RegisterClassExW failed GLE=%d", GetLastError());
        return 0;
    }

    // Hidden (never shown) top-level window — see the thread-banner comment
    // for why this is not a message-only window.
    g_hWtsWnd = CreateWindowExW(0, WTS_WND_CLASS, L"", WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hWtsWnd) {
        Wh_Log(L"WTS: CreateWindowExW failed GLE=%d", GetLastError());
        UnregisterClassW(WTS_WND_CLASS, GetModuleHandleW(nullptr));
        return 0;
    }

    TryRegisterSessionNotification(g_hWtsWnd);
    SetTimer(g_hWtsWnd, WTS_REFRESH_TIMER_ID, WTS_REFRESH_INTERVAL_MS, nullptr);
    SetTimer(g_hWtsWnd, RELAY_POLL_TIMER_ID, RELAY_POLL_INTERVAL_MS, nullptr);
    QueryWtsState();  // initial fill — widget may already be injected

    Wh_Log(L"WTS: notification window ready HWND=%p class=%s",
        g_hWtsWnd, WTS_WND_CLASS);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hWtsWnd && IsWindow(g_hWtsWnd))
        DestroyWindow(g_hWtsWnd);
    UnregisterClassW(WTS_WND_CLASS, GetModuleHandleW(nullptr));
    return 0;
}

void StartWtsThread() {
    if (g_hWtsThread) return;
    if (!g_hWtsThreadReady)
        g_hWtsThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hWtsThreadReady);
    if (!g_hWtsThreadReady) { Wh_Log(L"Thread readiness event creation failed"); return; }
    DWORD threadId = 0;
    g_hWtsThread = CreateThread(nullptr, 0, WtsThread, nullptr, 0, &threadId);
    g_wtsThreadId.store(g_hWtsThread ? threadId : 0);
}

void StopWtsThread() {
    DWORD threadId = g_wtsThreadId.load();
    if (threadId) {
        if (g_hWtsThreadReady)
            WaitForSingleObject(g_hWtsThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hWtsThread) {
        WaitForSingleObject(g_hWtsThread, INFINITE);
        CloseHandle(g_hWtsThread);
        g_hWtsThread = nullptr;
        g_wtsThreadId.store(0);
    }
}

// ── Correlated DVC requests and action feedback ───────────────────────────
// One owned worker performs open/write/read/close away from the XAML thread.
// Reading before closing preserves D-37's delivery-race fix. Requests carry
// protocol version, correlation ID and (for actions) PID + process birth.
// v1 replies are upgrade-required, never action confirmation. No retries.

struct ClientView {
    bool known = false, sessionActive = false, fullscreen = false, iconic = false;
    DWORD pid = 0;
    ULONGLONG birth = 0, snapshotTick = 0, lastReplyTick = 0;
    rdpstk::Result result = rdpstk::Result::Unavailable;
};
std::mutex g_ClientViewMutex;
ClientView g_ClientView;
constexpr ULONGLONG kClientViewStaleMs = 10000;
ClientView GetClientView() {
    std::lock_guard<std::mutex> lock(g_ClientViewMutex);
    ClientView view = g_ClientView;
    if (view.known && !rdpstk::Fresh(view.lastReplyTick, GetTickCount64(), kClientViewStaleMs)) {
        view.known = false; view.result = rdpstk::Result::Unavailable;
    }
    return view;
}
std::atomic<bool> g_RelayPollInFlight{false}, g_RelayActionBusy{false};
std::mutex g_ActionMutex;
BYTE g_ActionCommand = 0;
ClientView g_ActionBefore;
ULONGLONG g_ActionTick = 0;

void ApplyRelayReply(const rdpstk::Reply& reply) {
    {
        std::lock_guard<std::mutex> lock(g_ClientViewMutex);
        g_ClientView = {reply.known != 0, reply.active != 0, reply.fullscreen != 0,
            reply.iconic != 0, reply.targetPid, reply.targetBirth, reply.snapshotTick,
            GetTickCount64(), (rdpstk::Result)reply.result};
    }
    // Refresh even on unknown/failed/unchanged replies: expiration is a visible
    // transition and feedback needs a repaint after its retention interval.
    RefreshWidgetUI();
}
void ObserveHostAction() {
    if (!g_RelayActionBusy.load()) return;
    ClientView view = GetClientView();
    std::lock_guard<std::mutex> lock(g_ActionMutex);
    if (!g_ActionCommand) return; // send is still in flight
    const auto& before = g_ActionBefore;
    if (view.known && view.pid == before.pid && view.birth == before.birth &&
        view.snapshotTick > before.snapshotTick &&
        rdpstk::WindowOutcome(g_ActionCommand == RELAY_CMD_MINIMIZE_BYTE ? 1 : 3,
                              before.fullscreen, view.fullscreen, view.iconic)) {
        g_RelayActionBusy.store(false); g_ActionCommand = 0;
        HostNotice(L"Requested client window state observed");
    } else if (GetTickCount64() - g_ActionTick > 5000) {
        g_RelayActionBusy.store(false); g_ActionCommand = 0;
        HostNotice(L"Client action outcome unknown; not retried");
    }
}

rdpstk::Reply RunRelayRequest(BYTE command, const ClientView& before) {
    rdpstk::Request request;
    static std::atomic<ULONGLONG> nextRequest{1};
    request.requestId = nextRequest.fetch_add(1); request.command = command;
    if (command != RELAY_CMD_QUERY_STATUS_BYTE) {
        request.targetPid = before.pid; request.targetBirth = before.birth;
    }
    rdpstk::Reply reply; reply.command = command; reply.requestId = request.requestId;
    reply.result = (DWORD)rdpstk::Result::Unavailable;
    if (!IsRdpNow()) return reply;
    struct Channel {
        HANDLE handle = nullptr;
        ~Channel() { if (handle) WTSVirtualChannelClose(handle); }
    } channel;
    channel.handle = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION,
        const_cast<LPSTR>(RELAY_DVC_CHANNEL), WTS_CHANNEL_OPTION_DYNAMIC);
    if (!channel.handle) return reply;
    DWORD written = 0;
    if (!WTSVirtualChannelWrite(channel.handle, reinterpret_cast<PCHAR>(&request), sizeof(request), &written) ||
        written != sizeof(request)) { reply.result = (DWORD)rdpstk::Result::Unknown; return reply; }
    BYTE buffer[CHANNEL_PDU_LENGTH]{}; DWORD bytes = 0;
    if (!WTSVirtualChannelRead(channel.handle, 4000, reinterpret_cast<PCHAR>(buffer), sizeof(buffer), &bytes) || !bytes) {
        reply.result = (DWORD)rdpstk::Result::TimedOut; return reply;
    }
    rdpstk::Reply parsed;
    if (rdpstk::ParseReply(buffer, bytes, request, &parsed)) return parsed;
    reply.result = (DWORD)rdpstk::Result::Unsupported;
    return reply;
}

void SendRelayCommandAsync(BYTE command, PCWSTR label) {
    ClientView before = GetClientView();
    if (!before.known || !before.sessionActive) { HostNotice(rdpstk::ResultText(before.result)); return; }
    bool expected = false;
    if (!g_RelayActionBusy.compare_exchange_strong(expected, true)) {
        HostNotice(L"Another client action is in progress"); return;
    }
    HostNotice(std::wstring(label) + L": sending...");
    if (!g_HostWork.Post([command, label, before] {
        struct Reset { bool accepted = false; ~Reset() { if (!accepted) g_RelayActionBusy.store(false); } } reset;
        rdpstk::Reply reply;
        try { reply = RunRelayRequest(command, before); } WH_CATCH(L"Relay request")
        ApplyRelayReply(reply);
        auto result = (rdpstk::Result)reply.result;
        if (result == rdpstk::Result::Accepted) {
            std::lock_guard<std::mutex> lock(g_ActionMutex);
            g_ActionCommand = command; g_ActionBefore = GetClientView(); g_ActionTick = GetTickCount64();
            reset.accepted = true;
        } else { g_RelayActionBusy.store(false); }
        HostNotice(std::wstring(label) + L": " + rdpstk::ResultText(result));
    })) {
        g_RelayActionBusy.store(false); HostNotice(L"Client action unavailable during shutdown or busy queue");
    }
}

void PollClientStateAsync() {
    ObserveHostAction();
    RefreshWidgetUI(); // also repaint on cached state expiry / expired feedback
    if (!IsRdpNow()) return;
    bool expected = false;
    if (!g_RelayPollInFlight.compare_exchange_strong(expected, true)) return;
    if (!g_HostWork.Post([] {
        struct Reset { ~Reset() { g_RelayPollInFlight.store(false); } } reset;
        rdpstk::Reply reply;
        try { reply = RunRelayRequest(RELAY_CMD_QUERY_STATUS_BYTE, {}); } WH_CATCH(L"Relay poll")
        ApplyRelayReply(reply); ObserveHostAction();
    })) g_RelayPollInFlight.store(false);
}

// ── XAML injection state ──────────────────────────────────────────────────

constexpr std::wstring_view kWidgetRootName   = L"RdpstkHostWidgetRoot";
constexpr std::wstring_view kClientNameName   = L"RdpstkHostClientName";
constexpr std::wstring_view kStateName        = L"RdpstkHostState";
constexpr std::wstring_view kDisconnectName   = L"RdpstkHostDisconnect";
constexpr std::wstring_view kMinimizeName     = L"RdpstkHostMinimize";
constexpr std::wstring_view kFullscreenName   = L"RdpstkHostFullscreen";
constexpr std::wstring_view kClipboardName    = L"RdpstkHostClipboard";
constexpr std::wstring_view kClipboardWarnName = L"RdpstkHostClipboardWarn";
constexpr std::wstring_view kTaskbarFrameClass  = L"Taskbar.TaskbarFrame";
constexpr std::wstring_view kRootGridName       = L"RootGrid";
constexpr std::wstring_view kSystemTrayGridName = L"SystemTrayFrameGrid";
constexpr std::wstring_view kSystemTrayFrameClass = L"SystemTray.SystemTrayFrame";

// ── Per-monitor widget instances ──────────────────────────────────────────
// Same per-taskbar-frame instance tracking as the client mod's explorer
// branch, and for the same reason: Windows 11 builds a separate
// Taskbar.TaskbarFrame (with its own tray) per monitor, and the old single
// g_WidgetRoot / g_SystemTray globals let whichever monitor's tray bound
// first drive every monitor's margin, while only the last-injected widget
// was ever re-margined. One instance per frame: widget, root grid, that
// monitor's own tray, and its resize subscription.
struct WidgetInstance {
    weak_ref<Grid>             widget{ nullptr };
    weak_ref<Grid>             rootGrid{ nullptr };
    weak_ref<FrameworkElement> tray{ nullptr };
    event_token                trayResizeToken{};
};
std::mutex g_WidgetMutex;
std::vector<WidgetInstance> g_Instances;  // guarded by g_WidgetMutex
// Permanent margin diagnostics: run counter and ms baseline for the "[margin]"
// log lines. Kept deliberately — the tray's place in the visual tree has
// already moved once across Windows 11 builds, and when it moves again the
// only visible symptom is a silent overlap.
std::atomic<unsigned> g_MarginSeq{ 0 };
ULONGLONG g_ModInitTick = 0;
std::atomic<bool> g_ScanPending{ false };
std::atomic<bool> g_TaskbarViewDllLoaded{ false };
std::atomic<int> g_HookCallCounter{ 0 };
std::atomic<bool> g_Unloading{ false };
std::thread g_PollForDllThread;
std::thread g_InitialScanThread;

// ── Visual-tree helpers ───────────────────────────────────────────────────
// Cached vtable slot index — avoids rescanning every hook call once found.
std::atomic<int> g_FrameworkElementSlot{ -1 };

bool SlotHasVtablePointer(void* candidate) {
    // Read the pointer stored at this slot (safe — candidate is within the live
    // object). Require it to be committed MEM_IMAGE memory: vtables always live
    // in a module's .rdata section. Data members (refcount ≈ 2, heap pointers)
    // point into MEM_FREE or MEM_PRIVATE and are rejected before any QI call.
    void* vtable = *reinterpret_cast<void**>(candidate);
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(vtable, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Type  != MEM_IMAGE)  return false;
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE |
                                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & kReadable) && !(mbi.Protect & PAGE_GUARD);
}

FrameworkElement GetFrameworkElementFromNative(void* pThis) {
    auto trySlot = [&](int slot) -> FrameworkElement {
        void* candidate = static_cast<char*>(pThis) + slot * sizeof(void*);
        if (!SlotHasVtablePointer(candidate)) {
            return nullptr;
        }
        try {
            FrameworkElement fe{ nullptr };
            HRESULT hr = reinterpret_cast<::IUnknown*>(candidate)->QueryInterface(
                winrt::guid_of<FrameworkElement>(), winrt::put_abi(fe));
            return (SUCCEEDED(hr) && fe) ? fe : nullptr;
        } catch (...) {
            return nullptr;
        }
    };

    // Fast path: reuse cached slot.
    int cached = g_FrameworkElementSlot.load(std::memory_order_relaxed);
    if (cached >= 0) {
        if (auto fe = trySlot(cached)) return fe;
        g_FrameworkElementSlot.store(-1, std::memory_order_relaxed);
    }

    // Slow path: scan and cache.
    for (int slot = 0; slot <= 8; ++slot) {
        if (auto fe = trySlot(slot)) {
            g_FrameworkElementSlot.store(slot, std::memory_order_relaxed);
            return fe;
        }
    }
    return nullptr;
}

FrameworkElement WalkUpToTaskbarFrame(FrameworkElement start) {
    FrameworkElement cur = start;
    while (cur) {
        if (winrt::get_class_name(cur) == kTaskbarFrameClass) return cur;
        auto parent = VisualTreeHelper::GetParent(cur);
        cur = parent ? parent.try_as<FrameworkElement>() : nullptr;
    }
    return nullptr;
}

Grid FindRootGrid(FrameworkElement taskbarFrame) {
    if (!taskbarFrame) return nullptr;
    int count = VisualTreeHelper::GetChildrenCount(taskbarFrame);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(taskbarFrame, i);
        if (!child) continue;
        auto fe = child.try_as<FrameworkElement>();
        if (fe && std::wstring(fe.Name()) == kRootGridName)
            return fe.try_as<Grid>();
    }
    return nullptr;
}

template <typename T>
T FindByName(FrameworkElement parent, std::wstring_view name) {
    if (!parent) return nullptr;
    auto fe = parent.try_as<FrameworkElement>();
    if (fe && std::wstring(fe.Name()) == name) {
        if (auto t = fe.try_as<T>()) return t;
    }
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(parent, i);
        if (!child) continue;
        auto cfe = child.try_as<FrameworkElement>();
        if (!cfe) continue;
        if (auto found = FindByName<T>(cfe, name)) return found;
    }
    return nullptr;
}

template <typename T>
T FindByClassName(FrameworkElement parent, std::wstring_view className) {
    if (!parent) return nullptr;
    if (winrt::get_class_name(parent) == className) {
        if (auto t = parent.try_as<T>()) return t;
    }
    int count = VisualTreeHelper::GetChildrenCount(parent);
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(parent, i);
        if (!child) continue;
        auto cfe = child.try_as<FrameworkElement>();
        if (!cfe) continue;
        if (auto found = FindByClassName<T>(cfe, className)) return found;
    }
    return nullptr;
}

// Topmost FrameworkElement of the visual tree `start` lives in (the XamlRoot
// content). On Windows 11 (verified on build 26200.9168 via UI Automation, and
// matching the upstream taskbar-vertical mod's lookup) the system tray
// `SystemTray.SystemTrayFrame` is a *sibling* of `Taskbar.TaskbarFrame` under
// this root — it is NOT a descendant of `Grid#RootGrid`. Any search for the
// tray that starts at RootGrid (direct children or recursive) can never find
// it, which silently zeroes the tray width and overlaps the widget onto the
// clock and tray icons.
FrameworkElement FindVisualRoot(FrameworkElement start) {
    FrameworkElement cur = start;
    while (cur) {
        auto parent = VisualTreeHelper::GetParent(cur);
        auto pfe = parent ? parent.try_as<FrameworkElement>() : nullptr;
        if (!pfe) break;
        cur = pfe;
    }
    return cur;
}

// Locate the system tray element from anywhere in the taskbar's visual tree.
// Order: visual root → SystemTray.SystemTrayFrame (by class) →
// Grid#SystemTrayFrameGrid (by name), falling back to the frame itself, then
// to a by-name search from the root for layouts that nest the grid elsewhere.
FrameworkElement FindSystemTray(FrameworkElement anchor) {
    auto root = FindVisualRoot(anchor);
    if (!root) return nullptr;
    if (auto frame = FindByClassName<FrameworkElement>(root, kSystemTrayFrameClass)) {
        if (auto grid = FindByName<FrameworkElement>(frame, kSystemTrayGridName)) return grid;
        return frame;
    }
    return FindByName<FrameworkElement>(root, kSystemTrayGridName);
}

// Width of the Win32 TrayNotifyWnd child of Shell_TrayWnd, in DIPs. Used only
// as an independent cross-check next to the XAML tray's ActualWidth in the
// "[margin]" log lines; -1 when unavailable.
double TrayNotifyWndWidthDips() {
    HWND shell = FindWindowW(L"Shell_TrayWnd", nullptr);
    HWND tray  = shell ? FindWindowExW(shell, nullptr, L"TrayNotifyWnd", nullptr) : nullptr;
    RECT rc{};
    if (!tray || !GetWindowRect(tray, &rc)) return -1.0;
    UINT dpi = GetDpiForWindow(tray);
    if (!dpi) dpi = 96;
    return (rc.right - rc.left) * 96.0 / dpi;
}

// One-shot at injection: log the top-level children of the visual root, so the
// next time a Windows build moves the tray the log shows the new shape instead
// of the symptom. Must run on the UI thread.
void LogVisualRootChildren(FrameworkElement root) {
    if (!root) { Wh_Log(L"[tree] visual root not found"); return; }
    int count = VisualTreeHelper::GetChildrenCount(root);
    Wh_Log(L"[tree] visual root %s#%s children=%d actualWidth=%.1f",
           winrt::get_class_name(root).c_str(), root.Name().c_str(), count, root.ActualWidth());
    for (int i = 0; i < count; ++i) {
        auto child = VisualTreeHelper::GetChild(root, i);
        auto fe = child ? child.try_as<FrameworkElement>() : nullptr;
        if (!fe) continue;
        Wh_Log(L"[tree]   [%d] %s#%s actualWidth=%.1f",
               i, winrt::get_class_name(fe).c_str(), fe.Name().c_str(), fe.ActualWidth());
    }
}

SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

// ── Theme detection (shared toolkit pattern — verbatim from the client mod) ─
//
// The taskbar's own light/dark follows SystemUsesLightTheme (NOT
// AppsUseLightTheme). The toolkit's panels deliberately render the OPPOSITE
// tone — a light taskbar gets a dark panel, a dark taskbar a light one — so
// the widget keeps strong contrast against its surroundings. This block and
// the palette below are the toolkit's one shared theme pattern, carried as an
// identical copy in both mod files (the SetElementTooltip convention); the
// palettes must stay identical across the two files. Live reaction: the WTS
// thread's hidden top-level window watches
// WM_SETTINGCHANGE("ImmersiveColorSet") and reapplies the palette.
std::atomic<bool> g_SystemLightTheme{ false };

bool IsSystemLightTheme() {
    DWORD v = 0, cb = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &cb)
            != ERROR_SUCCESS)
        return false;  // value absent ⇒ Windows defaults to dark
    return v != 0;
}

// Re-reads the theme; true when it changed (the new value is already stored).
bool RefreshSystemTheme() {
    bool light = IsSystemLightTheme();
    return g_SystemLightTheme.exchange(light) != light;
}

// ── Toolkit panel visual spec (shared with the client mod) ────────────────
//
// One coherent look across both of the toolkit's taskbar panels, applied as
// identical code in both mod files. The numbers are the spec:
//   panel corner radius 8 · content margin 12 left / 8 right · button glyphs
//   Segoe MDL2 on a transparent, borderless button with 7×3 padding, 6 px
//   spacing, and a 4 px hover corner radius · name line at the configured
//   font size, state line 2 pt smaller.
struct PanelPalette {
    Windows::UI::Color tint;       // acrylic tint
    double             tintOpacity;
    Windows::UI::Color primary;    // full-strength text and button glyphs
    Windows::UI::Color secondary;  // dimmed second line
    Windows::UI::Color alert;      // not-responding red (client uses it)
    Windows::UI::Color warn;       // warning amber (the clipboard line)
};

PanelPalette GetPanelPalette() {
    if (g_SystemLightTheme.load(std::memory_order_relaxed)) {
        // Light system theme → DARK panel (inverted on purpose).
        return {
            ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A), 0.6,
            ColorHelper::FromArgb(0xFF, 0xFF, 0xFF, 0xFF),
            ColorHelper::FromArgb(0xB3, 0xFF, 0xFF, 0xFF),
            ColorHelper::FromArgb(0xFF, 0xFF, 0x60, 0x60),
            ColorHelper::FromArgb(0xFF, 0xFF, 0xC8, 0x3D),
        };
    }
    // Dark system theme → LIGHT panel.
    return {
        ColorHelper::FromArgb(0xFF, 0xF2, 0xF2, 0xF2), 0.7,
        ColorHelper::FromArgb(0xFF, 0x1B, 0x1B, 0x1B),
        ColorHelper::FromArgb(0xB3, 0x1B, 0x1B, 0x1B),
        ColorHelper::FromArgb(0xFF, 0xC4, 0x2B, 0x1C),
        ColorHelper::FromArgb(0xFF, 0x9A, 0x5B, 0x00),
    };
}

// The spec's button treatment, applied to every action button at creation.
// Foreground is deliberately NOT set here — ApplyThemeToWidget owns it.
void StyleActionButton(Button const& btn) {
    btn.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    btn.Background(MakeBrush(0x00, 0, 0, 0));
    btn.BorderThickness(ThicknessHelper::FromUniformLength(0));
    btn.Padding(ThicknessHelper::FromLengths(7, 3, 7, 3));
    btn.Margin(ThicknessHelper::FromLengths(6, 0, 0, 0));
    btn.CornerRadius(CornerRadiusHelper::FromUniformRadius(4.0));
    btn.VerticalAlignment(VerticalAlignment::Center);
    btn.IsEnabled(false);   // enabled by ApplyStateToWidget when applicable
    btn.Opacity(0.35);
}

// Attaches a wrapping, multi-line tooltip to an element. Same helper (and same
// reuse-the-existing-TextBlock behaviour) as the client mod's, so the two
// surfaces' tooltips look and behave identically. UI thread only.
void SetElementTooltip(DependencyObject const& target, PCWSTR text) {
    try {
        if (auto tip = ToolTipService::GetToolTip(target).try_as<ToolTip>()) {
            if (auto tb = tip.Content().try_as<TextBlock>()) {
                if (tb.Text() != text)
                    tb.Text(text);
                return;
            }
        }
        TextBlock tb;
        tb.Text(text);
        tb.TextWrapping(TextWrapping::Wrap);
        ToolTip tip;
        tip.Content(tb);
        ToolTipService::SetToolTip(target, tip);
    } WH_CATCH(L"SetElementTooltip")
}

// ── Widget ────────────────────────────────────────────────────────────────

void UpdateWidgetMarginFor(Grid const& widget, const wchar_t* reason);
void ApplyStateToWidget(Grid widget);

// Live widgets snapshot (dead weak refs skipped). Any thread.
std::vector<Grid> GetLiveWidgets() {
    std::vector<Grid> out;
    std::lock_guard<std::mutex> g(g_WidgetMutex);
    for (auto& inst : g_Instances)
        if (auto w = inst.widget.get()) out.push_back(w);
    return out;
}

// Paints the current (inverted) palette onto one widget: acrylic background,
// text foregrounds, button glyph color, and the amber warning line. Called at
// build time and again on a live theme change. UI thread only.
void ApplyThemeToWidget(Grid const& widget) {
    if (!widget) return;
    PanelPalette pal = GetPanelPalette();
    try {
        AcrylicBrush acrylic;
        acrylic.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
        acrylic.TintColor(pal.tint);
        acrylic.TintOpacity(pal.tintOpacity);
        widget.Background(acrylic);
    } WH_CATCH(L"ApplyThemeToWidget/acrylic")
    if (auto tb = FindByName<TextBlock>(widget, kClientNameName))
        tb.Foreground(SolidColorBrush(pal.primary));
    if (auto tb = FindByName<TextBlock>(widget, kStateName))
        tb.Foreground(SolidColorBrush(pal.secondary));
    if (auto tb = FindByName<TextBlock>(widget, kClipboardWarnName))
        tb.Foreground(SolidColorBrush(pal.warn));
    for (auto name : { kClipboardName, kMinimizeName, kFullscreenName,
                       kDisconnectName })
        if (auto btn = FindByName<Button>(widget, name))
            btn.Foreground(SolidColorBrush(pal.primary));
}

// Live theme change: re-read the system theme and, when it flipped, repaint
// every widget with the newly inverted palette. Any thread.
void RefreshThemeUI() {
    if (g_Unloading.load()) return;
    if (!RefreshSystemTheme()) return;
    bool light = g_SystemLightTheme.load();
    Wh_Log(L"[theme] system theme now %s — applying the %s panel palette",
        light ? L"light" : L"dark", light ? L"dark" : L"light");
    for (auto& widget : GetLiveWidgets()) {
        try {
            auto weak = make_weak(widget);
            g_Dispatch.Post(widget,
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [weak]() {
                    if (auto w = weak.get()) {
                        ApplyThemeToWidget(w);
                        ApplyStateToWidget(w);
                    }
                });
        } WH_CATCH(L"RefreshThemeUI/dispatch")
    }
}

// Bind this widget's instance to its own monitor's tray (and its SizeChanged
// subscription) if not already bound to a live element. Lazy on purpose: the
// tray subtree may not exist yet at cold-start injection, and every caller
// (inject, widget Loaded, tray resize, settings change) runs on the UI thread
// after a layout pass, so a tray that appears later is picked up on the next
// pass without a timer. The tray is found from THIS widget's own visual root,
// never cached globally, so one monitor's tray width cannot leak into another
// monitor's margin. Returns the bound tray element, or nullptr. Must run on
// the XAML UI thread.
FrameworkElement BindSystemTrayFor(Grid const& widget) {
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        for (auto& inst : g_Instances) {
            if (inst.widget.get() != widget) continue;
            if (auto tray = inst.tray.get()) return tray;
            break;
        }
    }
    FrameworkElement tray = FindSystemTray(widget);
    if (!tray) return nullptr;

    // Subscribe in every position mode: the handler only recomputes this one
    // widget's margin, and this way a later settings switch to Right needs no
    // re-subscription.
    event_token token = tray.SizeChanged(
        [weakWidget = make_weak(widget)](IInspectable const&,
                                         SizeChangedEventArgs const&) {
            if (auto w = weakWidget.get())
                UpdateWidgetMarginFor(w, L"tray-sizechanged");
        });
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        for (auto& inst : g_Instances) {
            if (inst.widget.get() != widget) continue;
            inst.tray = make_weak(tray);
            inst.trayResizeToken = token;
            break;
        }
    }
    Wh_Log(L"[margin] tray bound for widget %p: %s#%s actualWidth=%.1f",
           winrt::get_abi(widget), winrt::get_class_name(tray).c_str(),
           tray.Name().c_str(), tray.ActualWidth());
    return tray;
}

// Recompute one widget's margin based on the current position mode.
// Right: tracks that widget's own monitor's tray width so the widget stays
// adjacent to that monitor's system tray. Left/Center: a fixed margin from
// the respective edge (no tray dependency). `reason` is a short tag naming
// the caller for the "[margin]" log line. Must run on the XAML UI thread.
void UpdateWidgetMarginFor(Grid const& widget, const wchar_t* reason) {
    if (!widget) return;

    FrameworkElement tray = BindSystemTrayFor(widget);

    unsigned  seq = ++g_MarginSeq;
    ULONGLONG tMs = GetTickCount64() - g_ModInitTick;
    double gap       = (double)g_Settings.offsetX;
    double trayWidth = tray ? tray.ActualWidth() : 0.0;
    double margin    = gap;
    if (g_Settings.widgetPosition == WidgetPosition::Right) {
        margin = trayWidth + gap;
        widget.Margin(ThicknessHelper::FromLengths(0, 0, margin, 0));
    } else {
        // Left: Margin.Left is a gap from the left edge.
        // Center: HorizontalAlignment::Center centers the widget; Margin.Left
        //         nudges it rightward from that center point when offsetX > 0.
        widget.Margin(ThicknessHelper::FromLengths(gap, 0, 0, 0));
    }

    // Permanent, low-noise diagnostic: fires at inject, widget Loaded, tray
    // resize (icon count change) and settings change — not per layout pass.
    // trayNotifyWndDips is the Win32 tray window width as an independent
    // reference: in Right mode the PRIMARY taskbar's trayActualWidth should be
    // close to it (secondary taskbars have their own, smaller trays).
    Wh_Log(L"[margin] #%u t+%llums reason=%s widget=%p mode=%d tray=%s "
           L"trayActualWidth=%.1f trayNotifyWndDips=%.1f gap=%.0f -> margin=%.1f",
           seq, tMs, reason, winrt::get_abi(widget),
           (int)g_Settings.widgetPosition,
           tray ? L"found" : L"MISSING", trayWidth, TrayNotifyWndWidthDips(),
           gap, margin);
}

// Push the current WTS state into the widget's elements.
// Must run on the XAML UI thread.
void ApplyStateToWidget(Grid widget) {
    if (!widget) return;

    WtsState s;
    {
        std::lock_guard<std::mutex> g(g_WtsMutex);
        s = g_WtsState;
    }

    bool rdp = (s.protocol == kProtocolRdp);
    std::wstring nameLine = !s.clientName.empty() ? s.clientName
                          : rdp ? L"RDP client"
                                : L"Console";
    std::wstring stateLine = ConnectStateName(s.connectState);
    if (rdp) stateLine += L" · RDP";

    if (auto tb = FindByName<TextBlock>(widget, kClientNameName))
        tb.Text(nameLine);
    auto notice = HostNoticeText();
    ClientView peer = GetClientView();
    if (auto tb = FindByName<TextBlock>(widget, kStateName)) {
        auto text = !notice.empty() ? notice : rdp && !peer.known
            ? std::wstring(L"Client: ") + rdpstk::ResultText(peer.result) : stateLine;
        tb.Text(text);
        SetElementTooltip(tb, text.c_str());
    }
    if (auto btn = FindByName<Button>(widget, kDisconnectName)) {
        // Enabled only over RDP — never offer to disconnect the physical
        // console session (that would kick the local user to the lock screen).
        btn.IsEnabled(rdp);
        btn.Opacity(rdp ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kMinimizeName)) {
        // Enabled only over RDP — a minimize signal is meaningless on the
        // physical console (no remote mstsc window to minimize).
        btn.IsEnabled(rdp && peer.known && peer.sessionActive && !g_RelayActionBusy.load());
        btn.Opacity(btn.IsEnabled() ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kClipboardName)) {
        // Enabled only over RDP — there is no rdpclip on a console session, so
        // the action is meaningless there. Re-checked at click time as well.
        btn.IsEnabled(rdp);
        btn.Opacity(btn.IsEnabled() ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kFullscreenName)) {
        // Enabled only over RDP, like every other action button, re-checked
        // at click time as well. Glyph, accessible name, and tooltip relabel
        // from the CACHED client view (the relay's status replies — Task 5),
        // matching the client panel's own fullscreen button's relabeling
        // pattern, so this is never a blind, unlabeled toggle. When the view
        // is unknown (no reply yet, stale, or the client mod isn't reporting)
        // the button says so honestly instead of guessing a direction.
        btn.IsEnabled(rdp && peer.known && peer.sessionActive && !g_RelayActionBusy.load());
        btn.Opacity(btn.IsEnabled() ? 1.0 : 0.35);
        ClientView v = GetClientView();
        bool known = v.known && v.sessionActive;
        bool fs    = known && v.fullscreen;
        wchar_t glyph[2] = { fs ? (wchar_t)0xE73F : (wchar_t)0xE740, 0 };
        btn.Content(box_value(hstring{ glyph }));
        if (known) {
            AutomationProperties::SetName(btn,
                fs ? L"Switch the client to windowed"
                   : L"Switch the client to fullscreen");
            SetElementTooltip(btn, fs
                ? L"Switches the Remote Desktop window on the connecting "
                  L"computer to windowed mode."
                : L"Switches the Remote Desktop window on the connecting "
                  L"computer to fullscreen.");
        } else {
            AutomationProperties::SetName(btn,
                L"Switch the client between fullscreen and windowed");
            SetElementTooltip(btn,
                L"Switches the Remote Desktop window on the connecting "
                L"computer between fullscreen and windowed. Its current mode "
                L"is not known right now — the client has not reported it.");
        }
    }

    // The clipboard-helper line reports the negative and only the negative.
    // When a verified rdpclip is present — or when we could not tell — the
    // line is collapsed and claims nothing: an rdpclip that is running but
    // wedged is a real possibility this check cannot distinguish, so "clipboard
    // OK" is a claim the mod has no basis for (EXPANSION_RESEARCH §4.2).
    bool clipWarn = rdp && s.clipboard == ClipboardHelper::NotRunning;
    if (auto tb = FindByName<TextBlock>(widget, kClipboardWarnName)) {
        tb.Text(clipWarn ? L"Clipboard helper not running" : L"");
        tb.Visibility(clipWarn ? Visibility::Visible : Visibility::Collapsed);
    }
}

Grid BuildWidget() {
    Grid root;
    root.Name(kWidgetRootName);
    root.Width((double)g_Settings.panelWidth);
    root.HorizontalAlignment(
        g_Settings.widgetPosition == WidgetPosition::Left   ? HorizontalAlignment::Left   :
        g_Settings.widgetPosition == WidgetPosition::Center ? HorizontalAlignment::Center :
                                                              HorizontalAlignment::Right);
    root.VerticalAlignment(VerticalAlignment::Stretch);
    root.CornerRadius(CornerRadiusHelper::FromUniformRadius(8.0));
    Canvas::SetZIndex(root, 2);
    // Span all columns so alignment is relative to full taskbar width.
    Grid::SetColumnSpan(root, 9999);
    Grid::SetRowSpan(root, 9999);
    // Margin is set dynamically via UpdateWidgetMargin() based on the position mode.

    // Background and all foregrounds come from the theme-inverted palette —
    // applied once at the end of this function (ApplyThemeToWidget) and again
    // on every live theme change (RefreshThemeUI).

    Grid layout;
    layout.HorizontalAlignment(HorizontalAlignment::Stretch);
    layout.VerticalAlignment(VerticalAlignment::Center);
    layout.Margin(ThicknessHelper::FromLengths(12.0, 0.0, 8.0, 0.0));

    auto cols = layout.ColumnDefinitions();
    {
        ColumnDefinition cText;
        cText.Width(GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        cols.Append(cText);
        ColumnDefinition cClip;  // Restart-clipboard button
        cClip.Width(GridLengthHelper::Auto());
        cols.Append(cClip);
        ColumnDefinition cMin;   // Minimize button
        cMin.Width(GridLengthHelper::Auto());
        cols.Append(cMin);
        ColumnDefinition cFs;    // Fullscreen/windowed toggle button
        cFs.Width(GridLengthHelper::Auto());
        cols.Append(cFs);
        ColumnDefinition cBtn;   // Disconnect button
        cBtn.Width(GridLengthHelper::Auto());
        cols.Append(cBtn);
    }

    // Client name + connection state column.
    StackPanel textCol;
    textCol.Orientation(Orientation::Vertical);
    textCol.VerticalAlignment(VerticalAlignment::Center);

    TextBlock clientName;
    clientName.Name(kClientNameName);
    clientName.FontSize((double)g_Settings.fontSize);
    clientName.TextTrimming(TextTrimming::CharacterEllipsis);
    clientName.TextWrapping(TextWrapping::NoWrap);
    clientName.MaxLines(1);
    textCol.Children().Append(clientName);

    TextBlock state;
    state.Name(kStateName);
    state.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    state.TextTrimming(TextTrimming::CharacterEllipsis);
    state.TextWrapping(TextWrapping::NoWrap);
    state.MaxLines(1);
    textCol.Children().Append(state);

    // Clipboard-helper warning line. Collapsed unless QueryWtsState found no
    // verified rdpclip.exe in this session; there is deliberately no "clipboard
    // OK" counterpart (EXPANSION_RESEARCH §4.2). Amber rather than the state
    // line's dimmed white, so it reads as a warning and not as another status
    // field.
    TextBlock clipWarn;
    clipWarn.Name(kClipboardWarnName);
    clipWarn.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    clipWarn.TextTrimming(TextTrimming::CharacterEllipsis);
    clipWarn.TextWrapping(TextWrapping::NoWrap);
    clipWarn.MaxLines(1);
    clipWarn.Visibility(Visibility::Collapsed);
    textCol.Children().Append(clipWarn);

    Grid::SetColumn(textCol, 0);
    layout.Children().Append(textCol);

    // Disconnect — same clean-disconnect semantics as the Start menu's
    // Disconnect: the session keeps running, the client detaches. Glyph
    // matches the client mod's thumb-bar Disconnect (Segoe MDL2 0xE8BB).
    Button disconnect;
    disconnect.Name(kDisconnectName);
    disconnect.Content(box_value(hstring{L""}));
    StyleActionButton(disconnect);  // shared visual spec; theme sets foreground
    AutomationProperties::SetName(disconnect, L"Disconnect this RDP session");
    // Plain-language tooltip in the D-33 style: what the click does to the
    // session, not a restatement of the button's name. Matches the client
    // panel's Disconnect wording, phrased from this (host) side.
    SetElementTooltip(disconnect,
        L"Disconnects the person connected to this session. Their programs "
        L"keep running here.");
    disconnect.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            if (g_Unloading.load()) return;
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled) so a stale
            // widget state can never disconnect a console session.
            if (proto != kProtocolRdp || !IsRdpNow()) {
                Wh_Log(L"Disconnect clicked on non-RDP session — ignored");
                return;
            }
            Wh_Log(L"Disconnect: WTSDisconnectSession(current session)");
            // bWait=FALSE — never block the XAML UI thread on the operation.
            if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE,
                                      WTS_CURRENT_SESSION, FALSE)) {
                Wh_Log(L"WTSDisconnectSession failed GLE=%d", GetLastError());
                HostNotice(L"Disconnect request failed; see Windhawk log");
            } else HostNotice(L"Disconnect request accepted");
        }));
    Grid::SetColumn(disconnect, 4);
    layout.Children().Append(disconnect);

    // Fullscreen / windowed toggle for the CLIENT's Remote Desktop window
    // (Task 6). Sends RELAY_CMD_FULLSCREEN_TOGGLE over the same
    // open-write-read-close relay path as Minimize; the client mod's relay
    // receiver forwards it to the shared ToggleFullscreen. The glyph, name,
    // and tooltip are set by ApplyStateToWidget from the cached client view
    // (Task 5's status replies) so the button always names its actual
    // direction, or says the state is unknown. Initial glyph is the
    // fullscreen one only as a placeholder until the first Apply pass.
    Button fullscreen;
    fullscreen.Name(kFullscreenName);
    fullscreen.Content(box_value(hstring{L""}));
    StyleActionButton(fullscreen);  // shared visual spec; theme sets foreground
    AutomationProperties::SetName(fullscreen,
        L"Switch the client between fullscreen and windowed");
    fullscreen.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            if (g_Unloading.load()) return;
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled), matching
            // every other action button in this toolkit.
            if (proto != kProtocolRdp || !IsRdpNow()) {
                Wh_Log(L"Fullscreen toggle clicked on non-RDP session — ignored");
                return;
            }
            SendRelayCommandAsync(RELAY_CMD_FULLSCREEN_TOGGLE_BYTE,
                                  L"Fullscreen toggle");
        }));
    Grid::SetColumn(fullscreen, 3);
    layout.Children().Append(fullscreen);

    // Minimize the client window. Unlike Disconnect it does NOT act locally —
    // it sends one signal across the RDP Dynamic Virtual Channel to the
    // client-side mod's CitadelRdpTaskbarRelay receiver (command 0x01), via the
    // dvc-plugin relay, which minimizes the remote mstsc window on the client.
    // The open-write-read-close runs on the owned background worker so
    // explorer.exe's message loop never blocks on channel latency (D-12). Glyph
    // matches the client mod's thumb-bar Minimize (Segoe MDL2 0xE921).
    Button minimize;
    minimize.Name(kMinimizeName);
    minimize.Content(box_value(hstring{L""}));
    StyleActionButton(minimize);  // shared visual spec; theme sets foreground
    AutomationProperties::SetName(minimize, L"Minimize the RDP client window");
    // Same plain-language style as Disconnect and Restart Clipboard above.
    SetElementTooltip(minimize,
        L"Minimizes the Remote Desktop window on the computer connecting in. "
        L"The session stays connected.");
    minimize.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            if (g_Unloading.load()) return;
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled) so a stale
            // widget state can never fire a minimize on a console session.
            if (proto != kProtocolRdp || !IsRdpNow()) {
                Wh_Log(L"Minimize clicked on non-RDP session — ignored");
                return;
            }
            // Fire-and-forget on a background thread — never block the XAML UI
            // thread on WTSVirtualChannelOpenEx latency. The send now reads
            // the relay's reply before closing, which is what fixed the
            // dropped-command race (see SendRelayCommandAsync's banner).
            SendRelayCommandAsync(RELAY_CMD_MINIMIZE_BYTE, L"Minimize");
        }));
    Grid::SetColumn(minimize, 2);
    layout.Children().Append(minimize);

    // Restart the session's clipboard helper (rdpclip.exe) — the toolkit's fix
    // for the classic "copy-and-paste stopped working over RDP" failure
    // (EXPANSION_RESEARCH §4.1). Like Disconnect it acts locally, with no DVC
    // and no client-side involvement; like Minimize it runs off the XAML UI
    // thread on the owned worker, because the sequence blocks on
    // process termination. Glyph: Segoe MDL2 Paste (0xE77F).
    Button clipboard;
    clipboard.Name(kClipboardName);
    clipboard.Content(box_value(hstring{L"\uE77F"}));
    StyleActionButton(clipboard);  // shared visual spec; theme sets foreground
    AutomationProperties::SetName(clipboard, L"Restart the session's clipboard helper");
    // Plain-language tooltip in the style D-33 established for the client
    // panel's buttons — what the click does to the session, not a restatement
    // of the button's name. The data loss is stated outright rather than
    // softened: terminating rdpclip really does drop whatever the shared
    // clipboard is holding.
    SetElementTooltip(clipboard,
        L"Fixes copy-and-paste between this session and your computer by "
        L"restarting the clipboard helper. Whatever is currently on the shared "
        L"clipboard is discarded.");
    clipboard.Click(RoutedEventHandler(
        [](IInspectable const&, RoutedEventArgs const&) {
            if (g_Unloading.load()) return;
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled), so a stale
            // widget state can never run this against a console session.
            if (proto != kProtocolRdp || !IsRdpNow()) {
                Wh_Log(L"Restart clipboard clicked on non-RDP session — ignored");
                return;
            }
            RestartRdpClipAsync();
        }));
    Grid::SetColumn(clipboard, 1);
    layout.Children().Append(clipboard);

    root.Children().Append(layout);
    // Now that every named child exists, paint the theme-inverted palette.
    ApplyThemeToWidget(root);
    return root;
}

// Refresh from any thread: marshals ApplyStateToWidget onto each live
// widget's own UI thread (one widget per monitor's taskbar frame).
void RefreshWidgetUI() {
    if (g_Unloading.load()) return;
    for (auto& widget : GetLiveWidgets()) {
        try {
            auto weak = make_weak(widget);
            g_Dispatch.Post(widget,
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [weak]() {
                    if (auto w = weak.get()) ApplyStateToWidget(w);
                });
        } WH_CATCH(L"RefreshWidgetUI/dispatch")
    }
}

// ── Injection ─────────────────────────────────────────────────────────────

void InjectWidgetInto(Grid rootGrid) {
    if (!rootGrid) return;

    // Already injected into THIS frame's RootGrid?
    auto existing = FindByName<Grid>(rootGrid, kWidgetRootName);
    if (existing) {
        {
            std::lock_guard<std::mutex> g(g_WidgetMutex);
            bool tracked = false;
            for (auto& inst : g_Instances)
                if (inst.widget.get() == existing) { tracked = true; break; }
            if (!tracked) {
                WidgetInstance inst;
                inst.widget   = make_weak(existing);
                inst.rootGrid = make_weak(rootGrid);
                g_Instances.push_back(std::move(inst));
            }
        }
        ApplyStateToWidget(existing);
        return;
    }

    auto widget = BuildWidget();
    rootGrid.Children().Append(widget);

    size_t liveCount = 0;
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        // Prune entries whose widget died (its taskbar frame was torn down)
        // while adding the new one.
        g_Instances.erase(
            std::remove_if(g_Instances.begin(), g_Instances.end(),
                [](WidgetInstance& inst) { return inst.widget.get() == nullptr; }),
            g_Instances.end());
        WidgetInstance inst;
        inst.widget   = make_weak(widget);
        inst.rootGrid = make_weak(rootGrid);
        g_Instances.push_back(std::move(inst));
        liveCount = g_Instances.size();
    }

    // One-shot structural dump: the tray is a sibling of Taskbar.TaskbarFrame
    // under the visual root, not a child of RootGrid (see FindVisualRoot), and
    // the tray lookup in UpdateWidgetMarginFor() starts from that root.
    LogVisualRootChildren(FindVisualRoot(rootGrid));

    // Binds this frame's own tray (if it exists yet) and subscribes to its
    // SizeChanged so the margin tracks icon-count changes; sets the initial
    // margin.
    UpdateWidgetMarginFor(widget, L"inject");
    // Re-run once the first layout pass completes so ActualWidth() is valid
    // (and to bind a tray that did not exist at injection time).
    widget.Loaded([](IInspectable const& sender, RoutedEventArgs const&) {
        if (auto w = sender.try_as<Grid>())
            UpdateWidgetMarginFor(w, L"loaded");
    });
    ApplyStateToWidget(widget);

    Wh_Log(L"Widget injected into taskbar RootGrid (%u instance(s) tracked)",
           (unsigned)liveCount);
}

void ScheduleScanAsync(FrameworkElement startNode) {
    if (!startNode) return;
    if (g_Unloading.load()) return;
    bool expected = false;
    if (!g_ScanPending.compare_exchange_strong(expected, true)) return;

    auto weak = make_weak(startNode);
    try {
        g_Dispatch.Post(startNode,
            Windows::UI::Core::CoreDispatcherPriority::Low,
            [weak]() {
                g_ScanPending = false;
                if (g_Unloading.load()) return;
                auto node = weak.get();
                if (!node) return;
                try {
                    auto frame = WalkUpToTaskbarFrame(node);
                    if (!frame) return;
                    auto rootGrid = FindRootGrid(frame);
                    if (!rootGrid) return;
                    InjectWidgetInto(rootGrid);
                } catch (...) {
                    Wh_Log(L"[inject] Exception during XAML tree walk");
                }
            });
    } catch (...) {
        g_ScanPending = false;
        Wh_Log(L"[inject] Exception scheduling on dispatcher");
    }
}

void RemoveWidget() {
    // Take the whole instance list; each entry is removed on its own
    // dispatcher (one widget per monitor's taskbar frame).
    std::vector<WidgetInstance> instances;
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        instances = std::move(g_Instances);
        g_Instances.clear();
    }
    for (auto& inst : instances) {
        try {
            Grid rootGrid = inst.rootGrid.get();
            if (!rootGrid) continue;
            auto weakGrid = make_weak(rootGrid);
            auto weakTray = inst.tray;
            event_token token = inst.trayResizeToken;
            g_Dispatch.PostCleanup(rootGrid,
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [weakGrid, weakTray, token]() {
                    // All XAML cleanup must happen on the dispatcher thread.
                    if (token.value) {
                        if (auto tray = weakTray.get())
                            try { tray.SizeChanged(token); } catch (...) {}
                    }
                    auto g = weakGrid.get();
                    if (!g) return;
                    auto children = g.Children();
                    for (int i = (int)children.Size() - 1; i >= 0; --i) {
                        auto el = children.GetAt(i).try_as<FrameworkElement>();
                        if (el && std::wstring(el.Name()) == kWidgetRootName) {
                            children.RemoveAt(i);
                        }
                    }
                });
        } catch (...) { TrackedDispatch::RetainCode(); }
    }
}

// ── Hooks ─────────────────────────────────────────────────────────────────
// TaskListButton::UpdateVisualStates fires frequently once the taskbar's XAML
// tree is alive, and its `this` wraps a FrameworkElement inside that tree —
// a reliable entry point to find the tree and inject (same technique as the
// sibling Native Taskbar Media Controller mod).

using TaskListButton_UpdateVisualStates_t = void(WINAPI*)(void*);
TaskListButton_UpdateVisualStates_t TaskListButton_UpdateVisualStates_Original = nullptr;
void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
    // RAII guard so the counter is always decremented even if the original throws.
    struct HookGuard { ~HookGuard() { g_HookCallCounter--; } } guard;
    g_HookCallCounter++;
    TaskListButton_UpdateVisualStates_Original(pThis);
    if (!g_Unloading.load()) {
        auto elem = GetFrameworkElementFromNative(pThis);
        if (elem) ScheduleScanAsync(elem);
    }
}

bool HookTaskbarViewDllSymbols(HMODULE module) {
    WindhawkUtils::SYMBOL_HOOK hooks[] = {
        {
            { L"private: void __cdecl winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates(void)" },
            (void**)&TaskListButton_UpdateVisualStates_Original,
            (void*)TaskListButton_UpdateVisualStates_Hook,
            false,
        },
    };
    if (!WindhawkUtils::HookSymbols(module, hooks, ARRAYSIZE(hooks))) {
        Wh_Log(L"HookSymbols(Taskbar.View.dll) failed");
        return false;
    }
    return true;
}

// Post WM_SIZE to Shell_TrayWnd (and secondary bars) to trigger UpdateVisualStates
// without waiting for user interaction. Called after hooks are applied.
void TriggerInitialScan() {
    g_InitialScanThread = std::thread([]() {
        // Poll up to 30 s for Shell_TrayWnd so the WM_SIZE injection trigger
        // is not sent before the taskbar exists on a cold boot.
        HWND hTray = nullptr;
        for (int i = 0; i < 300 && !g_Unloading.load(); ++i) {
            Sleep(100);
            hTray = FindWindowW(L"Shell_TrayWnd", nullptr);
            if (hTray) break;
        }
        if (g_Unloading.load()) return;

        if (hTray) {
            RECT rc{};
            GetClientRect(hTray, &rc);
            PostMessageW(hTray, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
        }
        HWND hTray2 = FindWindowW(L"Shell_SecondaryTrayWnd", nullptr);
        while (hTray2) {
            PostMessageW(hTray2, WM_SIZE, SIZE_RESTORED, 0);
            hTray2 = FindWindowExW(nullptr, hTray2, L"Shell_SecondaryTrayWnd", nullptr);
        }
    });
}

// Polls for Taskbar.View.dll (or ExplorerExtensions.dll on older builds) every
// 100 ms for up to 60 s, then installs the UpdateVisualStates hook. Using
// GetModuleHandleW polling instead of a LoadLibraryExW hook avoids the
// cold-start trampoline race: on boot Explorer loads dozens of DLLs on multiple
// threads simultaneously, and patching LoadLibraryExW while another thread is
// executing it causes an unhandled hardware fault that kills Explorer.
void PollForTaskbarViewDll() {
    g_PollForDllThread = std::thread([]() {
        for (int i = 0; i < 600 && !g_Unloading.load(); ++i) {
            Sleep(100);
            HMODULE m = GetModuleHandleW(L"Taskbar.View.dll");
            if (!m) m = GetModuleHandleW(L"ExplorerExtensions.dll");
            if (!m) continue;

            bool already = g_TaskbarViewDllLoaded.exchange(true);
            if (already) break;

            if (!HookTaskbarViewDllSymbols(m)) break;
            Wh_ApplyHookOperations();
            if (!g_Unloading.load()) {
                TriggerInitialScan();
            }
            break;
        }
    });
}

} // namespace

// ── Mod entry points ──────────────────────────────────────────────────────

BOOL Wh_ModInit() {
    if (g_Unloading.load()) {
        Wh_Log(L"Taskbar code was retained after unload; restart Explorer before enabling again");
        return FALSE;
    }
    g_ModInitTick = GetTickCount64();
    LoadSettings();

    // Seed the theme state before anything is built; live flips arrive via
    // WM_SETTINGCHANGE on the WTS thread's hidden window.
    g_SystemLightTheme.store(IsSystemLightTheme());

    // Start the WTS notification thread unconditionally: it is plain
    // user32/wtsapi32 (no COM/WinRT), safe during Explorer's early boot, and
    // the widget needs current state whenever injection happens.
    if (!g_HostWork.Start()) return FALSE;
    StartWtsThread();

    HMODULE taskbarView = GetModuleHandleW(L"Taskbar.View.dll");
    if (!taskbarView) taskbarView = GetModuleHandleW(L"ExplorerExtensions.dll");

    if (taskbarView) {
        // Direct path: DLL already loaded. Hook and trigger immediately.
        g_TaskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarView)) {
            StopWtsThread(); g_HostWork.Stop();
            if (g_hWtsThreadReady) { CloseHandle(g_hWtsThreadReady); g_hWtsThreadReady = nullptr; }
            return FALSE;
        }
        TriggerInitialScan();
    } else {
        // Cold-start path: DLL not yet loaded. Poll for it and defer all
        // further initialization until Explorer's XAML side actually exists.
        PollForTaskbarViewDll();
    }

    Wh_Log(L"RDP Session Toolkit Taskbar Host v0.5.1 initialized — "
           L"position=%d width=%d",
           (int)g_Settings.widgetPosition, g_Settings.panelWidth);
    return TRUE;
}

void Wh_ModUninit() {
    g_Unloading = true;
    if (g_PollForDllThread.joinable()) g_PollForDllThread.join();
    if (g_InitialScanThread.joinable()) g_InitialScanThread.join();

    StopWtsThread();
    if (g_hWtsThreadReady) {
        CloseHandle(g_hWtsThreadReady);
        g_hWtsThreadReady = nullptr;
    }

    g_HostWork.Stop();
    g_Dispatch.Drain();
    RemoveWidget();
    g_Dispatch.Drain();

    // Spin until in-flight hooks finish.
    while (g_HookCallCounter.load() > 0) Sleep(10);


    Wh_Log(L"RDP Session Toolkit Taskbar Host uninitialized");
}

void Wh_ModSettingsChanged() {
    LoadSettings();

    for (auto& widget : GetLiveWidgets()) {
        try {
            auto weak = make_weak(widget);
            int w = g_Settings.panelWidth;
            double fs = (double)g_Settings.fontSize;
            HorizontalAlignment ha =
                g_Settings.widgetPosition == WidgetPosition::Left   ? HorizontalAlignment::Left   :
                g_Settings.widgetPosition == WidgetPosition::Center ? HorizontalAlignment::Center :
                                                                      HorizontalAlignment::Right;
            g_Dispatch.Post(widget,
                Windows::UI::Core::CoreDispatcherPriority::Normal,
                [weak, w, fs, ha]() {
                    auto g = weak.get();
                    if (!g) return;
                    g.Width((double)w);
                    g.HorizontalAlignment(ha);
                    if (auto tb = FindByName<TextBlock>(g, kClientNameName))
                        tb.FontSize(fs);
                    if (auto tb = FindByName<TextBlock>(g, kStateName))
                        tb.FontSize(std::max(8.0, fs - 2.0));
                    if (auto tb = FindByName<TextBlock>(g, kClipboardWarnName))
                        tb.FontSize(std::max(8.0, fs - 2.0));
                    ApplyStateToWidget(g);
                    UpdateWidgetMarginFor(g, L"settings");  // UI thread only
                });
        } catch (...) {}
    }

    Wh_Log(L"Settings reloaded — position=%d width=%d font=%d",
           (int)g_Settings.widgetPosition, g_Settings.panelWidth,
           g_Settings.fontSize);
}
