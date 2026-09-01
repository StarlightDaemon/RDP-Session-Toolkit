// ==WindhawkMod==
// @id              rdp-session-toolkit-taskbar-client
// @name            RDP Session Toolkit — Taskbar Client
// @description     A taskbar companion for Remote Desktop sessions: a panel in your own taskbar with buttons for minimize, restore, switching between fullscreen and windowed, reconnecting, and disconnecting, plus a status line whose tooltip shows session time, idle time, connection quality, and a warning if the session stops responding. Also removes the floating connection bar that Windows shows during fullscreen sessions.
// @version         0.9.1
// @author          StarlightDaemon
// @github          https://github.com/StarlightDaemon
// @include         mstsc.exe
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -lgdi32 -lshcore -lole32 -loleaut32 -ladvapi32 -lshell32 -lbcrypt -lruntimeobject -luser32 -lwindowsapp
// @license         MIT
// ==/WindhawkMod==

// Provenance: ported (forked, no shared git history) from the
// "Hide RDP Connection Bar" repository
// (https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar), commit
// dc82b10d4c8713d71f5e556649b114a6d43dad04 (mod version 1.1.9). That mod
// remains the standalone, narrow-scope original; this file is the client-side
// component of the RDP Session Toolkit and evolves independently under its
// own mod id and version line.
//
// As of v0.6.0 this single mod targets both mstsc.exe and explorer.exe. The
// explorer.exe branch is the former standalone
// rdp-session-toolkit-taskbar-client-embedded mod (last at v0.2.0), folded in
// here: a single Windhawk mod can target multiple processes, so the split into
// two mod ids was never necessary. Because both branches now share one mod id,
// the explorer branch resolves the client mod's status file directly with
// Wh_GetModStoragePath (the path is per mod id, identical in both processes)
// instead of guessing at a sibling storage directory name. See DECISIONS.md
// D-26 for the consolidation; D-21..D-25 for the folded branch's own history.
// The explorer branch's taskbar XAML injection ultimately derives from this
// repo's host mod (DECISIONS.md D-7/D-25), where it has run live.

// ==WindhawkModReadme==
/*
# RDP Session Toolkit — Taskbar Client

The client-side taskbar-integration component of the **RDP Session Toolkit**.
One mod that targets **two processes on the client machine**:

- **`mstsc.exe`** — the floating overlay, the stuck-session watchdog, the
  connection-quality sink, the relay receiver, and the status writer / local
  command receiver (everything described below).
- **`explorer.exe`** — a wide, always-visible RDP status panel embedded in the
  client machine's own taskbar (the "Taskbar-embedded panel" section near the
  end). This was previously a separate mod
  (`rdp-session-toolkit-taskbar-client-embedded`); as of v0.6.0 it is this same
  mod's `explorer.exe` branch.

Enable the mod once and Windhawk injects the right branch into each process.

There are **two presentation surfaces**, and they are independent:

1. the **taskbar-embedded panel** in your own taskbar (`explorer.exe`), which
   is the main one and is always available;
2. the **floating overlay button** on the RDP monitor (`mstsc.exe`), off by
   default.

To use only the taskbar-embedded panel, simply leave *Show floating overlay
button* off (and keep *Detect stuck sessions* on, so the status snapshot keeps
being written — see that setting).

> **Changed in v0.9.0.** A third surface, the **taskbar thumbnail toolbar**
> (buttons under the mstsc taskbar hover preview), has been removed. The
> taskbar-embedded panel offers the same five actions with the same rules,
> always visible instead of hover-only, so the toolbar was a second copy of
> the same UI to keep in sync. Its rich status tooltip was not lost — it moved
> to the panel's own status text (hover it). The *Show taskbar thumbnail
> toolbar* setting is gone; *Show connection quality*, *Show session duration
> and idle time*, and *Fullscreen / windowed toggle button* now apply to the
> panel.

Ported from the standalone
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar)
mod; further toolkit components (a remote-side mod and a DVC relay plugin)
interoperate with it.

## mstsc.exe branch

Hides the floating Remote Desktop connection bar in fullscreen sessions on
Windows 11, where the native options to hide the bar may not persist reliably.

Offers one optional mstsc-side control surface — a **floating overlay button**
(off by default, *Show floating overlay button*) — alongside the always-present
taskbar-embedded panel described further below.

The floating overlay button (when enabled) is pinned to any corner of the
screen:

- Four stacked rows: hostname display on top, then session time and this
  computer's idle time, then Minimize and Restore side by side, Disconnect
  on the bottom
- Minimize sends the fullscreen session to the taskbar; Restore brings
  it back from the same button, which stays on screen while the session
  is minimized. Whichever of the two does not apply to the current state
  is dimmed and clicking it does nothing
- Full border outline for visibility, or top-accent-only — your choice
- Fades to near-invisible when idle, brightens on hover
- Configurable keyboard hotkey to disconnect without touching the mouse
- Follows the RDP window if moved to a different monitor
- DPI-aware — scales correctly on 4K and HiDPI displays
- Drag the button anywhere on screen — the position persists across
  reconnects; changing the position settings in the Windhawk UI resets it
  back to the configured default

Two further actions have no overlay row of their own and are offered on the
taskbar-embedded panel, driven from this branch:

- **Fullscreen / windowed toggle** (*Fullscreen / windowed toggle button*):
  switches the live session between fullscreen and windowed without
  disconnecting, by sending Remote Desktop's own Ctrl+Alt+Break toggle to the
  session window — nothing else is reimplemented. If the session window
  cannot be brought to the foreground, nothing is sent.
- **Reconnect** (*Enable Reconnect and Force reconnect*, off by default):
  cleanly disconnects (same path as Disconnect) and reopens the same
  connection — same target, same `.rdp` file or switches — in the preferred
  display mode from the settings: fullscreen (`/f`), windowed (`/w:` `/h:`, at
  a fixed size or the session's current size), or all monitors
  (`/multimon`).

This branch also measures and publishes the session status the panel displays:
how long the session has been open, how long this computer's own keyboard and
mouse have been idle (local input idle — not remote-session activity), the
connection quality level Remote Desktop itself reports (4 levels) with its
bandwidth and round-trip time, and whether the session window has stopped
responding. Nothing is invented — until Remote Desktop makes its first quality
report the panel's tooltip says exactly that.

## Stuck-session alert

Independently of the button, the mod watches the session window for a hang
(`IsHungAppWindow`). Once it has not responded for the configured number of
seconds, a small alert appears at the top of the session's monitor with
**Force reconnect** and **Dismiss**. Force reconnect gives the client a few
seconds to close cleanly, then ends it and reopens the connection in the
preferred display mode. The mod never reconnects on its own — every
reconnect, forced or clean, starts with your click.

## Toolkit relay receiver (stub)

The mod also creates a hidden message-only window (class
`CitadelRdpTaskbarRelay`) at load. The RDP Session Toolkit's DVC relay plugin
sends it `WM_COPYDATA` commands. The first payload byte is the command; only
Minimize (0x01) is implemented so far. Senders are validated: only this mod's
own process (the local self-test) or the registered DVC relay plugin's exact
EXE are accepted; anything else is logged and ignored. The "Debug: send test
minimize via relay" setting exercises the receiver locally without any other
toolkit component installed.

## Taskbar-embedded widget channel

A second, separate message-only window (class `CitadelRdpTaskbarLocalWidget`)
serves the taskbar-embedded panel — this same mod's `explorer.exe` branch,
running in this machine's own Explorer. Once a second this mod also writes a
small status snapshot —
session active, duration, this PC's idle time, connection quality, stuck
state, minimized / fullscreen — to `local-widget-status.dat` in its Windhawk
mod storage folder, which the widget reads and treats as stale after a few
seconds without a fresh write. Widget commands (minimize, restore, fullscreen
toggle, reconnect, disconnect) must carry a 32-byte shared secret stored as
`HKCU\Software\RDPSessionToolkit\LocalWidgetSecret`, generated with the
system CSPRNG by whichever of the two branches starts first; any command
without the exact secret is ignored. The relay receiver above is not involved
and unchanged.

## Taskbar-embedded panel (explorer.exe branch)

When injected into the client machine's own `explorer.exe`, this mod puts a
**wide, always-visible status panel** directly into the Windows 11 taskbar,
embedded in the taskbar's own XAML tree. The panel shows:

- the remote host name, and the session duration / connection quality Remote
  Desktop reports (or an honest "quality n/a" / "not responding" line).
  **Hover that text** for the full detail — how long the session has been
  open, how long this computer's own keyboard and mouse have been idle, the
  reported quality level with its bandwidth and round-trip time, and the
  not-responding warning with its running count. (This is the tooltip that
  used to hang off the thumbnail toolbar's status icon; it moved here in
  v0.9.0 when that toolbar was removed. *Show session duration and idle time*
  and *Show connection quality* control which lines it carries.)
- five always-visible buttons — **Minimize**, **Restore**,
  **Switch to fullscreen / windowed**, **Reconnect**, and **Disconnect** —
  each directly clickable, each with its own tooltip explaining in plain
  language what the click actually does to the session, and each enabled,
  disabled, or relabelled from the session's real state. Reconnect is hidden
  unless *Enable Reconnect and Force reconnect* is on; the fullscreen toggle
  is hidden unless *Fullscreen / windowed toggle button* is on.

The panel does nothing to the RDP session by itself. It reads the status
snapshot the mstsc branch writes once a second (treating it as **stale** — no
session — after a few seconds without a refresh, so an uncleanly-exited client
can never leave a phantom session on the taskbar), and sends actions to the
mstsc branch's local command window (`CitadelRdpTaskbarLocalWidget`) over
`WM_COPYDATA`, authenticated by the shared secret described above. Because both
branches are the same mod id, the panel finds the snapshot file directly via
`Wh_GetModStoragePath` — the same directory the mstsc branch writes to.

Its own layout settings are the `embedded…` group (position, width, font size,
offset, show-when-no-session); what it *shows* is additionally governed by the
shared *Show session duration and idle time*, *Show connection quality*,
*Fullscreen / windowed toggle button*, and *Enable Reconnect and Force
reconnect* settings above. One session at a time, like the rest of the
toolkit.

## Requirements

Requires Windhawk 1.6 or later — earlier versions lack the
`Wh_GetModStoragePath` API used to persist the dragged button position and to
locate the status file. The `explorer.exe` branch additionally needs Windows 11
(22H2 or later recommended) for the taskbar XAML injection.

## Note

If the disconnect button does not appear after enabling it, close and reopen
the Remote Desktop connection. The button is created when the session starts;
it cannot appear for a session that is already running.

Click and drag the button to reposition it anywhere on screen; the dragged
position persists across reconnects. Changing the Button position, Corner
offset, or Custom offset setting in the Windhawk UI resets it back to the
configured default.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- hideBar: true
  $name: Hide connection bar
  $description: Hides the native RDP connection bar. Turn off to restore it.
- showOverlay: false
  $name: Show floating overlay button
  $description: Shows a floating button pinned to a corner of the RDP monitor (fullscreen sessions) with Minimize, Restore, and Disconnect controls. Off by default and entirely optional — the taskbar-embedded panel, configured under its own group below, is always available and covers windowed sessions too. If it does not appear, close and reopen the Remote Desktop connection.
- buttonPosition: top-right
  $name: Overlay button position (mstsc.exe)
  $description: Which corner of the RDP monitor to place the floating overlay button. Applies to the mstsc.exe surface only — the taskbar-embedded panel has its own position under the Taskbar-embedded panel group below.
  $options:
  - top-right: Top Right
  - top-left: Top Left
  - bottom-right: Bottom Right
  - bottom-left: Bottom Left
- offsetPreset: medium
  $name: Corner offset
  $description: How far to nudge the button away from the corner. Use Custom offset to override with an exact value.
  $options:
  - none: None (0 px)
  - small: Small (16 px)
  - medium: Medium (32 px)
  - large: Large (64 px)
  - xlarge: XL (96 px)
  - xxlarge: XXL (256 px)
  - xxxlarge: XXXL (512 px)
- offsetCustom: 0
  $name: Custom offset (pixels)
  $description: Exact pixel offset. Overrides Corner offset when non-zero.
- showBorder: true
  $name: Show full border
  $description: Draws a full outline around the button. Turn off for top-accent-only style.
- showHostname: true
  $name: Show hostname on button
  $description: Displays the remote host name above the disconnect label.
- fadeWhenIdle: false
  $name: Fade when idle
  $description: Fades the button to near-invisible after a few seconds of no hover. Brightens when you move the mouse over it.
- enableHotkey: false
  $name: Enable disconnect hotkey
  $description: Keyboard shortcut to disconnect without clicking the button.
- hotkeyModifier: ctrl-alt
  $name: Hotkey modifier keys
  $description: Modifier keys held for the hotkey. Only used when hotkey is enabled.
  $options:
  - ctrl-alt: Ctrl + Alt
  - ctrl-shift: Ctrl + Shift
  - alt-shift: Alt + Shift
- hotkeyKey: d
  $name: Hotkey key
  $description: Key pressed with the modifier. Only used when hotkey is enabled.
  $options:
  - d: D
  - q: Q
  - f4: F4
  - end: End
  - pause: Pause / Break
- showSessionInfo: true
  $name: Show session duration and idle time
  $description: Shows how long the session window has been open and how long this computer's own keyboard and mouse have been idle. Appears as a row on the floating overlay button (fullscreen sessions only) and in the tooltip of the taskbar-embedded panel's status text (all sessions — hover it). Idle time is measured on this computer, not from remote-session activity.
- showConnectionQuality: true
  $name: Show connection quality
  $description: Adds the connection quality Remote Desktop itself reports (4 levels, Poor to Excellent) to the tooltip of the taskbar-embedded panel's status text, along with the reported bandwidth and round-trip time. The tooltip says it is still waiting until the first report arrives, and says so plainly if the session was already open when the mod loaded.
- enableReconnect: false
  $name: Enable Reconnect and Force reconnect
  $description: Off by default. When on, adds a Reconnect button to the taskbar-embedded panel that cleanly disconnects this session (same as Disconnect) and then reopens the same connection in the preferred display mode below, and lets the stuck-session alert offer Force reconnect. When off, no surface can relaunch the connection — the stuck-session alert still appears but offers only Dismiss. Nothing ever reconnects without your click.
- reconnectDisplayMode: fullscreen
  $name: Preferred display mode for reconnects
  $description: How a reopened session is displayed by Reconnect and Force reconnect. Windowed uses the width and height below.
  $options:
  - fullscreen: Fullscreen (/f)
  - windowed: "Windowed (/w: and /h:)"
  - multimon: All monitors (/multimon)
- reconnectWindowWidth: 0
  $name: Windowed reconnect width
  $description: Remote desktop width for windowed reconnects. 0 = reuse the current session window's size at the moment you reconnect.
- reconnectWindowHeight: 0
  $name: Windowed reconnect height
  $description: Remote desktop height for windowed reconnects. 0 = reuse the current session window's size at the moment you reconnect.
- stuckDetection: true
  $name: Detect stuck sessions
  $description: Watches the session window for a hang. Once it has stopped responding for the threshold below, a small alert appears on the session's own monitor and the taskbar-embedded panel shows a not-responding line. With Enable Reconnect on, the alert offers Force reconnect (which ends the stuck client and reopens the connection using the reconnect display mode); with it off, the alert offers Dismiss only. Never reconnects on its own — it always waits for your click. Keep this on if you turn the floating overlay off, since the watchdog also keeps the status snapshot flowing to the panel.
- stuckThresholdSeconds: 10
  $name: Stuck threshold (seconds)
  $description: How many consecutive seconds the session window must be reported as not responding before the alert appears. Windows itself only reports a window as not responding after about 5 seconds, so the alert appears roughly that much later than this value.
- showFullscreenToggle: true
  $name: Fullscreen / windowed toggle button
  $description: Shows the fullscreen / windowed button on the taskbar-embedded panel. It switches the session between fullscreen and windowed without disconnecting, by sending Remote Desktop's own Ctrl+Alt+Break toggle to the session window, so it behaves exactly like pressing that shortcut. Turn it off to leave the button out of the panel's row.
- debugRelayTestMinimize: false
  $name: "Debug: send test minimize via relay"
  $description: Turning this on sends one test WM_COPYDATA minimize command to the mod's own CitadelRdpTaskbarRelay message window, exercising the relay receiver without the DVC relay plugin existing. Turn it off and on again to send another. Has no other effect.
- embeddedWidgetPosition: Right
  $name: Taskbar-embedded panel position (explorer.exe)
  $description: "Where on the taskbar the always-visible status panel appears. Applies to the explorer.exe surface only — the floating overlay button above has its own corner setting."
  $options:
  - Right: Right — next to clock & tray
  - Left: Left — taskbar far left
  - Center: Center — middle of taskbar
- embeddedOffsetX: 8
  $name: Taskbar-embedded panel offset (px)
  $description: "Fine-tune the embedded panel placement. Right: gap from the system tray. Left: gap from the left edge. Center: nudge from center (positive = shift right)."
- embeddedPanelWidth: 340
  $name: Taskbar-embedded panel width (px)
  $description: Fixed width of the embedded status-and-buttons panel — the host name, quality/duration line, and all five action buttons must fit inside it.
- embeddedFontSize: 11
  $name: Taskbar-embedded panel font size
  $description: Font size of the embedded panel's host name and quality/duration text, in XAML pixels.
- embeddedShowWhenNoSession: true
  $name: Show taskbar-embedded panel when no session is active
  $description: Keeps a dimmed embedded panel on the taskbar while no Remote Desktop session is running, so you can see the widget is loaded. Turn off to hide the panel entirely until a session starts.
*/
// ==/WindhawkModSettings==

#include <windhawk_api.h>
#include <windhawk_utils.h>

#include <windows.h>
#include <shellscalingapi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <oaidl.h>
#include <ocidl.h>
#include <oleauto.h>
#include <bcrypt.h>
#include <stdio.h>

// winbase.h defines GetCurrentTime() as a macro wrapping GetTickCount().
// winrt XAML headers declare a virtual GetCurrentTime(int64_t*) method.
// Undefine the macro before pulling in WinRT to avoid the collision. Needed by
// the explorer.exe branch's XAML injection; harmless for the mstsc.exe branch.
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

// WH_CATCH logs hresult, std::exception, and unknown exceptions with a context
// label. Usage: try { ... } WH_CATCH(L"context"). Used by the explorer.exe
// branch's WinRT code.
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

// ── Host-process detection (which of our two @include targets are we in?) ──
//
// A single Windhawk mod file is compiled once and injected into every process
// its @include lines match. This mod targets two: mstsc.exe (the RDP client)
// and explorer.exe (the taskbar-embedded panel). Wh_ModInit resolves which one
// this instance is actually running in, exactly once, and stores it here; every
// branch's machinery is started only for its own process, so a function that
// assumes it is inside mstsc.exe can never run inside explorer.exe and vice
// versa. If we somehow land in neither (a future @include change, an odd host),
// the mod loads as an inert no-op rather than guessing a branch.
enum class HostProcess { Unknown, Mstsc, Explorer };
HostProcess g_hostProcess = HostProcess::Unknown;

HostProcess DetectHostProcess() {
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, ARRAYSIZE(path)) == 0)
        return HostProcess::Unknown;
    PCWSTR name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    if (_wcsicmp(name, L"mstsc.exe") == 0)    return HostProcess::Mstsc;
    if (_wcsicmp(name, L"explorer.exe") == 0) return HostProcess::Explorer;
    return HostProcess::Unknown;
}

// ── Shared contracts (used by both branches) ───────────────────────────────
//
// Channel name, command byte values, the status-file struct + its magic, the
// command payload + its magic, and the shared-secret registry location are ONE
// definition here, used by both the mstsc.exe writer/receiver and the
// explorer.exe reader/sender. Before the two mods were consolidated (D-26)
// these were two manually-synchronized copies across two files (D-22/D-23); a
// single mod id makes them a single definition. NOTE: dvc-plugin/relay keeps
// its own copy of the *relay* channel's constants per D-10 — that is a
// genuinely separate build system and deliberately out of scope here.

// Local command channel (widget → mstsc branch) window class + protocol.
constexpr auto   LOCAL_WIDGET_CLASS        = L"CitadelRdpTaskbarLocalWidget";
constexpr PCWSTR LOCAL_WIDGET_REG_KEY      = L"Software\\RDPSessionToolkit";
constexpr PCWSTR LOCAL_WIDGET_REG_VALUE    = L"LocalWidgetSecret";
constexpr PCWSTR LOCAL_WIDGET_SECRET_MUTEX = L"Local\\RDPSessionToolkit.LocalWidgetSecret";
constexpr DWORD  LOCAL_WIDGET_SECRET_BYTES = 32;
constexpr DWORD  kLocalWidgetCmdMagic      = 0x434C5752;  // 'RWLC'
constexpr DWORD  kLocalWidgetCmdVersion    = 1;

enum LocalWidgetCommand : BYTE {
    LWCMD_MINIMIZE          = 0x01,
    LWCMD_RESTORE           = 0x02,
    LWCMD_FULLSCREEN_TOGGLE = 0x03,
    LWCMD_RECONNECT         = 0x04,
    LWCMD_DISCONNECT        = 0x05,
    // 0x00 stays permanently unassigned, as in the relay protocol.
};

// Command payload layout is a contract between the two branches; bump
// kLocalWidgetCmdVersion on any change.
#pragma pack(push, 1)
struct LocalWidgetCommandPayload {
    DWORD magic;
    DWORD version;
    BYTE  command;
    BYTE  reserved[3];
    BYTE  secret[LOCAL_WIDGET_SECRET_BYTES];
};
#pragma pack(pop)

// Status snapshot the mstsc branch writes ~1/s and the explorer branch reads.
constexpr DWORD  kLocalWidgetStatusMagic    = 0x53545352;  // 'RSTS'
constexpr DWORD  kLocalWidgetStatusVersion  = 1;
constexpr PCWSTR kLocalWidgetStatusFileName = L"local-widget-status.dat";

// Status record layout is a contract between the two branches; bump
// kLocalWidgetStatusVersion on any change.
#pragma pack(push, 8)
struct LocalWidgetStatus {
    DWORD     magic;
    DWORD     version;
    DWORD     writerPid;
    BOOL      sessionActive;      // the mstsc branch has a live session frame
    ULONGLONG sessionDurationMs;
    DWORD     localIdleMs;        // this PC's own input idle (D-15)
    int       quality;            // 1–4; 0 = nothing reported yet (D-16)
    LONG      bandwidth;
    LONG      rtt;
    BOOL      qualityAvailable;   // the event sink is advised on the control
    BOOL      hung;               // watchdog: not responding past threshold
    int       hungSeconds;
    BOOL      iconic;
    BOOL      fullscreen;
    ULONGLONG writeTick;          // GetTickCount64 at write — a system-wide clock
    wchar_t   hostname[64];
};
#pragma pack(pop)

// ── Mod storage directory (resolved once, cached) ─────────────────────────
//
// Wh_GetModStoragePath is fixed for the lifetime of the mod — it cannot change
// after Wh_ModInit — yet until v0.7.0 both branches re-queried it on every
// 1 s tick (the mstsc writer per snapshot, the explorer reader per poll): a
// call into the Windhawk engine per second per process for a constant. It is
// now resolved exactly once per process — on first use via thread-safe static
// initialization, and primed (so the result is logged at a predictable
// moment) from each branch's ModInit. The directory is created once here too.
// Wh_GetModStoragePath is per mod id, so BOTH branches (one mod id now)
// resolve the same directory: the explorer branch reads the very file the
// mstsc branch writes (D-26).
struct ModStorageDir {
    bool    ok = false;
    wchar_t path[MAX_PATH] = {};
};

const ModStorageDir& GetModStorageDir() {
    static const ModStorageDir dir = [] {
        ModStorageDir d;
        if (Wh_GetModStoragePath(d.path, ARRAYSIZE(d.path)) == 0 || !d.path[0]) {
            d.path[0] = L'\0';
            Wh_Log(L"ModStorage: Wh_GetModStoragePath failed — storage-backed "
                   L"features (status snapshot, button position) disabled");
            return d;
        }
        CreateDirectoryW(d.path, nullptr);
        d.ok = true;
        Wh_Log(L"ModStorage: directory cached once for this process: %s", d.path);
        return d;
    }();
    return dir;
}

// <cached storage dir>\<fileName>. False (and nothing written to the buffer)
// if the directory is unavailable or the result would not fit.
bool BuildModStorageFilePath(PCWSTR fileName, wchar_t* pathBuffer, size_t bufferChars) {
    const ModStorageDir& dir = GetModStorageDir();
    if (!dir.ok)
        return false;
    if (wcslen(dir.path) + 1 + wcslen(fileName) >= bufferChars)
        return false;
    wcscpy_s(pathBuffer, bufferChars, dir.path);
    size_t len = wcslen(pathBuffer);
    if (len > 0 && pathBuffer[len - 1] != L'\\')
        wcscat_s(pathBuffer, bufferChars, L"\\");
    wcscat_s(pathBuffer, bufferChars, fileName);
    return true;
}

// Path to the status file (see GetModStorageDir for why both branches agree).
bool GetLocalWidgetStatusFilePath(wchar_t* pathBuffer, size_t bufferChars) {
    return BuildModStorageFilePath(kLocalWidgetStatusFileName, pathBuffer, bufferChars);
}

// Wipes secret material in a way the optimizer cannot elide. Local instead
// of the SDK's SecureZeroMemory macro: under mingw-w64 that macro expands to
// an ntdll import (RtlSecureZeroMemory), which the documented compile flags
// do not link — a full-link check of this file failed on exactly that symbol.
// A volatile store loop is the standard portable equivalent.
void WipeSecret(void* p, size_t n) {
    volatile BYTE* v = static_cast<volatile BYTE*>(p);
    while (n--) *v++ = 0;
}

// Reads the shared secret. True only for a REG_BINARY value of exactly the
// expected length; anything else reads as "no secret".
bool ReadLocalWidgetSecret(BYTE* out) {
    DWORD type = 0, cb = LOCAL_WIDGET_SECRET_BYTES;
    LSTATUS r = RegGetValueW(HKEY_CURRENT_USER, LOCAL_WIDGET_REG_KEY,
        LOCAL_WIDGET_REG_VALUE, RRF_RT_REG_BINARY, &type, out, &cb);
    return r == ERROR_SUCCESS && type == REG_BINARY &&
           cb == LOCAL_WIDGET_SECRET_BYTES;
}

