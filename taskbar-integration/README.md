# taskbar-integration

The Windhawk-mod side of the toolkit. Holds two mod source files in sibling
subfolders — one per side of the RDP connection:

- [`client/`](client/) — the client-side mod. As of **v0.6.0** it targets two
  processes on the client machine: `mstsc.exe` (the optional floating overlay,
  the watchdog, and the status/command plumbing) and `explorer.exe` (the
  taskbar-embedded panel, formerly a separate `client-embedded/` mod, folded in
  per D-26). As of **v0.9.0** the panel is the mod's primary surface: the
  mstsc-side taskbar **thumbnail toolbar** was removed as a duplicate of it
  (D-33). Built and live-tested.
- [`host/`](host/) — the host-side (remote) mod, in the host's `explorer.exe` (built and live-tested)

## Contents

1. **Client-side mod** — [`client/rdp-session-toolkit-taskbar-client.wh.cpp`](client/rdp-session-toolkit-taskbar-client.wh.cpp)
   (mod id `rdp-session-toolkit-taskbar-client`). Runs inside `mstsc.exe` on the
   local machine. Ported (forked, no shared git history) from the
   [Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar)
   repo at commit `dc82b10d4c8713d71f5e556649b114a6d43dad04` (v1.1.9), then
   extended with a toolkit relay receiver. It provides:
   - everything the original mod does (connection-bar hiding, overlay
     disconnect button, hostname display, hotkey, drag-to-reposition);
   - the session status the taskbar-embedded panel displays — this PC's input
     idle time and the connection quality Remote Desktop
     reports (a second `IMsTscAxEvents` sink; see D-15/D-16 in
     [`DECISIONS.md`](../.raiden/state/DECISIONS.md)) — published once a second
     into the status snapshot the panel reads. *(Through v0.8.0 this branch
     also drew a taskbar **thumbnail toolbar** carrying the same five actions
     and a status icon. It was removed at v0.9.0 as a second copy of the
     panel's UI; the status icon's rich tooltip was rebuilt on the panel's own
     status text — D-33.)*
   - v0.4.0 session controls: **fullscreen/windowed toggle** (sends Remote
     Desktop's own Ctrl+Alt+Break to the session window, D-14), **reconnect**
     with a settings-backed preferred display mode (`/f`, `/w:`+`/h:`, or
     `/multimon`; clean close then relaunch, D-17), and a **stuck-session
     watchdog** (`IsHungAppWindow`) that shows an on-screen alert with a manual
     Force reconnect — never automatic (D-18);
   - a hidden `HWND_MESSAGE` **relay receiver window** (class
     `CitadelRdpTaskbarRelay`) created at mod load, which the
     [`dvc-plugin`](../dvc-plugin/README.md) relay signals via `WM_COPYDATA`
     (0x01 minimize, 0x02 fullscreen toggle — see the protocol section below).
2. **Host-side mod** — [`host/rdp-session-toolkit-taskbar-host.wh.cpp`](host/rdp-session-toolkit-taskbar-host.wh.cpp)
   (mod id `rdp-session-toolkit-taskbar-host`, own version line starting at
   0.1.0). Runs inside `explorer.exe` on the remote RDP session host. It
   injects a widget directly into the Windows 11 taskbar's own XAML tree
   (`Grid#RootGrid` under `Taskbar.TaskbarFrame` — the technique proven by the
   sibling
   [Native Taskbar Media Controller](https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller)
   mod; see D-7 in [`DECISIONS.md`](../.raiden/state/DECISIONS.md)) showing:
   - the **connecting client's name** (`WTSClientName`) and the session's live
     **connection state** (`WTSConnectState`), refreshed via
     `WM_WTSSESSION_CHANGE` notifications plus a periodic safety-net re-query;
   - a **Disconnect** button that calls `WTSDisconnectSession` against
     `WTS_CURRENT_SERVER_HANDLE` / `WTS_CURRENT_SESSION` — enabled only while
     the session is attached over RDP, dimmed and inert on a console session
     so it can never kick the locally-signed-in user to the lock screen;
   - **Minimize** and **fullscreen/windowed toggle** buttons for the *client's*
     Remote Desktop window, sent over the `dvc::taskbar::relay` DVC as an
     open-write-**read**-close (the read is the relay's status reply — both
     delivery confirmation and the client's session/fullscreen state, D-37/
     D-38), plus a 3 s no-side-effect status poll that keeps the fullscreen
     button labeled with its real direction;
   - a **Restart Clipboard** button and a "clipboard helper not running" line
     (D-35), both RDP-gated like everything else.
3. **Taskbar-embedded panel (the client mod's `explorer.exe` branch)** —
   part of [`client/rdp-session-toolkit-taskbar-client.wh.cpp`](client/rdp-session-toolkit-taskbar-client.wh.cpp),
   not a separate mod.
   The client mod also targets **`explorer.exe`** on the client machine (its
   second `@include`): when injected there, its **explorer.exe branch** injects
   a wide, always-visible RDP status **panel** into the Windows 11 taskbar's own
   XAML tree — `Grid#RootGrid` under `Taskbar.TaskbarFrame`, the same technique
   as the host mod (D-21/D-25). This was previously a third, separate mod
   (`rdp-session-toolkit-taskbar-client-embedded`, last v0.2.0); it was folded
   into this mod at v0.6.0 (**D-26**), since a single Windhawk mod can target
   more than one process. The panel shows the remote host name over a
   state-and-protocol line ("Active · RDP", or an honest "not responding"
   line — the same two-line format as the host widget), and five
   directly-clickable buttons —
   Minimize / Restore / Switch-to-fullscreen-or-windowed / Reconnect /
   Disconnect — each with a plain-language tooltip, and each enabled, disabled
   or relabelled from the session's real state. Hovering the status text shows
   the full detail: this PC's idle time, quality with bandwidth and
   round-trip time, and the not-responding warning (D-33; the session-duration
   clock was removed at v0.10.0, D-36). It
   acts only through the mstsc.exe branch:
   - **Status channel** (D-22): the mstsc.exe branch writes
     `local-widget-status.dat` to the mod's Windhawk storage directory about
     once a second; the explorer.exe branch reads it every second and treats a
     record older than 4 s as "no session". Both branches share one mod id, so
     the reader finds that file directly via `Wh_GetModStoragePath` — no
     sibling-directory guessing (D-26).
   - **Command channel** (D-23): a separate mstsc-branch message window
     `CitadelRdpTaskbarLocalWidget` accepts five `WM_COPYDATA` commands, each
     carrying a 32-byte shared secret stored as
     `HKCU\Software\RDPSessionToolkit\LocalWidgetSecret` (generated with
     `BCryptGenRandom` by whichever branch starts first). Without the exact
     secret a command is ignored. `CitadelRdpTaskbarRelay` is not involved (D-24).

   To use only the taskbar panel, keep the mod enabled and leave *Show floating
   overlay button* off. Since v0.9.2, the always-alive local command window
   publishes status once a second regardless of the overlay or stuck-session
   settings. Since v0.9.0 there is no third surface to turn off.

## Relay receiver protocol

- Window: message-only (`HWND_MESSAGE` parent), class `CitadelRdpTaskbarRelay`,
  created at mod load, destroyed at mod unload. Discover it with
  `FindWindowEx(HWND_MESSAGE, NULL, L"CitadelRdpTaskbarRelay", NULL)`.
- Message: `WM_COPYDATA`. The first payload byte is the command; further bytes
  are reserved for future command-specific arguments and ignored today.
- Commands: `0x01` = minimize the RDP session window, `0x02` = fullscreen /
  windowed toggle — each routed to the same shared action function every other
  surface uses. `0x03` is reserved: it is the host's status query, answered by
  the relay plugin itself and never forwarded here. `0x00` is permanently
  unassigned. More commands are added without changing the window or class
  setup.
- Sender validation (D-11): only this mod's own process (the local self-test)
  or the registered DVC relay plugin's exact `LocalServer32` EXE image path is
  accepted; everything else is logged and ignored.

### Exercising the receiver locally

No DVC pipe or relay plugin is needed:

1. In the Windhawk settings UI for the mod, flip **"Debug: send test minimize
   via relay"** on. The mod sends its own relay window one `WM_COPYDATA`
   minimize command (fires once per off→on flip).
2. With an RDP session open and not minimized, the session window minimizes;
   the Windhawk log shows the `Relay: command 0x01 ...` receipt line.

## Compile check

```powershell
.\compile-check.ps1
```

Compile-checks every `*.wh.cpp` under this folder (both subfolders, `client/`
and `host/`) with the Windhawk-bundled clang (the toolkit's established
toolchain). It also validates each source's `==WindhawkModSettings==` YAML block
(the colon-space landmine of D-20). Windhawk itself compiles and loads the mods
in production; this script only verifies the sources build cleanly.

## Why two mod files (and why the client mod targets two processes)

Windhawk mods are injected per target process. The **client vs. host** split is
real and stays (D-2): those run on different machines and share no code path, so
they are separate mods that communicate over the RDP Dynamic Virtual Channel.
The two **client-side** presentations, by contrast, are two ends of the same
same-machine feature, so they are **one mod with two `@include` targets** — the
client mod is injected into both `mstsc.exe` (optional floating overlay,
watchdog, status writer, command receiver) and the client machine's own
`explorer.exe`
(the taskbar-embedded panel, status reader, command sender). A single Windhawk
mod file *can* target more than one process; `Wh_ModInit` detects which one it
is in and runs only that branch. They coordinate over a local status file plus a
message window (D-22/D-23), and — being one mod id — resolve that file through
the same `Wh_GetModStoragePath` (D-26). The earlier framing that "a single mod
file cannot target more than one process" was mistaken and is corrected by D-26.
