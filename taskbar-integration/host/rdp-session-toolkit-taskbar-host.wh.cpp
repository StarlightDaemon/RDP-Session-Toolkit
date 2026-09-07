// ==WindhawkMod==
// @id              rdp-session-toolkit-taskbar-host
// @name            RDP Session Toolkit — Taskbar Host
// @description     Host-side taskbar integration for RDP Session Toolkit. Injects a native XAML widget into the Windows 11 taskbar of the RDP session host showing the connecting client's name and the session's connection state, with disconnect, minimize, fullscreen-toggle and restart-clipboard buttons. Minimize and the fullscreen toggle signal the client over an RDP Dynamic Virtual Channel, which also carries the client's fullscreen state back so the toggle labels its real direction. Restart clipboard kills and relaunches the session's own rdpclip.exe to repair broken copy-and-paste.
// @version         0.4.0
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
RDP Dynamic Virtual Channel (`dvc::taskbar::relay`), writes a single
command byte, which the client-side DVC relay plugin forwards to the
client mod's relay receiver — minimizing the remote `mstsc` window on your
local machine — and then **reads the relay's small status reply back before
closing**. The reply doubles as delivery confirmation: the original
write-then-immediately-close shape could tear the channel down before the
queued byte reached the client, which is why the button used to drop clicks.
The open-write-read-close runs on a short-lived background thread so the
taskbar's UI thread never blocks on channel latency; if no relay is listening
(the channel fails to open) it is logged and silently dropped, with no dialog
or other interruption to `explorer.exe`.

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

namespace {

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

    WTSFreeMemory(procs);
    return scan;
}

// Unload safety, exactly as D-12 handles the minimize send: an atomic in-flight
// counter that Wh_ModUninit drains with a bounded wait, so no detached thread
// is still executing this DLL's code when Windhawk unloads it.
std::atomic<int> g_ClipboardRestartsInFlight{ 0 };
// Serializes restarts. A second click while one is still in flight would
// terminate the instance the first click had just launched; extra clicks are
// logged and dropped instead.
std::atomic<bool> g_ClipboardRestartBusy{ false };

// The whole find → verify → terminate → wait → launch sequence.
// Background thread only (see RestartRdpClipAsync).
void RestartRdpClipNow() {
    wchar_t expectedPath[MAX_PATH] = {};
    if (!GetRdpClipImagePath(expectedPath, ARRAYSIZE(expectedPath))) {
        Wh_Log(L"Restart clipboard: could not resolve System32\\rdpclip.exe — "
               L"aborting without terminating or launching anything");
        return;
    }

    RdpClipScan scan = ScanForRdpClipInSession(expectedPath, /*logSkips=*/true);
    if (!scan.enumerated) {
        Wh_Log(L"Restart clipboard: process enumeration failed — aborting without "
               L"terminating or launching anything");
        return;
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
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!h) {
            Wh_Log(L"Restart clipboard: OpenProcess(pid=%lu) failed GLE=%d — skipped",
                   pid, GetLastError());
            continue;
        }
        if (!TerminateProcess(h, 0)) {
            Wh_Log(L"Restart clipboard: TerminateProcess(pid=%lu) failed GLE=%d",
                   pid, GetLastError());
            CloseHandle(h);
            continue;
        }
        terminatedAny = true;
        // Bounded wait on the handle — never an assumption that termination is
        // instantaneous.
        DWORD w = WaitForSingleObject(h, kRdpClipExitWaitMs);
        if (w == WAIT_OBJECT_0)
            Wh_Log(L"Restart clipboard: pid=%lu terminated and exited", pid);
        else
            Wh_Log(L"Restart clipboard: pid=%lu terminated but did not signal exit "
                   L"within %lu ms (wait=%lu) — continuing anyway",
                   pid, kRdpClipExitWaitMs, w);
        CloseHandle(h);
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
    if (after.enumerated && !after.pids.empty()) {
        Wh_Log(L"Restart clipboard: rdpclip.exe is already running again (pid=%lu) "
               L"— not launching a second one", after.pids.front());
        QueryWtsState();
        return;
    }

    // CreateProcessW on the resolved System32 path. The replacement inherits
    // this process's session and token automatically — no impersonation, no
    // session plumbing, and it lands in the right session by construction.
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
        return;
    }
    Wh_Log(L"Restart clipboard: launched '%s' pid=%lu",
           expectedPath, pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    // Refresh the widget's clipboard line now rather than up to 30 s later.
    // This goes through QueryWtsState — the same function the safety-net timer
    // calls — so no new timer and no second status code path.
    QueryWtsState();
}