// Get-or-create, serialized across both branches (and both processes) by a
// session-local named mutex so two simultaneous first starts cannot each
// generate a different secret and leave one side holding the loser's.
bool EnsureLocalWidgetSecret(PCWSTR source) {
    HANDLE hMutex = CreateMutexW(nullptr, FALSE, LOCAL_WIDGET_SECRET_MUTEX);
    if (hMutex)
        WaitForSingleObject(hMutex, 5000);  // WAIT_ABANDONED still owns it

    BYTE secret[LOCAL_WIDGET_SECRET_BYTES] = {};
    bool ok = ReadLocalWidgetSecret(secret);
    if (ok) {
        Wh_Log(L"%s — local widget secret present in HKCU\\%s", source,
            LOCAL_WIDGET_REG_KEY);
    } else {
        NTSTATUS st = BCryptGenRandom(nullptr, secret, LOCAL_WIDGET_SECRET_BYTES,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (!BCRYPT_SUCCESS(st)) {
            Wh_Log(L"%s — BCryptGenRandom failed 0x%08X; no local widget secret",
                source, (unsigned)st);
        } else {
            HKEY hKey = nullptr;
            LSTATUS r = RegCreateKeyExW(HKEY_CURRENT_USER, LOCAL_WIDGET_REG_KEY, 0,
                nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr);
            if (r == ERROR_SUCCESS) {
                r = RegSetValueExW(hKey, LOCAL_WIDGET_REG_VALUE, 0, REG_BINARY,
                    secret, LOCAL_WIDGET_SECRET_BYTES);
                RegCloseKey(hKey);
            }
            ok = (r == ERROR_SUCCESS);
            Wh_Log(L"%s — local widget secret %s (HKCU\\%s\\%s, %lu)", source,
                ok ? L"generated" : L"generation FAILED", LOCAL_WIDGET_REG_KEY,
                LOCAL_WIDGET_REG_VALUE, (unsigned long)r);
        }
    }
    WipeSecret(secret, sizeof(secret));

    if (hMutex) {
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
    }
    return ok;
}

// Human-readable label for a reported connection-quality level (D-16).
PCWSTR QualityLabel(int q) {
    switch (q) {
    case 4:  return L"Excellent";
    case 3:  return L"Good";
    case 2:  return L"Fair";
    case 1:  return L"Poor";
    default: return L"unknown";
    }
}

// "1h 23m" / "23m" / "<1m" — the coarse, minute-granular form used by the
// rich status tooltip. Shared: the mstsc branch used it for the thumbnail
// toolbar's status-icon tooltip; that tooltip now lives on the explorer
// branch's taskbar-embedded panel (D-33), and it is the only caller left.
void FormatCoarse(ULONGLONG ms, wchar_t* out, size_t cch) {
    ULONGLONG mins = ms / 60000;
    unsigned h = (unsigned)(mins / 60), m = (unsigned)(mins % 60);
    if (h)      swprintf_s(out, cch, L"%uh %02um", h, m);
    else if (m) swprintf_s(out, cch, L"%um", m);
    else        wcscpy_s(out, cch, L"<1m");
}

// "1:23:45" / "23:45" — the compact, second-granular clock form used by both
// the overlay's status row and the embedded panel's duration line.
void FormatClock(ULONGLONG ms, wchar_t* out, size_t cch) {
    ULONGLONG s = ms / 1000;
    unsigned h = (unsigned)(s / 3600), m = (unsigned)(s / 60 % 60),
             sec = (unsigned)(s % 60);
    if (h) swprintf_s(out, cch, L"%u:%02u:%02u", h, m, sec);
    else   swprintf_s(out, cch, L"%u:%02u", m, sec);
}

// ── mstsc.exe branch ───────────────────────────────────────────────────────
// Everything from here to the end of `namespace client` runs only when this
// mod is injected into mstsc.exe (see DetectHostProcess / Wh_ModInit).
namespace client {

// ── Constants ─────────────────────────────────────────────────────────────

// 96×72 since v0.4.0 (was 80×56): one more display row for the session
// clock, and a little extra width so that row's text fits at a legible size.
constexpr int  BTN_W           = 96;
constexpr int  BTN_H           = 72;
constexpr BYTE ALPHA_FULL      = 230;
constexpr BYTE ALPHA_FADED     = 35;
constexpr UINT FADE_DELAY_MS   = 4000;
constexpr int  FADE_TIMER_ID   = 42;
constexpr int  ICONIC_TIMER_ID = 43;
constexpr UINT ICONIC_POLL_MS  = 400;
constexpr int  STATUS_TIMER_ID = 44;
constexpr UINT STATUS_POLL_MS  = 1000;

// Row layout, top to bottom (logical px, scaled at paint/click time):
// [0, ZONE_STATUS_TOP) = hostname display (not clickable),
// [ZONE_STATUS_TOP, ZONE_MINRESTORE_TOP) = session clock / local idle
// display (not clickable), [ZONE_MINRESTORE_TOP, ZONE_DISCONNECT_TOP) =
// Minimize (left half) / Restore (right half) side by side,
// [ZONE_DISCONNECT_TOP, BTN_H) = Disconnect.
constexpr int  ZONE_STATUS_TOP     = 20;
constexpr int  ZONE_MINRESTORE_TOP = 36;
constexpr int  ZONE_DISCONNECT_TOP = 56;
constexpr int  HOTKEY_ID       = 1;
constexpr auto BTN_CLASS       = L"WH_RdpstkClientBtn";
constexpr UINT WM_CREATE_BTN   = WM_APP + 1;
constexpr UINT WM_HIDE_BTN     = WM_APP + 2;
constexpr UINT WM_REPAINT_BTN  = WM_APP + 3;

// ── Settings ──────────────────────────────────────────────────────────────

/*
 * These globals are non-atomic scalars written by LoadSettings() on Windhawk's
 * settings-changed thread and read concurrently by hook callbacks and the helper
 * thread without synchronization. This is a data race and technically undefined
 * behavior per the C++ standard. The decision is deliberate: on x86/x64 all
 * naturally-aligned word-sized loads and stores are atomic at the hardware level;
 * no cross-field invariant exists across these flags; and the worst observable
 * outcome is a setting taking effect one repaint late, which already matches the
 * mod's recreate-on-change model. The fields that carry real cross-thread invariants
 * (g_hLastMonitor, g_hotkeyRegistered) are separately guarded with std::atomic.
 */
bool g_hideBar        = true;
bool g_showOverlay    = false;  // floating overlay button (mstsc monitor corner)
bool g_buttonOnRight  = true;
bool g_buttonAtBottom = false;
int  g_buttonOffset   = 32;
bool g_showBorder     = true;
bool g_showHostname   = true;
bool g_fadeWhenIdle   = false;
bool g_enableHotkey   = false;
bool g_debugRelayTest = false;
bool g_showSessionInfo = true;
// showFullscreenToggle and showConnectionQuality are read by the explorer.exe
// branch, not here (v0.9.0): the taskbar-embedded panel is the only surface
// that presents either, now that the thumbnail toolbar is gone (D-33). Same
// one-setting-read-by-the-owning-branch pattern as enableReconnect, which the
// embedded branch already reads for its own button. The quality event sink
// below still runs unconditionally in this process — it publishes into the
// status snapshot; what is *displayed* is the panel's decision.
// Gates EVERY relaunch surface — the taskbar-embedded panel's Reconnect
// button, the local widget command it sends, and the stuck-session alert's
// Force reconnect (D-28).
// Off by default: relaunching a connection is the one action here that can
// do something unexpected (a plan captured against a temp .rdp file an
// external launcher has since deleted, say), so it is opt-in.
bool g_enableReconnect = false;
int  g_reconnectMode  = 0;      // ReconnectMode (declared with the helper)
int  g_reconnectW     = 0;      // 0 = current session window size
int  g_reconnectH     = 0;
bool g_stuckDetection = true;
int  g_stuckThresholdSec = 10;
std::atomic<bool> g_hotkeyRegistered { false };
UINT g_hotkeyMod      = MOD_CONTROL | MOD_ALT;
UINT g_hotkeyVk       = 'D';

void LoadSettings() {
    g_hideBar      = Wh_GetIntSetting(L"hideBar")      != 0;
    g_showOverlay  = Wh_GetIntSetting(L"showOverlay")  != 0;
    g_showBorder   = Wh_GetIntSetting(L"showBorder")   != 0;
    g_showHostname = Wh_GetIntSetting(L"showHostname") != 0;
    g_fadeWhenIdle = Wh_GetIntSetting(L"fadeWhenIdle") != 0;
    g_enableHotkey = Wh_GetIntSetting(L"enableHotkey") != 0;
    g_debugRelayTest = Wh_GetIntSetting(L"debugRelayTestMinimize") != 0;
    g_showSessionInfo = Wh_GetIntSetting(L"showSessionInfo") != 0;
    g_enableReconnect = Wh_GetIntSetting(L"enableReconnect") != 0;
    g_reconnectW     = Wh_GetIntSetting(L"reconnectWindowWidth");
    g_reconnectH     = Wh_GetIntSetting(L"reconnectWindowHeight");
    g_stuckDetection = Wh_GetIntSetting(L"stuckDetection") != 0;
    g_stuckThresholdSec = Wh_GetIntSetting(L"stuckThresholdSeconds");
    if (g_stuckThresholdSec < 1) g_stuckThresholdSec = 1;

    // Reconnect display mode (ReconnectMode values)
    PCWSTR rmode = Wh_GetStringSetting(L"reconnectDisplayMode");
    if      (lstrcmpW(rmode, L"windowed") == 0) g_reconnectMode = 1;
    else if (lstrcmpW(rmode, L"multimon") == 0) g_reconnectMode = 2;
    else                                        g_reconnectMode = 0;
    Wh_FreeStringSetting(rmode);

    // Position dropdown
    PCWSTR pos = Wh_GetStringSetting(L"buttonPosition");
    g_buttonOnRight  = lstrcmpW(pos, L"top-left")    != 0
                    && lstrcmpW(pos, L"bottom-left")  != 0;
    g_buttonAtBottom = lstrcmpW(pos, L"bottom-right") == 0
                    || lstrcmpW(pos, L"bottom-left")  == 0;
    Wh_FreeStringSetting(pos);

    // Offset — custom overrides preset when non-zero
    int custom = Wh_GetIntSetting(L"offsetCustom");
    if (custom != 0) {
        g_buttonOffset = custom;
    } else {
        PCWSTR preset = Wh_GetStringSetting(L"offsetPreset");
        if      (lstrcmpW(preset, L"none")     == 0) g_buttonOffset =   0;
        else if (lstrcmpW(preset, L"small")    == 0) g_buttonOffset =  16;
        else if (lstrcmpW(preset, L"large")    == 0) g_buttonOffset =  64;
        else if (lstrcmpW(preset, L"xlarge")   == 0) g_buttonOffset =  96;
        else if (lstrcmpW(preset, L"xxlarge")  == 0) g_buttonOffset = 256;
        else if (lstrcmpW(preset, L"xxxlarge") == 0) g_buttonOffset = 512;
        else                                          g_buttonOffset =  32;
        Wh_FreeStringSetting(preset);
    }

    // Hotkey modifier
    PCWSTR mod = Wh_GetStringSetting(L"hotkeyModifier");
    if      (lstrcmpW(mod, L"ctrl-shift") == 0) g_hotkeyMod = MOD_CONTROL | MOD_SHIFT;
    else if (lstrcmpW(mod, L"alt-shift")  == 0) g_hotkeyMod = MOD_ALT     | MOD_SHIFT;
    else                                         g_hotkeyMod = MOD_CONTROL | MOD_ALT;
    Wh_FreeStringSetting(mod);

    // Hotkey key
    PCWSTR key = Wh_GetStringSetting(L"hotkeyKey");
    if      (lstrcmpW(key, L"q")     == 0) g_hotkeyVk = 'Q';
    else if (lstrcmpW(key, L"f4")    == 0) g_hotkeyVk = VK_F4;
    else if (lstrcmpW(key, L"end")   == 0) g_hotkeyVk = VK_END;
    else if (lstrcmpW(key, L"pause") == 0) g_hotkeyVk = VK_PAUSE;
    else                                   g_hotkeyVk = 'D';
    Wh_FreeStringSetting(key);
}

// ── Shared state ──────────────────────────────────────────────────────────

CRITICAL_SECTION          g_cs;
HWND                      g_hBBar           = nullptr;
HWND                      g_hRdpFrame       = nullptr;
WNDPROC                   g_origBBarWndProc = nullptr;
WNDPROC                   g_origFrameWndProc = nullptr;  // guarded by g_cs

// Registered window message for the frame subclass. Written once in
// Wh_ModInit before any hook can run, read-only afterwards — no
// synchronization needed. RegisterWindowMessage (not a WM_APP offset) so the
// value cannot collide with anything mstsc itself uses on its frame window.
// One message remains since v0.9.0: the thumb bar's TaskbarButtonCreated /
// refresh / status-refresh messages went with it (D-33); the event sink's
// home-thread teardown did not.
UINT g_msgSinkTeardown = 0;  // release the RDP event sink on the frame thread
std::atomic<HMONITOR>     g_hLastMonitor    { nullptr };
wchar_t                   g_hostname[256]   = {};
HANDLE                    g_hHelperThread   = nullptr;
std::atomic<DWORD>        g_helperThreadId  { 0 };
// Manual-reset, signaled once HelperThread's message queue exists — see the
// StartHelperThread/StopHelperThread comments below.
HANDLE                    g_hHelperThreadReady = nullptr;

// Dragged button position, overriding the settings-derived default when set.
// Guarded by g_cs: written by the helper thread on drag finalize and cleared
// by Wh_ModSettingsChanged on a relevant settings change (different thread).
bool                      g_hasDragPos      = false;
bool                      g_dragOnRight     = true;
bool                      g_dragAtBottom    = false;
int                       g_dragDx          = 0;
int                       g_dragDy          = 0;

// ── Hook originals ────────────────────────────────────────────────────────

using CreateWindowExW_t  = decltype(&CreateWindowExW);
using ShowWindow_t       = decltype(&ShowWindow);
using SetWindowPos_t     = decltype(&SetWindowPos);
using SetWindowTextW_t   = decltype(&SetWindowTextW);

CreateWindowExW_t  pOrigCreateWindowExW  = nullptr;
ShowWindow_t       pOrigShowWindow       = nullptr;
SetWindowPos_t     pOrigSetWindowPos     = nullptr;
SetWindowTextW_t   pOrigSetWindowTextW   = nullptr;

// ── Monitor helper ────────────────────────────────────────────────────────

RECT GetMonitorRect(HWND hRef) {
    HMONITOR hMon = hRef && IsWindow(hRef)
        ? MonitorFromWindow(hRef, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof(mi) };
    if (hMon && GetMonitorInfoW(hMon, &mi))
        return mi.rcMonitor;
    return { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
}

RECT GetRdpMonitorRect() {
    EnterCriticalSection(&g_cs);
    HWND hRef = g_hRdpFrame ? g_hRdpFrame : g_hBBar;
    LeaveCriticalSection(&g_cs);
    return GetMonitorRect(hRef);
}

HMONITOR GetRdpMonitor() {
    EnterCriticalSection(&g_cs);
    HWND hRef = g_hRdpFrame ? g_hRdpFrame : g_hBBar;
    LeaveCriticalSection(&g_cs);
    return hRef && IsWindow(hRef)
        ? MonitorFromWindow(hRef, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
}

// Constrains a w×h rect at (x, y) to stay fully within mon, preferring to
// keep the top-left corner in bounds first. Shared by the live WM_MOUSEMOVE
// drag-follow code and FinalizeDragPosition's end-of-drag safety net so both
// clamp against the same math.
POINT ClampToMonitorRect(int x, int y, int w, int h, const RECT& mon) {
    if (x < mon.left) x = mon.left;
    if (y < mon.top) y = mon.top;
    if (x + w > mon.right)  x = mon.right  - w;
    if (y + h > mon.bottom) y = mon.bottom - h;
    return { x, y };
}

// ── Button position persistence ─────────────────────────────────────────────

struct PersistedButtonPos {
    DWORD magic;
    DWORD version;
    BOOL  onRight;
    BOOL  atBottom;
    int   dx;
    int   dy;
};

constexpr DWORD kButtonPosMagic   = 0x50425244; // 'DRBP'
constexpr DWORD kButtonPosVersion = 1;
constexpr PCWSTR kButtonPosFileName = L"button-pos.dat";

// Uses the once-cached mod storage directory (GetModStorageDir, shared).
bool GetButtonPosFilePath(wchar_t* pathBuffer, size_t bufferChars) {
    return BuildModStorageFilePath(kButtonPosFileName, pathBuffer, bufferChars);
}

void PersistDragPosition(bool onRight, bool atBottom, int dx, int dy) {
    wchar_t path[MAX_PATH + 32];
    if (!GetButtonPosFilePath(path, ARRAYSIZE(path)))
        return;

    PersistedButtonPos data{ kButtonPosMagic, kButtonPosVersion,
        onRight ? TRUE : FALSE, atBottom ? TRUE : FALSE, dx, dy };

    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        Wh_Log(L"Failed to open button position file for writing, GLE=%d", GetLastError());
        return;
    }
    DWORD written = 0;
    WriteFile(hFile, &data, sizeof(data), &written, nullptr);
    CloseHandle(hFile);
}

void ClearPersistedDragPosition() {
    wchar_t path[MAX_PATH + 32];
    if (GetButtonPosFilePath(path, ARRAYSIZE(path)))
        DeleteFileW(path);
}

bool LoadPersistedDragPosition(bool* onRight, bool* atBottom, int* dx, int* dy) {
    wchar_t path[MAX_PATH + 32];
    if (!GetButtonPosFilePath(path, ARRAYSIZE(path)))
        return false;

    HANDLE hFile = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    PersistedButtonPos data{};
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &data, sizeof(data), &read, nullptr);
    CloseHandle(hFile);

    if (!ok || read != sizeof(data) || data.magic != kButtonPosMagic ||
        data.version != kButtonPosVersion)
        return false;

    *onRight  = data.onRight  != FALSE;
    *atBottom = data.atBottom != FALSE;
    *dx = data.dx;
    *dy = data.dy;
    return true;
}

// Clamps the button to the RDP monitor's full rect, derives the nearest
// corner and (dx, dy) offset from it, and persists the result. Called on
// drag finalize (button-up past the drag threshold, or capture loss).
void FinalizeDragPosition(HWND hwnd) {
    HMONITOR hMon = GetRdpMonitor();
    // Matches CreateOrRepositionButton's reference rect (mi.rcMonitor, not
    // rcWork) so the offset computed here reproduces the same on-screen
    // position when reapplied — the button is meant to sit flush against
    // the physical screen edge, same as the settings-driven default.
    MONITORINFO mi = { sizeof(mi) };
    RECT mon = (hMon && GetMonitorInfoW(hMon, &mi))
        ? mi.rcMonitor
        : RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    UINT dpiX = 96, dpiY = 96;
    if (hMon && FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96; dpiY = 96;
    }

    RECT rc;
    GetWindowRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    POINT clamped = ClampToMonitorRect(rc.left, rc.top, w, h, mon);
    int x = clamped.x, y = clamped.y;

    if (x != rc.left || y != rc.top) {
        pOrigSetWindowPos(hwnd, nullptr, x, y, 0, 0,
            SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    bool onRight  = centerX > (mon.left + mon.right)  / 2;
    bool atBottom = centerY > (mon.top  + mon.bottom) / 2;

    int dxPx = onRight  ? (mon.right  - (x + w)) : (x - mon.left);
    int dyPx = atBottom ? (mon.bottom - (y + h)) : (y - mon.top);
    int dx = MulDiv(dxPx, 96, dpiX);
    int dy = MulDiv(dyPx, 96, dpiY);

    EnterCriticalSection(&g_cs);
    g_hasDragPos   = true;
    g_dragOnRight  = onRight;
    g_dragAtBottom = atBottom;
    g_dragDx       = dx;
    g_dragDy       = dy;
    LeaveCriticalSection(&g_cs);

    PersistDragPosition(onRight, atBottom, dx, dy);

    Wh_Log(L"Button drag finalized: onRight=%d atBottom=%d dx=%d dy=%d",
        (int)onRight, (int)atBottom, dx, dy);
}

// ── Hostname ──────────────────────────────────────────────────────────────

void UpdateHostname() {
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);

    wchar_t title[512] = {};
    if (hFrame && IsWindow(hFrame))
        GetWindowTextW(hFrame, title, 512);

    // Strip " - Remote Desktop Connection" suffix
    wchar_t* sep = wcsstr(title, L" - ");
    if (sep) *sep = L'\0';

    EnterCriticalSection(&g_cs);
    wcsncpy_s(g_hostname, title[0] ? title : L"", _TRUNCATE);
    LeaveCriticalSection(&g_cs);
    Wh_Log(L"Hostname: %s", g_hostname);
}

// ── Disconnect ────────────────────────────────────────────────────────────

void ClearPendingReconnect();  // reconnect helper, below

// keepPendingReconnect is true only for the reconnect helper's own close;
// every plain Disconnect (overlay, panel, hotkey) drops any parked
// reconnect plan so a later ordinary close can never relaunch by surprise.
void DisconnectSession(HWND hRef, bool keepPendingReconnect = false) {
    if (!keepPendingReconnect)
        ClearPendingReconnect();
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = GetAncestor(hRef, GA_ROOT);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = FindWindowW(L"TscShellContainerClass", nullptr);
    Wh_Log(L"Disconnect: WM_CLOSE → %p", hFrame);
    if (hFrame)
        PostMessageW(hFrame, WM_CLOSE, 0, 0);
}

// ── Minimize / Restore ────────────────────────────────────────────────────

// Resolves the RDP frame for the minimize/restore zones. Deliberately never
// falls back to GetAncestor of the button itself the way DisconnectSession
// does — minimizing the wrong window is worse than a no-op.
HWND GetRdpFrameForAction() {
    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    LeaveCriticalSection(&g_cs);
    if (!hFrame || !IsWindow(hFrame))
        hFrame = FindWindowW(L"TscShellContainerClass", nullptr);
    return hFrame;
}

// Live minimize-state query. No hook tracks this: a taskbar-initiated
// minimize runs in explorer.exe and is not guaranteed to produce any
// in-process ShowWindow call this mod's hooks would see, so the state is
// sampled fresh whenever it matters (paint, zone click, poll timer).
bool IsRdpFrameIconic() {
    HWND hFrame = GetRdpFrameForAction();
    return hFrame && IsIconic(hFrame) != FALSE;
}

// The one shared minimize action, used by the relay receiver and the local
// widget receiver alike, so every requester takes the identical path. A
// request while already minimized (or with no frame) is a deliberate no-op.
// pOrigShowWindow can be null for a very early relay command — the hooks
// are applied only after Wh_ModInit returns — so fall back to the plain API.
bool MinimizeRdpFrame(PCWSTR source) {
    HWND hFrame = GetRdpFrameForAction();
    if (hFrame && !IsIconic(hFrame)) {
        Wh_Log(L"%s — SW_MINIMIZE → %p", source, hFrame);
        (pOrigShowWindow ? pOrigShowWindow : ShowWindow)(hFrame, SW_MINIMIZE);
        return true;
    }
    Wh_Log(L"%s — minimize while inactive, ignored", source);
    return false;
}

// The shared restore counterpart of MinimizeRdpFrame — the overlay's Restore
// zone and the local widget receiver both take this one path. A request while
// not minimized (or with no frame) is a deliberate
// no-op, mirroring the minimize side.
bool RestoreRdpFrame(PCWSTR source) {
    HWND hFrame = GetRdpFrameForAction();
    if (hFrame && IsIconic(hFrame)) {
        Wh_Log(L"%s — SW_RESTORE → %p", source, hFrame);
        (pOrigShowWindow ? pOrigShowWindow : ShowWindow)(hFrame, SW_RESTORE);
        return true;
    }
    Wh_Log(L"%s — restore while not minimized, ignored", source);
    return false;
}

// ── Reconnect helper (shared by Reconnect and Force reconnect) ────────────
//
// Two steps, independent of what triggered them (DECISIONS.md D-17):
//   1. BuildReconnectPlan captures everything needed to reopen this exact
//      connection as a ready-to-run mstsc command line: this process's own
//      command line minus any display switches — so /v:, an .rdp file,
//      /admin, /restrictedAdmin, gateway options … all carry over — plus
//      /v:<host> from the cached window title if the command line named no
//      target, plus the display switches for the preferred mode (/f, or
//      /w: /h:, or /multimon).
//   2. LaunchReconnect starts that command line — at most once per process,
//      guarded by an atomic, however many paths race to it.
// The clean trigger (Reconnect button) parks the plan as "pending", posts
// the established WM_CLOSE (D-4), and the frame's WM_DESTROY launches it —
// the last moment this process is reliably alive, reached only if the close
// really happened (a cancelled close prompt just leaves the plan to expire).
// The force trigger (stuck-session watchdog) launches it itself before
// terminating the process. Neither ever runs without an explicit click.

enum ReconnectMode : int {
    RECONNECT_FULLSCREEN = 0,
    RECONNECT_WINDOWED   = 1,
    RECONNECT_MULTIMON   = 2,
};

struct ReconnectPlan {
    bool      valid        = false;
    wchar_t   exe[MAX_PATH] = {};
    wchar_t   cmdLine[4096] = {};
    // The connection-file argument carried over from the command line (a bare
    // argument — the .rdp file mstsc was started with), if any. Re-checked on
    // disk at the moment of launch, not just when the plan was built (D-28).
    wchar_t   rdpFile[MAX_PATH] = {};
    ULONGLONG expiresTick  = 0;   // pending plans expire (GetTickCount64)
};

constexpr ULONGLONG RECONNECT_PENDING_TTL_MS = 60000;

ReconnectPlan     g_pendingReconnect;               // guarded by g_cs
std::atomic<bool> g_reconnectLaunched { false };    // once per process

bool IsDisplaySwitch(PCWSTR a) {
    if (!a || (a[0] != L'/' && a[0] != L'-')) return false;
    PCWSTR s = a + 1;
    return _wcsicmp(s, L"f") == 0 || _wcsicmp(s, L"multimon") == 0 ||
           _wcsicmp(s, L"span") == 0 || _wcsnicmp(s, L"w:", 2) == 0 ||
           _wcsnicmp(s, L"h:", 2) == 0;
}

// /v:<server> or a bare argument (an .rdp file path) both name a target.
bool IsTargetArg(PCWSTR a) {
    if (!a || !a[0]) return false;
    if (a[0] == L'/' || a[0] == L'-')
        return _wcsnicmp(a + 1, L"v:", 2) == 0;
    return true;
}

bool AppendArg(wchar_t* cmd, size_t cch, PCWSTR arg) {
    bool quote = wcschr(arg, L' ') != nullptr && arg[0] != L'"';
    size_t need = wcslen(cmd) + 1 + wcslen(arg) + (quote ? 2 : 0) + 1;
    if (need > cch)
        return false;
    wcscat_s(cmd, cch, L" ");
    if (quote) wcscat_s(cmd, cch, L"\"");
    wcscat_s(cmd, cch, arg);
    if (quote) wcscat_s(cmd, cch, L"\"");
    return true;
}

PCWSTR ReconnectModeLabel() {
    switch (g_reconnectMode) {
    case RECONNECT_WINDOWED: return L"windowed";
    case RECONNECT_MULTIMON: return L"all monitors";
    default:                 return L"fullscreen";
    }
}

// Captures the relaunch for the current connection. Safe to call from any
// thread, including while the frame thread is hung: it reads only the
// cached hostname, this process's command line, and the frame's client rect
// (GetClientRect reads window data without messaging the owning thread).
bool BuildReconnectPlan(ReconnectPlan* plan, PCWSTR source) {
    *plan = ReconnectPlan{};
    if (!GetModuleFileNameW(nullptr, plan->exe, ARRAYSIZE(plan->exe))) {
        Wh_Log(L"%s — GetModuleFileNameW failed GLE=%d", source, GetLastError());
        return false;
    }
    wchar_t* cmd = plan->cmdLine;
    const size_t cch = ARRAYSIZE(plan->cmdLine);
    swprintf_s(cmd, cch, L"\"%s\"", plan->exe);

    bool hasTarget = false;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) {
            if (IsDisplaySwitch(argv[i]))
                continue;
            if (IsTargetArg(argv[i])) {
                hasTarget = true;
                // A bare target argument is the connection (.rdp) file;
                // remember it for the launch-time existence check.
                if (argv[i][0] != L'/' && argv[i][0] != L'-' && !plan->rdpFile[0])
                    wcsncpy_s(plan->rdpFile, argv[i], _TRUNCATE);
            }
            if (!AppendArg(cmd, cch, argv[i])) {
                Wh_Log(L"%s — command line too long to carry over", source);
                LocalFree(argv);
                return false;
            }
        }
        LocalFree(argv);
    }

    if (!hasTarget) {
        wchar_t host[256];
        EnterCriticalSection(&g_cs);
        wcsncpy_s(host, g_hostname, _TRUNCATE);
        LeaveCriticalSection(&g_cs);
        if (!host[0]) {
            Wh_Log(L"%s — no target: the command line names none and the "
                   L"window title carries no host yet", source);
            return false;
        }
        wchar_t v[300];
        swprintf_s(v, ARRAYSIZE(v), L"/v:%s", host);
        if (!AppendArg(cmd, cch, v))
            return false;
    }

    bool ok = true;
    switch (g_reconnectMode) {
    case RECONNECT_MULTIMON:
        ok = AppendArg(cmd, cch, L"/multimon");
        break;
    case RECONNECT_WINDOWED: {
        int w = g_reconnectW, h = g_reconnectH;
        if (w <= 0 || h <= 0) {
            // "Remembered" = the size the session has right now.
            HWND hFrame = GetRdpFrameForAction();
            RECT rc = {};
            if (hFrame && GetClientRect(hFrame, &rc)) {
                w = rc.right - rc.left;
                h = rc.bottom - rc.top;
            }
        }
        if (w < 200 || h < 200) { w = 1280; h = 800; }  // mstsc's floor is 200
        wchar_t ws[32], hs[32];
        swprintf_s(ws, ARRAYSIZE(ws), L"/w:%d", w);
        swprintf_s(hs, ARRAYSIZE(hs), L"/h:%d", h);
        ok = AppendArg(cmd, cch, ws) && AppendArg(cmd, cch, hs);
        break;
    }
    default:
        ok = AppendArg(cmd, cch, L"/f");
        break;
    }
    if (!ok)
        return false;

    plan->valid = true;
    if (plan->rdpFile[0]) {
        bool existsNow = GetFileAttributesW(plan->rdpFile) != INVALID_FILE_ATTRIBUTES;
        Wh_Log(L"%s — reconnect plan (%s): %s [connection file: %s, exists now=%d; "
               L"re-checked at launch]", source, ReconnectModeLabel(), cmd,
               plan->rdpFile, (int)existsNow);
    } else {
        Wh_Log(L"%s — reconnect plan (%s): %s", source, ReconnectModeLabel(), cmd);
    }
    return true;
}

// Pre-launch safety check (D-28). A plan that carries a connection-file
// argument is only as good as that file at the moment the launch actually
// fires — not when the plan was built. The observed failure mode: mstsc was
// started by an external launcher against a temp .rdp file that the launcher
// deleted or regenerated by the time the relaunch ran, so the replacement
// client opened on a missing file. Relative paths resolve against this
// process's current directory, which is also what CreateProcessW inherits
// below, so the check and the launch see the same file.
bool ReconnectFileStillExists(const ReconnectPlan* plan, PCWSTR source) {
    if (!plan->rdpFile[0])
        return true;  // the plan names its target with /v:, nothing to check
    DWORD attrs = GetFileAttributesW(plan->rdpFile);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        Wh_Log(L"%s — NOT launched: the connection file the plan references no "
               L"longer exists at launch time (GLE=%d): %s", source,
               GetLastError(), plan->rdpFile);
        return false;
    }
    if (attrs & FILE_ATTRIBUTE_DIRECTORY) {
        Wh_Log(L"%s — NOT launched: the connection file path is a directory "
               L"now: %s", source, plan->rdpFile);
        return false;
    }
    return true;
}

