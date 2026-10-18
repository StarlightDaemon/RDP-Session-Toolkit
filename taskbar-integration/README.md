# Taskbar integration

The Windhawk side of RDP Session Toolkit: a client mod on the connecting PC and
a host mod in the remote session. Both embed panels in the Windows 11 taskbar;
the client mod also has an mstsc.exe branch for session control and observation.

| Mod | Candidate version | Process targets |
|---|---|---|
| [Taskbar Client](client/rdp-session-toolkit-taskbar-client.wh.cpp) | 0.11.1 | Client PC: mstsc.exe and explorer.exe |
| [Taskbar Host](host/rdp-session-toolkit-taskbar-host.wh.cpp) | 0.5.1 | Remote host: explorer.exe |

Use relay **0.3.1** for cross-machine controls. The matched components are in
[Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1).
The [Windows x64 ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
includes the built relay, mod sources, diagnostics and acceptance material.
Windhawk compiles the sources during installation; no relay rebuild is needed.
Follow the [home update, test and rollback guide](../HOME_UPDATE_TEST_ROLLBACK.md)
for backups and both machines' update order. The release also provides the
[standalone guide](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md)
and [asset checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).

This is a manual home-test candidate. Local regression and compile/link checks
passed; real two-machine and Explorer acceptance remains required. The earlier
live-use report is scoped to the preceding baseline in
[D-41](../.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).
The release tag identifies the implementation revision; refreshed assets include
the later guide/download-link revision. The ZIP manifest and
[work log](../.raiden/state/WORK_LOG.md) record exact provenance and checksums.

## Client mod

One mod ID, `rdp-session-toolkit-taskbar-client`, targets two local processes.
The mstsc.exe branch owns connection-bar hiding, the optional overlay, watchdog,
connection-quality sink, status publication and command receivers. The Explorer
branch injects the primary, always-visible taskbar panel and sends actions to
the mstsc branch.

The panel displays the remote host name and client session/window state, with
Minimize, Restore, Fullscreen/windowed, Disconnect and opt-in Reconnect controls.
Hovering the status text shows this PC's keyboard/mouse idle time, an
unresponsive-window warning and quality/bandwidth/round-trip data when mstsc
reports it. Missing quality data stays unknown; local idle time does not measure
remote-session activity.

The floating fullscreen overlay is off by default. Reconnect and Force reconnect
share a separate opt-in setting, also off by default, and never run without a
click. Fullscreen switching uses Remote Desktop's Ctrl+Alt+Break shortcut after
foreground acquisition; uncertain delivery is not treated as success.

The separate embedded-client mod and thumbnail toolbar are retired. The
session-duration clock was removed; idle time remains. Status publication runs
independently of the overlay and watchdog settings.

## Host mod

`rdp-session-toolkit-taskbar-host` runs in remote explorer.exe. WTS supplies the
current session's connecting client name and connection state, refreshed through
session notifications and a periodic safety query.

- **Minimize / Fullscreen toggle for the client:** use the
  [DVC relay](../dvc-plugin/README.md). Correlated replies and periodic queries
  return client state; stale or unavailable state is shown as unknown.
- **Disconnect:** calls WTSDisconnectSession for this session without signing it out.
- **Restart Clipboard:** restarts verified System32 rdpclip.exe processes in
  this session, checking actual termination before replacement. This discards
  shared clipboard contents; feedback can report failure or uncertainty.
- **Clipboard helper not running:** reports absence, without claiming a present
  helper proves that copy/paste is healthy.

Local Disconnect and clipboard recovery retain their RDP-session gates and do
not depend on relay readiness. Host work uses one owned bounded queue.

## Status, commands and feedback

The mstsc branch writes `local-widget-status-v3.dat` in the client mod's Windhawk
storage about once a second. Both client branches share the same mod ID and
storage path. Readers check layout, canonical fields, PID/creation identity and
freshness; records older than four seconds are stale. Whole-record replacement
prevents partial generations; a failed write leaves the old record to expire.

Local panel actions use `CitadelRdpTaskbarLocalWidget` and bounded WM_COPYDATA
sends with a target-bound payload and a shared HKCU secret. The relay uses the
separate `CitadelRdpTaskbarRelay` receiver. Its payload is a complete protocol v3
BoundCommand, not a first command byte with ignored trailing data. The canonical
contracts live in [ToolkitRuntime.h](../dvc-plugin/relay/common/ToolkitRuntime.h),
with the marked block mirrored verbatim in both standalone mod sources.

Commands bind PID and process creation time. Extra mstsc processes in the same
Windows session, including unconnected dialogs, or multiple frames/receivers
pause peer controls. There is no session chooser. Legacy payloads are refused;
matching components are required. Feedback distinguishes accepted requests,
observed state, rejection, busy/unavailable/ambiguous peers, incompatibility and
timeout/unknown outcomes. Unknown actions are never retried automatically.

The registered relay image-path check is advisory: WM_COPYDATA's sender HWND is
caller-controlled. Neither it nor the local shared secret authenticates against
hostile same-user code. The local channel preserves UIPI refusal. See the
[implementation review](../IMPLEMENTATION_REVIEW.md) for security/compatibility
limits and callback teardown behavior. Unresolved cleanup can retain inert code
until process exit; restart that process before re-enabling a retained module.

## Installation and development checks

Use independently installed Windhawk and the home guide. These mods are loaded
through Windhawk's New mod editor rather than its mod catalog. The current
sources target Windows x64 and Windows 11 taskbar XAML; the client requires
Windhawk 1.6 or later. Toolkit setup must not install or upgrade Windhawk (D-42).

From the repository root:

```powershell
./taskbar-integration/compile-check.ps1
./probes/runtime/run-checks.ps1
```

The compile check compiles and links each mod using Windhawk's bundled MinGW
clang and checks the settings block for known structural errors. It does not
load a mod or prove Explorer rendering. The [runtime tests](../probes/runtime/README.md)
exercise isolated contracts and lifetime/clipboard fixtures. In restricted
environments use a fresh ignored workspace directory for process-local TEMP/TMP
before clang. Real dispatcher, unload, input, monitor/DPI/theme and resource
checks belong to the [acceptance matrix](../TOP_THREE_ACCEPTANCE.md) and
LOOP-011/LOOP-002 in the [ledger](../.raiden/state/OPEN_LOOPS.md).

For an optional live receiver-only exercise at home, enable **Debug: send test
minimize via relay** in the client settings. Each off-to-on change attempts one
PID/creation-bound minimize with SendMessageTimeoutW and logs the self-test result.
It refuses ambiguous targets and does not prove DVC activation or delivery. Run
it only when intentionally testing the active RDP window.

The client source retains its fork provenance from
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar).
Architectural history is in [DECISIONS.md](../.raiden/state/DECISIONS.md);
[HANDOFF.md](../HANDOFF.md) maps current source and continuation work.