// Detached one-shot per click, the D-12 pattern verbatim: no window, no message
// loop, no state past its own return. It must not run on the XAML UI thread —
// the sequence blocks on process termination and a settle, and explorer.exe's
// message loop can never wait on that.
void RestartRdpClipAsync() {
    bool expected = false;
    if (!g_ClipboardRestartBusy.compare_exchange_strong(expected, true)) {
        Wh_Log(L"Restart clipboard: a restart is already in flight — click ignored");
        return;
    }
    g_ClipboardRestartsInFlight.fetch_add(1, std::memory_order_acq_rel);
    std::thread([]() {
        // The worker allocates (the scan's vector), so it is wrapped: an
        // exception escaping a detached thread would call std::terminate and
        // take explorer.exe down with it. The latch and the counter are
        // released either way.
        try {
            RestartRdpClipNow();
        } WH_CATCH(L"RestartRdpClip")
        g_ClipboardRestartBusy.store(false);
        g_ClipboardRestartsInFlight.fetch_sub(1, std::memory_order_acq_rel);
    }).detach();
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
#pragma pack(push, 1)
struct RelayStatusReply {
    DWORD magic;          // kRelayStatusReplyMagic
    DWORD version;        // kRelayStatusReplyVersion
    BYTE  stateKnown;     // 1 = the two fields below came from a fresh,
                          //     valid client status snapshot; 0 = the relay
                          //     could not read one (missing registry pointer,
                          //     missing/stale/mismatched file) and they are
                          //     meaningless — never render them as facts
    BYTE  sessionActive;  // client mod reports a live mstsc session frame
    BYTE  fullscreen;     // that session is currently fullscreen
    BYTE  reserved;
};
#pragma pack(pop)
constexpr DWORD kRelayStatusReplyMagic   = 0x50525352;  // 'RSRP'
constexpr DWORD kRelayStatusReplyVersion = 1;

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
// thread's timer, fires the poll on a detached one-shot. 3 s sits in the
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
            // Fire-and-forget: the poll itself runs on a detached one-shot
            // (it blocks on channel open + read), so this timer handler never
            // stalls the WTS thread's message loop.
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
    DWORD threadId = 0;
    g_hWtsThread = CreateThread(nullptr, 0, WtsThread, nullptr, 0, &threadId);
    g_wtsThreadId.store(threadId);
}