// The one launch point. First caller wins; everyone else is told so.
bool LaunchReconnect(ReconnectPlan* plan, PCWSTR source) {
    if (!plan->valid)
        return false;
    if (g_reconnectLaunched.exchange(true)) {
        Wh_Log(L"%s — a reconnect was already launched by this process, skipped",
            source);
        return false;
    }
    // Same outcome as a failed CreateProcessW: logged, reported as a failed
    // reconnect to the caller, and the once-per-process guard handed back.
    if (!ReconnectFileStillExists(plan, source)) {
        g_reconnectLaunched.store(false);
        return false;
    }
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(plan->exe, plan->cmdLine, nullptr, nullptr, FALSE, 0,
                        nullptr, nullptr, &si, &pi)) {
        Wh_Log(L"%s — CreateProcessW failed GLE=%d", source, GetLastError());
        g_reconnectLaunched.store(false);
        return false;
    }
    Wh_Log(L"%s — launched replacement mstsc pid=%u", source, pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void SetPendingReconnect(const ReconnectPlan& plan) {
    EnterCriticalSection(&g_cs);
    g_pendingReconnect = plan;
    g_pendingReconnect.expiresTick = GetTickCount64() + RECONNECT_PENDING_TTL_MS;
    LeaveCriticalSection(&g_cs);
}

void ClearPendingReconnect() {
    EnterCriticalSection(&g_cs);
    bool had = g_pendingReconnect.valid;
    g_pendingReconnect.valid = false;
    LeaveCriticalSection(&g_cs);
    if (had)
        Wh_Log(L"Reconnect: pending plan dropped (plain disconnect requested)");
}

// Frame WM_DESTROY hook point: the session really closed — launch the parked
// plan, if it is still fresh.
void LaunchPendingReconnect() {
    ReconnectPlan plan;
    EnterCriticalSection(&g_cs);
    plan = g_pendingReconnect;
    g_pendingReconnect.valid = false;
    LeaveCriticalSection(&g_cs);
    if (!plan.valid)
        return;
    if (GetTickCount64() > plan.expiresTick) {
        Wh_Log(L"Reconnect: pending plan expired before the frame closed — not launched");
        return;
    }
    LaunchReconnect(&plan, L"Reconnect (frame destroyed with plan pending)");
}

// Clean trigger: capture, park, close via the established disconnect path.
void ReconnectSessionClean(HWND hRef, PCWSTR source) {
    ReconnectPlan plan;
    if (!BuildReconnectPlan(&plan, source))
        return;
    SetPendingReconnect(plan);
    Wh_Log(L"%s — plan parked, closing session (relaunch fires from WM_DESTROY)",
        source);
    DisconnectSession(hRef, /*keepPendingReconnect=*/true);
}

// ── Session clock / local idle ────────────────────────────────────────────
//
// Session start is stamped at the existing connection-detection hook point —
// CreateWindowExW_Hook latching the TscShellContainerClass frame — so the
// clock starts when mstsc opens the session window. Idle time is this
// computer's own physical input idle time (GetLastInputInfo): keyboard/mouse
// activity on the client machine, whether or not it is aimed into the RDP
// session. That is the intended meaning — it is NOT a measure of activity
// inside the remote session (DECISIONS.md D-15).

std::atomic<ULONGLONG> g_sessionStartTick { 0 };  // GetTickCount64; 0 = none yet

ULONGLONG GetSessionDurationMs() {
    ULONGLONG start = g_sessionStartTick.load();
    return start ? GetTickCount64() - start : 0;
}

DWORD GetLocalIdleMs() {
    LASTINPUTINFO lii = { sizeof(lii), 0 };
    if (!GetLastInputInfo(&lii))
        return 0;
    return GetTickCount() - lii.dwTime;  // unsigned wrap-safe
}

// FormatClock ("1:23:45" / "23:45", the overlay's compact second-granular
// form) and FormatCoarse ("1h 23m", the tooltip's coarse form) are shared
// contracts now — defined once above the branch namespaces.

// ── Stuck-session state ───────────────────────────────────────────────────
//
// Written by the watchdog thread (see "Stuck-session watchdog" below), read
// by every presentation surface.
std::atomic<bool> g_sessionHung { false };   // past the threshold, alert up
std::atomic<int>  g_hungSeconds { 0 };       // consecutive hung polls

// Overlay status row text; empty when the feature is off (the row stays
// blank, same convention as the hostname row). A hang overrides the clock:
// the overlay lives on the helper thread, so it keeps painting while the
// frame thread is stuck.
void FormatOverlayStatus(wchar_t* out, size_t cch) {
    out[0] = L'\0';
    if (g_stuckDetection && g_sessionHung.load()) {
        swprintf_s(out, cch, L"⚠ not responding (%d s)", g_hungSeconds.load());
        return;
    }
    if (!g_showSessionInfo || !g_sessionStartTick.load())
        return;
    wchar_t dur[32], idle[32];
    FormatClock(GetSessionDurationMs(), dur, ARRAYSIZE(dur));
    FormatClock(GetLocalIdleMs(), idle, ARRAYSIZE(idle));
    swprintf_s(out, cch, L"%s · idle %s", dur, idle);
}

// ── Connection quality state ──────────────────────────────────────────────
//
// Written by the RDP control event sink (see "RDP control event sink" below)
// on the control's STA thread whenever mstscax fires
// IMsTscAxEvents::OnNetworkStatusChanged; read by the frame thread (status
// icon/tooltip) and the helper thread (overlay). Quality 0 = nothing
// reported yet. Per Microsoft's documentation the level is 1–4, higher is
// better: 1 = <512 KBps, 2 = 512–1,999, 3 = 2,000–9,999, 4 = ≥10,000 KBps.
// The indicator never invents a value: until a real event arrives it shows
// neutral and the tooltip says so (DECISIONS.md D-16).
std::atomic<int>       g_netQuality   { 0 };
std::atomic<long>      g_netBandwidth { 0 };
std::atomic<long>      g_netRtt       { 0 };
std::atomic<ULONGLONG> g_netLastTick  { 0 };
std::atomic<bool>      g_sinkAdvised  { false };  // a sink is live on the control

// ── Connection-quality diagnostics (D-29) ─────────────────────────────────
// Counters for the creation paths the mod watches for the RDP control, plus
// a one-line summary logged at fixed moments (frame creation, ~10 s into a
// session) so a live test yields evidence even if nothing in the advise path
// ever fires.
std::atomic<unsigned> g_coCreateCalls          { 0 };  // CoCreateInstance hook entries
std::atomic<unsigned> g_dllGetClassObjectCalls { 0 };  // mstscax!DllGetClassObject hook entries
std::atomic<unsigned> g_factoryCreateCalls     { 0 };  // mstscax factory CreateInstance hook entries
std::atomic<bool>     g_rdpEventsLateDiagLogged { false };

void LogRdpEventsDiag(PCWSTR when) {
    Wh_Log(L"RdpEvents: [diag @ %s] CoCreateInstance hook calls=%u, "
           L"mstscax!DllGetClassObject calls=%u, mstscax factory CreateInstance "
           L"calls=%u, sink advised=%d, mstscax.dll loaded=%d, quality=%d",
           when, g_coCreateCalls.load(), g_dllGetClassObjectCalls.load(),
           g_factoryCreateCalls.load(), (int)g_sinkAdvised.load(),
           (int)(GetModuleHandleW(L"mstscax.dll") != nullptr),
           g_netQuality.load());
}

// QualityLabel is a shared contract now — defined once above the branch
// namespaces.

// The rich status tooltip that used to hang off the thumbnail toolbar's
// status icon (session duration, this PC's idle time, connection quality with
// bandwidth and round-trip time, and the not-responding warning) is not built
// here any more. It lives on the explorer.exe branch now, as
// FormatEmbeddedStatusTooltip, rebuilt from the very same fields this branch
// already publishes in the status snapshot (D-33). It could not simply be
// moved: this function read mstsc-side globals that do not exist in the other
// process.

// Wakes this branch's one live presentation surface after a status change
// from any thread: the helper thread repaints the floating overlay. The
// taskbar-embedded panel is not poked — it is in another process and picks
// the change up from the 1 s status snapshot on its own poll. (Before v0.9.0
// this also posted to the frame thread to re-sync the thumbnail toolbar's
// status icon and tooltip; that surface is gone — D-33.)
void NotifyStatusChanged() {
    DWORD helperThreadId = g_helperThreadId.load();
    if (helperThreadId)
        PostThreadMessageW(helperThreadId, WM_REPAINT_BTN, 0, 0);
}

// ── Fullscreen / windowed toggle ──────────────────────────────────────────
//
// Mechanism (DECISIONS.md D-14): this mod drives NO fullscreen-transition
// logic of its own. It synthesizes, with SendInput, the exact key chord
// Remote Desktop documents for this — Ctrl+Alt+Break — after making sure
// the RDP frame is the foreground window, so the chord lands on mstsc's own
// already-working handler. Nothing in the code this mod already hooks
// exposes the transition; the control's documented IMsRdpClient::FullScreen
// property was identified as a candidate direct path but deliberately not
// used (see D-14). Safety rule: if the foreground cannot be acquired, the
// chord is NOT sent — Ctrl+Break delivered to some other window (a console,
// say) would be a real interrupt, and a silent no-op is the better failure.

// Fullscreen detection by window geometry/style, not by any mstsc internal:
// the fullscreen frame drops its caption and covers its monitor exactly
// (or, spanned, more than one monitor — which the style test still catches).
bool IsRdpFrameFullscreen(HWND hFrame) {
    if (!hFrame || !IsWindow(hFrame))
        return false;
    LONG style = GetWindowLongW(hFrame, GWL_STYLE);
    if ((style & WS_CAPTION) != WS_CAPTION)
        return true;
    if (IsIconic(hFrame))
        return false;  // geometry is meaningless while minimized
    RECT rc = {};
    GetWindowRect(hFrame, &rc);
    HMONITOR hMon = MonitorFromWindow(hFrame, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    return hMon && GetMonitorInfoW(hMon, &mi) && EqualRect(&rc, &mi.rcMonitor);
}

bool IsFrameForeground(HWND hFrame) {
    HWND fg = GetForegroundWindow();
    return fg && (fg == hFrame || GetAncestor(fg, GA_ROOT) == hFrame);
}

// The toggle is driven from the taskbar-embedded panel, i.e. from a click in
// explorer.exe, so this process may not currently hold foreground rights.
// (The same was true of the taskbar thumbnail toolbar this code was first
// written for.) Plain SetForegroundWindow first; if
// that is refused, briefly join the foreground thread's input queue and ask
// again — the long-established way a process takes the foreground right
// after the user actually clicked something of its own.
//
// For the taskbar-embedded panel's toggle, the explorer.exe branch now grants
// this process the foreground right (AllowSetForegroundWindow) immediately
// before sending the command (D-27), so the plain SetForegroundWindow below
// is expected to succeed on that path; the AttachThreadInput fallback stays
// as the safety net for every other caller.
bool BringFrameToForeground(HWND hFrame) {
    if (IsFrameForeground(hFrame))
        return true;

    // Snapshot the foreground state BEFORE the call, and capture the call's
    // own BOOL — the two facts needed to tell the documented success paths
    // apart (D-31). GetForegroundWindow inside an RDP session's own desktop
    // has been observed returning a flat null even where a normal desktop
    // would report a window; IsFrameForeground (which reads it) is therefore
    // not a trustworthy success oracle on this path, so we also lean on the
    // BOOL SetForegroundWindow returns.
    HWND fgBefore = GetForegroundWindow();
    BOOL sfw1     = SetForegroundWindow(hFrame);
    Wh_Log(L"BringFrameToForeground: frame=%p foreground-before=%p SetForegroundWindow=%d",
        hFrame, fgBefore, (int)sfw1);

    if (IsFrameForeground(hFrame)) {
        Wh_Log(L"BringFrameToForeground: frame is foreground after first call");
        return true;
    }

    // No-current-foreground-window direct path. Per Microsoft's documented
    // SetForegroundWindow criteria, "there is no foreground window, and the
    // foreground process is not being debugged" is itself a condition under
    // which the call succeeds unconditionally — no foreground rights needed.
    // The AttachThreadInput fallback below assumes there IS another
    // foreground thread to attach to; when there is none it has nothing to
    // do, so this case must be handled on its own. Trust the BOOL here rather
    // than IsFrameForeground, whose GetForegroundWindow read is the very thing
    // that is unreliable in this environment. A FALSE return falls through.
    if (!fgBefore) {
        Wh_Log(L"BringFrameToForeground: no current foreground window — "
               L"direct no-foreground path, SetForegroundWindow=%d (%s)",
               (int)sfw1, sfw1 ? L"trusting success" : L"refused, falling through");
        if (sfw1)
            return true;
    }

    HWND  fg       = GetForegroundWindow();
    DWORD fgThread = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
    DWORD me       = GetCurrentThreadId();
    if (fgThread && fgThread != me) {
        BOOL att  = AttachThreadInput(me, fgThread, TRUE);
        BOOL sfw2 = FALSE;
        if (att) {
            sfw2 = SetForegroundWindow(hFrame);
            BringWindowToTop(hFrame);
            AttachThreadInput(me, fgThread, FALSE);
        }
        Wh_Log(L"BringFrameToForeground: attach fallback ENTERED — "
               L"fg=%p fgThread=%u AttachThreadInput=%d SetForegroundWindow=%d",
               fg, fgThread, (int)att, (int)sfw2);
    } else {
        Wh_Log(L"BringFrameToForeground: attach fallback SKIPPED "
               L"(fg=%p fgThread=%u me=%u)", fg, fgThread, me);
    }

    bool ok = IsFrameForeground(hFrame);
    Wh_Log(L"BringFrameToForeground: final IsFrameForeground=%d (GetForegroundWindow=%p)",
        (int)ok, GetForegroundWindow());
    return ok;
}

// Ctrl down, Alt down, Break down/up, Alt up, Ctrl up. Break is the
// extended-key form of scan code 0x46 (what the keyboard sends for
// Ctrl+Pause), reported to apps as VK_CANCEL.
void SendCtrlAltBreakChord() {
    INPUT in[6] = {};
    auto key = [&in](int i, WORD vk, bool up, bool extended) {
        in[i].type       = INPUT_KEYBOARD;
        in[i].ki.wVk     = vk;
        in[i].ki.wScan   = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in[i].ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0)
                         | (extended ? KEYEVENTF_EXTENDEDKEY : 0);
    };
    key(0, VK_CONTROL, false, false);
    key(1, VK_MENU,    false, false);
    key(2, VK_CANCEL,  false, true);
    key(3, VK_CANCEL,  true,  true);
    key(4, VK_MENU,    true,  false);
    key(5, VK_CONTROL, true,  false);
    UINT sent = SendInput(6, in, sizeof(INPUT));
    if (sent != 6)
        Wh_Log(L"Fullscreen toggle: SendInput sent %u/6 events, GLE=%d",
            sent, GetLastError());
}

void ToggleFullscreen(PCWSTR source) {
    HWND hFrame = GetRdpFrameForAction();
    if (!hFrame) {
        Wh_Log(L"%s — no RDP frame, ignored", source);
        return;
    }
    if (IsIconic(hFrame))
        (pOrigShowWindow ? pOrigShowWindow : ShowWindow)(hFrame, SW_RESTORE);

    bool wasFullscreen = IsRdpFrameFullscreen(hFrame);
    if (!BringFrameToForeground(hFrame)) {
        Wh_Log(L"%s — foreground not acquired (foreground=%p), "
               L"Ctrl+Alt+Break NOT sent", source, GetForegroundWindow());
        return;
    }
    Wh_Log(L"%s — frame %p is foreground, sending Ctrl+Alt+Break (%s → %s)",
        source, hFrame,
        wasFullscreen ? L"fullscreen" : L"windowed",
        wasFullscreen ? L"windowed"   : L"fullscreen");
    SendCtrlAltBreakChord();
}

// ── Taskbar-embedded widget: status file ─────────────────────────────────
//
// The taskbar-embedded client widget (mod
// rdp-session-toolkit-taskbar-client-embedded, injected into this machine's
// own explorer.exe) cannot read this process's state directly, so this mod
// publishes a small fixed-layout snapshot to a file in its own Windhawk mod
// storage directory about once a second. It is driven by the EXISTING 1 s
// ticks — the overlay's status timer and the watchdog poll — never by a
// timer of its own; the writer throttles so two live ticks still produce one
// write per interval. The widget treats the snapshot as stale once writeTick
// is more than a few seconds old (DECISIONS.md D-22), so a client that exits
// uncleanly can never leave a phantom session on the taskbar. Same
// magic+version+fixed-fields pattern as PersistedButtonPos. Single session
// only: with two mstsc instances the last writer wins, consistent with how
// the rest of this toolkit treats that case.
//
// Write discipline: OPEN_ALWAYS and one WriteFile of the whole fixed-size
// record at offset 0 — never CREATE_ALWAYS — so a reader can never observe a
// truncated file; the record is either the previous snapshot or this one.

// The status record (LocalWidgetStatus), its magic/version/filename, and
// GetLocalWidgetStatusFilePath are shared contracts now — defined once above
// the branch namespaces. Only this writer-side throttle is client-specific.
constexpr ULONGLONG LOCAL_WIDGET_STATUS_MIN_INTERVAL_MS = 900;

std::atomic<ULONGLONG> g_localWidgetStatusLastTick { 0 };
std::atomic<bool>      g_localWidgetStatusLogged   { false };  // first-write log

// Snapshot + write. Reads only cached state, atomics, and window data that
// needs no cross-thread messaging (IsIconic, GetWindowLong, GetWindowRect),
// so it is safe from the watchdog thread while the frame thread is hung.
// `force` bypasses the throttle for the one-shot "session gone" record.
void WriteLocalWidgetStatus(bool force = false) {
    ULONGLONG now  = GetTickCount64();
    ULONGLONG last = g_localWidgetStatusLastTick.load();
    if (!force) {
        if (now - last < LOCAL_WIDGET_STATUS_MIN_INTERVAL_MS)
            return;
        // Two 1 s ticks can race here; exactly one of them wins the write.
        if (!g_localWidgetStatusLastTick.compare_exchange_strong(last, now))
            return;
    } else {
        g_localWidgetStatusLastTick.store(now);
    }

    EnterCriticalSection(&g_cs);
    HWND hFrame = g_hRdpFrame;
    wchar_t host[64];
    wcsncpy_s(host, g_hostname, _TRUNCATE);
    LeaveCriticalSection(&g_cs);
    bool active = hFrame && IsWindow(hFrame);

    // D-29 diagnostics: one summary ~10 s into the session, from whichever
    // 1 s tick gets here first — by then the control has long been created.
    if (active && GetSessionDurationMs() >= 10000 &&
        !g_rdpEventsLateDiagLogged.exchange(true))
        LogRdpEventsDiag(L"10 s into session");

    LocalWidgetStatus s = {};
    s.magic             = kLocalWidgetStatusMagic;
    s.version           = kLocalWidgetStatusVersion;
    s.writerPid         = GetCurrentProcessId();
    s.sessionActive     = active ? TRUE : FALSE;
    s.sessionDurationMs = active ? GetSessionDurationMs() : 0;
    s.localIdleMs       = GetLocalIdleMs();
    s.quality           = g_netQuality.load();
    s.bandwidth         = g_netBandwidth.load();
    s.rtt               = g_netRtt.load();
    s.qualityAvailable  = g_sinkAdvised.load() ? TRUE : FALSE;
    s.hung              = (g_stuckDetection && g_sessionHung.load()) ? TRUE : FALSE;
    s.hungSeconds       = g_hungSeconds.load();
    s.iconic            = (active && IsIconic(hFrame)) ? TRUE : FALSE;
    s.fullscreen        = (active && IsRdpFrameFullscreen(hFrame)) ? TRUE : FALSE;
    s.writeTick         = now;
    wcsncpy_s(s.hostname, host, _TRUNCATE);

    wchar_t path[MAX_PATH + 40];
    if (!GetLocalWidgetStatusFilePath(path, ARRAYSIZE(path)))
        return;
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (!g_localWidgetStatusLogged.exchange(true))
            Wh_Log(L"LocalWidget: status file open failed GLE=%d (%s)",
                GetLastError(), path);
        return;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(hFile, &s, sizeof(s), &written, nullptr);
    CloseHandle(hFile);
    if (!g_localWidgetStatusLogged.exchange(true))
        Wh_Log(L"LocalWidget: status snapshots → %s (ok=%d, %u bytes)",
            path, (int)(ok && written == sizeof(s)), written);
}

void DeleteLocalWidgetStatusFile() {
    wchar_t path[MAX_PATH + 40];
    if (GetLocalWidgetStatusFilePath(path, ARRAYSIZE(path)))
        DeleteFileW(path);
}

// ── RDP frame subclass ─────────────────────────────────────────────────────
//
// Since v0.9.0 the frame subclass exists for exactly two jobs, both of which
// must run on this window's own thread:
//   * WM_DESTROY — launch a parked reconnect plan, release the RDP control's
//     event sink, restore the original wndproc, and publish one final
//     "session gone" snapshot for the taskbar-embedded widget.
//   * g_msgSinkTeardown — release the event sink at mod unload. The connection
//     point is an STA object advised on mstsc's UI thread — the same thread
//     that owns this frame — so Unadvise must happen here; Wh_ModUninit sends
//     this message synchronously to reach it.
// The taskbar thumbnail toolbar this subclass was originally built for is
// gone (DECISIONS.md D-33), and its ITaskbarList3 state, its refresh/status
// messages, and its WM_SIZE re-sync went with it. The sink teardown
// deliberately did NOT: it used to be invoked alongside TeardownThumbBar on
// both the WM_DESTROY and the mod-unload path, and it still fires on both,
// now under its own message.

// Defined in "RDP control event sink" below; the frame subclass is where its
// home-thread teardown gets invoked.
void UnadviseRdpEvents(PCWSTR reason);

LRESULT CALLBACK FrameSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc;
    EnterCriticalSection(&g_cs);
    origProc = g_origFrameWndProc;
    LeaveCriticalSection(&g_cs);

    if (g_msgSinkTeardown && msg == g_msgSinkTeardown) {
        // Sent synchronously by Wh_ModUninit so the sink's Unadvise/Release
        // runs here, on the connection point's home thread.
        UnadviseRdpEvents(L"mod unload");
        return 0;
    } else if (msg == WM_DESTROY) {
        // First thing: if a reconnect is parked, this is the moment — the
        // session window is going away for real and the process is still
        // fully alive.
        LaunchPendingReconnect();
        UnadviseRdpEvents(L"frame WM_DESTROY");
        if (origProc)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(origProc));
        EnterCriticalSection(&g_cs);
        if (g_hRdpFrame == hwnd)
            g_hRdpFrame = nullptr;
        g_origFrameWndProc = nullptr;
        LeaveCriticalSection(&g_cs);
        // Tell the taskbar-embedded widget the session is gone right now,
        // rather than leaving it to the staleness timeout.
        WriteLocalWidgetStatus(/*force=*/true);
    }

    return origProc
        ? CallWindowProcW(origProc, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── BBar subclass — cleanup only ─────────────────────────────────────────

LRESULT CALLBACK BBarSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC origProc;
    EnterCriticalSection(&g_cs);
    origProc = g_origBBarWndProc;
    LeaveCriticalSection(&g_cs);

    if (msg == WM_DESTROY) {
        if (origProc)
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(origProc));
        EnterCriticalSection(&g_cs);
        g_hBBar           = nullptr;
        // The frame outlives the bar. Clear the frame ref here only when the
        // frame subclass isn't independently tracking its lifetime (it clears
        // g_hRdpFrame itself on the frame's own WM_DESTROY).
        if (!g_origFrameWndProc)
            g_hRdpFrame = nullptr;
        g_origBBarWndProc = nullptr;
        bool showOverlay = g_showOverlay;
        LeaveCriticalSection(&g_cs);
        g_hLastMonitor.store(nullptr);
        DWORD helperThreadId = g_helperThreadId.load();
        if (showOverlay && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_HIDE_BTN, 0, 0);
    }

    return origProc
        ? CallWindowProcW(origProc, hwnd, msg, wParam, lParam)
        : DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ── Disconnect button window ──────────────────────────────────────────────

HWND g_hBtn = nullptr;

// Click/drag disambiguation state. Only ever touched on the helper thread
// (the thread that owns g_hBtn and runs its message loop), so — like g_hBtn
// itself — these need no synchronization.
bool  g_btnPotentialDrag = false;
bool  g_btnDragging      = false;
POINT g_btnDragStart     = {};  // screen coords of the WM_LBUTTONDOWN
POINT g_btnWindowStart   = {};  // window top-left (screen coords) at grab time

// Last minimize / fullscreen state observed by the helper thread (paint or
// poll timer), so the poll only triggers a repaint on an actual change.
// Helper-thread-only, like the drag state above.
bool  g_lastIconic       = false;
bool  g_lastFullscreen   = false;

