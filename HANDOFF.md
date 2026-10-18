# Project Handoff — RDP Session Toolkit

This is a personal-use Windows 11 toolkit with implemented production components
and a published home-test candidate. The probe-only brainstorming handoff is
preserved in Git. Start with [README.md](README.md) for versions/features/downloads
and [CURRENT_STATE.md](.raiden/state/CURRENT_STATE.md) for continuity. Agents must
follow the root [AGENTS.md](AGENTS.md) startup and write boundaries.

## Runtime map

| Source entry point | Responsibility |
|---|---|
| [Client mod](taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp) | One Windhawk mod with mstsc.exe and local explorer.exe branches |
| [Host mod](taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp) | Remote explorer.exe widget, WTS session facts, local recovery and DVC requests |
| [Relay plugin](dvc-plugin/relay/plugin/RdpRelayPlugin.cpp) | Client COM DVC plugin, framing, forwarding and replies |
| [Relay process](dvc-plugin/relay/plugin/Main.cpp) | COM server lifetime, logging, command line and registration |
| [Relay contracts](dvc-plugin/relay/common/RelayIds.h) | Production identifiers and snapshot mirror |
| [Runtime helpers](dvc-plugin/relay/common/ToolkitRuntime.h) | Canonical protocol, targeting, validation and worker helpers |
| [Diagnostics](scripts/Get-ToolkitDiagnostics.ps1) | Read-only local observations and explicit local export |

The mstsc branch owns connection-bar hiding, optional overlay, watchdog, quality
sink, receivers and status publication. Its local Explorer branch injects the
primary taskbar panel. Status updates independently of overlay/watchdog visibility.
There is no separate embedded mod, thumbnail toolbar or duration clock. Reconnect
and Force reconnect share one opt-in setting; the overlay is also off by default.

The host widget gets session state and connecting client name from WTS.
Disconnect and Restart Clipboard are local, RDP-gated actions independent of
relay readiness. Clipboard recovery verifies process image/session and actual
termination; helper presence never means copy/paste health.

## Data and action paths

1. The mstsc branch publishes `local-widget-status-v3.dat` in client mod storage.
   The local panel and relay validate identity, freshness and canonical fields.
   Publication replaces a whole record through an exclusively created temporary
   file; failed replacement leaves the prior generation to expire.
2. Local panel actions use the separate `CitadelRdpTaskbarLocalWidget` message
   window, a shared HKCU secret and bounded WM_COPYDATA sends. The mstsc branch
   validates the bound target and handles the action.
3. Host peer controls open the production DVC, send a framed request and read a
   correlated reply before closing. The client COM relay finds the receiver,
   forwards a target-bound command and returns result/state. Periodic host
   queries also return client window state.

Production identifiers live in RelayIds.h, distinct from the retired probe.
The [probe test record](dvc-plugin/TESTING.md) already documents activation and
signal delivery. Do not re-register the probe for candidate deployment. The
implemented architecture carries controls and client state, not remote-app
state mirrored into the local taskbar.

## Contracts to preserve

- Protocol v3 binds requests to PID and creation time and correlates replies.
  Legacy actions have no fallback. The guard counts all mstsc processes in the
  same Windows session, including unconnected dialogs, and checks frames and
  receivers. Ambiguity pauses peer controls; it does not choose a session.
- Feedback separates accepted requests from observed state. Shortcut delivery,
  disconnect/reconnect acceptance and uncertain clipboard work must not be
  presented as proven final outcomes. Never retry an uncertain action automatically.
- The marked ToolkitRuntime.h block is mirrored verbatim in both standalone
  mods so each stays installable as one file. Update the canonical header and
  mirrors together. Regression checks compare them and compile actual snapshot
  layouts, including field offsets.
- Host jobs use an owned bounded queue; client workers are owned. Taskbar
  delegates/native callbacks need bounded admission and teardown accounting.
  Failed COM unsubscription or unresolved callback cleanup can retain references
  and module code until process exit. If stopped code is retained, restart the
  affected process before enabling again. Live lifecycle acceptance is essential.
- Relay shutdown follows plugin/channel/factory references and must tolerate
  reentrant close/write. Routine polls are silent; logs are bounded under
  `%LOCALAPPDATA%\RDPSessionToolkit`.
- Registration is per-user HKCU, with quoted LocalServer32 command and raw
  ServerExecutable identity. Preserve foreign values, reject conflicting
  ownership and scope rollback/removal. There is no default HKLM fallback; new
  machine registration is unsupported. Explicit machine removal supports legacy
  cleanup. This is not a complete deployment transaction.

These are implementation notes; rationale and supersession belong in D-43
through D-45 in [DECISIONS.md](.raiden/state/DECISIONS.md).

## Security and compatibility

The personal-use same-user trust boundary remains. WM_COPYDATA's sender HWND is
caller-supplied; the image-path check is advisory, not authentication against
hostile same-user code. The local secret and user-writable snapshot do not create
that isolation. Preserve UIPI refusal rather than opening cross-integrity message
delivery. See [IMPLEMENTATION_REVIEW.md](IMPLEMENTATION_REVIEW.md).

Diagnostics omit endpoint names by default, never collect command secrets,
command lines, .rdp contents or clipboard contents, and neither repair nor upload.
Exports require a new explicitly selected file. Source/EXE/receiver presence does
not prove loaded versions or taskbar injection.

The candidate is Windows x64 with Windows 11 taskbar integration. Windhawk's
bundled MinGW clang builds both mods and relay. The mods use C++/WinRT for
taskbar/XAML work; the relay uses plain Win32 COM and static linking. The old
handoff's blanket claim that the toolchain lacks C++/WinRT is obsolete.

## Validation and next work

[Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
provides the built relay, mod sources and guide. Its tag identifies the runtime
implementation; the refreshed asset manifest records the later documentation
revision. Exact revisions/checksums and dated results live in
[WORK_LOG.md](.raiden/state/WORK_LOG.md). The relay is unsigned; this is a manual
candidate, not an installer.

Local regression and compile/link checks passed. They do not exercise Explorer
injection/dispatchers, real two-endpoint DVC activation, foreground input, visuals
or installed resource use. Earlier baseline evidence is scoped by
[D-41](.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).
Use [HOME_UPDATE_TEST_ROLLBACK.md](HOME_UPDATE_TEST_ROLLBACK.md) for operator steps
and [TOP_THREE_ACCEPTANCE.md](TOP_THREE_ACCEPTANCE.md) for the extended matrix.

Continue through LOOP-011/LOOP-002 in [OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md)
for candidate evidence and LOOP-010 for deployment gates. A session chooser and
broader proposals need separate decisions. Follow [ROADMAP.md](ROADMAP.md),
[REFINEMENT_PLAN.md](REFINEMENT_PLAN.md) and [PACKAGING_RESEARCH.md](PACKAGING_RESEARCH.md)
before installer work. D-42 requires independently installed Windhawk.

For source changes, use [runtime checks](probes/runtime/README.md) and
[packaging probes](probes/packaging/README.md). From the repository root:

```powershell
./probes/runtime/run-checks.ps1
./taskbar-integration/compile-check.ps1
./dvc-plugin/relay/build.ps1
```

These commands build/check without installing the runtime. In restricted
environments set process-local TEMP/TMP to a fresh ignored `out/` directory
before clang. The [candidate builder](scripts/build-refinement-candidate.ps1)
creates a separate ignored folder/ZIP; it does not run every regression check.
Do not run the relay without arguments as a build check: it starts a COM server.
Installation, registry changes and live reloads belong to the home procedure,
with backups and an alternate connection path in place.