void StopWtsThread() {
    DWORD threadId = g_wtsThreadId.load();
    if (threadId) {
        if (g_hWtsThreadReady)
            WaitForSingleObject(g_hWtsThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hWtsThread) {
        WaitForSingleObject(g_hWtsThread, 3000);
        CloseHandle(g_hWtsThread);
        g_hWtsThread = nullptr;
        g_wtsThreadId.store(0);
    }
}

// ── Relay DVC send: open → write → READ → close ───────────────────────────
//
// Sends one command byte to the client over the production DVC channel, then
// reads the relay plugin's small status reply back on the same channel before
// closing. The client-side relay plugin forwards command bytes (minimize,
// fullscreen toggle) to the client mod's CitadelRdpTaskbarRelay receiver, and
// answers the status query itself; either way it writes a RelayStatusReply
// back before the host's close (see DECISIONS.md D-12 for the original send
// shape, D-38 for the read leg).
//
// WHY THE READ EXISTS — the Minimize-button root cause (Task 2, D-37). The
// original shape here was open-write-close with the close immediately after
// the write. WTSVirtualChannelWrite on a dynamic channel completes when the
// data is QUEUED to the RDP stack, not when it is delivered; an immediate
// WTSVirtualChannelClose races the asynchronous delivery, and the whole
// channel can be torn down before the queued byte ever reaches the client
// plugin's OnDataReceived. The probe trigger — the only send path ever proven
// on real hardware (LOOP-003/LOOP-006) — guarded against exactly this with a
// deliberate Sleep(500) before its close ("give the last write time to drain
// before the channel tears down", dvc-plugin/probe/trigger/Trigger.cpp); that
// drain was silently dropped when the send was adapted into this mod, which
// is the structural difference between Minimize (cross-machine, broken) and
// Disconnect / Restart Clipboard (purely local, fine). The bounded read is
// the principled replacement for the sleep: the reply can only arrive after
// the client plugin has received and processed the command, so a successful
// read IS the delivery confirmation — and it carries the client's state back
// as a bonus.
//
// Threading (DECISIONS.md D-12): a short-lived, detached background thread
// per send — no window, no message loop, no state past its own return. It
// must not run on the XAML UI / click thread: open latency plus a read
// timeout of up to RELAY_REPLY_TIMEOUT_MS is far too long for explorer.exe's
// message loop.
//
// Unload safety: an atomic in-flight counter lets Wh_ModUninit wait (bounded)
// for any outstanding send to finish, so no detached thread is still executing
// this DLL's code when Windhawk unloads it.
std::atomic<int> g_RelaySendsInFlight{ 0 };

// Bounded wait for the relay's reply. Generous against session round-trip
// latency; the reply normally arrives in tens of milliseconds.
constexpr ULONG RELAY_REPLY_TIMEOUT_MS = 2000;

// ── Cached client view (from RelayStatusReply) ────────────────────────────
// The host's best knowledge of the client's state, refreshed by every send's
// read leg — clicks and the periodic status poll alike. Staleness: a view
// older than kClientViewStaleMs (poll interval + slack) degrades back to
// unknown, so a client that stops answering can never leave a frozen
// "fullscreen"/"windowed" label on the widget.
struct ClientView {
    bool      known = false;      // a valid reply with stateKnown=1 was read
    bool      sessionActive = false;
    bool      fullscreen = false;
    ULONGLONG lastReplyTick = 0;  // GetTickCount64 at the last valid reply
};
std::mutex g_ClientViewMutex;
ClientView g_ClientView;
constexpr ULONGLONG kClientViewStaleMs = 10000;

// Snapshot for display, staleness applied.
ClientView GetClientView() {
    std::lock_guard<std::mutex> g(g_ClientViewMutex);
    ClientView v = g_ClientView;
    if (v.known && GetTickCount64() - v.lastReplyTick > kClientViewStaleMs)
        v.known = false;
    return v;
}

// Parses a RelayStatusReply out of a raw DVC read. The reply is searched for
// at offset 0 and at offset 8 (sizeof(CHANNEL_PDU_HEADER)) — server-side
// virtual-channel reads have historically prefixed data with that header, and
// checking both offsets costs nothing while the exact framing on this OS
// build is unverified on hardware. Logs which offset matched.
bool ParseRelayStatusReply(const BYTE* buf, DWORD len, RelayStatusReply* out) {
    for (DWORD off : { 0ul, 8ul }) {
        if (len < off + sizeof(RelayStatusReply)) continue;
        RelayStatusReply r;
        memcpy(&r, buf + off, sizeof(r));
        if (r.magic != kRelayStatusReplyMagic) continue;
        if (r.version != kRelayStatusReplyVersion) {
            Wh_Log(L"Relay reply: magic at offset %lu but version %lu != %lu "
                   L"— ignored", off, r.version, (DWORD)kRelayStatusReplyVersion);
            continue;
        }
        Wh_Log(L"Relay reply: parsed at offset %lu — known=%u active=%u "
               L"fullscreen=%u", off, r.stateKnown, r.sessionActive, r.fullscreen);
        *out = r;
        return true;
    }
    return false;
}

// Applies a parsed reply to the cached view; refreshes the widget when the
// displayed facts changed.
void ApplyRelayStatusReply(const RelayStatusReply& r) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> g(g_ClientViewMutex);
        ClientView next;
        next.known         = r.stateKnown != 0;
        next.sessionActive = r.sessionActive != 0;
        next.fullscreen    = r.fullscreen != 0;
        next.lastReplyTick = GetTickCount64();
        changed = next.known != g_ClientView.known ||
                  next.sessionActive != g_ClientView.sessionActive ||
                  next.fullscreen != g_ClientView.fullscreen;
        g_ClientView = next;
    }
    if (changed) RefreshWidgetUI();
}