// Last overlay status text the helper thread acted on, so the 1 s status
// timer only repaints on a real change. Helper-thread-only.
wchar_t g_lastOverlayStatus[64] = {};

LRESULT CALLBACK BtnWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            dpiX = 96; dpiY = 96;
        }

        auto ScaleX = [dpiX](int v) { return MulDiv(v, dpiX, 96); };
        auto ScaleY = [dpiY](int v) { return MulDiv(v, dpiY, 96); };

        // Background
        HBRUSH hbrBg = CreateSolidBrush(RGB(24, 24, 24));
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        // Blue accent / border
        HBRUSH hbrAccent = CreateSolidBrush(RGB(0, 120, 212));
        RECT accent = { rc.left, rc.top, rc.right, rc.top + ScaleY(3) };
        FillRect(hdc, &accent, hbrAccent);
        if (g_showBorder) {
            RECT left   = { rc.left,           rc.top, rc.left  + ScaleX(2), rc.bottom };
            RECT right  = { rc.right - ScaleX(2), rc.top, rc.right,          rc.bottom };
            RECT bottom = { rc.left, rc.bottom - ScaleY(2), rc.right,        rc.bottom };
            FillRect(hdc, &left,   hbrAccent);
            FillRect(hdc, &right,  hbrAccent);
            FillRect(hdc, &bottom, hbrAccent);
        }
        DeleteObject(hbrAccent);

        SetBkMode(hdc, TRANSPARENT);

        // Row separators — hairlines marking the action rows (the two
        // display rows on top, hostname and status, share one group)
        int ySepHost   = rc.top + ScaleY(ZONE_STATUS_TOP);
        int ySepTop    = rc.top + ScaleY(ZONE_MINRESTORE_TOP);
        int ySepBottom = rc.top + ScaleY(ZONE_DISCONNECT_TOP);
        int xMid       = (rc.left + rc.right) / 2;
        HBRUSH hbrSep = CreateSolidBrush(RGB(70, 70, 70));
        RECT sepTop    = { rc.left + ScaleX(6), ySepTop,    rc.right - ScaleX(6), ySepTop + 1 };
        RECT sepBottom = { rc.left + ScaleX(6), ySepBottom, rc.right - ScaleX(6), ySepBottom + 1 };
        RECT sepMid    = { xMid, ySepTop + ScaleY(2), xMid + 1, ySepBottom - ScaleY(2) };
        FillRect(hdc, &sepTop,    hbrSep);
        FillRect(hdc, &sepBottom, hbrSep);
        FillRect(hdc, &sepMid,    hbrSep);
        DeleteObject(hbrSep);

        auto DrawLabel = [&](PCWSTR text, int height, int weight, COLORREF color,
                             const RECT& r, UINT format) {
            SetTextColor(hdc, color);
            HFONT hFont = CreateFontW(
                ScaleY(height), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            RECT rDraw = r;
            DrawTextW(hdc, text, -1, &rDraw, format);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
        };

        // Minimize/Restore zone state — queried live; nothing tracks external
        // transitions, so every paint samples fresh (see IsRdpFrameIconic)
        bool iconic = IsRdpFrameIconic();
        g_lastIconic = iconic;
        constexpr COLORREF kZoneEnabled  = RGB(200, 200, 200);
        constexpr COLORREF kZoneDisabled = RGB(85, 85, 85);

        // Hostname — top row, display only, not clickable. Row still takes
        // up its space and is left blank when showHostname is off.
        wchar_t hostname[256];
        EnterCriticalSection(&g_cs);
        wcsncpy_s(hostname, g_hostname, _TRUNCATE);
        LeaveCriticalSection(&g_cs);

        if (g_showHostname && hostname[0]) {
            RECT rHost = { rc.left + ScaleX(4), rc.top + ScaleY(3),
                           rc.right - ScaleX(4), ySepHost };
            DrawLabel(hostname, 11, FW_NORMAL, RGB(140, 140, 140), rHost,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        // Session clock / local idle — second row, display only, not
        // clickable. Blank when showSessionInfo is off; red while the
        // watchdog reports the session as not responding.
        wchar_t status[64];
        FormatOverlayStatus(status, ARRAYSIZE(status));
        wcscpy_s(g_lastOverlayStatus, status);
        if (status[0]) {
            bool hung = g_stuckDetection && g_sessionHung.load();
            RECT rStat = { rc.left + ScaleX(4), ySepHost,
                           rc.right - ScaleX(4), ySepTop };
            DrawLabel(status, 9, hung ? FW_BOLD : FW_NORMAL,
                hung ? RGB(255, 110, 110) : RGB(150, 150, 150), rStat,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }

        // Minimize — middle row, left half, dimmed while already minimized
        RECT rMin = { rc.left, ySepTop + 1, xMid, ySepBottom };
        DrawLabel(L"–  Minimize", 10, FW_NORMAL,
            iconic ? kZoneDisabled : kZoneEnabled, rMin,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Restore — middle row, right half, dimmed until minimized
        RECT rRest = { xMid, ySepTop + 1, rc.right, ySepBottom };
        DrawLabel(L"□  Restore", 10, FW_NORMAL,
            iconic ? kZoneEnabled : kZoneDisabled, rRest,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        // Disconnect — bottom row
        bool hotkeyConflict = g_enableHotkey && !g_hotkeyRegistered;
        PCWSTR disconnectLabel = hotkeyConflict ? L"✕  Hotkey Failed" : L"✕  Disconnect";
        COLORREF discColor  = hotkeyConflict ? RGB(255, 100, 100) : RGB(235, 235, 235);
        int      discWeight = hotkeyConflict ? FW_BOLD : FW_NORMAL;

        RECT rDisc = { rc.left, ySepBottom + 1, rc.right, rc.bottom - ScaleY(2) };
        DrawLabel(disconnectLabel, 14, discWeight, discColor, rDisc,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_btnPotentialDrag = true;
        g_btnDragging = false;

        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &pt);
        g_btnDragStart = pt;

        RECT rc;
        GetWindowRect(hwnd, &rc);
        g_btnWindowStart = { rc.left, rc.top };
        return 0;
    }

    case WM_LBUTTONUP: {
        bool wasDragging  = g_btnDragging;
        bool wasPotential = g_btnPotentialDrag;
        ReleaseCapture(); // synchronously sends WM_CAPTURECHANGED, which
                           // finalizes the drag and resets both flags
        if (!wasDragging && wasPotential) {
            HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            UINT dpiX = 96, dpiY = 96;
            if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
                dpiY = 96;
            int clickY = GET_Y_LPARAM(lParam);
            int yMinRestoreTop = MulDiv(ZONE_MINRESTORE_TOP, dpiY, 96);
            int yDisconnectTop = MulDiv(ZONE_DISCONNECT_TOP, dpiY, 96);

            if (clickY < yMinRestoreTop) {
                // Hostname and status rows — display only, not clickable
            } else if (clickY < yDisconnectTop) {
                // Minimize/Restore row — horizontal half selects the action
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);
                int xMid = (rcClient.left + rcClient.right) / 2;
                int clickX = GET_X_LPARAM(lParam);

                if (clickX < xMid) {
                    // Minimize — left half. State checked live at click
                    // time; a click while already minimized is a deliberate
                    // no-op (the zone is drawn dimmed in that state)
                    HWND hFrame = GetRdpFrameForAction();
                    if (hFrame && !IsIconic(hFrame)) {
                        Wh_Log(L"Minimize zone clicked — SW_MINIMIZE → %p", hFrame);
                        pOrigShowWindow(hFrame, SW_MINIMIZE);
                    } else {
                        Wh_Log(L"Minimize zone clicked while inactive — ignored");
                    }
                } else {
                    // Restore — right half. Same live check, no-op unless
                    // minimized (shared path: RestoreRdpFrame)
                    RestoreRdpFrame(L"Restore zone clicked");
                }
                InvalidateRect(hwnd, nullptr, TRUE);
            } else {
                HWND hRef;
                EnterCriticalSection(&g_cs);
                hRef = g_hBBar ? g_hBBar : hwnd;
                LeaveCriticalSection(&g_cs);
                DisconnectSession(hRef);
            }
        }
        return 0;
    }

    case WM_CAPTURECHANGED: {
        if (g_btnDragging) {
            g_btnDragging = false;
            FinalizeDragPosition(hwnd);
        }
        g_btnPotentialDrag = false;
        return 0;
    }

    case WM_HOTKEY: {
        if (wParam == HOTKEY_ID) {
            HWND hRef;
            EnterCriticalSection(&g_cs);
            hRef = g_hBBar ? g_hBBar : hwnd;
            LeaveCriticalSection(&g_cs);
            DisconnectSession(hRef);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_btnPotentialDrag || g_btnDragging) {
            POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ClientToScreen(hwnd, &pt);
            int totalDx = pt.x - g_btnDragStart.x;
            int totalDy = pt.y - g_btnDragStart.y;

            if (!g_btnDragging) {
                int absDx = totalDx < 0 ? -totalDx : totalDx;
                int absDy = totalDy < 0 ? -totalDy : totalDy;
                if (absDx > GetSystemMetrics(SM_CXDRAG) ||
                    absDy > GetSystemMetrics(SM_CYDRAG)) {
                    g_btnDragging = true;
                    g_btnPotentialDrag = false;
                }
            }

            if (g_btnDragging) {
                RECT rc;
                GetWindowRect(hwnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;

                // Same monitor reference as FinalizeDragPosition (the RDP
                // frame's monitor via GetRdpMonitor(), not the cursor's
                // current monitor) so the live clamp and the end-of-drag
                // clamp never disagree and cause a snap-back.
                HMONITOR hMon = GetRdpMonitor();
                MONITORINFO mi = { sizeof(mi) };
                RECT mon = (hMon && GetMonitorInfoW(hMon, &mi))
                    ? mi.rcMonitor
                    : RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

                POINT clamped = ClampToMonitorRect(
                    g_btnWindowStart.x + totalDx, g_btnWindowStart.y + totalDy,
                    w, h, mon);

                pOrigSetWindowPos(hwnd, nullptr, clamped.x, clamped.y,
                    0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }

        if (g_fadeWhenIdle) {
            KillTimer(hwnd, FADE_TIMER_ID);
            SetLayeredWindowAttributes(hwnd, 0, ALPHA_FULL, LWA_ALPHA);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        break;

    case WM_MOUSELEAVE:
        if (g_fadeWhenIdle)
            SetTimer(hwnd, FADE_TIMER_ID, FADE_DELAY_MS, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == FADE_TIMER_ID) {
            KillTimer(hwnd, FADE_TIMER_ID);
            SetLayeredWindowAttributes(hwnd, 0, ALPHA_FADED, LWA_ALPHA);
        } else if (wParam == ICONIC_TIMER_ID) {
            // Background correctness check: nothing notifies this window when
            // the session is minimized/restored externally (e.g. from the
            // taskbar, in explorer.exe), so poll at low frequency and repaint
            // only on an actual change
            bool iconic     = IsRdpFrameIconic();
            bool fullscreen = IsRdpFrameFullscreen(GetRdpFrameForAction());
            if (iconic != g_lastIconic || fullscreen != g_lastFullscreen) {
                g_lastIconic     = iconic;
                g_lastFullscreen = fullscreen;
                InvalidateRect(hwnd, nullptr, TRUE);
                // Before v0.9.0 this also nudged the taskbar thumb bar on the
                // frame's thread. The taskbar-embedded panel needs no nudge:
                // WriteLocalWidgetStatus (below, on the 1 s status tick and
                // the watchdog poll) already samples iconic/fullscreen fresh,
                // and the panel polls the snapshot once a second.
            }
        } else if (wParam == STATUS_TIMER_ID) {
            // Session clock / idle display: repaint the overlay row only when
            // its second-granular text actually changed. The same tick also
            // publishes the taskbar-embedded widget's status snapshot
            // (throttled inside; the watchdog's 1 s poll shares the writer),
            // which is what carries the duration / idle / quality / hung
            // detail to the panel's own status tooltip.
            WriteLocalWidgetStatus();
            wchar_t overlay[64];
            FormatOverlayStatus(overlay, ARRAYSIZE(overlay));
            if (wcscmp(overlay, g_lastOverlayStatus) != 0) {
                wcscpy_s(g_lastOverlayStatus, overlay);
                InvalidateRect(hwnd, nullptr, TRUE);
            }
        }
        return 0;

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, g_btnDragging ? IDC_SIZEALL : IDC_HAND));
        return TRUE;

    case WM_DISPLAYCHANGE: {
        g_hLastMonitor.store(nullptr);
        DWORD helperThreadId = g_helperThreadId.load();
        if (helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, FADE_TIMER_ID);
        KillTimer(hwnd, ICONIC_TIMER_ID);
        KillTimer(hwnd, STATUS_TIMER_ID);
        if (g_enableHotkey)
            UnregisterHotKey(hwnd, HOTKEY_ID);
        g_hBtn = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CreateOrRepositionButton() {
    HMONITOR hMon = GetRdpMonitor();
    MONITORINFO mi = { sizeof(mi) };
    RECT mon = (hMon && GetMonitorInfoW(hMon, &mi)) ? mi.rcMonitor : 
               RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };

    UINT dpiX = 96, dpiY = 96;
    if (hMon && FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96; dpiY = 96;
    }

    int scaledW = MulDiv(BTN_W, dpiX, 96);
    int scaledH = MulDiv(BTN_H, dpiY, 96);

    // A dragged position (corner + two-axis offset) overrides the
    // settings-derived default when one has been persisted. The default
    // path (dx=0, dy=g_buttonOffset) reproduces the original flush-edge,
    // vertical-offset-only placement exactly.
    bool hasDragPos;
    bool onRight, atBottom;
    int  offsetDx, offsetDy;
    EnterCriticalSection(&g_cs);
    hasDragPos = g_hasDragPos;
    onRight    = g_dragOnRight;
    atBottom   = g_dragAtBottom;
    offsetDx   = g_dragDx;
    offsetDy   = g_dragDy;
    LeaveCriticalSection(&g_cs);
    if (!hasDragPos) {
        onRight  = g_buttonOnRight;
        atBottom = g_buttonAtBottom;
        offsetDx = 0;
        offsetDy = g_buttonOffset;
    }

    int scaledDx = MulDiv(offsetDx, dpiX, 96);
    int scaledDy = MulDiv(offsetDy, dpiY, 96);

    int btnX = onRight  ? (mon.right  - scaledW - scaledDx) : (mon.left + scaledDx);
    int btnY = atBottom ? (mon.bottom - scaledH - scaledDy) : (mon.top  + scaledDy);

    Wh_Log(L"Button: x=%d y=%d w=%d h=%d (monitor %d,%d-%d,%d)",
        btnX, btnY, scaledW, scaledH,
        mon.left, mon.top, mon.right, mon.bottom);

    if (g_hBtn && IsWindow(g_hBtn)) {
        pOrigSetWindowPos(g_hBtn, HWND_TOPMOST,
            btnX, btnY, scaledW, scaledH,
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        
        HRGN hRgn = CreateRoundRectRgn(0, 0, scaledW + 1, scaledH + 1, MulDiv(8, dpiX, 96), MulDiv(8, dpiY, 96));
        SetWindowRgn(g_hBtn, hRgn, FALSE);
        UpdateHostname();
        InvalidateRect(g_hBtn, nullptr, TRUE);
        return;
    }

    g_hBtn = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        BTN_CLASS, L"",
        WS_POPUP,
        btnX, btnY, scaledW, scaledH,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);

    if (!g_hBtn) {
        Wh_Log(L"Button CreateWindowExW FAILED GLE=%d", GetLastError());
        return;
    }

    // Rounded corners
    HRGN hRgn = CreateRoundRectRgn(0, 0, scaledW + 1, scaledH + 1, MulDiv(8, dpiX, 96), MulDiv(8, dpiY, 96));
    SetWindowRgn(g_hBtn, hRgn, FALSE);

    // Start faded if idle-fade is on, otherwise full opacity
    SetLayeredWindowAttributes(g_hBtn, 0,
        g_fadeWhenIdle ? ALPHA_FADED : ALPHA_FULL, LWA_ALPHA);

    // Low-frequency poll so the minimize/restore zones track external
    // minimize/restore transitions this process is never notified about
    SetTimer(g_hBtn, ICONIC_TIMER_ID, ICONIC_POLL_MS, nullptr);
    // 1 s status tick for the session clock row and the taskbar-embedded
    // panel's status snapshot; the repaint only happens on a real change.
    SetTimer(g_hBtn, STATUS_TIMER_ID, STATUS_POLL_MS, nullptr);

    pOrigShowWindow(g_hBtn, SW_SHOWNOACTIVATE);
    pOrigSetWindowPos(g_hBtn, HWND_TOPMOST,
        btnX, btnY, scaledW, scaledH,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if (g_enableHotkey) {
        if (RegisterHotKey(g_hBtn, HOTKEY_ID, g_hotkeyMod, g_hotkeyVk)) {
            Wh_Log(L"Hotkey registered mod=0x%x vk=0x%x", g_hotkeyMod, g_hotkeyVk);
            g_hotkeyRegistered = true;
        } else {
            Wh_Log(L"Hotkey registration FAILED GLE=%d", GetLastError());
            g_hotkeyRegistered = false;
        }
    }

    UpdateHostname();
    InvalidateRect(g_hBtn, nullptr, TRUE);

    Wh_Log(L"Button created HWND=%p at (%d,%d)", g_hBtn, btnX, btnY);
}

// ── Helper thread ─────────────────────────────────────────────────────────

DWORD WINAPI HelperThread(LPVOID) {
    // Force this thread's message queue into existence, then signal
    // readiness immediately — before RegisterClassExW, so the signal fires
    // no matter what happens after. StopHelperThread waits on this before
    // posting WM_QUIT, so the quit message can never be sent before there is
    // a queue able to receive it (e.g. showOverlay toggled on then off again
    // within the same settings-changed pass, before this thread has had a
    // chance to run at all).
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hHelperThreadReady)
        SetEvent(g_hHelperThreadReady);

    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = BtnWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = BTN_CLASS;
    wc.hCursor       = LoadCursorW(nullptr, IDC_HAND);
    RegisterClassExW(&wc);

    EnterCriticalSection(&g_cs);
    bool bbarReady = (g_hBBar != nullptr);
    LeaveCriticalSection(&g_cs);
    if (bbarReady)
        CreateOrRepositionButton();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_CREATE_BTN) {
            CreateOrRepositionButton();
        } else if (msg.message == WM_HIDE_BTN) {
            if (g_hBtn && IsWindow(g_hBtn))
                pOrigShowWindow(g_hBtn, SW_HIDE);
        } else if (msg.message == WM_REPAINT_BTN) {
            if (g_hBtn && IsWindow(g_hBtn))
                InvalidateRect(g_hBtn, nullptr, TRUE);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_hBtn && IsWindow(g_hBtn))
        DestroyWindow(g_hBtn);
    UnregisterClassW(BTN_CLASS, GetModuleHandleW(nullptr));
    return 0;
}

void StartHelperThread() {
    if (g_hHelperThread) return;
    if (!g_hHelperThreadReady)
        g_hHelperThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hHelperThreadReady);
    DWORD threadId = 0;
    g_hHelperThread = CreateThread(
        nullptr, 0, HelperThread, nullptr, 0, &threadId);
    g_helperThreadId.store(threadId);
}

void StopHelperThread() {
    DWORD threadId = g_helperThreadId.load();
    if (threadId) {
        // Wait for HelperThread to signal that its message queue exists
        // before posting WM_QUIT. Without this, calling StopHelperThread
        // immediately after StartHelperThread (showOverlay toggled on then
        // off again within the same settings-changed pass) can race ahead of
        // the thread creating its queue: PostThreadMessageW then fails with
        // ERROR_INVALID_THREAD_ID, silently, and the thread is abandoned
        // running inside a DLL that may be unloading. The only work between
        // thread start and the signal is a Peek call and a class
        // registration — both fast, local, and non-blocking — so this wait
        // resolves in practice almost immediately.
        if (g_hHelperThreadReady)
            WaitForSingleObject(g_hHelperThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hHelperThread) {
        WaitForSingleObject(g_hHelperThread, 3000);
        CloseHandle(g_hHelperThread);
        g_hHelperThread = nullptr;
        g_helperThreadId.store(0);
    }
}

// ── Stuck-session watchdog ────────────────────────────────────────────────
//
// Polls IsHungAppWindow on the RDP frame once a second from its own thread,
// so it keeps working precisely when the frame thread does not. After the
// configured number of consecutive hung seconds it shows a small topmost
// alert at the top-center of the RDP monitor offering Force reconnect; it
// NEVER reconnects on its own (DECISIONS.md D-18). The alert is its own
// topmost window on purpose, and always was: a surface whose clicks are
// delivered to the frame's (hung) thread could not be used at the one moment
// it matters. That reasoning originally set it apart from the taskbar
// thumbnail toolbar (whose THBN_CLICKED went to that very thread); it applies
// just as directly to the taskbar-embedded panel, whose clicks reach mstsc as
// a WM_COPYDATA the hung thread would likewise never pump. The hung state
// still reaches the panel, because the snapshot writer below runs on this
// watchdog's own thread.
//
// Force reconnect: build the plan → park it → post WM_CLOSE → wait a bounded
// grace period for a clean close (then WM_DESTROY relaunches, as for the
// Reconnect button) → if the frame is still there, launch the replacement
// here and TerminateProcess this client. A genuinely hung process may never
// run the clean WM_CLOSE path, which is why the hard stop exists; the plan
// is captured before anything is closed, and both launch paths funnel into
// LaunchReconnect's once-per-process guard so they can never double-launch.

constexpr auto  ALERT_CLASS       = L"WH_RdpstkClientAlert";
constexpr int   ALERT_W           = 320;
constexpr int   ALERT_H           = 64;
constexpr int   ALERT_ROW_SPLIT   = 34;     // logical px; below = action row
constexpr int   WATCHDOG_TIMER_ID = 45;
constexpr UINT  WATCHDOG_POLL_MS  = 1000;
constexpr DWORD FORCE_GRACE_MS    = 3000;

std::atomic<bool> g_forceReconnectInFlight { false };
std::atomic<int>  g_oneShotThreads { 0 };   // drained by Wh_ModUninit

// Watchdog-thread-only, like g_hBtn on the helper thread.
HWND               g_hAlertWnd          = nullptr;
bool               g_alertDismissed     = false;  // for this hang episode
HANDLE             g_hWatchdogThread    = nullptr;
std::atomic<DWORD> g_watchdogThreadId   { 0 };
HANDLE             g_hWatchdogThreadReady = nullptr;

struct ForceReconnectCtx {
    ReconnectPlan plan;
    HWND          hFrame;
};

DWORD WINAPI ForceReconnectThread(LPVOID param) {
    ForceReconnectCtx* ctx = static_cast<ForceReconnectCtx*>(param);

    // A frame that turns out to be responsive enough to close cleanly inside
    // the grace period relaunches through the normal WM_DESTROY path.
    SetPendingReconnect(ctx->plan);
    PostMessageW(ctx->hFrame, WM_CLOSE, 0, 0);
    Wh_Log(L"Force reconnect: WM_CLOSE posted to %p, waiting up to %u ms for a clean close",
        ctx->hFrame, FORCE_GRACE_MS);

    bool closed = false;
    for (DWORD waited = 0; waited < FORCE_GRACE_MS && !closed; waited += 100) {
        Sleep(100);
        closed = !IsWindow(ctx->hFrame);
    }

    if (closed) {
        Wh_Log(L"Force reconnect: frame closed cleanly within grace — "
               L"relaunch handled by the frame's WM_DESTROY");
        delete ctx;
        g_forceReconnectInFlight.store(false);
        g_oneShotThreads.fetch_sub(1);
        return 0;
    }

    Wh_Log(L"Force reconnect: frame still alive after %u ms — launching the "
           L"replacement and terminating this client", FORCE_GRACE_MS);
    ClearPendingReconnect();
    if (!LaunchReconnect(&ctx->plan, L"Force reconnect")) {
        // The operator asked to END the hung client; a replacement that
        // cannot be launched (missing connection file, CreateProcessW
        // failure — both logged above) does not change that half. Said
        // plainly in the log so "reconnect" is never assumed to have happened.
        Wh_Log(L"Force reconnect: replacement NOT launched (see above) — "
               L"terminating the hung client anyway, as clicked; reopen the "
               L"connection manually");
    }
    // Hard stop. Not ExitProcess: that runs DLL detach code on a process
    // whose UI thread may be wedged on the loader lock.
    TerminateProcess(GetCurrentProcess(), 1);
    return 0;  // not reached
}

// The only caller is the alert window's Force reconnect zone — a click.
void ForceReconnectSession(PCWSTR source) {
    // Defense in depth: the alert does not offer the zone while Reconnect is
    // disabled (D-28), but the setting is the authority either way.
    if (!g_enableReconnect) {
        Wh_Log(L"%s — Reconnect is disabled (enableReconnect off), ignored", source);
        return;
    }
    if (g_forceReconnectInFlight.exchange(true)) {
        Wh_Log(L"%s — force reconnect already in flight, ignored", source);
        return;
    }
    HWND hFrame = GetRdpFrameForAction();
    if (!hFrame) {
        Wh_Log(L"%s — no RDP frame, ignored", source);
        g_forceReconnectInFlight.store(false);
        return;
    }
    ForceReconnectCtx* ctx = new ForceReconnectCtx{};
    ctx->hFrame = hFrame;
    if (!BuildReconnectPlan(&ctx->plan, source)) {
        delete ctx;
        g_forceReconnectInFlight.store(false);
        return;
    }
    g_oneShotThreads.fetch_add(1);
    HANDLE h = CreateThread(nullptr, 0, ForceReconnectThread, ctx, 0, nullptr);
    if (!h) {
        Wh_Log(L"%s — CreateThread failed GLE=%d", source, GetLastError());
        g_oneShotThreads.fetch_sub(1);
        delete ctx;
        g_forceReconnectInFlight.store(false);
        return;
    }
    CloseHandle(h);
}

// Top-center of the RDP monitor, DPI-scaled. MonitorFromWindow and
// GetMonitorInfo read window/monitor data without messaging the (possibly
// hung) frame thread.
void ShowAlertWindow(HWND hAlert) {
    HMONITOR hMon = GetRdpMonitor();
    MONITORINFO mi = { sizeof(mi) };
    RECT mon = (hMon && GetMonitorInfoW(hMon, &mi))
        ? mi.rcMonitor
        : RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    UINT dpiX = 96, dpiY = 96;
    if (hMon && FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        dpiX = 96; dpiY = 96;
    }
    int w = MulDiv(ALERT_W, dpiX, 96);
    int h = MulDiv(ALERT_H, dpiY, 96);
    int x = (mon.left + mon.right - w) / 2;
    int y = mon.top + MulDiv(24, dpiY, 96);
    SetWindowPos(hAlert, HWND_TOPMOST, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    HRGN hRgn = CreateRoundRectRgn(0, 0, w + 1, h + 1,
        MulDiv(8, dpiX, 96), MulDiv(8, dpiY, 96));
    SetWindowRgn(hAlert, hRgn, FALSE);
}

void WatchdogPoll(HWND hAlert) {
    // This 1 s poll is the second driver of the taskbar-embedded widget's
    // status snapshot: it runs whenever stuckDetection is on, regardless of
    // showOverlay and of fullscreen (the overlay's status timer needs both),
    // and keeps publishing while the frame thread is hung. The writer itself
    // throttles, so the two ticks never double-write.
    WriteLocalWidgetStatus();

    HWND hFrame = GetRdpFrameForAction();
    bool hung = hFrame && IsHungAppWindow(hFrame);

    if (!hung) {
        if (g_sessionHung.exchange(false)) {
            Wh_Log(L"Watchdog: session responding again (after %d s)",
                g_hungSeconds.load());
            NotifyStatusChanged();
        }
        g_hungSeconds.store(0);
        g_alertDismissed = false;
        if (IsWindowVisible(hAlert))
            ShowWindow(hAlert, SW_HIDE);
        return;
    }

    // Consecutive seconds IsHungAppWindow has said "hung" — which itself
    // only turns true ~5 s after the thread stopped pumping.
    int secs = g_hungSeconds.fetch_add(1) + 1;
    if (secs < g_stuckThresholdSec)
        return;

    if (!g_sessionHung.exchange(true)) {
        Wh_Log(L"Watchdog: session not responding for %d s (threshold %d s) — "
               L"alert shown, waiting for the operator", secs, g_stuckThresholdSec);
        NotifyStatusChanged();
    }
    if (g_alertDismissed)
        return;
    if (!IsWindowVisible(hAlert))
        ShowAlertWindow(hAlert);
    InvalidateRect(hAlert, nullptr, TRUE);  // live seconds counter
}

LRESULT CALLBACK AlertWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
            dpiX = 96; dpiY = 96;
        }
        auto ScaleX = [dpiX](int v) { return MulDiv(v, dpiX, 96); };
        auto ScaleY = [dpiY](int v) { return MulDiv(v, dpiY, 96); };

        HBRUSH hbrBg = CreateSolidBrush(RGB(40, 22, 22));
        FillRect(hdc, &rc, hbrBg);
        DeleteObject(hbrBg);

        HBRUSH hbrAccent = CreateSolidBrush(RGB(210, 60, 60));
        RECT accent = { rc.left, rc.top, rc.right, rc.top + ScaleY(3) };
        RECT left   = { rc.left, rc.top, rc.left + ScaleX(2), rc.bottom };
        RECT right  = { rc.right - ScaleX(2), rc.top, rc.right, rc.bottom };
        RECT bottom = { rc.left, rc.bottom - ScaleY(2), rc.right, rc.bottom };
        FillRect(hdc, &accent, hbrAccent);
        FillRect(hdc, &left,   hbrAccent);
        FillRect(hdc, &right,  hbrAccent);
        FillRect(hdc, &bottom, hbrAccent);
        DeleteObject(hbrAccent);

        SetBkMode(hdc, TRANSPARENT);
        auto DrawLabel = [&](PCWSTR text, int height, int weight, COLORREF color,
                             const RECT& r, UINT format) {
            SetTextColor(hdc, color);
            HFONT hFont = CreateFontW(
                ScaleY(height), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            HFONT hOld = (HFONT)SelectObject(hdc, hFont);
            RECT rDraw = r;
            DrawTextW(hdc, text, -1, &rDraw, format);
            SelectObject(hdc, hOld);
            DeleteObject(hFont);
        };

        int ySplit = rc.top + ScaleY(ALERT_ROW_SPLIT);
        int xMid   = (rc.left + rc.right) / 2;

        wchar_t line[96];
        if (g_forceReconnectInFlight.load())
            wcscpy_s(line, L"Force reconnecting…");
        else
            swprintf_s(line, ARRAYSIZE(line),
                L"Remote Desktop is not responding (%d s)", g_hungSeconds.load());
        RECT rTop = { rc.left + ScaleX(8), rc.top + ScaleY(3), rc.right - ScaleX(8), ySplit };
        DrawLabel(line, 13, FW_SEMIBOLD, RGB(245, 230, 230), rTop,
            DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        // Action row: Force reconnect | Dismiss when Reconnect is enabled;
        // Dismiss alone, full width, when it is not (D-28). The warning
        // itself is shown either way.
        bool offerForce = g_enableReconnect;
        HBRUSH hbrSep = CreateSolidBrush(RGB(110, 60, 60));
        RECT sep    = { rc.left + ScaleX(6), ySplit, rc.right - ScaleX(6), ySplit + 1 };
        FillRect(hdc, &sep, hbrSep);
        if (offerForce) {
            RECT sepMid = { xMid, ySplit + ScaleY(4), xMid + 1, rc.bottom - ScaleY(4) };
            FillRect(hdc, &sepMid, hbrSep);
        }
        DeleteObject(hbrSep);

        if (offerForce) {
            RECT rForce = { rc.left, ySplit + 1, xMid, rc.bottom - ScaleY(2) };
            DrawLabel(L"⟳  Force reconnect", 12, FW_BOLD, RGB(255, 190, 190), rForce,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER);
            RECT rDismiss = { xMid, ySplit + 1, rc.right, rc.bottom - ScaleY(2) };
            DrawLabel(L"Dismiss", 12, FW_NORMAL, RGB(170, 150, 150), rDismiss,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        } else {
            RECT rDismiss = { rc.left, ySplit + 1, rc.right, rc.bottom - ScaleY(2) };
            DrawLabel(L"Dismiss", 12, FW_NORMAL, RGB(170, 150, 150), rDismiss,
                DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONUP: {
        HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        UINT dpiX = 96, dpiY = 96;
        if (FAILED(GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
            dpiY = 96;
        if (GET_Y_LPARAM(lParam) < MulDiv(ALERT_ROW_SPLIT, dpiY, 96))
            return 0;  // message row — not clickable
        RECT rc;
        GetClientRect(hwnd, &rc);
        // The left half is Force reconnect only while that zone is drawn;
        // with Reconnect disabled the whole row is Dismiss (D-28).
        bool forceZone = g_enableReconnect &&
                         GET_X_LPARAM(lParam) < (rc.left + rc.right) / 2;
        if (forceZone) {
            Wh_Log(L"Watchdog: Force reconnect clicked");
            ForceReconnectSession(L"Alert Force reconnect clicked");
            InvalidateRect(hwnd, nullptr, TRUE);
        } else {
            Wh_Log(L"Watchdog: alert dismissed for this hang episode");
            g_alertDismissed = true;
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    }

    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;

    case WM_TIMER:
        if (wParam == WATCHDOG_TIMER_ID)
            WatchdogPoll(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, WATCHDOG_TIMER_ID);
        g_hAlertWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

DWORD WINAPI WatchdogThread(LPVOID) {
    // Same queue-first readiness handshake as the helper and relay threads.
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hWatchdogThreadReady)
        SetEvent(g_hWatchdogThreadReady);

    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = AlertWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = ALERT_CLASS;
    wc.hCursor       = LoadCursorW(nullptr, IDC_HAND);
    if (!RegisterClassExW(&wc)) {
        Wh_Log(L"Watchdog: RegisterClassExW failed GLE=%d", GetLastError());
        return 0;
    }

    // Created hidden; WS_EX_NOACTIVATE so a click never steals focus from
    // whatever the operator is doing. The poll timer lives on it.
    g_hAlertWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        ALERT_CLASS, L"", WS_POPUP, 0, 0, ALERT_W, ALERT_H,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hAlertWnd) {
        Wh_Log(L"Watchdog: CreateWindowExW failed GLE=%d", GetLastError());
        UnregisterClassW(ALERT_CLASS, GetModuleHandleW(nullptr));
        return 0;
    }
    SetTimer(g_hAlertWnd, WATCHDOG_TIMER_ID, WATCHDOG_POLL_MS, nullptr);
    Wh_Log(L"Watchdog: polling IsHungAppWindow every %u ms, threshold %d s",
        WATCHDOG_POLL_MS, g_stuckThresholdSec);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hAlertWnd && IsWindow(g_hAlertWnd))
        DestroyWindow(g_hAlertWnd);
    UnregisterClassW(ALERT_CLASS, GetModuleHandleW(nullptr));
    g_sessionHung.store(false);
    g_hungSeconds.store(0);
    return 0;
}

void StartWatchdogThread() {
    if (g_hWatchdogThread) return;
    if (!g_hWatchdogThreadReady)
        g_hWatchdogThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hWatchdogThreadReady);
    DWORD threadId = 0;
    g_hWatchdogThread = CreateThread(
        nullptr, 0, WatchdogThread, nullptr, 0, &threadId);
    g_watchdogThreadId.store(threadId);
}

void StopWatchdogThread() {
    DWORD threadId = g_watchdogThreadId.load();
    if (threadId) {
        // Same race as StopHelperThread (see its comment).
        if (g_hWatchdogThreadReady)
            WaitForSingleObject(g_hWatchdogThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hWatchdogThread) {
        WaitForSingleObject(g_hWatchdogThread, 3000);
        CloseHandle(g_hWatchdogThread);
        g_hWatchdogThread = nullptr;
        g_watchdogThreadId.store(0);
    }
}

// ── Toolkit relay receiver ────────────────────────────────────────────────
//
// Hidden message-only window that future RDP Session Toolkit components —
// specifically the DVC relay plugin — signal via WM_COPYDATA. Protocol: the
// first byte of the payload is the command; any further bytes are reserved
// for future command-specific arguments and ignored today. New commands only
// add a RelayCommand value and a case in RelayWndProc — the window and class
// setup never changes.

constexpr auto RELAY_CLASS = L"CitadelRdpTaskbarRelay";

enum RelayCommand : BYTE {
    RELAY_CMD_MINIMIZE = 0x01,
    // Future commands (restore, disconnect, status updates) take new values
    // here. 0x00 stays permanently unassigned so an all-zeros payload can
    // never be a valid command.
};

// ── Relay sender validation ───────────────────────────────────────────────
//
// The production DVC relay plugin's COM CLSID (see dvc-plugin/relay;
// DECISIONS.md D-10/D-11). The relay plugin is the ONLY external process
// permitted to drive this receiver. A cross-process sender is authenticated by
// resolving its full image path and comparing it to the EXE registered as this
// CLSID's per-user COM LocalServer32 — i.e. the exact relay binary the user
// installed and mstsc activates. Kept as a literal (not a shared header)
// because the mod and the plugin are separate components with separate build
// systems; DECISIONS.md D-10 is the single source of truth that keeps this
// copy and dvc-plugin/relay/common/RelayIds.h in sync.
constexpr auto RELAY_PLUGIN_CLSID =
    L"{6FC96481-9467-496E-BA33-A202ED052F39}";

// Reads the relay plugin's registered LocalServer32 EXE path from HKCU.
// Returns false (and leaves out[] empty) if the plugin is not registered — in
// which case no external sender can be authenticated and the receiver fails
// closed.
bool GetRelayPluginExePath(wchar_t* out, DWORD cch) {
    if (!out || cch == 0) return false;
    out[0] = L'\0';
    wchar_t key[256];
    wsprintfW(key, L"Software\\Classes\\CLSID\\%s\\LocalServer32",
        RELAY_PLUGIN_CLSID);
    DWORD cb = cch * (DWORD)sizeof(wchar_t);
    LSTATUS r = RegGetValueW(HKEY_CURRENT_USER, key, nullptr,
        RRF_RT_REG_SZ, nullptr, out, &cb);
    return r == ERROR_SUCCESS && out[0] != L'\0';
}

// Resolves a process's full image path via a limited-rights handle.
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

// True iff a WM_COPYDATA sender (identified by senderPid) is allowed to drive
// the relay receiver. Two authorized cases, and only two:
//   1) Our own process — the built-in local self-test (SendRelayTestMinimize)
//      sends from inside this mod and passes a same-process window as wParam,
//      so senderPid == our pid. This keeps the self-test working.
//   2) The registered DVC relay plugin — a separate process whose full image
//      path equals the EXE registered as RELAY_PLUGIN_CLSID's HKCU
//      LocalServer32. That is the exact binary mstsc activates as the relay.
// Everything else (unknown pid, unregistered relay, path mismatch) is rejected.
bool IsAuthorizedRelaySender(DWORD senderPid) {
    if (senderPid == 0)
        return false;                              // sender could not be identified
    if (senderPid == GetCurrentProcessId())
        return true;                               // in-process local self-test

    wchar_t expected[MAX_PATH] = {};
    if (!GetRelayPluginExePath(expected, ARRAYSIZE(expected))) {
        Wh_Log(L"Relay: no registered relay plugin (CLSID LocalServer32 absent) "
            L"— rejecting external sender pid=%u", senderPid);
        return false;                              // fail closed
    }
    wchar_t actual[MAX_PATH] = {};
    if (!GetProcessImagePath(senderPid, actual, ARRAYSIZE(actual))) {
        Wh_Log(L"Relay: could not resolve image path for sender pid=%u — rejected",
            senderPid);
        return false;
    }
    // Win32 paths are case-insensitive; ordinal-ignore-case is the right compare.
    if (CompareStringOrdinal(actual, -1, expected, -1, TRUE) == CSTR_EQUAL)
        return true;

    Wh_Log(L"Relay: sender pid=%u image '%s' != registered relay '%s' — rejected",
        senderPid, actual, expected);
    return false;
}

// Relay-thread-only, like g_hBtn on the helper thread: only the thread that
// owns the window touches it.
HWND               g_hRelayWnd     = nullptr;
HANDLE             g_hRelayThread  = nullptr;
std::atomic<DWORD> g_relayThreadId { 0 };
// Manual-reset, signaled once RelayThread's message queue exists — see the
// StartRelayThread/StopRelayThread comments below.
HANDLE             g_hRelayThreadReady = nullptr;

LRESULT CALLBACK RelayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_COPYDATA: {
        // Sender validation (see DECISIONS.md D-11; closes the former TODO now
        // that the relay plugin exists with a stable identity). wParam is, by
        // the WM_COPYDATA contract, the sender's window handle; we resolve it
        // to a pid and accept only our own process (the local self-test) or the
        // registered DVC relay plugin's exact EXE. Anything else is dropped
        // before the payload is even inspected.
        DWORD senderPid = 0;
        HWND hSender = reinterpret_cast<HWND>(wParam);
        if (hSender)
            GetWindowThreadProcessId(hSender, &senderPid);

        if (!IsAuthorizedRelaySender(senderPid)) {
            Wh_Log(L"Relay: WM_COPYDATA from unauthorized sender pid=%u — ignored",
                senderPid);
            return FALSE;
        }

        const COPYDATASTRUCT* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        if (!cds || cds->cbData < 1 || !cds->lpData) {
            Wh_Log(L"Relay: WM_COPYDATA with no payload from pid=%u — ignored",
                senderPid);
            return FALSE;
        }

        const BYTE cmd = *static_cast<const BYTE*>(cds->lpData);
        Wh_Log(L"Relay: command 0x%02X (%u-byte payload) from pid=%u",
            cmd, cds->cbData, senderPid);

        switch (cmd) {
        case RELAY_CMD_MINIMIZE:
            return MinimizeRdpFrame(L"Relay minimize command") ? TRUE : FALSE;
        default:
            Wh_Log(L"Relay: unknown command 0x%02X — ignored", cmd);
            return FALSE;
        }
    }

    case WM_DESTROY:
        g_hRelayWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

DWORD WINAPI RelayThread(LPVOID) {
    // Force this thread's message queue into existence, then signal
    // readiness immediately — before class/window creation, so the signal
    // fires no matter what happens after, including an early return on
    // RegisterClassExW/CreateWindowExW failure. See the identical pattern
    // and StopRelayThread's comment for why this matters.
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hRelayThreadReady)
        SetEvent(g_hRelayThreadReady);

    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = RelayWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = RELAY_CLASS;
    if (!RegisterClassExW(&wc)) {
        Wh_Log(L"Relay: RegisterClassExW failed GLE=%d", GetLastError());
        return 0;
    }

    g_hRelayWnd = CreateWindowExW(0, RELAY_CLASS, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hRelayWnd) {
        Wh_Log(L"Relay: CreateWindowExW failed GLE=%d", GetLastError());
        UnregisterClassW(RELAY_CLASS, GetModuleHandleW(nullptr));
        return 0;
    }
    Wh_Log(L"Relay: message window ready HWND=%p class=%s",
        g_hRelayWnd, RELAY_CLASS);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hRelayWnd && IsWindow(g_hRelayWnd))
        DestroyWindow(g_hRelayWnd);
    UnregisterClassW(RELAY_CLASS, GetModuleHandleW(nullptr));
    return 0;
}

void StartRelayThread() {
    if (g_hRelayThread) return;
    if (!g_hRelayThreadReady)
        g_hRelayThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hRelayThreadReady);
    DWORD threadId = 0;
    g_hRelayThread = CreateThread(
        nullptr, 0, RelayThread, nullptr, 0, &threadId);
    g_relayThreadId.store(threadId);
}

void StopRelayThread() {
    DWORD threadId = g_relayThreadId.load();
    if (threadId) {
        // Same race as StopHelperThread (see its comment): wait for
        // RelayThread to signal that its message queue exists before
        // posting WM_QUIT, so the quit message can never be sent before the
        // thread is able to receive it.
        if (g_hRelayThreadReady)
            WaitForSingleObject(g_hRelayThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hRelayThread) {
        WaitForSingleObject(g_hRelayThread, 3000);
        CloseHandle(g_hRelayThread);
        g_hRelayThread = nullptr;
        g_relayThreadId.store(0);
    }
}

// Local exercise path for the receiver: finds the relay window the same way an
// external sender would and sends it one minimize command. Driven by the
// debugRelayTestMinimize setting flipping on. It passes the relay window itself
// as wParam — a window owned by THIS process — so the receiver's sender
// validation (IsAuthorizedRelaySender) resolves senderPid to our own pid and
// authorizes it as the in-process self-test (see DECISIONS.md D-11). The
// receiver therefore logs our pid, not 0, for this path.
void SendRelayTestMinimize() {
    HWND hRelay = FindWindowExW(HWND_MESSAGE, nullptr, RELAY_CLASS, nullptr);
    if (!hRelay) {
        Wh_Log(L"Relay test: relay window not found");
        return;
    }
    BYTE cmd = RELAY_CMD_MINIMIZE;
    COPYDATASTRUCT cds = {};
    cds.cbData = sizeof(cmd);
    cds.lpData = &cmd;
    LRESULT handled = SendMessageW(hRelay, WM_COPYDATA,
        reinterpret_cast<WPARAM>(hRelay),
        reinterpret_cast<LPARAM>(&cds));
    Wh_Log(L"Relay test: minimize command sent to %p, handled=%d",
        hRelay, (int)handled);
}

// ── Taskbar-embedded widget: local command receiver ──────────────────────
//
// A second, entirely separate message-only window (class
// CitadelRdpTaskbarLocalWidget) for the taskbar-embedded client widget that
// runs in this machine's own explorer.exe. Deliberately NOT
// CitadelRdpTaskbarRelay and not routed through it (DECISIONS.md D-13): that
// receiver's exact-binary-path validation is the cross-machine channel's
// guarantee and stays untouched. explorer.exe is a ubiquitous shared process,
// so this sender is authenticated by possession, not identity: a 32-byte
// secret from BCryptGenRandom, generated once by whichever of the two mods
// initializes first and stored as REG_BINARY
// HKCU\Software\RDPSessionToolkit\LocalWidgetSecret — a project-level,
// per-user location both mods can reach (NOT either mod's
// Wh_GetModStoragePath directory, which differs per mod). Every command
// carries the secret; the receiver re-reads the registry value for every
// command and fails closed if it is absent, malformed, or different. The
// fixed-layout payload carries a magic and version so a stray WM_COPYDATA
// can never be mistaken for a command. Each command routes to the exact
// shared action function every other surface already uses.
//
// UIPI: no ChangeWindowMessageFilterEx on this window. If mstsc ever runs
// elevated, WM_COPYDATA from the medium-integrity widget is blocked and the
// channel simply does nothing — the conservative failure (see D-23).

// The window class, registry-key location, secret byte count, command
// magic/version, LocalWidgetCommand values, LocalWidgetCommandPayload layout,
// ReadLocalWidgetSecret, and EnsureLocalWidgetSecret are all shared contracts
// now — defined once above the branch namespaces. Only the receiver-side
// validation below (LocalWidgetSecretsEqual, ValidateLocalWidgetCommand) is
// client-specific.

// Constant-time compare so a rejected guess leaks nothing through timing.
bool LocalWidgetSecretsEqual(const BYTE* a, const BYTE* b, size_t n) {
    volatile BYTE diff = 0;
    for (size_t i = 0; i < n; i++)
        diff = (BYTE)(diff | (a[i] ^ b[i]));
    return diff == 0;
}

// True iff a WM_COPYDATA payload is a well-formed local widget command that
// carries the current shared secret. Every defect fails closed and is logged
// once per message; the payload is not interpreted before the secret passes.
bool ValidateLocalWidgetCommand(const COPYDATASTRUCT* cds, DWORD senderPid,
                                BYTE* cmdOut) {
    if (!cds || !cds->lpData || cds->cbData != sizeof(LocalWidgetCommandPayload)) {
        Wh_Log(L"LocalWidget: malformed payload (%u bytes) from pid=%u — ignored",
            cds ? cds->cbData : 0u, senderPid);
        return false;
    }
    LocalWidgetCommandPayload p;
    memcpy(&p, cds->lpData, sizeof(p));
    if (p.magic != kLocalWidgetCmdMagic || p.version != kLocalWidgetCmdVersion) {
        Wh_Log(L"LocalWidget: bad magic/version (0x%08X/%u) from pid=%u — ignored",
            p.magic, p.version, senderPid);
        return false;
    }
    BYTE expected[LOCAL_WIDGET_SECRET_BYTES] = {};
    if (!ReadLocalWidgetSecret(expected)) {
        Wh_Log(L"LocalWidget: no shared secret in HKCU\\%s — rejecting pid=%u",
            LOCAL_WIDGET_REG_KEY, senderPid);
        return false;  // fail closed
    }
    bool match = LocalWidgetSecretsEqual(p.secret, expected, LOCAL_WIDGET_SECRET_BYTES);
    WipeSecret(expected, sizeof(expected));
    WipeSecret(p.secret, sizeof(p.secret));
    if (!match) {
        Wh_Log(L"LocalWidget: secret mismatch from pid=%u — rejected", senderPid);
        return false;
    }
    *cmdOut = p.command;
    return true;
}

// Local-widget-thread-only, like g_hRelayWnd on the relay thread.
HWND               g_hLocalWidgetWnd         = nullptr;
HANDLE             g_hLocalWidgetThread      = nullptr;
std::atomic<DWORD> g_localWidgetThreadId     { 0 };
HANDLE             g_hLocalWidgetThreadReady = nullptr;  // queue-exists signal

LRESULT CALLBACK LocalWidgetWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_COPYDATA: {
        DWORD senderPid = 0;
        HWND hSender = reinterpret_cast<HWND>(wParam);
        if (hSender)
            GetWindowThreadProcessId(hSender, &senderPid);

        BYTE cmd = 0;
        if (!ValidateLocalWidgetCommand(
                reinterpret_cast<const COPYDATASTRUCT*>(lParam), senderPid, &cmd))
            return FALSE;
        Wh_Log(L"LocalWidget: command 0x%02X from pid=%u", cmd, senderPid);

        // Every case is one of this branch's shared action functions; the
        // actions check live state themselves and are idempotent, so no
        // mutual exclusion against the other UI surfaces is needed.
        switch (cmd) {
        case LWCMD_MINIMIZE:
            return MinimizeRdpFrame(L"Local widget Minimize") ? TRUE : FALSE;
        case LWCMD_RESTORE:
            return RestoreRdpFrame(L"Local widget Restore") ? TRUE : FALSE;
        case LWCMD_FULLSCREEN_TOGGLE:
            ToggleFullscreen(L"Local widget Fullscreen toggle");
            return TRUE;
        case LWCMD_RECONNECT: {
            // The same gate every Reconnect surface uses (D-28): the widget
            // hides its button on this setting too, but the receiver is the
            // authority for what actually relaunches.
            if (!g_enableReconnect) {
                Wh_Log(L"Local widget Reconnect — Reconnect is disabled "
                       L"(enableReconnect off), ignored");
                return FALSE;
            }
            HWND hFrame = GetRdpFrameForAction();
            if (!hFrame) {
                Wh_Log(L"Local widget Reconnect — no RDP frame, ignored");
                return FALSE;
            }
            ReconnectSessionClean(hFrame, L"Local widget Reconnect");
            return TRUE;
        }
        case LWCMD_DISCONNECT: {
            HWND hFrame = GetRdpFrameForAction();
            if (!hFrame) {
                Wh_Log(L"Local widget Disconnect — no RDP frame, ignored");
                return FALSE;
            }
            Wh_Log(L"Local widget Disconnect");
            DisconnectSession(hFrame);
            return TRUE;
        }
        default:
            Wh_Log(L"LocalWidget: unknown command 0x%02X — ignored", cmd);
            return FALSE;
        }
    }

    case WM_DESTROY:
        g_hLocalWidgetWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

DWORD WINAPI LocalWidgetThread(LPVOID) {
    // Same queue-first readiness handshake as the relay, helper, and
    // watchdog threads (see StopHelperThread's comment for the race).
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hLocalWidgetThreadReady)
        SetEvent(g_hLocalWidgetThreadReady);

    WNDCLASSEXW wc   = { sizeof(wc) };
    wc.lpfnWndProc   = LocalWidgetWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = LOCAL_WIDGET_CLASS;
    if (!RegisterClassExW(&wc)) {
        Wh_Log(L"LocalWidget: RegisterClassExW failed GLE=%d", GetLastError());
        return 0;
    }

    g_hLocalWidgetWnd = CreateWindowExW(0, LOCAL_WIDGET_CLASS, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_hLocalWidgetWnd) {
        Wh_Log(L"LocalWidget: CreateWindowExW failed GLE=%d", GetLastError());
        UnregisterClassW(LOCAL_WIDGET_CLASS, GetModuleHandleW(nullptr));
        return 0;
    }
    Wh_Log(L"LocalWidget: message window ready HWND=%p class=%s",
        g_hLocalWidgetWnd, LOCAL_WIDGET_CLASS);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hLocalWidgetWnd && IsWindow(g_hLocalWidgetWnd))
        DestroyWindow(g_hLocalWidgetWnd);
    UnregisterClassW(LOCAL_WIDGET_CLASS, GetModuleHandleW(nullptr));
    return 0;
}

void StartLocalWidgetThread() {
    if (g_hLocalWidgetThread) return;
    if (!g_hLocalWidgetThreadReady)
        g_hLocalWidgetThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hLocalWidgetThreadReady);
    DWORD threadId = 0;
    g_hLocalWidgetThread = CreateThread(
        nullptr, 0, LocalWidgetThread, nullptr, 0, &threadId);
    g_localWidgetThreadId.store(threadId);
}

void StopLocalWidgetThread() {
    DWORD threadId = g_localWidgetThreadId.load();
    if (threadId) {
        // Queue-exists-before-quit, as for every other thread here.
        if (g_hLocalWidgetThreadReady)
            WaitForSingleObject(g_hLocalWidgetThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hLocalWidgetThread) {
        WaitForSingleObject(g_hLocalWidgetThread, 3000);
        CloseHandle(g_hLocalWidgetThread);
        g_hLocalWidgetThread = nullptr;
        g_localWidgetThreadId.store(0);
    }
}

// ── RDP control event sink (connection quality) ──────────────────────────
//
// mstsc.exe hosts the public MsRdpClient ActiveX control (mstscax.dll). The
// v0.4.0 design (D-16) assumed it creates the control through
// CoCreateInstance — the EXE's only COM-creation import — and hooked that.
// Closer inspection of the binary (D-29, "Creation path 2" below) shows
// mstsc hand-loads mstscax.dll and takes the control's class factory
// directly via DllGetClassObject, a path CoCreateInstance never sees; the
// mod now watches BOTH paths. Either way, once the control is in hand this
// mod advises, through the control's documented IConnectionPointContainer, a
// SECOND sink on its documented IMsTscAxEvents dispinterface. Only public,
// documented COM is involved at that point: nothing internal to mstsc or
// mstscax is called or assumed, and mstsc's own sink keeps receiving
// everything exactly as before.
//
// DISPIDs are resolved at runtime from the registered MSTSCLib type library
// rather than hard-coded: on this machine OnNetworkStatusChanged is 32, which
// is NOT what memory or older references suggest — a wrong constant would
// silently match nothing. If the type library cannot be loaded the code falls
// back to the values read from this machine's mstscax.dll and says so in the
// log (DECISIONS.md D-16).

const GUID kLIBID_MSTSCLib =
    { 0x8C11EFA1, 0x92C3, 0x11D1, { 0xBC, 0x1E, 0x00, 0xC0, 0x4F, 0xA3, 0x14, 0x89 } };
const GUID kDIID_IMsTscAxEvents =
    { 0x336D5562, 0xEFA8, 0x482E, { 0x8C, 0xB3, 0xC5, 0xC0, 0xFC, 0x7A, 0x7D, 0xB6 } };
// Standard IIDs defined locally, consistent with the TaskbarList GUIDs above.
const IID kIID_IUnknown =
    { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const IID kIID_IDispatch =
    { 0x00020400, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
const IID kIID_IConnectionPointContainer =
    { 0xB196B284, 0xBAB4, 0x101A, { 0xB6, 0x9C, 0x00, 0xAA, 0x00, 0x34, 0x1D, 0x07 } };

// Fallbacks = values read from this machine's registered MSTSCLib 1.0.
DISPID g_dispidNetworkStatus = 32;  // OnNetworkStatusChanged
DISPID g_dispidConnected     = 2;   // OnConnected
DISPID g_dispidDisconnected  = 4;   // OnDisconnected
DISPID g_dispidEnterFs       = 5;   // OnEnterFullScreenMode
DISPID g_dispidLeaveFs       = 6;   // OnLeaveFullScreenMode

void ResolveEventDispids() {
    ITypeLib* pTypeLib = nullptr;
    HRESULT hr = LoadRegTypeLib(kLIBID_MSTSCLib, 1, 0, 0, &pTypeLib);
    if (FAILED(hr) || !pTypeLib) {
        Wh_Log(L"RdpEvents: LoadRegTypeLib(MSTSCLib 1.0) failed hr=0x%08X — "
               L"using built-in DISPIDs (OnNetworkStatusChanged=%d)",
               hr, g_dispidNetworkStatus);
        return;
    }
    ITypeInfo* pTypeInfo = nullptr;
    hr = pTypeLib->GetTypeInfoOfGuid(kDIID_IMsTscAxEvents, &pTypeInfo);
    pTypeLib->Release();
    if (FAILED(hr) || !pTypeInfo) {
        Wh_Log(L"RdpEvents: IMsTscAxEvents type info not found hr=0x%08X — "
               L"using built-in DISPIDs", hr);
        return;
    }
    struct { PCWSTR name; DISPID* out; } names[] = {
        { L"OnNetworkStatusChanged", &g_dispidNetworkStatus },
        { L"OnConnected",            &g_dispidConnected     },
        { L"OnDisconnected",         &g_dispidDisconnected  },
        { L"OnEnterFullScreenMode",  &g_dispidEnterFs       },
        { L"OnLeaveFullScreenMode",  &g_dispidLeaveFs       },
    };
    for (auto& n : names) {
        wchar_t buf[64];
        wcscpy_s(buf, n.name);
        LPOLESTR pName = buf;
        DISPID id = DISPID_UNKNOWN;
        if (SUCCEEDED(pTypeInfo->GetIDsOfNames(&pName, 1, &id)) &&
            id != DISPID_UNKNOWN) {
            if (id != *n.out)
                Wh_Log(L"RdpEvents: %s DISPID is %d (built-in was %d)",
                    n.name, id, *n.out);
            *n.out = id;
        } else {
            Wh_Log(L"RdpEvents: %s not in type library — keeping %d",
                n.name, *n.out);
        }
    }
    pTypeInfo->Release();
    Wh_Log(L"RdpEvents: DISPIDs resolved from type library — "
           L"OnNetworkStatusChanged=%d", g_dispidNetworkStatus);
}

long VariantToLong(const VARIANT& v) {
    VARIANT tmp;
    VariantInit(&tmp);
    long result = 0;
    if (SUCCEEDED(VariantChangeType(&tmp, const_cast<VARIANT*>(&v), 0, VT_I4)))
        result = tmp.lVal;
    VariantClear(&tmp);
    return result;
}

// Minimal IDispatch sink. Called by mstscax on the control's STA thread;
// everything it touches is atomic state plus message posts.
class RdpEventSink final : public IDispatch {
    LONG m_ref = 1;
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, kIID_IUnknown) || IsEqualIID(riid, kIID_IDispatch) ||
            IsEqualIID(riid, kDIID_IMsTscAxEvents)) {
            *ppv = static_cast<IDispatch*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override {
        return (ULONG)InterlockedIncrement(&m_ref);
    }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = (ULONG)InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override {
        if (pctinfo) *pctinfo = 0;
        return S_OK;
    }
    STDMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo** ppTInfo) override {
        if (ppTInfo) *ppTInfo = nullptr;
        return E_NOTIMPL;
    }
    STDMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override {
        return E_NOTIMPL;
    }
    STDMETHODIMP Invoke(DISPID dispIdMember, REFIID, LCID, WORD,
                        DISPPARAMS* pDispParams, VARIANT*, EXCEPINFO*,
                        UINT*) override {
        if (dispIdMember == g_dispidNetworkStatus) {
            // OnNetworkStatusChanged([in] ULONG qualityLevel, [in] LONG
            // bandwidth, [in] LONG rtt) — rgvarg is in reverse order.
            if (pDispParams && pDispParams->cArgs >= 3 && pDispParams->rgvarg) {
                long q   = VariantToLong(pDispParams->rgvarg[2]);
                long bw  = VariantToLong(pDispParams->rgvarg[1]);
                long rtt = VariantToLong(pDispParams->rgvarg[0]);
                g_netQuality.store((int)q);
                g_netBandwidth.store(bw);
                g_netRtt.store(rtt);
                g_netLastTick.store(GetTickCount64());
                Wh_Log(L"RdpEvents: network status quality=%ld bandwidth=%ld rtt=%ld",
                    q, bw, rtt);
                NotifyStatusChanged();
            } else {
                Wh_Log(L"RdpEvents: OnNetworkStatusChanged with %u args — ignored",
                    pDispParams ? pDispParams->cArgs : 0u);
            }
        } else if (dispIdMember == g_dispidConnected) {
            // Tighter session-clock origin than frame creation, when available.
            g_sessionStartTick.store(GetTickCount64());
            Wh_Log(L"RdpEvents: OnConnected — session clock restarted");
            NotifyStatusChanged();
        } else if (dispIdMember == g_dispidDisconnected) {
            long reason = (pDispParams && pDispParams->cArgs >= 1 && pDispParams->rgvarg)
                ? VariantToLong(pDispParams->rgvarg[0]) : 0;
            g_netQuality.store(0);
            Wh_Log(L"RdpEvents: OnDisconnected reason=%ld — quality cleared", reason);
            NotifyStatusChanged();
        } else if (dispIdMember == g_dispidEnterFs || dispIdMember == g_dispidLeaveFs) {
            // Repaint the overlay on the real transition; the panel's own
            // fullscreen button relabels off the next status snapshot.
            Wh_Log(L"RdpEvents: %s", dispIdMember == g_dispidEnterFs
                ? L"OnEnterFullScreenMode" : L"OnLeaveFullScreenMode");
            NotifyStatusChanged();
        }
        return S_OK;
    }
};

// Sink bookkeeping. The connection point is an STA object: Advise happened
// on the creating (UI) thread and Unadvise must happen there too, so the
// pointer is only ever touched from that thread (checked by thread id).
IConnectionPoint*  g_pEventsCP    = nullptr;
DWORD              g_eventsCookie = 0;
RdpEventSink*      g_pEventSink   = nullptr;
std::atomic<DWORD> g_sinkThreadId { 0 };
std::atomic<bool>  g_adviseBusy   { false };  // re-entrancy guard

void GuidToString(REFGUID g, wchar_t* out, int cch) {
    if (StringFromGUID2(g, out, cch) == 0)
        wcscpy_s(out, cch, L"{?}");
}

// Module that implements a COM object, judged by where its QueryInterface
// lives (vtable slot 0). Diagnostic only: tells an mstscax object apart from
// the many other COM objects mstsc creates, without needing a CLSID list.
bool GetObjectModuleName(IUnknown* pUnk, wchar_t* out, DWORD cch) {
    out[0] = L'\0';
    void** vtbl = *reinterpret_cast<void***>(pUnk);
    if (!vtbl)
        return false;
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(vtbl[0]), &hMod) || !hMod)
        return false;
    wchar_t path[MAX_PATH] = {};
    if (!GetModuleFileNameW(hMod, path, ARRAYSIZE(path)))
        return false;
    PCWSTR name = wcsrchr(path, L'\\');
    wcscpy_s(out, cch, name ? name + 1 : path);
    return true;
}

// Tries to advise the sink on a freshly created COM object. `source` names
// the creation path that produced it; `verbose` logs every step even for
// objects that turn out not to be the control (the first N CoCreateInstance
// calls, and everything from mstscax's own factory), otherwise only objects
// implemented by mstscax.dll are logged. Each step — QI for
// IConnectionPointContainer, FindConnectionPoint, Advise — reports its
// HRESULT so the log says exactly where the path stopped (D-29).
void TryAdviseRdpControl(IUnknown* pUnk, PCWSTR source, REFCLSID rclsid, bool verbose) {
    if (g_sinkAdvised.load() || g_adviseBusy.exchange(true))
        return;
    wchar_t clsidStr[48], mod[64];
    GuidToString(rclsid, clsidStr, ARRAYSIZE(clsidStr));
    GetObjectModuleName(pUnk, mod, ARRAYSIZE(mod));
    bool fromMstscax = _wcsicmp(mod, L"mstscax.dll") == 0;
    bool log = verbose || fromMstscax;

    IConnectionPointContainer* pCPC = nullptr;
    HRESULT hrQi = pUnk->QueryInterface(kIID_IConnectionPointContainer, (void**)&pCPC);
    if (log)
        Wh_Log(L"RdpEvents: [%s] object %p clsid=%s module=%s — "
               L"QI(IConnectionPointContainer) hr=0x%08X%s",
               source, pUnk, clsidStr, mod[0] ? mod : L"?", hrQi,
               fromMstscax ? L" ← mstscax object" : L"");
    if (FAILED(hrQi) || !pCPC) {
        g_adviseBusy.store(false);
        return;
    }

    IConnectionPoint* pCP = nullptr;
    HRESULT hr = pCPC->FindConnectionPoint(kDIID_IMsTscAxEvents, &pCP);
    pCPC->Release();
    if (log)
        Wh_Log(L"RdpEvents: [%s] FindConnectionPoint(IMsTscAxEvents) hr=0x%08X%s",
               source, hr,
               (SUCCEEDED(hr) && pCP) ? L" — this object IS the RDP control" : L"");
    if (FAILED(hr) || !pCP) {
        g_adviseBusy.store(false);
        return;
    }

    // This object sources IMsTscAxEvents — it is the RDP control.
    ResolveEventDispids();
    RdpEventSink* pSink = new RdpEventSink();
    DWORD cookie = 0;
    hr = pCP->Advise(static_cast<IDispatch*>(pSink), &cookie);
    if (SUCCEEDED(hr)) {
        g_pEventsCP    = pCP;
        g_eventsCookie = cookie;
        g_pEventSink   = pSink;
        g_sinkThreadId.store(GetCurrentThreadId());
        g_sinkAdvised.store(true);
        Wh_Log(L"RdpEvents: [%s] advised IMsTscAxEvents sink on RDP control %p "
               L"(cookie=%u, thread=%u)", source, pUnk, cookie, GetCurrentThreadId());
        NotifyStatusChanged();
    } else {
        Wh_Log(L"RdpEvents: [%s] Advise failed hr=0x%08X — no quality indicator",
            source, hr);
        pSink->Release();
        pCP->Release();
    }
    g_adviseBusy.store(false);
}

// Runs only on the sink's home thread: the frame subclass calls it on the
// teardown message (mod unload) and on the frame's WM_DESTROY — mstsc's one
// UI thread owns both the frame and the control. Anywhere else it refuses:
// a leaked reference at unload beats a cross-apartment Unadvise.
void UnadviseRdpEvents(PCWSTR reason) {
    if (!g_pEventsCP)
        return;
    if (GetCurrentThreadId() != g_sinkThreadId.load()) {
        Wh_Log(L"RdpEvents: unadvise (%s) requested off the sink's thread — skipped",
            reason);
        return;
    }
    HRESULT hr = g_pEventsCP->Unadvise(g_eventsCookie);
    g_pEventsCP->Release();
    g_pEventsCP    = nullptr;
    g_eventsCookie = 0;
    if (g_pEventSink) {
        g_pEventSink->Release();
        g_pEventSink = nullptr;
    }
    g_sinkAdvised.store(false);
    Wh_Log(L"RdpEvents: unadvised (%s) hr=0x%08X", reason, hr);
}

// ── Creation path 1: CoCreateInstance (combase) ───────────────────────────
//
// Kept as built in v0.4.0 (D-16), now instrumented: the first N calls are
// logged individually (CLSID, context, HRESULT, and the QI/FindConnectionPoint
// outcome for the object), later ones only when they yield an mstscax object.
// Live evidence up to v0.6.0: no RdpEvents line ever appeared — see path 2
// for the reason identified.

using CoCreateInstance_t = decltype(&CoCreateInstance);
CoCreateInstance_t pOrigCoCreateInstance = nullptr;

constexpr unsigned RDPEVENTS_VERBOSE_COCREATE_CALLS = 40;

HRESULT WINAPI CoCreateInstance_Hook(REFCLSID rclsid, LPUNKNOWN pUnkOuter,
                                     DWORD dwClsContext, REFIID riid,
                                     LPVOID* ppv) {
    HRESULT hr = pOrigCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
    unsigned n = g_coCreateCalls.fetch_add(1) + 1;
    bool verbose = n <= RDPEVENTS_VERBOSE_COCREATE_CALLS;
    if (n == 1)
        Wh_Log(L"RdpEvents: CoCreateInstance hook fired for the first time (thread=%u)",
            GetCurrentThreadId());
    if (verbose) {
        wchar_t c[48];
        GuidToString(rclsid, c, ARRAYSIZE(c));
        Wh_Log(L"RdpEvents: CoCreateInstance #%u clsid=%s ctx=0x%X aggregated=%d hr=0x%08X",
            n, c, dwClsContext, (int)(pUnkOuter != nullptr), hr);
        if (n == RDPEVENTS_VERBOSE_COCREATE_CALLS)
            Wh_Log(L"RdpEvents: per-call CoCreateInstance logging ends here (%u calls); "
                   L"later calls are logged only when they yield an mstscax.dll object", n);
    }
    // Aggregated creations hand back the inner IUnknown, which must not be
    // QueryInterface'd for anything — skip those. Everything else gets one
    // cheap, standard QI; non-RDP objects just answer E_NOINTERFACE.
    if (SUCCEEDED(hr) && !pUnkOuter && ppv && *ppv && !g_sinkAdvised.load())
        TryAdviseRdpControl(static_cast<IUnknown*>(*ppv), L"CoCreateInstance",
            rclsid, verbose);
    return hr;
}

// ── Creation path 2: mstscax's own class factory (D-29) ───────────────────
//
// Why path 1 could never see the control — identified by inspecting this
// machine's mstsc.exe (10.0.26100.8875), not guessed:
//   • mstsc.exe imports NOTHING from mstscax.dll (no static or delay-load
//     import), while mstscax.dll exports only DllGetClassObject /
//     DllGetTscCtlVer / DllCanUnloadNow / Dll(Un)RegisterServer.
//   • mstsc.exe's read-only data holds the UTF-16 string "mstscax.dll"
//     immediately followed by the ASCII string "DllGetClassObject" (file
//     offsets 0x11EFD8 / 0x11EFF0). "DllGetClassObject" is not an import of
//     mstsc.exe, so that string exists only to be passed to GetProcAddress.
//   • The two MsRdpClient CLSIDs mstsc embeds — the "version 12" control
//     {1DF7C823-B2D4-4B54-975A-F2AC5D7CF8B8} and the legacy "version 2"
//     {7CACBD7B-0D99-468F-AC33-22E495C0AFE5} — sit a few hundred bytes away
//     in the same data region.
//   • CoCreateInstance reaches mstsc only as a delay-loaded ole32 import
//     that forwards to combase — so path 1's hook target IS correct; it just
//     watches a door the control does not come through.
// That is the hand-load-the-control-DLL pattern: LoadLibrary("mstscax.dll"),
// GetProcAddress("DllGetClassObject"), IClassFactory::CreateInstance on the
// factory that returns. An object created that way never passes through
// CoCreateInstance.
//
// Mechanism: mstscax.dll is loaded here in Wh_ModInit (mstsc loads the same
// System32 module moments later; the refcount simply goes up) so that its
// DllGetClassObject export, and the CreateInstance method of the class
// factory it hands out for the control CLSIDs (read from the factory's
// vtable), can be hooked as ordinary Windhawk-managed hooks together with
// everything else — no Wh_ApplyHookOperations at DLL-load time, no pointers
// of ours handed to mstsc, cleanly removed at unload. The CreateInstance hook
// offers every object the factory produces to TryAdviseRdpControl: the same
// sink, the same advise, reached at the control's actual birthplace. mstsc's
// own sink and behavior are untouched. The DllGetClassObject hook is there
// for evidence (which CLSID mstsc asks for) and to flag a factory whose
// CreateInstance is NOT the one hooked, should that ever happen.

const IID kIID_IClassFactory =
    { 0x00000001, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
// The control CLSIDs embedded in mstsc.exe (see above), newest first.
const CLSID kCLSID_MsRdpClient12 =
    { 0x1DF7C823, 0xB2D4, 0x4B54, { 0x97, 0x5A, 0xF2, 0xAC, 0x5D, 0x7C, 0xF8, 0xB8 } };
const CLSID kCLSID_MsRdpClient2 =
    { 0x7CACBD7B, 0x0D99, 0x468F, { 0xAC, 0x33, 0x22, 0xE4, 0x95, 0xC0, 0xAF, 0xE5 } };

using DllGetClassObject_t = HRESULT (WINAPI*)(REFCLSID, REFIID, LPVOID*);
DllGetClassObject_t pOrigMstscaxDllGetClassObject = nullptr;

// IClassFactory::CreateInstance as a plain function: `this` is the first
// argument under the one x64 calling convention.
using FactoryCreateInstance_t = HRESULT (WINAPI*)(IClassFactory*, IUnknown*, REFIID, void**);
constexpr int FACTORY_HOOK_SLOTS = 2;   // one per distinct CreateInstance implementation
void*                   g_factoryCreateTarget[FACTORY_HOOK_SLOTS] = {};
FactoryCreateInstance_t g_factoryCreateOrig[FACTORY_HOOK_SLOTS]   = {};
constexpr int           VTBL_SLOT_CREATEINSTANCE = 3;  // QI, AddRef, Release, CreateInstance

bool IsHookedFactoryCreateInstance(void* fn) {
    for (int i = 0; i < FACTORY_HOOK_SLOTS; i++)
        if (g_factoryCreateTarget[i] && g_factoryCreateTarget[i] == fn)
            return true;
    return false;
}

HRESULT OnFactoryCreateInstance(int slot, IClassFactory* self, IUnknown* pUnkOuter,
                                REFIID riid, void** ppv) {
    HRESULT hr = g_factoryCreateOrig[slot](self, pUnkOuter, riid, ppv);
    unsigned n = g_factoryCreateCalls.fetch_add(1) + 1;
    wchar_t i[48];
    GuidToString(riid, i, ARRAYSIZE(i));
    Wh_Log(L"RdpEvents: mstscax factory CreateInstance #%u (factory %p) riid=%s "
           L"aggregated=%d hr=0x%08X (thread=%u)",
           n, self, i, (int)(pUnkOuter != nullptr), hr, GetCurrentThreadId());
    if (SUCCEEDED(hr) && !pUnkOuter && ppv && *ppv && !g_sinkAdvised.load()) {
        // The factory does not carry its CLSID through CreateInstance; the
        // DllGetClassObject log line just before it names the class.
        static const CLSID kNoClsid = {};
        TryAdviseRdpControl(static_cast<IUnknown*>(*ppv),
            L"mstscax factory CreateInstance", kNoClsid, true);
    }
    return hr;
}

HRESULT WINAPI FactoryCreateInstance_Hook0(IClassFactory* self, IUnknown* pUnkOuter,
                                           REFIID riid, void** ppv) {
    return OnFactoryCreateInstance(0, self, pUnkOuter, riid, ppv);
}
HRESULT WINAPI FactoryCreateInstance_Hook1(IClassFactory* self, IUnknown* pUnkOuter,
                                           REFIID riid, void** ppv) {
    return OnFactoryCreateInstance(1, self, pUnkOuter, riid, ppv);
}

HRESULT WINAPI MstscaxDllGetClassObject_Hook(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    HRESULT hr = pOrigMstscaxDllGetClassObject(rclsid, riid, ppv);
    unsigned n = g_dllGetClassObjectCalls.fetch_add(1) + 1;
    wchar_t c[48], i[48];
    GuidToString(rclsid, c, ARRAYSIZE(c));
    GuidToString(riid, i, ARRAYSIZE(i));
    PCWSTR factoryNote = L"";
    if (SUCCEEDED(hr) && ppv && *ppv && IsEqualIID(riid, kIID_IClassFactory)) {
        void** vtbl = *reinterpret_cast<void***>(*ppv);
        factoryNote = IsHookedFactoryCreateInstance(vtbl[VTBL_SLOT_CREATEINSTANCE])
            ? L" — factory's CreateInstance is the hooked one"
            : L" — WARNING: factory's CreateInstance is NOT hooked (different "
              L"factory class than the ones resolved at init)";
    }
    Wh_Log(L"RdpEvents: mstscax!DllGetClassObject #%u clsid=%s riid=%s hr=0x%08X "
           L"(thread=%u)%s", n, c, i, hr, GetCurrentThreadId(), factoryNote);
    return hr;
}

// Wh_ModInit helper: load mstscax.dll, hook its DllGetClassObject, and hook
// the CreateInstance of the factory it returns for each control CLSID. Every
// step logs its outcome so a live test shows exactly how far this got.
void InstallMstscaxFactoryHooks() {
    bool preloaded = GetModuleHandleW(L"mstscax.dll") != nullptr;
    HMODULE hAx = preloaded ? GetModuleHandleW(L"mstscax.dll")
                            : LoadLibraryW(L"mstscax.dll");
    if (!hAx) {
        Wh_Log(L"RdpEvents: LoadLibraryW(mstscax.dll) failed GLE=%d — factory path "
               L"not hooked", GetLastError());
        return;
    }
    Wh_Log(L"RdpEvents: mstscax.dll %s at init (%p)%s",
        preloaded ? L"was already loaded" : L"loaded by this mod", hAx,
        preloaded ? L" — a control created before this point cannot be observed "
                    L"(reopen the session)" : L"");

    auto pDGCO = reinterpret_cast<DllGetClassObject_t>(
        GetProcAddress(hAx, "DllGetClassObject"));
    if (!pDGCO) {
        Wh_Log(L"RdpEvents: mstscax!DllGetClassObject export not found GLE=%d",
            GetLastError());
        return;
    }
    BOOL ok = Wh_SetFunctionHook(reinterpret_cast<void*>(pDGCO),
        reinterpret_cast<void*>(MstscaxDllGetClassObject_Hook),
        reinterpret_cast<void**>(&pOrigMstscaxDllGetClassObject));
    Wh_Log(L"RdpEvents: hook mstscax!DllGetClassObject (%p) — Wh_SetFunctionHook=%d",
        pDGCO, ok);

    // Resolve the factory's CreateInstance for each control CLSID and hook
    // each distinct implementation (normally one shared by both). Calling
    // DllGetClassObject here only creates — and immediately releases — a
    // class factory object; no control is created.
    struct { const CLSID* clsid; PCWSTR label; } controls[] = {
        { &kCLSID_MsRdpClient12, L"MsRdpClient (version 12)" },
        { &kCLSID_MsRdpClient2,  L"MsRdpClient (version 2)"  },
    };
    int slots = 0;
    for (auto& c : controls) {
        IClassFactory* pFactory = nullptr;
        HRESULT hr = pDGCO(*c.clsid, kIID_IClassFactory, (void**)&pFactory);
        if (FAILED(hr) || !pFactory) {
            Wh_Log(L"RdpEvents: DllGetClassObject(%s) at init failed hr=0x%08X",
                c.label, hr);
            continue;
        }
        void** vtbl = *reinterpret_cast<void***>(pFactory);
        void* fn = vtbl[VTBL_SLOT_CREATEINSTANCE];
        pFactory->Release();
        if (IsHookedFactoryCreateInstance(fn)) {
            Wh_Log(L"RdpEvents: %s factory CreateInstance=%p — same implementation, "
                   L"already hooked", c.label, fn);
            continue;
        }
        if (slots >= FACTORY_HOOK_SLOTS) {
            Wh_Log(L"RdpEvents: %s factory CreateInstance=%p — no hook slot left, "
                   L"NOT hooked", c.label, fn);
            continue;
        }
        void* hook = slots == 0 ? reinterpret_cast<void*>(FactoryCreateInstance_Hook0)
                                : reinterpret_cast<void*>(FactoryCreateInstance_Hook1);
        ok = Wh_SetFunctionHook(fn, hook,
            reinterpret_cast<void**>(&g_factoryCreateOrig[slots]));
        if (ok)
            g_factoryCreateTarget[slots] = fn;
        Wh_Log(L"RdpEvents: hook %s factory CreateInstance (%p) — Wh_SetFunctionHook=%d",
            c.label, fn, ok);
        if (ok)
            slots++;
    }
    if (slots == 0)
        Wh_Log(L"RdpEvents: no factory CreateInstance hooked — only the "
               L"CoCreateInstance path and the DllGetClassObject log remain");
}

// ── Hooks ─────────────────────────────────────────────────────────────────

bool IsBBarClass(LPCWSTR lpClassName) {
    if (!lpClassName) return false;
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) return false;
    return lstrcmpW(lpClassName, L"BBarWindowClass") == 0;
}

// The RDP frame check falls back to GetClassNameW because, unlike the BBar
// case, mstsc could plausibly create its main frame from a class atom.
bool IsTscFrameClass(LPCWSTR lpClassName, HWND hwnd) {
    if (lpClassName && reinterpret_cast<ULONG_PTR>(lpClassName) > 0xFFFF)
        return lstrcmpW(lpClassName, L"TscShellContainerClass") == 0;
    wchar_t cls[64] = {};
    return GetClassNameW(hwnd, cls, ARRAYSIZE(cls)) != 0 &&
           lstrcmpW(cls, L"TscShellContainerClass") == 0;
}

HWND WINAPI CreateWindowExW_Hook(
    DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
    DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
    HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
    HWND hwnd = pOrigCreateWindowExW(
        dwExStyle, lpClassName, lpWindowName,
        dwStyle, X, Y, nWidth, nHeight,
        hWndParent, hMenu, hInstance, lpParam);

    // Phase 1: latch the RDP frame directly when mstsc creates its main
    // window, independent of the connection bar (which is fullscreen-only) —
    // windowed sessions must be tracked too, for the session clock, the
    // status snapshot the taskbar-embedded panel reads, and the reconnect
    // helper's WM_DESTROY launch point. The bar-triggered path below is kept
    // unchanged; for the same session it re-latches the same frame handle.
    // The showThumbbar term is gone with the thumbnail toolbar (D-33); only
    // hideBar and showOverlay gate this now.
    if (hwnd && (g_hideBar || g_showOverlay) && IsTscFrameClass(lpClassName, hwnd)) {
        Wh_Log(L"[DIAG] RDP frame class matched, HWND=%p", hwnd);

        EnterCriticalSection(&g_cs);
        bool alreadySubclassed = g_origFrameWndProc != nullptr;
        LeaveCriticalSection(&g_cs);

        if (!alreadySubclassed) {
            // Same pattern as the BBar subclass; safe here for the same
            // reason — CreateWindowExW returns on the window's own thread,
            // which is not pumping messages while we're inside the hook.
            WNDPROC origProc = reinterpret_cast<WNDPROC>(
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(FrameSubclassProc)));

            if (!origProc) {
                Wh_Log(L"[DIAG] FrameSubclassProc attach FAILED for HWND=%p, GLE=%d",
                    hwnd, GetLastError());
            } else {
                Wh_Log(L"[DIAG] FrameSubclassProc attached to HWND=%p, origProc=%p",
                    hwnd, origProc);
            }

            EnterCriticalSection(&g_cs);
            g_hRdpFrame        = hwnd;
            g_origFrameWndProc = origProc;
            LeaveCriticalSection(&g_cs);

            // Session clock starts here — the connection-detection point.
            g_sessionStartTick.store(GetTickCount64());

            Wh_Log(L"RDP frame detected HWND=%p — subclassed", hwnd);
            LogRdpEventsDiag(L"frame created");
        }
    }

    if (hwnd && (g_hideBar || g_showOverlay) && IsBBarClass(lpClassName)) {
        HWND hFrame = hWndParent ? GetAncestor(hWndParent, GA_ROOT) : nullptr;
        RECT mon    = GetMonitorRect(hFrame ? hFrame : hwnd);
        Wh_Log(L"BBar detected HWND=%p frame=%p monitor=%d,%d-%d,%d",
            hwnd, hFrame, mon.left, mon.top, mon.right, mon.bottom);

        WNDPROC origProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(BBarSubclassProc)));

        EnterCriticalSection(&g_cs);
        g_hBBar           = hwnd;
        g_hRdpFrame       = hFrame;
        g_origBBarWndProc = origProc;
        LeaveCriticalSection(&g_cs);

        if (g_hideBar)
            pOrigShowWindow(hwnd, SW_HIDE);

        DWORD helperThreadId = g_helperThreadId.load();
        if (g_showOverlay && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
    }

    return hwnd;
}

BOOL WINAPI ShowWindow_Hook(HWND hWnd, int nCmdShow) {
    if (g_hideBar && hWnd && hWnd == g_hBBar) {
        Wh_Log(L"ShowWindow: suppressing nCmdShow=%d on BBar", nCmdShow);
        return pOrigShowWindow(hWnd, SW_HIDE);
    }
    return pOrigShowWindow(hWnd, nCmdShow);
}

BOOL WINAPI SetWindowPos_Hook(
    HWND hWnd, HWND hWndInsertAfter,
    int X, int Y, int cx, int cy, UINT uFlags)
{
    if (g_hideBar && hWnd && hWnd == g_hBBar) {
        if (uFlags & SWP_SHOWWINDOW) {
            Wh_Log(L"SetWindowPos: stripping SWP_SHOWWINDOW from BBar");
            uFlags = (uFlags & ~SWP_SHOWWINDOW) | SWP_HIDEWINDOW;
        }
    }

    BOOL result = pOrigSetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);

    if (g_showOverlay && hWnd && hWnd == g_hRdpFrame && !(uFlags & SWP_NOMOVE)) {
        HMONITOR hMon = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
        if (hMon && hMon != g_hLastMonitor.load()) {
            g_hLastMonitor.store(hMon);
            Wh_Log(L"RDP frame changed monitor — repositioning button");
            DWORD helperThreadId = g_helperThreadId.load();
            if (helperThreadId)
                PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
        }
    }

    return result;
}

BOOL WINAPI SetWindowTextW_Hook(HWND hWnd, LPCWSTR lpString) {
    BOOL result = pOrigSetWindowTextW(hWnd, lpString);

    EnterCriticalSection(&g_cs);
    bool isFrame = (hWnd == g_hRdpFrame);
    LeaveCriticalSection(&g_cs);

    if (isFrame) {
        // Always cached — the reconnect helper derives /v:<host> from it
        // when the command line names no target — and repainted on the
        // overlay only when it is actually displayed there.
        UpdateHostname();
        if (g_showOverlay && g_showHostname) {
            DWORD helperThreadId = g_helperThreadId.load();
            if (helperThreadId)
                PostThreadMessageW(helperThreadId, WM_REPAINT_BTN, 0, 0);
        }
    }
    return result;
}

// ── mstsc.exe branch entry points (dispatched from Wh_ModInit below) ──

BOOL ModInit() {
    InitializeCriticalSection(&g_cs);
    LoadSettings();

    // Resolve (and log) the mod storage directory exactly once, here, rather
    // than on the first 1 s tick — every later path builder uses the cache.
    GetModStorageDir();

    // Registered before any hook is installed, so the frame subclass only
    // ever sees it fully initialized. This mod's private frame-thread request
    // to release the RDP control's event sink on its home thread.
    g_msgSinkTeardown = RegisterWindowMessageW(L"WH_RdpstkClient_SinkTeardown");

    bool dragOnRight, dragAtBottom;
    int  dragDx, dragDy;
    if (LoadPersistedDragPosition(&dragOnRight, &dragAtBottom, &dragDx, &dragDy)) {
        EnterCriticalSection(&g_cs);
        g_hasDragPos   = true;
        g_dragOnRight  = dragOnRight;
        g_dragAtBottom = dragAtBottom;
        g_dragDx       = dragDx;
        g_dragDy       = dragDy;
        LeaveCriticalSection(&g_cs);
        Wh_Log(L"Loaded persisted button position: onRight=%d atBottom=%d dx=%d dy=%d",
            (int)dragOnRight, (int)dragAtBottom, dragDx, dragDy);
    }

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(CreateWindowExW),
        reinterpret_cast<void*>(CreateWindowExW_Hook),
        reinterpret_cast<void**>(&pOrigCreateWindowExW));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(ShowWindow),
        reinterpret_cast<void*>(ShowWindow_Hook),
        reinterpret_cast<void**>(&pOrigShowWindow));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(SetWindowPos),
        reinterpret_cast<void*>(SetWindowPos_Hook),
        reinterpret_cast<void**>(&pOrigSetWindowPos));

    Wh_SetFunctionHook(
        reinterpret_cast<void*>(SetWindowTextW),
        reinterpret_cast<void*>(SetWindowTextW_Hook),
        reinterpret_cast<void**>(&pOrigSetWindowTextW));

    // CoCreateInstance lives in combase.dll (ole32's export forwards there —
    // verified against this machine's ole32: a forwarder to
    // api-ms-win-core-com-l1-1-0, i.e. combase); hook the real implementation
    // so every caller in the process is seen. Falls back to the import if
    // needed. Logged, including Wh_SetFunctionHook's result, so "did the hook
    // even install" is answered by the log (D-29).
    {
        HMODULE hCombase = GetModuleHandleW(L"combase.dll");
        if (!hCombase) hCombase = LoadLibraryW(L"combase.dll");
        void* target = hCombase
            ? reinterpret_cast<void*>(GetProcAddress(hCombase, "CoCreateInstance"))
            : nullptr;
        bool viaExport = target != nullptr;
        if (!target) target = reinterpret_cast<void*>(CoCreateInstance);
        BOOL ok = Wh_SetFunctionHook(target,
            reinterpret_cast<void*>(CoCreateInstance_Hook),
            reinterpret_cast<void**>(&pOrigCoCreateInstance));
        Wh_Log(L"RdpEvents: hook CoCreateInstance target=%p (%s) — Wh_SetFunctionHook=%d",
            target, viaExport ? L"combase.dll export" : L"import fallback", ok);
    }

    // The control's actual creation path (D-29): mstscax's own class factory.
    InstallMstscaxFactoryHooks();

    // The relay receiver exists for the whole mod lifetime, independent of
    // the overlay visibility setting — toolkit components must be able to
    // reach it whenever the mod is loaded.
    StartRelayThread();

    // The taskbar-embedded widget's local command receiver: a second,
    // separate window (D-13/D-23), alive for the whole mod lifetime like the
    // relay receiver. Its shared secret is created here unless the widget
    // mod already did.
    EnsureLocalWidgetSecret(L"Wh_ModInit");
    StartLocalWidgetThread();

    // The helper thread owns the floating overlay window only — start it iff
    // the overlay is enabled.
    if (g_showOverlay)
        StartHelperThread();

    // The stuck-session watchdog is independent of the overlay setting: its
    // alert is its own window, and it must keep running when the frame thread
    // can't.
    if (g_stuckDetection)
        StartWatchdogThread();

    Wh_Log(L"RDP Session Toolkit Taskbar Client v0.9.1 initialized "
           L"[mstsc.exe branch] — "
           L"hide=%d overlay=%d hotkey=%d fade=%d hostname=%d "
           L"sessionInfo=%d reconnect=%d(mode=%d) stuck=%d(threshold=%ds)",
           (int)g_hideBar, (int)g_showOverlay,
           (int)g_enableHotkey, (int)g_fadeWhenIdle, (int)g_showHostname,
           (int)g_showSessionInfo,
           (int)g_enableReconnect, g_reconnectMode,
           (int)g_stuckDetection, g_stuckThresholdSec);
    return TRUE;
}

