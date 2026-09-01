// ==WindhawkMod==
// @id              rdp-session-toolkit-taskbar-host
// @name            RDP Session Toolkit — Taskbar Host
// @description     Host-side taskbar integration for RDP Session Toolkit. Injects a native XAML widget into the Windows 11 taskbar of the RDP session host showing the connecting client's name and the session's connection state, with disconnect and minimize buttons. Minimize signals the client over an RDP Dynamic Virtual Channel to minimize the mstsc window.
// @version         0.2.0
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
RDP Dynamic Virtual Channel (`dvc::taskbar::relay`) and writes a single
command byte, which the client-side DVC relay plugin forwards to the
client mod's relay receiver — minimizing the remote `mstsc` window on your
local machine. The open-write-close runs on a short-lived background thread
so the taskbar's UI thread never blocks on channel latency; if no relay is
listening (the channel fails to open) it is logged and silently dropped, with
no dialog or other interruption to `explorer.exe`.

Like Disconnect, the button is enabled only over RDP (a minimize signal is
meaningless on the physical console).

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
- PanelWidth: 220
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
    int panelWidth = 220;
    int fontSize = 11;
    int offsetX = 8;
    WidgetPosition widgetPosition = WidgetPosition::Right;
} g_Settings;

void LoadSettings() {
    g_Settings.panelWidth = Wh_GetIntSetting(L"PanelWidth");
    g_Settings.fontSize   = Wh_GetIntSetting(L"FontSize");
    g_Settings.offsetX    = Wh_GetIntSetting(L"OffsetX");
    if (g_Settings.panelWidth <= 0) g_Settings.panelWidth = 220;
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

// ── WTS session state ─────────────────────────────────────────────────────
// All three fields come from WTSQuerySessionInformation against
// WTS_CURRENT_SERVER_HANDLE / WTS_CURRENT_SESSION — i.e. the session this
// explorer.exe instance runs in, on the local (host) machine.

// WTSClientProtocolType value for an RDP connection (0 = console, 1 = legacy
// ICA, 2 = RDP — per the WTSQuerySessionInformation documentation).
constexpr USHORT kProtocolRdp = 2;

// Production DVC channel this mod opens to signal the client (see DECISIONS.md
// D-10). MUST match the relay plugin's RELAY_CHANNEL_NAME
// (dvc-plugin/relay/common/RelayIds.h). ANSI, because WTSVirtualChannelOpenEx
// takes an LPSTR channel name.
constexpr char RELAY_DVC_CHANNEL[] = "dvc::taskbar::relay";
// The single command byte the client mod's relay receiver interprets as
// "minimize" (its RELAY_CMD_MINIMIZE). Sent as exactly one byte.
constexpr BYTE RELAY_CMD_MINIMIZE_BYTE = 0x01;

struct WtsState {
    std::wstring clientName;                              // WTSClientName; empty when none reported
    WTS_CONNECTSTATE_CLASS connectState = WTSDisconnected; // WTSConnectState
    USHORT protocol = 0;                                   // WTSClientProtocolType

    bool operator==(const WtsState& o) const {
        return clientName == o.clientName &&
               connectState == o.connectState &&
               protocol == o.protocol;
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

    bool changed;
    {
        std::lock_guard<std::mutex> g(g_WtsMutex);
        changed = !(s == g_WtsState);
        g_WtsState = std::move(s);
    }
    if (changed) {
        {
            std::lock_guard<std::mutex> g(g_WtsMutex);
            Wh_Log(L"WTS: state → client=\"%s\" state=%s protocol=%u",
                   g_WtsState.clientName.c_str(),
                   ConnectStateName(g_WtsState.connectState),
                   g_WtsState.protocol);
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

    case WM_TIMER:
        if (wParam == WTS_REFRESH_TIMER_ID) {
            TryRegisterSessionNotification(hwnd);
            QueryWtsState();
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, WTS_REFRESH_TIMER_ID);
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

// ── Minimize-the-client DVC send ──────────────────────────────────────────
//
// Sends one minimize command to the client over the production DVC channel:
// WTSVirtualChannelOpenEx(WTS_CHANNEL_OPTION_DYNAMIC) → write exactly one byte
// (RELAY_CMD_MINIMIZE_BYTE) → close. The client-side relay plugin receives it
// and forwards the byte to the client mod's CitadelRdpTaskbarRelay receiver,
// which minimizes the mstsc window (see DECISIONS.md D-12).
//
// Threading (DECISIONS.md D-12): this runs on a short-lived, detached
// background thread, NOT the persistent-thread-with-readiness-event pattern
// used for the WTS notification listener. That pattern earns its complexity by
// owning a long-lived window + message loop that must be torn down in order;
// this is a single, self-contained open-write-close per click with no window,
// no message loop, and no state to own past its own return — a one-shot. It
// must not run on the XAML UI / click thread because WTSVirtualChannelOpenEx
// can have real latency and explorer.exe's message loop must never block on it.
//
// Unload safety: an atomic in-flight counter lets Wh_ModUninit wait (bounded)
// for any outstanding send to finish, so no detached thread is still executing
// this DLL's code when Windhawk unloads it.
std::atomic<int> g_MinimizeSendsInFlight{ 0 };

void SendMinimizeToClientAsync() {
    g_MinimizeSendsInFlight.fetch_add(1, std::memory_order_acq_rel);
    std::thread([]() {
        // NULL (not INVALID_HANDLE_VALUE) is the failure return here.
        HANDLE hChannel = WTSVirtualChannelOpenEx(
            WTS_CURRENT_SESSION,
            const_cast<LPSTR>(RELAY_DVC_CHANNEL),
            WTS_CHANNEL_OPTION_DYNAMIC);
        if (!hChannel) {
            // No relay plugin listening on the client (channel never opened),
            // or no DVC infrastructure. Log and do nothing further — never a
            // dialog, never anything that interrupts explorer.exe.
            Wh_Log(L"Minimize send: WTSVirtualChannelOpenEx('%hs') failed GLE=%d "
                   L"— no relay listening? dropping", RELAY_DVC_CHANNEL,
                   GetLastError());
            g_MinimizeSendsInFlight.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }

        BYTE cmd = RELAY_CMD_MINIMIZE_BYTE;
        DWORD written = 0;
        if (!WTSVirtualChannelWrite(hChannel, reinterpret_cast<PCHAR>(&cmd),
                                    1, &written)) {
            Wh_Log(L"Minimize send: WTSVirtualChannelWrite failed GLE=%d",
                   GetLastError());
        } else {
            Wh_Log(L"Minimize send: wrote 0x%02X (%lu byte) to DVC '%hs'",
                   cmd, written, RELAY_DVC_CHANNEL);
        }

        WTSVirtualChannelClose(hChannel);
        g_MinimizeSendsInFlight.fetch_sub(1, std::memory_order_acq_rel);
    }).detach();
}

// ── XAML injection state ──────────────────────────────────────────────────

constexpr std::wstring_view kWidgetRootName   = L"RdpstkHostWidgetRoot";
constexpr std::wstring_view kClientNameName   = L"RdpstkHostClientName";
constexpr std::wstring_view kStateName        = L"RdpstkHostState";
constexpr std::wstring_view kDisconnectName   = L"RdpstkHostDisconnect";
constexpr std::wstring_view kMinimizeName     = L"RdpstkHostMinimize";
constexpr std::wstring_view kTaskbarFrameClass  = L"Taskbar.TaskbarFrame";
constexpr std::wstring_view kRootGridName       = L"RootGrid";
constexpr std::wstring_view kSystemTrayGridName = L"SystemTrayFrameGrid";
constexpr std::wstring_view kSystemTrayFrameClass = L"SystemTray.SystemTrayFrame";

std::mutex g_WidgetMutex;
weak_ref<Grid> g_WidgetRoot{ nullptr };
weak_ref<Grid> g_RootGrid{ nullptr };
weak_ref<FrameworkElement> g_SystemTray{ nullptr };
event_token g_TrayResizeToken{};
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

// ── Widget ────────────────────────────────────────────────────────────────

void UpdateWidgetMargin(const wchar_t* reason);

// Bind g_SystemTray (and its SizeChanged subscription) if it is not already
// bound to a live element. Lazy on purpose: the tray subtree may not exist yet
// at cold-start injection, and every caller (inject, widget Loaded, tray
// resize, settings change) runs on the UI thread after a layout pass, so a
// tray that appears later is picked up on the next pass without a timer.
// Returns the bound tray element, or nullptr. Must run on the XAML UI thread.
FrameworkElement BindSystemTray(FrameworkElement anchor) {
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        if (auto tray = g_SystemTray.get()) return tray;
    }
    FrameworkElement tray = FindSystemTray(anchor);
    if (!tray) return nullptr;

    // A dead weak_ref means the old element (and its subscription) is gone.
    g_TrayResizeToken = {};
    // Subscribe in every position mode: the handler only recomputes the margin,
    // and this way a later settings switch to Right needs no re-subscription.
    g_TrayResizeToken = tray.SizeChanged(
        [](IInspectable const&, SizeChangedEventArgs const&) {
            UpdateWidgetMargin(L"tray-sizechanged");
        });
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        g_SystemTray = make_weak(tray);
    }
    Wh_Log(L"[margin] tray bound: %s#%s actualWidth=%.1f",
           winrt::get_class_name(tray).c_str(), tray.Name().c_str(), tray.ActualWidth());
    return tray;
}

// Recompute the widget's margin based on the current position mode.
// Right: tracks tray width so the widget stays adjacent to the system tray.
// Left/Center: a fixed margin from the respective edge (no tray dependency).
// `reason` is a short tag naming the caller for the "[margin]" log line.
// Must run on the XAML UI thread.
void UpdateWidgetMargin(const wchar_t* reason) {
    Grid widget{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
    }
    if (!widget) return;

    FrameworkElement tray = BindSystemTray(widget);

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
    // reference: in Right mode trayActualWidth should be close to it.
    Wh_Log(L"[margin] #%u t+%llums reason=%s mode=%d tray=%s trayActualWidth=%.1f "
           L"trayNotifyWndDips=%.1f gap=%.0f -> margin=%.1f",
           seq, tMs, reason, (int)g_Settings.widgetPosition,
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

    // System-integrated Acrylic background, matching the sibling reference
    // mod's default look.
    try {
        AcrylicBrush acrylic;
        acrylic.BackgroundSource(AcrylicBackgroundSource::HostBackdrop);
        acrylic.TintColor(ColorHelper::FromArgb(0xFF, 0x1A, 0x1A, 0x1A));
        acrylic.TintOpacity(0.6);
        root.Background(acrylic);
    } WH_CATCH(L"BuildWidget/acrylic")

    Grid layout;
    layout.HorizontalAlignment(HorizontalAlignment::Stretch);
    layout.VerticalAlignment(VerticalAlignment::Center);
    layout.Margin(ThicknessHelper::FromLengths(10.0, 0.0, 8.0, 0.0));

    auto cols = layout.ColumnDefinitions();
    {
        ColumnDefinition cText;
        cText.Width(GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        cols.Append(cText);
        ColumnDefinition cMin;   // Minimize button
        cMin.Width(GridLengthHelper::Auto());
        cols.Append(cMin);
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
    clientName.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    clientName.FontSize((double)g_Settings.fontSize);
    clientName.TextTrimming(TextTrimming::CharacterEllipsis);
    clientName.TextWrapping(TextWrapping::NoWrap);
    clientName.MaxLines(1);
    textCol.Children().Append(clientName);

    TextBlock state;
    state.Name(kStateName);
    state.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    state.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    state.TextTrimming(TextTrimming::CharacterEllipsis);
    state.TextWrapping(TextWrapping::NoWrap);
    state.MaxLines(1);
    textCol.Children().Append(state);

    Grid::SetColumn(textCol, 0);
    layout.Children().Append(textCol);

    // Disconnect — same clean-disconnect semantics as the Start menu's
    // Disconnect: the session keeps running, the client detaches. Glyph
    // matches the client mod's thumb-bar Disconnect (Segoe MDL2 0xE8BB).
    Button disconnect;
    disconnect.Name(kDisconnectName);
    disconnect.Content(box_value(hstring{L""}));
    disconnect.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    disconnect.Background(MakeBrush(0x00, 0, 0, 0));
    disconnect.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    disconnect.BorderThickness(ThicknessHelper::FromUniformLength(0));
    disconnect.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    disconnect.Margin(ThicknessHelper::FromLengths(8, 0, 0, 0));
    disconnect.VerticalAlignment(VerticalAlignment::Center);
    disconnect.IsEnabled(false);  // enabled by ApplyStateToWidget when over RDP
    disconnect.Opacity(0.35);
    AutomationProperties::SetName(disconnect, L"Disconnect this RDP session");
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
    Grid::SetColumn(disconnect, 2);
    layout.Children().Append(disconnect);

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
    minimize.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    minimize.Background(MakeBrush(0x00, 0, 0, 0));
    minimize.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    minimize.BorderThickness(ThicknessHelper::FromUniformLength(0));
    minimize.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    minimize.Margin(ThicknessHelper::FromLengths(8, 0, 0, 0));
    minimize.VerticalAlignment(VerticalAlignment::Center);
    minimize.IsEnabled(false);  // enabled by ApplyStateToWidget when over RDP
    minimize.Opacity(0.35);
    AutomationProperties::SetName(minimize, L"Minimize the RDP client window");
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
            // thread on WTSVirtualChannelOpenEx latency.
            SendMinimizeToClientAsync();
        }));
    Grid::SetColumn(minimize, 1);
    layout.Children().Append(minimize);

    root.Children().Append(layout);
    return root;
}

// Refresh from any thread: marshals ApplyStateToWidget onto the UI thread.
void RefreshWidgetUI() {
    Grid widget{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
    }
    if (!widget) return;
    try {
        auto weak = make_weak(widget);
        widget.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weak]() {
                if (auto w = weak.get()) ApplyStateToWidget(w);
            });
    } WH_CATCH(L"RefreshWidgetUI/dispatch")
}

// ── Injection ─────────────────────────────────────────────────────────────

void InjectWidgetInto(Grid rootGrid) {
    if (!rootGrid) return;

    // Already injected?
    auto existing = FindByName<Grid>(rootGrid, kWidgetRootName);
    if (existing) {
        {
            std::lock_guard<std::mutex> g(g_WidgetMutex);
            g_WidgetRoot = make_weak(existing);
            g_RootGrid   = make_weak(rootGrid);
        }
        ApplyStateToWidget(existing);
        return;
    }

    auto widget = BuildWidget();
    rootGrid.Children().Append(widget);

    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        g_WidgetRoot = make_weak(widget);
        g_RootGrid   = make_weak(rootGrid);
    }

    // One-shot structural dump: the tray is a sibling of Taskbar.TaskbarFrame
    // under the visual root, not a child of RootGrid (see FindVisualRoot), and
    // the tray lookup in UpdateWidgetMargin() starts from that root.
    LogVisualRootChildren(FindVisualRoot(rootGrid));

    // Binds the tray (if it exists yet) and subscribes to its SizeChanged so
    // the margin tracks icon-count changes; sets the initial margin.
    UpdateWidgetMargin(L"inject");
    // Re-run once the first layout pass completes so ActualWidth() is valid
    // (and to bind a tray that did not exist at injection time).
    widget.Loaded([](IInspectable const&, RoutedEventArgs const&) {
        UpdateWidgetMargin(L"loaded");
    });
    ApplyStateToWidget(widget);

    Wh_Log(L"Widget injected into taskbar RootGrid");
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
    Grid widget{ nullptr };
    Grid rootGrid{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
        rootGrid = g_RootGrid.get();
        g_WidgetRoot = nullptr;
        g_RootGrid = nullptr;
    }
    if (!widget || !rootGrid) return;

    try {
        auto weakGrid = make_weak(rootGrid);
        rootGrid.Dispatcher().RunAsync(
            Windows::UI::Core::CoreDispatcherPriority::Normal,
            [weakGrid]() {
                // All XAML cleanup must happen on the dispatcher thread.
                {
                    std::lock_guard<std::mutex> lk(g_WidgetMutex);
                    if (g_TrayResizeToken.value) {
                        if (auto tray = g_SystemTray.get())
                            try { tray.SizeChanged(g_TrayResizeToken); } catch (...) {}
                        g_TrayResizeToken = {};
                    }
                    g_SystemTray = nullptr;  // force a fresh bind on re-inject
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

    Wh_Log(L"RDP Session Toolkit Taskbar Host v0.2.0 initialized — "
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

    // Wait (bounded) for any in-flight one-shot minimize DVC sends to drain, so
    // no detached sender thread is still running this DLL's code when Windhawk
    // unloads it. Each send is a quick open-write-close; 3 s is ample headroom.
    for (int i = 0; i < 300 && g_MinimizeSendsInFlight.load() > 0; ++i) Sleep(10);

    Wh_Log(L"RDP Session Toolkit Taskbar Host uninitialized");
}

void Wh_ModSettingsChanged() {
    LoadSettings();

    Grid widget{ nullptr };
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        widget = g_WidgetRoot.get();
    }
    if (!widget) return;
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
                ApplyStateToWidget(g);
                UpdateWidgetMargin(L"settings");  // must run on UI thread
            });
    } catch (...) {}

    Wh_Log(L"Settings reloaded — position=%d width=%d font=%d",
           (int)g_Settings.widgetPosition, g_Settings.panelWidth,
           g_Settings.fontSize);
}