// The one relay send path: open, write the command byte, read the reply
// (bounded), close. Detached one-shot per call (D-12); every step logs its
// outcome so a live test says exactly where a failed send stopped.
void SendRelayCommandAsync(BYTE cmd, PCWSTR label) {
    g_RelaySendsInFlight.fetch_add(1, std::memory_order_acq_rel);
    std::thread([cmd, label]() {
        // NULL (not INVALID_HANDLE_VALUE) is the failure return here.
        HANDLE hChannel = WTSVirtualChannelOpenEx(
            WTS_CURRENT_SESSION,
            const_cast<LPSTR>(RELAY_DVC_CHANNEL),
            WTS_CHANNEL_OPTION_DYNAMIC);
        if (!hChannel) {
            // No relay plugin listening on the client (channel never opened),
            // or no DVC infrastructure. Log and do nothing further — never a
            // dialog, never anything that interrupts explorer.exe.
            Wh_Log(L"%s send: WTSVirtualChannelOpenEx('%hs') failed GLE=%d "
                   L"— no relay listening? dropping", label, RELAY_DVC_CHANNEL,
                   GetLastError());
            g_RelaySendsInFlight.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }

        BYTE sendByte = cmd;
        DWORD written = 0;
        if (!WTSVirtualChannelWrite(hChannel, reinterpret_cast<PCHAR>(&sendByte),
                                    1, &written)) {
            Wh_Log(L"%s send: WTSVirtualChannelWrite failed GLE=%d",
                   label, GetLastError());
        } else {
            Wh_Log(L"%s send: wrote 0x%02X (%lu byte) to DVC '%hs' — waiting "
                   L"for the relay's reply", label, sendByte, written,
                   RELAY_DVC_CHANNEL);

            // The read leg. This is both the delivery confirmation (see the
            // banner comment) and the client-state refresh.
            BYTE buf[256] = {};
            DWORD bytesRead = 0;
            if (!WTSVirtualChannelRead(hChannel, RELAY_REPLY_TIMEOUT_MS,
                                       reinterpret_cast<PCHAR>(buf),
                                       sizeof(buf), &bytesRead)) {
                Wh_Log(L"%s send: WTSVirtualChannelRead failed/timed out "
                       L"GLE=%d — delivery of 0x%02X is UNCONFIRMED (an old "
                       L"relay build that never replies also lands here)",
                       label, GetLastError(), sendByte);
            } else {
                RelayStatusReply reply{};
                if (ParseRelayStatusReply(buf, bytesRead, &reply)) {
                    Wh_Log(L"%s send: delivery confirmed (%lu-byte reply)",
                           label, bytesRead);
                    ApplyRelayStatusReply(reply);
                } else {
                    Wh_Log(L"%s send: read %lu byte(s) but no valid "
                           L"RelayStatusReply in them — delivery likely, state "
                           L"not updated", label, bytesRead);
                }
            }
        }

        WTSVirtualChannelClose(hChannel);
        g_RelaySendsInFlight.fetch_sub(1, std::memory_order_acq_rel);
    }).detach();
}