void ModSettingsChanged() {
    bool prevOverlay   = g_showOverlay;
    bool prevOnRight   = g_buttonOnRight;
    bool prevAtBottom  = g_buttonAtBottom;
    int  prevOffset    = g_buttonOffset;
    bool prevRelayTest = g_debugRelayTest;
    bool prevStuck     = g_stuckDetection;

    LoadSettings();

    // Watchdog on/off follows its setting; the threshold is read live by
    // the running poll, so it needs no restart.
    if (prevStuck != g_stuckDetection) {
        StopWatchdogThread();
        if (g_stuckDetection)
            StartWatchdogThread();
    }

    // Debug relay exercise: fires once per off→on flip of the setting, not
    // on every settings save while it is left on.
    if (g_debugRelayTest && !prevRelayTest)
        SendRelayTestMinimize();

    // buttonPosition, offsetPreset, and offsetCustom all funnel into these
    // three derived values — if none of them changed, the settings-driven
    // default didn't change either, so there's nothing to reset.
    if (g_buttonOnRight != prevOnRight || g_buttonAtBottom != prevAtBottom ||
        g_buttonOffset != prevOffset) {
        EnterCriticalSection(&g_cs);
        g_hasDragPos = false;
        LeaveCriticalSection(&g_cs);
        ClearPersistedDragPosition();
        Wh_Log(L"Button position settings changed — cleared dragged position");
    }

    // Overlay helper thread follows the overlay flag only: recycle it on any
    // change so an off→on shows the button and an on→off tears it down.
    if (prevOverlay || g_showOverlay) {
        StopHelperThread();
        if (g_showOverlay)
            StartHelperThread();
    }

    EnterCriticalSection(&g_cs);
    HWND hBBar = g_hBBar;
    LeaveCriticalSection(&g_cs);

    if (hBBar && IsWindow(hBBar)) {
        pOrigShowWindow(hBBar, g_hideBar ? SW_HIDE : SW_SHOWNOACTIVATE);
        DWORD helperThreadId = g_helperThreadId.load();
        if (g_showOverlay && helperThreadId)
            PostThreadMessageW(helperThreadId, WM_CREATE_BTN, 0, 0);
    }

    // Nothing to push to the frame's thread any more: the thumbnail toolbar
    // that needed a per-settings-change re-sync there is gone (D-33), and the
    // taskbar-embedded panel reloads its own settings in the explorer.exe
    // branch's Wh_ModSettingsChanged.

    Wh_Log(L"Settings reloaded [mstsc.exe branch] — hide=%d overlay=%d hotkey=%d "
           L"fade=%d hostname=%d sessionInfo=%d reconnect=%d(mode=%d) "
           L"stuck=%d(threshold=%ds)",
           (int)g_hideBar, (int)g_showOverlay,
           (int)g_enableHotkey, (int)g_fadeWhenIdle, (int)g_showHostname,
           (int)g_showSessionInfo,
           (int)g_enableReconnect, g_reconnectMode,
           (int)g_stuckDetection, g_stuckThresholdSec);
}

void ModUninit() {
    StopWatchdogThread();
    StopRelayThread();
    StopLocalWidgetThread();
    StopHelperThread();

    // No writer any more: remove the snapshot so the widget sees "no
    // session" immediately instead of after the staleness window.
    DeleteLocalWidgetStatusFile();

    // A force-reconnect one-shot thread, if any, either returns within its
    // grace period or terminates the process; give it that long so no
    // detached thread is still running this DLL's code at unload.
    for (int i = 0; i < 70 && g_oneShotThreads.load() > 0; i++)
        Sleep(50);

    if (g_hWatchdogThreadReady) {
        CloseHandle(g_hWatchdogThreadReady);
        g_hWatchdogThreadReady = nullptr;
    }
    if (g_hRelayThreadReady) {
        CloseHandle(g_hRelayThreadReady);
        g_hRelayThreadReady = nullptr;
    }
    if (g_hLocalWidgetThreadReady) {
        CloseHandle(g_hLocalWidgetThreadReady);
        g_hLocalWidgetThreadReady = nullptr;
    }
    if (g_hHelperThreadReady) {
        CloseHandle(g_hHelperThreadReady);
        g_hHelperThreadReady = nullptr;
    }

    EnterCriticalSection(&g_cs);
    HWND    hBBar     = g_hBBar;
    WNDPROC origProc  = g_origBBarWndProc;
    HWND    hFrame    = g_hRdpFrame;
    WNDPROC origFrame = g_origFrameWndProc;
    LeaveCriticalSection(&g_cs);

    // The RDP control's event sink must be released on the frame's own
    // thread — the connection point is an STA object advised there, so
    // Unadvise/Release has to happen where it was created (UnadviseRdpEvents
    // refuses to run anywhere else). Ask the still-installed subclass
    // synchronously, then restore the original wndproc. A hung frame thread
    // only costs the timeout: better a leaked reference at unload than a
    // blocked unload or a cross-apartment Release. This is the mod-unload
    // half of the two UnadviseRdpEvents paths; the frame's own WM_DESTROY is
    // the other, and both survived the thumbnail toolbar's removal (D-33).
    if (hFrame && origFrame && IsWindow(hFrame)) {
        DWORD_PTR result = 0;
        if (g_msgSinkTeardown)
            SendMessageTimeoutW(hFrame, g_msgSinkTeardown, 0, 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &result);
        SetWindowLongPtrW(hFrame, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(origFrame));
        EnterCriticalSection(&g_cs);
        g_origFrameWndProc = nullptr;
        LeaveCriticalSection(&g_cs);
    }

    if (hBBar && origProc && IsWindow(hBBar))
        SetWindowLongPtrW(hBBar, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(origProc));

    if (hBBar && IsWindow(hBBar))
        pOrigShowWindow(hBBar, SW_SHOWNOACTIVATE);

    DeleteCriticalSection(&g_cs);
}

} // namespace client