// ── Periodic client-state poll ────────────────────────────────────────────
// The host needs the client's fullscreen state between clicks, not only right
// after one — the fullscreen button must relabel itself when the user toggles
// from the client side. Every RELAY_POLL_INTERVAL_MS the WTS thread's timer
// fires one status query through the exact same open-write-read-close path as
// every click (no separate mechanism), gated on the session being RDP and on
// no other poll still being in flight. The interval constant
// (RELAY_POLL_INTERVAL_MS) lives with the WTS timer ids above, where the
// timer is armed.
std::atomic<bool> g_RelayPollInFlight{ false };

void PollClientStateAsync() {
    USHORT proto;
    {
        std::lock_guard<std::mutex> g(g_WtsMutex);
        proto = g_WtsState.protocol;
    }
    if (proto != kProtocolRdp) return;  // console session: nothing to ask
    bool expected = false;
    if (!g_RelayPollInFlight.compare_exchange_strong(expected, true))
        return;  // previous poll still open/reading — never stack them
    g_RelaySendsInFlight.fetch_add(1, std::memory_order_acq_rel);
    std::thread([]() {
        HANDLE hChannel = WTSVirtualChannelOpenEx(
            WTS_CURRENT_SESSION,
            const_cast<LPSTR>(RELAY_DVC_CHANNEL),
            WTS_CHANNEL_OPTION_DYNAMIC);
        if (hChannel) {
            BYTE cmd = RELAY_CMD_QUERY_STATUS_BYTE;
            DWORD written = 0;
            if (WTSVirtualChannelWrite(hChannel, reinterpret_cast<PCHAR>(&cmd),
                                       1, &written)) {
                BYTE buf[256] = {};
                DWORD bytesRead = 0;
                if (WTSVirtualChannelRead(hChannel, RELAY_REPLY_TIMEOUT_MS,
                                          reinterpret_cast<PCHAR>(buf),
                                          sizeof(buf), &bytesRead)) {
                    RelayStatusReply reply{};
                    if (ParseRelayStatusReply(buf, bytesRead, &reply))
                        ApplyRelayStatusReply(reply);
                }
                // A failed/timed-out poll read is deliberately NOT logged per
                // tick — with no relay listening it would repeat every 3 s
                // forever. The staleness window quietly degrades the view to
                // unknown instead, which is the honest rendering.
            }
            WTSVirtualChannelClose(hChannel);
        }
        g_RelaySendsInFlight.fetch_sub(1, std::memory_order_acq_rel);
        g_RelayPollInFlight.store(false);
    }).detach();
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
    if (!RefreshSystemTheme()) return;
    bool light = g_SystemLightTheme.load();
    Wh_Log(L"[theme] system theme now %s — applying the %s panel palette",
        light ? L"light" : L"dark", light ? L"dark" : L"light");
    for (auto& widget : GetLiveWidgets()) {
        try {
            auto weak = make_weak(widget);
            widget.Dispatcher().RunAsync(
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
    if (auto tb = FindByName<TextBlock>(widget, kStateName))
        tb.Text(stateLine);
    if (auto btn = FindByName<Button>(widget, kDisconnectName)) {
        // Enabled only over RDP — never offer to disconnect the physical
        // console session (that would kick the local user to the lock screen).
        btn.IsEnabled(rdp);
        btn.Opacity(rdp ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kMinimizeName)) {
        // Enabled only over RDP — a minimize signal is meaningless on the
        // physical console (no remote mstsc window to minimize).
        btn.IsEnabled(rdp);
        btn.Opacity(rdp ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kClipboardName)) {
        // Enabled only over RDP — there is no rdpclip on a console session, so
        // the action is meaningless there. Re-checked at click time as well.
        btn.IsEnabled(rdp);
        btn.Opacity(rdp ? 1.0 : 0.35);
    }
    if (auto btn = FindByName<Button>(widget, kFullscreenName)) {
        // Enabled only over RDP, like every other action button, re-checked
        // at click time as well. Glyph, accessible name, and tooltip relabel
        // from the CACHED client view (the relay's status replies — Task 5),
        // matching the client panel's own fullscreen button's relabeling
        // pattern, so this is never a blind, unlabeled toggle. When the view
        // is unknown (no reply yet, stale, or the client mod isn't reporting)
        // the button says so honestly instead of guessing a direction.
        btn.IsEnabled(rdp);
        btn.Opacity(rdp ? 1.0 : 0.35);
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
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled) so a stale
            // widget state can never disconnect a console session.
            if (proto != kProtocolRdp) {
                Wh_Log(L"Disconnect clicked on non-RDP session — ignored");
                return;
            }
            Wh_Log(L"Disconnect: WTSDisconnectSession(current session)");
            // bWait=FALSE — never block the XAML UI thread on the operation.
            if (!WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE,
                                      WTS_CURRENT_SESSION, FALSE)) {
                Wh_Log(L"WTSDisconnectSession failed GLE=%d", GetLastError());
            }
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
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled), matching
            // every other action button in this toolkit.
            if (proto != kProtocolRdp) {
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
    // The open-write-close happens on a detached background thread so
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
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled) so a stale
            // widget state can never fire a minimize on a console session.
            if (proto != kProtocolRdp) {
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
    // thread on a detached one-shot (D-12), because the sequence blocks on
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
            USHORT proto;
            {
                std::lock_guard<std::mutex> g(g_WtsMutex);
                proto = g_WtsState.protocol;
            }
            // Re-checked at click time (not only via IsEnabled), so a stale
            // widget state can never run this against a console session.
            if (proto != kProtocolRdp) {
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
    for (auto& widget : GetLiveWidgets()) {
        try {
            auto weak = make_weak(widget);
            widget.Dispatcher().RunAsync(
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
        startNode.Dispatcher().RunAsync(
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
        Grid rootGrid = inst.rootGrid.get();
        if (!rootGrid) continue;
        try {
            auto weakGrid = make_weak(rootGrid);
            auto weakTray = inst.tray;
            event_token token = inst.trayResizeToken;
            rootGrid.Dispatcher().RunAsync(
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
        } catch (...) {}
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
    g_ModInitTick = GetTickCount64();
    LoadSettings();

    // Seed the theme state before anything is built; live flips arrive via
    // WM_SETTINGCHANGE on the WTS thread's hidden window.
    g_SystemLightTheme.store(IsSystemLightTheme());

    // Start the WTS notification thread unconditionally: it is plain
    // user32/wtsapi32 (no COM/WinRT), safe during Explorer's early boot, and
    // the widget needs current state whenever injection happens.
    StartWtsThread();

    HMODULE taskbarView = GetModuleHandleW(L"Taskbar.View.dll");
    if (!taskbarView) taskbarView = GetModuleHandleW(L"ExplorerExtensions.dll");

    if (taskbarView) {
        // Direct path: DLL already loaded. Hook and trigger immediately.
        g_TaskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarView)) return FALSE;
        TriggerInitialScan();
    } else {
        // Cold-start path: DLL not yet loaded. Poll for it and defer all
        // further initialization until Explorer's XAML side actually exists.
        PollForTaskbarViewDll();
    }

    Wh_Log(L"RDP Session Toolkit Taskbar Host v0.4.0 initialized — "
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

    RemoveWidget();

    // Spin until in-flight hooks finish.
    for (int i = 0; i < 50 && g_HookCallCounter.load() > 0; ++i) Sleep(100);

    // Wait (bounded) for any in-flight one-shot relay DVC sends (clicks and
    // status polls alike) to drain, so no detached sender thread is still
    // running this DLL's code when Windhawk unloads it. A send's bounded worst
    // case is the open latency plus the RELAY_REPLY_TIMEOUT_MS read; 5 s is
    // headroom over that, not a guess.
    for (int i = 0; i < 500 && g_RelaySendsInFlight.load() > 0; ++i) Sleep(10);

    // Same discipline for the clipboard-restart one-shot. Its bounded worst
    // case is the terminate wait plus the settle (~2.3 s) plus a CreateProcess,
    // so 5 s of drain is headroom rather than a guess.
    for (int i = 0; i < 500 && g_ClipboardRestartsInFlight.load() > 0; ++i) Sleep(10);

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
            widget.Dispatcher().RunAsync(
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