// ── explorer.exe branch ────────────────────────────────────────────────────
// The taskbar-embedded status panel. Folded in from the former standalone
// rdp-session-toolkit-taskbar-client-embedded mod (v0.2.0, DECISIONS.md
// D-21..D-25); runs only when this mod is injected into explorer.exe. This is a
// structural move — the behavior is preserved exactly as it was built — with
// one deliberate simplification (D-26): the status file is now located directly
// via Wh_GetModStoragePath (per mod id, identical in both this process and the
// mstsc.exe writer's) instead of guessing at a sibling storage directory name.
namespace embedded {

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;

// ── Settings ──────────────────────────────────────────────────────────────

enum class WidgetPosition { Right, Left, Center };

struct ModSettings {
    int  panelWidth = 340;
    int  fontSize = 11;
    int  offsetX = 8;
    bool showWhenNoSession = true;
    // Same mod id as the mstsc branch, so the one enableReconnect setting
    // (D-28) is readable here too: the panel hides its Reconnect button when
    // the receiver would refuse the command anyway.
    bool reconnectEnabled = false;
    // Three more settings the mstsc branch used to own, read here since
    // v0.9.0 for exactly the same reason (D-33): the taskbar-embedded panel
    // is now the only surface that presents any of them, so the branch that
    // draws them is the branch that reads them.
    //   showSessionInfo       -> the duration / idle lines of the status
    //                            element's tooltip
    //   showConnectionQuality -> the quality / bandwidth / rtt line of it
    //   showFullscreenToggle  -> whether the fullscreen button is on the row
    // The mstsc branch keeps reading showSessionInfo for its own floating
    // overlay row; it is simply read in both places.
    bool showSessionInfo = true;
    bool showConnectionQuality = true;
    bool fullscreenToggleEnabled = true;
    WidgetPosition widgetPosition = WidgetPosition::Right;
} g_Settings;

void LoadSettings() {
    g_Settings.panelWidth        = Wh_GetIntSetting(L"embeddedPanelWidth");
    g_Settings.fontSize          = Wh_GetIntSetting(L"embeddedFontSize");
    g_Settings.offsetX           = Wh_GetIntSetting(L"embeddedOffsetX");
    g_Settings.showWhenNoSession = Wh_GetIntSetting(L"embeddedShowWhenNoSession") != 0;
    g_Settings.reconnectEnabled  = Wh_GetIntSetting(L"enableReconnect") != 0;
    g_Settings.showSessionInfo   = Wh_GetIntSetting(L"showSessionInfo") != 0;
    g_Settings.showConnectionQuality =
        Wh_GetIntSetting(L"showConnectionQuality") != 0;
    g_Settings.fullscreenToggleEnabled =
        Wh_GetIntSetting(L"showFullscreenToggle") != 0;
    if (g_Settings.panelWidth <= 0) g_Settings.panelWidth = 340;
    if (g_Settings.fontSize  <= 0) g_Settings.fontSize = 11;
    if (g_Settings.offsetX   <  0) g_Settings.offsetX = 8;
    PCWSTR pos = Wh_GetStringSetting(L"embeddedWidgetPosition");
    if (wcscmp(pos, L"Left") == 0)
        g_Settings.widgetPosition = WidgetPosition::Left;
    else if (wcscmp(pos, L"Center") == 0)
        g_Settings.widgetPosition = WidgetPosition::Center;
    else
        g_Settings.widgetPosition = WidgetPosition::Right;
    Wh_FreeStringSetting(pos);
}

// ── Contracts shared with the mstsc.exe branch ────────────────────────────
//
// The status struct + its magic/version/filename, the command-channel window
// class + registry location + payload + command values, the shared-secret
// helpers (ReadLocalWidgetSecret / EnsureLocalWidgetSecret), the presentation
// helpers (QualityLabel / FormatClock), and GetLocalWidgetStatusFilePath are
// all single definitions above the branch namespaces now (TASK 4) — no longer
// duplicated here. Only STATUS_STALE_MS is specific to this reader.
//
// D-26 simplification: GetLocalWidgetStatusFilePath uses Wh_GetModStoragePath
// directly. Because the mstsc branch and this branch are the same mod id, that
// resolves the very directory the mstsc branch writes its snapshot to — the
// former sibling-directory name-guessing (ResolveModsWritableDir /
// kClientModStorageNames / the local@ prefix dance) is gone entirely.
constexpr ULONGLONG STATUS_STALE_MS = 4000;  // snapshot older than this = no session

// Reads the status file. True only for a complete record with the right magic
// and version; everything else is "no snapshot".
bool ReadStatusFile(const std::wstring& path, LocalWidgetStatus* out) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;
    LocalWidgetStatus s = {};
    DWORD read = 0;
    BOOL ok = ReadFile(hFile, &s, sizeof(s), &read, nullptr);
    CloseHandle(hFile);
    if (!ok || read != sizeof(s) || s.magic != kLocalWidgetStatusMagic ||
        s.version != kLocalWidgetStatusVersion)
        return false;
    s.hostname[ARRAYSIZE(s.hostname) - 1] = L'\0';
    *out = s;
    return true;
}

// What the widget shows, derived from the snapshot.
struct WidgetState {
    bool              hasSnapshot = false;  // a valid record was read
    bool              stale       = false;  // …but too old to trust
    bool              active      = false;  // live session (fresh && sessionActive)
    ULONGLONG         ageMs       = 0;      // of the snapshot, when one exists
    bool              receiverFound = false; // the mstsc branch's command window exists
    LocalWidgetStatus s = {};
};

std::mutex  g_StateMutex;
WidgetState g_State;

// A writeTick in the future (a file left over from a previous boot, when
// GetTickCount64 restarted) is treated as stale too.
WidgetState ReadWidgetState() {
    WidgetState st;
    wchar_t path[MAX_PATH + 40];
    if (GetLocalWidgetStatusFilePath(path, ARRAYSIZE(path))) {
        ULONGLONG now = GetTickCount64();
        LocalWidgetStatus s;
        if (ReadStatusFile(path, &s)) {
            st.hasSnapshot = true;
            st.s = s;
            st.stale  = st.s.writeTick > now || (now - st.s.writeTick) > STATUS_STALE_MS;
            st.ageMs  = st.s.writeTick > now ? 0 : now - st.s.writeTick;
            st.active = !st.stale && st.s.sessionActive != FALSE;
        }
    }
    st.receiverFound =
        FindWindowExW(HWND_MESSAGE, nullptr, LOCAL_WIDGET_CLASS, nullptr) != nullptr;
    return st;
}

// ── Presentation helpers ──────────────────────────────────────────────────
// QualityLabel and FormatClock are shared contracts now — defined once above
// the branch namespaces.

// The rich status tooltip, migrated from the thumbnail toolbar's status icon
// (D-33). The original — FormatStatusTooltip in the mstsc.exe branch — could
// not be called from here: it read that process's own globals (session start
// tick, GetLastInputInfo, the quality atomics, the watchdog flags), none of
// which exist in explorer.exe. Every one of those values, however, is already
// in the status snapshot this branch reads once a second, so the tooltip is
// rebuilt here from the snapshot instead, line for line:
//
//   Session 1h 23m · this PC idle 4m
//   Quality 3/4 (Good) · bandwidth 4096 · rtt 32 ms
//   NOT RESPONDING for 14 s — …
//
// Same gating as the original (showSessionInfo / showConnectionQuality; the
// hung line needs no stuckDetection check because the writer only ever sets
// `hung` while that setting is on), same "say so rather than invent a value"
// discipline for quality, and the same fallback line when nothing applies.
// The one addition the original had no need for: this surface is alive while
// no session is, so a missing or stale snapshot gets an honest line of its
// own rather than an empty tooltip.
void FormatEmbeddedStatusTooltip(const WidgetState& st, wchar_t* out, size_t cch) {
    out[0] = L'\0';

    if (!st.active) {
        if (st.hasSnapshot && st.stale)
            swprintf_s(out, cch,
                L"No Remote Desktop session.\nThe last status update from the "
                L"client was %llu s ago, so it is no longer trusted.",
                st.ageMs / 1000);
        else if (st.hasSnapshot)
            wcscpy_s(out, cch, L"No Remote Desktop session is open.");
        else if (st.receiverFound)
            wcscpy_s(out, cch,
                L"No Remote Desktop session. The client mod is running in "
                L"mstsc.exe but has not written a status snapshot yet.");
        else
            wcscpy_s(out, cch,
                L"No Remote Desktop session, and no mstsc.exe running this "
                L"mod to report one.");
        return;
    }

    if (g_Settings.showSessionInfo) {
        wchar_t dur[32], idle[32];
        FormatCoarse(st.s.sessionDurationMs, dur, ARRAYSIZE(dur));
        FormatCoarse(st.s.localIdleMs, idle, ARRAYSIZE(idle));
        swprintf_s(out, cch, L"Session %s \xB7 this PC idle %s", dur, idle);
    }

    if (g_Settings.showConnectionQuality) {
        wchar_t line[160];
        int q = st.s.quality;
        if (!st.s.qualityAvailable)
            wcscpy_s(line, L"Quality: unavailable \x2014 the client mod did not "
                           L"hook the RDP control (reopen the session)");
        else if (q < 1 || q > 4)
            wcscpy_s(line, L"Quality: waiting for Remote Desktop's first report");
        else
            swprintf_s(line, ARRAYSIZE(line),
                L"Quality %d/4 (%s) \xB7 bandwidth %ld \xB7 rtt %ld ms",
                q, QualityLabel(q), st.s.bandwidth, st.s.rtt);
        if (out[0]) wcscat_s(out, cch, L"\n");
        wcscat_s(out, cch, line);
    }

    if (st.s.hung) {
        wchar_t line[160];
        if (g_Settings.reconnectEnabled)
            swprintf_s(line, ARRAYSIZE(line),
                L"NOT RESPONDING for %d s \x2014 use the alert on the session's "
                L"own screen to force reconnect", st.s.hungSeconds);
        else
            swprintf_s(line, ARRAYSIZE(line),
                L"NOT RESPONDING for %d s", st.s.hungSeconds);
        if (out[0]) wcscat_s(out, cch, L"\n");
        wcscat_s(out, cch, line);
    }

    if (!out[0])
        wcscpy_s(out, cch, L"RDP Session Toolkit");
}

PCWSTR LabelForCommand(BYTE cmd) {
    switch (cmd) {
    case LWCMD_MINIMIZE:          return L"Minimize";
    case LWCMD_RESTORE:           return L"Restore";
    case LWCMD_FULLSCREEN_TOGGLE: return L"Fullscreen toggle";
    case LWCMD_RECONNECT:         return L"Reconnect";
    case LWCMD_DISCONNECT:        return L"Disconnect";
    default:                      return L"Unknown";
    }
}

// Single source of truth for whether an action element is available, derived
// from the same status-file state — used both to enable/disable/relabel the
// always-visible elements and to re-check a click before it is sent. The
// one-function-decides-every-slot discipline is inherited from the thumbnail
// toolbar's ComputeThumbButtons (DECISIONS.md D-19); that surface is gone
// (D-33), the discipline stays.
bool CommandEnabledForState(const WidgetState& st, BYTE cmd) {
    bool a = st.active, iconic = a && st.s.iconic;
    switch (cmd) {
    case LWCMD_MINIMIZE:          return a && !iconic;
    case LWCMD_RESTORE:           return a && iconic;
    case LWCMD_RECONNECT:         return a && g_Settings.reconnectEnabled;  // D-28
    case LWCMD_FULLSCREEN_TOGGLE:
    case LWCMD_DISCONNECT:        return a;
    default:                      return false;
    }
}

// ── Command channel: widget → mstsc branch ─────────────────────────────────
//
// One WM_COPYDATA per click, carrying the fixed-layout payload with the shared
// secret read fresh from HKCU at send time. SendMessageTimeoutW, not a plain
// SendMessageW, so a hung mstsc can never wedge the sender. Runs on the status
// thread, never the XAML UI thread.

bool SendLocalWidgetCommand(HWND hSenderWnd, BYTE cmd, PCWSTR label) {
    HWND hTarget = FindWindowExW(HWND_MESSAGE, nullptr, LOCAL_WIDGET_CLASS, nullptr);
    if (!hTarget) {
        Wh_Log(L"Command %s: no %s window — is the client mod loaded in mstsc?",
            label, LOCAL_WIDGET_CLASS);
        return false;
    }
    LocalWidgetCommandPayload p = {};
    p.magic   = kLocalWidgetCmdMagic;
    p.version = kLocalWidgetCmdVersion;
    p.command = cmd;
    if (!ReadLocalWidgetSecret(p.secret)) {
        Wh_Log(L"Command %s: no shared secret in HKCU\\%s — not sent", label,
            LOCAL_WIDGET_REG_KEY);
        return false;
    }
    COPYDATASTRUCT cds = {};
    cds.dwData = kLocalWidgetCmdMagic;
    cds.cbData = sizeof(p);
    cds.lpData = &p;
    DWORD_PTR result = 0;
    LRESULT ok = SendMessageTimeoutW(hTarget, WM_COPYDATA,
        reinterpret_cast<WPARAM>(hSenderWnd), reinterpret_cast<LPARAM>(&cds),
        SMTO_ABORTIFHUNG | SMTO_BLOCK, 2000, &result);
    WipeSecret(p.secret, sizeof(p.secret));
    if (!ok) {
        Wh_Log(L"Command %s: SendMessageTimeoutW failed GLE=%d (target %p)",
            label, GetLastError(), hTarget);
        return false;
    }
    Wh_Log(L"Command %s (0x%02X) → %p, handled=%d", label, cmd, hTarget, (int)result);
    return result != 0;
}

// ── Status-thread-owned window handle ─────────────────────────────────────
// The hidden polling window (created in StatusThread, below) is also the
// sender identity passed to SendLocalWidgetCommand: WM_COPYDATA's wParam is
// this process's own window, and the receiver logs the sender pid from it.
HWND g_hStatusWnd = nullptr;

// ── Status thread ─────────────────────────────────────────────────────────
//
// Owns a hidden window whose 1 s timer polls the mstsc branch's snapshot and
// pushes it into the always-visible XAML elements. Outgoing commands arrive
// here as thread messages from the XAML click handlers (WM_EMBED_SEND_COMMAND)
// so the WM_COPYDATA send always runs off the XAML UI thread. Queue-first
// readiness handshake, so StopStatusThread can never post WM_QUIT before the
// thread can receive it.

constexpr auto     STATUS_CLASS          = L"WH_RdpstkEmbedStatus";
constexpr UINT_PTR STATUS_TIMER_ID       = 1;
constexpr UINT     STATUS_POLL_MS        = 1000;
constexpr UINT     WM_EMBED_SEND_COMMAND = WM_APP + 1;   // wParam = LocalWidgetCommand byte

HANDLE             g_hStatusThread      = nullptr;
std::atomic<DWORD> g_statusThreadId     { 0 };
HANDLE             g_hStatusThreadReady = nullptr;

void RefreshWidgetUI();  // XAML side, below

// Poll: read the snapshot and push it into the widget's elements every tick —
// the status text and button states are always on screen, and the duration
// column needs a live per-second tick regardless of whether anything changed.
void PollStatus() {
    WidgetState fresh = ReadWidgetState();
    {
        std::lock_guard<std::mutex> g(g_StateMutex);
        const WidgetState& old = g_State;
        if (fresh.active != old.active || fresh.stale != old.stale)
            Wh_Log(L"Status: %s (snapshot=%d stale=%d age=%llums host=%s)",
                fresh.active ? L"session ACTIVE" : L"no session",
                (int)fresh.hasSnapshot, (int)fresh.stale, fresh.ageMs,
                fresh.s.hostname);
        g_State = fresh;
    }
    RefreshWidgetUI();
}

LRESULT CALLBACK StatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_TIMER:
        if (wParam == STATUS_TIMER_ID) {
            PollStatus();
            return 0;
        }
        break;

    case WM_DESTROY:
        KillTimer(hwnd, STATUS_TIMER_ID);
        g_hStatusWnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Fullscreen toggle only (DECISIONS.md D-27): hand the mstsc branch the right
// to take the foreground before the command is sent. The mstsc side must
// bring its frame to the foreground to deliver the Ctrl+Alt+Break chord
// (ToggleFullscreen), and SetForegroundWindow only succeeds for a process
// that currently holds the foreground right. The click on the panel is input
// delivered to THIS process (explorer.exe), so at this moment explorer holds
// that right — AllowSetForegroundWindow passes it to mstsc. The grantor must
// hold the right itself, which is why this is done here, per click, and not
// once at init. The target pid is resolved from the receiver window exactly as
// the mstsc-side sender validation resolves pids from a window handle
// (GetWindowThreadProcessId). Minimize / Restore / Disconnect / Reconnect
// need no foreground right and are deliberately not granted one.
void GrantForegroundRightToReceiver(PCWSTR label) {
    HWND hTarget = FindWindowExW(HWND_MESSAGE, nullptr, LOCAL_WIDGET_CLASS, nullptr);
    DWORD pid = 0;
    if (hTarget)
        GetWindowThreadProcessId(hTarget, &pid);
    if (!pid) {
        Wh_Log(L"Command %s: receiver pid unresolved (window %p) — foreground "
               L"right not granted", label, hTarget);
        return;
    }
    if (AllowSetForegroundWindow(pid)) {
        Wh_Log(L"Command %s: AllowSetForegroundWindow(pid=%u) granted", label, pid);
    } else {
        // ERROR_ACCESS_DENIED here means this process does not hold the right
        // right now (the click is no longer the last input event) — the
        // mstsc side then falls back to its AttachThreadInput path.
        Wh_Log(L"Command %s: AllowSetForegroundWindow(pid=%u) FAILED GLE=%d — "
               L"mstsc will have to rely on its own fallback", label, pid,
               GetLastError());
    }
}

// Re-checks the command against the freshest state before sending — the
// click-time re-check discipline established with the thumbnail toolbar
// (DECISIONS.md D-8/D-12/D-19) and kept after it was removed (D-33), so a
// stale XAML IsEnabled can never let a disallowed command through.
void OnSendCommand(BYTE cmd) {
    WidgetState st;
    {
        std::lock_guard<std::mutex> g(g_StateMutex);
        st = g_State;
    }
    if (!CommandEnabledForState(st, cmd)) {
        Wh_Log(L"Command %s ignored — not valid for current state (active=%d iconic=%d)",
            LabelForCommand(cmd), (int)st.active, (int)(st.active && st.s.iconic));
        return;
    }
    // The fullscreen toggle is the one command whose handler on the mstsc
    // side needs the foreground; grant it right before the send (D-27).
    if (cmd == LWCMD_FULLSCREEN_TOGGLE)
        GrantForegroundRightToReceiver(LabelForCommand(cmd));
    // wParam of WM_COPYDATA is the sender's window: the hidden status window
    // (this process) — the receiver logs its pid.
    SendLocalWidgetCommand(g_hStatusWnd, cmd, LabelForCommand(cmd));
}

DWORD WINAPI StatusThread(LPVOID) {
    MSG dummy;
    PeekMessageW(&dummy, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    if (g_hStatusThreadReady)
        SetEvent(g_hStatusThreadReady);

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wcs   = { sizeof(wcs) };
    wcs.lpfnWndProc   = StatusWndProc;
    wcs.hInstance     = hInst;
    wcs.lpszClassName = STATUS_CLASS;
    if (!RegisterClassExW(&wcs)) {
        Wh_Log(L"Status: RegisterClassExW failed GLE=%d", GetLastError());
        return 0;
    }

    g_hStatusWnd = CreateWindowExW(0, STATUS_CLASS, L"", WS_OVERLAPPED,
        0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (!g_hStatusWnd) {
        Wh_Log(L"Status: CreateWindowExW failed GLE=%d", GetLastError());
        UnregisterClassW(STATUS_CLASS, hInst);
        return 0;
    }

    SetTimer(g_hStatusWnd, STATUS_TIMER_ID, STATUS_POLL_MS, nullptr);
    PollStatus();
    Wh_Log(L"Status: thread ready (poll %u ms, stale after %llu ms)",
        STATUS_POLL_MS, STATUS_STALE_MS);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_EMBED_SEND_COMMAND && !msg.hwnd) {
            OnSendCommand((BYTE)(INT_PTR)msg.wParam);
        } else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_hStatusWnd && IsWindow(g_hStatusWnd))
        DestroyWindow(g_hStatusWnd);
    UnregisterClassW(STATUS_CLASS, hInst);
    return 0;
}

void StartStatusThread() {
    if (g_hStatusThread) return;
    if (!g_hStatusThreadReady)
        g_hStatusThreadReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    else
        ResetEvent(g_hStatusThreadReady);
    DWORD threadId = 0;
    g_hStatusThread = CreateThread(nullptr, 0, StatusThread, nullptr, 0, &threadId);
    g_statusThreadId.store(threadId);
}

void StopStatusThread() {
    DWORD threadId = g_statusThreadId.load();
    if (threadId) {
        if (g_hStatusThreadReady)
            WaitForSingleObject(g_hStatusThreadReady, INFINITE);
        PostThreadMessageW(threadId, WM_QUIT, 0, 0);
    }
    if (g_hStatusThread) {
        WaitForSingleObject(g_hStatusThread, 3000);
        CloseHandle(g_hStatusThread);
        g_hStatusThread = nullptr;
        g_statusThreadId.store(0);
    }
}

// ── XAML injection state ──────────────────────────────────────────────────

constexpr std::wstring_view kWidgetRootName   = L"RdpstkEmbedWidgetRoot";
constexpr std::wstring_view kHostNameName     = L"RdpstkEmbedHostName";
constexpr std::wstring_view kStatusLineName   = L"RdpstkEmbedStatusLine";
// The whole host-name + status-line column. Named only so ApplyStateToWidget
// can reach it to refresh the rich status tooltip (D-33) — the tooltip covers
// the column, not just the one line of text, so hovering anywhere over the
// panel's text shows it.
constexpr std::wstring_view kStatusColumnName = L"RdpstkEmbedStatusColumn";
constexpr std::wstring_view kMinimizeName     = L"RdpstkEmbedMinimize";
constexpr std::wstring_view kRestoreName      = L"RdpstkEmbedRestore";
constexpr std::wstring_view kFullscreenName   = L"RdpstkEmbedFullscreen";
constexpr std::wstring_view kReconnectName    = L"RdpstkEmbedReconnect";
constexpr std::wstring_view kDisconnectName   = L"RdpstkEmbedDisconnect";
constexpr std::wstring_view kTaskbarFrameClass  = L"Taskbar.TaskbarFrame";
constexpr std::wstring_view kRootGridName       = L"RootGrid";
constexpr std::wstring_view kSystemTrayGridName = L"SystemTrayFrameGrid";
constexpr std::wstring_view kSystemTrayFrameClass = L"SystemTray.SystemTrayFrame";

std::mutex g_WidgetMutex;
weak_ref<Grid> g_WidgetRoot{ nullptr };
weak_ref<Grid> g_RootGrid{ nullptr };
weak_ref<FrameworkElement> g_SystemTray{ nullptr };
event_token g_TrayResizeToken{};
std::atomic<unsigned> g_MarginSeq{ 0 };
ULONGLONG g_ModInitTick = 0;
std::atomic<bool> g_ScanPending{ false };
std::atomic<bool> g_TaskbarViewDllLoaded{ false };
std::atomic<int> g_HookCallCounter{ 0 };
std::atomic<bool> g_Unloading{ false };
std::thread g_PollForDllThread;
std::thread g_InitialScanThread;

// ── Visual-tree helpers (from the host mod) ───────────────────────────────
std::atomic<int> g_FrameworkElementSlot{ -1 };

bool SlotHasVtablePointer(void* candidate) {
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

    int cached = g_FrameworkElementSlot.load(std::memory_order_relaxed);
    if (cached >= 0) {
        if (auto fe = trySlot(cached)) return fe;
        g_FrameworkElementSlot.store(-1, std::memory_order_relaxed);
    }
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

// The system tray is a sibling of Taskbar.TaskbarFrame under the visual
// root, not a descendant of RootGrid (host mod finding) — search from the
// root or the tray is never found and the widget overlaps the clock.
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

FrameworkElement FindSystemTray(FrameworkElement anchor) {
    auto root = FindVisualRoot(anchor);
    if (!root) return nullptr;
    if (auto frame = FindByClassName<FrameworkElement>(root, kSystemTrayFrameClass)) {
        if (auto grid = FindByName<FrameworkElement>(frame, kSystemTrayGridName)) return grid;
        return frame;
    }
    return FindByName<FrameworkElement>(root, kSystemTrayGridName);
}

SolidColorBrush MakeBrush(uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    return SolidColorBrush(ColorHelper::FromArgb(a, r, g, b));
}

// Attaches (or updates) a wrapping, multi-line tooltip on an element. An
// existing ToolTip's TextBlock is reused rather than replaced, so the
// once-a-second status refresh does not build XAML objects on every tick and
// does not disturb a tooltip the user currently has open. UI thread only.
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

void UpdateWidgetMargin(const wchar_t* reason);

// Lazy tray bind (host mod pattern): the tray subtree may not exist at
// cold-start injection; every caller runs on the UI thread after a layout
// pass, so a tray that appears later is picked up on the next pass.
FrameworkElement BindSystemTray(FrameworkElement anchor) {
    {
        std::lock_guard<std::mutex> g(g_WidgetMutex);
        if (auto tray = g_SystemTray.get()) return tray;
    }
    FrameworkElement tray = FindSystemTray(anchor);
    if (!tray) return nullptr;

    g_TrayResizeToken = {};
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

// Right: tracks the tray width so the panel sits beside the system tray.
// Left/Center: a fixed margin from the respective edge. UI thread only.
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
        widget.Margin(ThicknessHelper::FromLengths(gap, 0, 0, 0));
    }
    Wh_Log(L"[margin] #%u t+%llums reason=%s mode=%d tray=%s trayActualWidth=%.1f "
           L"gap=%.0f -> margin=%.1f",
           seq, tMs, reason, (int)g_Settings.widgetPosition,
           tray ? L"found" : L"MISSING", trayWidth, gap, margin);
}

// Pushes the current status-file state into the status text and the five
// action buttons. CommandEnabledForState is the single source of truth for
// enable state, shared with OnSendCommand's click-time re-check. UI thread only.
void ApplyStateToWidget(Grid widget) {
    if (!widget) return;
    WidgetState st;
    {
        std::lock_guard<std::mutex> g(g_StateMutex);
        st = g_State;
    }
    bool active = st.active, fullscreen = active && st.s.fullscreen;

    if (auto tb = FindByName<TextBlock>(widget, kHostNameName))
        tb.Text(active ? (st.s.hostname[0] ? st.s.hostname : L"Remote Desktop session")
                       : L"No active session");

    if (auto tb = FindByName<TextBlock>(widget, kStatusLineName)) {
        wchar_t line[128];
        if (active) {
            wchar_t dur[32];
            FormatClock(st.s.sessionDurationMs, dur, ARRAYSIZE(dur));
            if (st.s.hung)
                swprintf_s(line, ARRAYSIZE(line), L"\x26A0 not responding (%ds)",
                    st.s.hungSeconds);
            else if (!st.s.qualityAvailable || st.s.quality < 1 || st.s.quality > 4)
                swprintf_s(line, ARRAYSIZE(line), L"%s \xB7 quality n/a", dur);
            else
                swprintf_s(line, ARRAYSIZE(line), L"%s \xB7 %s", dur,
                    QualityLabel(st.s.quality));
        } else {
            wcscpy_s(line, ARRAYSIZE(line), (st.hasSnapshot && st.stale)
                ? L"last seen a while ago" : L"\x2014");
        }
        tb.Text(line);
        tb.Foreground(active && st.s.hung ? MakeBrush(0xFF, 0xFF, 0x60, 0x60)
                                           : MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    }

    // The rich status tooltip the thumbnail toolbar's status icon used to
    // carry (D-33): session duration, this PC's idle time, connection quality
    // with bandwidth and round-trip time, and the not-responding warning —
    // all of it detail the one-line status text above has no room for.
    if (auto col = FindByName<FrameworkElement>(widget, kStatusColumnName)) {
        wchar_t tip[512];
        FormatEmbeddedStatusTooltip(st, tip, ARRAYSIZE(tip));
        SetElementTooltip(col, tip);
    }

    auto setButton = [&](std::wstring_view name, BYTE cmd) {
        auto btn = FindByName<Button>(widget, name);
        if (!btn) return;
        bool enabled = CommandEnabledForState(st, cmd);
        btn.IsEnabled(enabled);
        btn.Opacity(enabled ? 1.0 : 0.35);
    };
    setButton(kMinimizeName,   LWCMD_MINIMIZE);
    setButton(kRestoreName,    LWCMD_RESTORE);
    setButton(kFullscreenName, LWCMD_FULLSCREEN_TOGGLE);
    setButton(kReconnectName,  LWCMD_RECONNECT);
    setButton(kDisconnectName, LWCMD_DISCONNECT);

    // Reconnect is opt-in (D-28): the button is removed from the row, not
    // merely dimmed, while the setting is off — the same treatment the thumb
    // bar gave its slot.
    if (auto btn = FindByName<Button>(widget, kReconnectName))
        btn.Visibility(g_Settings.reconnectEnabled ? Visibility::Visible
                                                   : Visibility::Collapsed);

    // showFullscreenToggle is read here since v0.9.0 (D-33): this panel is
    // the only surface left that offers the toggle, so it is the surface that
    // honors the setting — same removed-from-the-row treatment as Reconnect.
    if (auto btn = FindByName<Button>(widget, kFullscreenName))
        btn.Visibility(g_Settings.fullscreenToggleEnabled ? Visibility::Visible
                                                          : Visibility::Collapsed);

    // Relabels itself from the session's real state (DECISIONS.md D-14/D-19),
    // tooltip included, so the tooltip never describes the wrong direction.
    if (auto btn = FindByName<Button>(widget, kFullscreenName)) {
        wchar_t glyph[2] = { fullscreen ? (wchar_t)0xE73F : (wchar_t)0xE740, 0 };
        btn.Content(box_value(hstring{ glyph }));
        AutomationProperties::SetName(btn,
            fullscreen ? L"Switch to windowed" : L"Switch to fullscreen");
        SetElementTooltip(btn, fullscreen
            ? L"Switches to windowed mode."
            : L"Switches to fullscreen.");
    }

    widget.Visibility((active || g_Settings.showWhenNoSession)
        ? Visibility::Visible : Visibility::Collapsed);
}

// One flat icon button, styled like the host mod's Disconnect/Minimize
// buttons (transparent background, white glyph; IsEnabled/Opacity are set by
// ApplyStateToWidget once the widget is injected). The click handler posts to
// the status thread (WM_EMBED_SEND_COMMAND) rather than calling
// SendLocalWidgetCommand directly, so the WM_COPYDATA send always runs off
// the XAML UI thread (DECISIONS.md D-23) and the command is re-checked
// against live state (OnSendCommand) before it goes out.
Button MakeActionButton(std::wstring_view name, wchar_t glyphChar,
                        PCWSTR accessibleName, PCWSTR tooltip, BYTE cmd,
                        int column) {
    Button btn;
    btn.Name(name);
    wchar_t glyph[2] = { glyphChar, 0 };
    btn.Content(box_value(hstring{ glyph }));
    btn.FontFamily(FontFamily(L"Segoe MDL2 Assets"));
    btn.Background(MakeBrush(0x00, 0, 0, 0));
    btn.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    btn.BorderThickness(ThicknessHelper::FromUniformLength(0));
    btn.Padding(ThicknessHelper::FromLengths(6, 2, 6, 2));
    btn.Margin(ThicknessHelper::FromLengths(8, 0, 0, 0));
    btn.VerticalAlignment(VerticalAlignment::Center);
    btn.IsEnabled(false);  // enabled by ApplyStateToWidget when applicable
    btn.Opacity(0.35);
    AutomationProperties::SetName(btn, accessibleName);
    SetElementTooltip(btn, tooltip);
    btn.Click(RoutedEventHandler(
        [cmd](IInspectable const&, RoutedEventArgs const&) {
            DWORD tid = g_statusThreadId.load();
            if (tid)
                PostThreadMessageW(tid, WM_EMBED_SEND_COMMAND, (WPARAM)cmd, 0);
        }));
    Grid::SetColumn(btn, column);
    return btn;
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

    // System-integrated Acrylic background, matching the host mod's panel.
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
        for (int i = 0; i < 5; i++) {
            ColumnDefinition cBtn;
            cBtn.Width(GridLengthHelper::Auto());
            cols.Append(cBtn);
        }
    }

    // Host name + quality/duration column — same visual role as the host
    // mod's client-name/connect-state column.
    StackPanel textCol;
    textCol.Orientation(Orientation::Vertical);
    textCol.VerticalAlignment(VerticalAlignment::Center);

    TextBlock hostName;
    hostName.Name(kHostNameName);
    hostName.Foreground(MakeBrush(0xFF, 0xFF, 0xFF, 0xFF));
    hostName.FontSize((double)g_Settings.fontSize);
    hostName.TextTrimming(TextTrimming::CharacterEllipsis);
    hostName.TextWrapping(TextWrapping::NoWrap);
    hostName.MaxLines(1);
    textCol.Children().Append(hostName);

    TextBlock statusLine;
    statusLine.Name(kStatusLineName);
    statusLine.Foreground(MakeBrush(0xB3, 0xFF, 0xFF, 0xFF));
    statusLine.FontSize(std::max(8.0, (double)g_Settings.fontSize - 2.0));
    statusLine.TextTrimming(TextTrimming::CharacterEllipsis);
    statusLine.TextWrapping(TextWrapping::NoWrap);
    statusLine.MaxLines(1);
    textCol.Children().Append(statusLine);

    textCol.Name(kStatusColumnName);
    // Seeded here; ApplyStateToWidget replaces the text on every 1 s poll with
    // the live duration / idle / quality / not-responding detail (D-33).
    SetElementTooltip(textCol, L"RDP Session Toolkit");
    Grid::SetColumn(textCol, 0);
    layout.Children().Append(textCol);

    // Five always-visible action buttons — same glyphs, order, and
    // enable/relabel rules the thumbnail toolbar's slots had (DECISIONS.md
    // D-19), sent over the local command channel unchanged (D-23). Each
    // carries a plain-language tooltip saying what the click actually does to
    // the session, rather than restating the button's own name.
    layout.Children().Append(MakeActionButton(kMinimizeName, 0xE921,
        L"Minimize the RDP session",
        L"Minimizes the session to your taskbar. Stays connected.",
        LWCMD_MINIMIZE, 1));
    layout.Children().Append(MakeActionButton(kRestoreName, 0xE923,
        L"Restore the RDP session",
        L"Brings the minimized session back to your screen.",
        LWCMD_RESTORE, 2));
    layout.Children().Append(MakeActionButton(kFullscreenName, 0xE740,
        L"Switch to fullscreen",
        L"Switches between fullscreen and windowed.",
        LWCMD_FULLSCREEN_TOGGLE, 3));
    layout.Children().Append(MakeActionButton(kReconnectName, 0xE72C,
        L"Reconnect",
        L"Disconnects and reconnects to the same session.",
        LWCMD_RECONNECT, 4));
    layout.Children().Append(MakeActionButton(kDisconnectName, 0xE8BB,
        L"Disconnect this RDP session",
        L"Disconnects this session. Programs keep running remotely.",
        LWCMD_DISCONNECT, 5));

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
    UpdateWidgetMargin(L"inject");
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
                {
                    std::lock_guard<std::mutex> lk(g_WidgetMutex);
                    if (g_TrayResizeToken.value) {
                        if (auto tray = g_SystemTray.get())
                            try { tray.SizeChanged(g_TrayResizeToken); } catch (...) {}
                        g_TrayResizeToken = {};
                    }
                    g_SystemTray = nullptr;
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

// ── Hooks (from the host mod) ─────────────────────────────────────────────

using TaskListButton_UpdateVisualStates_t = void(WINAPI*)(void*);
TaskListButton_UpdateVisualStates_t TaskListButton_UpdateVisualStates_Original = nullptr;
void WINAPI TaskListButton_UpdateVisualStates_Hook(void* pThis) {
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

void TriggerInitialScan() {
    g_InitialScanThread = std::thread([]() {
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

// GetModuleHandleW polling instead of a LoadLibraryExW hook: avoids the
// cold-start trampoline race the host mod documents.
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

// ── explorer.exe branch entry points (dispatched from Wh_ModInit below) ──

BOOL ModInit() {
    g_ModInitTick = GetTickCount64();
    LoadSettings();

    // Resolve (and log) the mod storage directory exactly once, here; the 1 s
    // status poll then reads the snapshot from the cached path.
    GetModStorageDir();

    // Shared secret for the command channel: created here unless the mstsc
    // branch already did (D-23).
    EnsureLocalWidgetSecret(L"Wh_ModInit(explorer)");

    // Plain user32/gdi32 thread — safe during Explorer's early boot, and the
    // widget needs current state whenever injection happens.
    StartStatusThread();

    HMODULE taskbarView = GetModuleHandleW(L"Taskbar.View.dll");
    if (!taskbarView) taskbarView = GetModuleHandleW(L"ExplorerExtensions.dll");

    if (taskbarView) {
        g_TaskbarViewDllLoaded = true;
        if (!HookTaskbarViewDllSymbols(taskbarView)) return FALSE;
        TriggerInitialScan();
    } else {
        PollForTaskbarViewDll();
    }

    Wh_Log(L"RDP Session Toolkit Taskbar Client v0.9.1 initialized "
           L"[explorer.exe branch — taskbar-embedded panel] — "
           L"position=%d width=%d showWhenNoSession=%d reconnect=%d "
           L"fsToggle=%d sessionInfo=%d quality=%d",
           (int)g_Settings.widgetPosition, g_Settings.panelWidth,
           (int)g_Settings.showWhenNoSession, (int)g_Settings.reconnectEnabled,
           (int)g_Settings.fullscreenToggleEnabled,
           (int)g_Settings.showSessionInfo,
           (int)g_Settings.showConnectionQuality);
    return TRUE;
}

void ModUninit() {
    g_Unloading = true;
    if (g_PollForDllThread.joinable()) g_PollForDllThread.join();
    if (g_InitialScanThread.joinable()) g_InitialScanThread.join();

    StopStatusThread();   // destroys the polling window
    if (g_hStatusThreadReady) {
        CloseHandle(g_hStatusThreadReady);
        g_hStatusThreadReady = nullptr;
    }

    RemoveWidget();

    // Spin until in-flight hooks finish.
    for (int i = 0; i < 50 && g_HookCallCounter.load() > 0; ++i) Sleep(100);

    Wh_Log(L"RDP Session Toolkit Taskbar Client uninitialized "
           L"[explorer.exe branch]");
}

void ModSettingsChanged() {
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
                if (auto tb = FindByName<TextBlock>(g, kHostNameName))
                    tb.FontSize(fs);
                if (auto tb = FindByName<TextBlock>(g, kStatusLineName))
                    tb.FontSize(std::max(8.0, fs - 2.0));
                ApplyStateToWidget(g);
                UpdateWidgetMargin(L"settings");
            });
    } catch (...) {}

    Wh_Log(L"Settings reloaded [explorer.exe branch] — position=%d width=%d "
           L"showWhenNoSession=%d reconnect=%d fsToggle=%d sessionInfo=%d "
           L"quality=%d",
           (int)g_Settings.widgetPosition, g_Settings.panelWidth,
           (int)g_Settings.showWhenNoSession, (int)g_Settings.reconnectEnabled,
           (int)g_Settings.fullscreenToggleEnabled,
           (int)g_Settings.showSessionInfo,
           (int)g_Settings.showConnectionQuality);
}

} // namespace embedded

} // namespace

// ── Windhawk entry points — dispatch to the branch for this process ─────────

BOOL Wh_ModInit() {
    g_hostProcess = DetectHostProcess();
    switch (g_hostProcess) {
    case HostProcess::Mstsc:
        return client::ModInit();
    case HostProcess::Explorer:
        return embedded::ModInit();
    default:
        Wh_Log(L"RDP Session Toolkit Taskbar Client v0.9.1 loaded in a process "
               L"that is neither mstsc.exe nor explorer.exe — inert no-op");
        return TRUE;
    }
}

void Wh_ModSettingsChanged() {
    switch (g_hostProcess) {
    case HostProcess::Mstsc:    client::ModSettingsChanged();   break;
    case HostProcess::Explorer: embedded::ModSettingsChanged(); break;
    default: break;
    }
}

void Wh_ModUninit() {
    switch (g_hostProcess) {
    case HostProcess::Mstsc:    client::ModUninit();   break;
    case HostProcess::Explorer: embedded::ModUninit(); break;
    default: break;
    }
}
