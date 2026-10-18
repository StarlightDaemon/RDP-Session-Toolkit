# DVC relay

The client-side Dynamic Virtual Channel (DVC) relay connects host-widget peer
controls to the client Windhawk mod and returns client window state. It is an
out-of-process COM LocalServer activated by mstsc.exe, with its own executable
and registration. The host sender lives in the host mod, not a separate trigger.

## Published candidate

Relay **0.3.1** pairs with client **0.11.1** and host **0.5.1** in
[Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1).
Download the [Windows x64 ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
for the built relay, exact relay source, mod sources, diagnostics, manifest and
acceptance material. No relay rebuild is needed. Start with the
[home update, test and rollback guide](../HOME_UPDATE_TEST_ROLLBACK.md), also
available as a [standalone asset](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md),
and verify the [release checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).

This is a manual, unsigned prerelease. Local regression and build checks passed;
real two-machine and lifecycle acceptance is still required. Earlier live tests
cover the preceding baseline, scoped in
[D-41](../.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).
The tag identifies the implementation revision; refreshed assets include the
later guide/download-link revision. Exact revisions and asset verification are
recorded in the ZIP manifest and [work log](../.raiden/state/WORK_LOG.md).

## Components and source

- [relay/](relay/): production relay implementation.
- [plugin/Main.cpp](relay/plugin/Main.cpp): COM server lifetime, CLI and registration.
- [plugin/RdpRelayPlugin.cpp](relay/plugin/RdpRelayPlugin.cpp): DVC callbacks,
  forwarding and state replies.
- [common/RelayIds.h](relay/common/RelayIds.h): production identifiers and snapshot mirror.
- [common/ToolkitRuntime.h](relay/common/ToolkitRuntime.h): canonical protocol,
  target selection, validation and shared worker helpers. Its marked block is
  mirrored verbatim in both standalone Windhawk mods.
- [probe/](probe/): retired activation experiment. The successful
  [two-machine test](TESTING.md) is historical evidence; do not re-register the
  probe for the candidate.

## Control and state path

The path is **remote host widget → DVC → client relay → mstsc mod receiver**,
with a correlated reply read before the host closes the channel. Protocol v3
requests carry correlation and target identity; replies carry capabilities and
explicit results. Command values
identify Minimize, Fullscreen toggle and a no-action status query inside that
frame; a raw legacy action byte is not a supported current request.

Actions bind the target PID and process creation time, checked by sender and
receiver. Extra mstsc processes in the same Windows session, including connection
dialogs, multiple frames or conflicting receivers pause peer controls. The relay
does not choose between sessions. New components refuse legacy actions safely,
without an unbound fallback; use matching components for controls.

The relay finds client storage through the published HKCU ClientModStoragePath
value and reads `local-widget-status-v3.dat`. It validates the complete snapshot,
canonical fields, process identity and freshness before returning state. Missing,
invalid or stale observations remain unknown. Replies distinguish acceptance
from observed state and report unavailable, ambiguous, rejected, incompatible,
busy or timeout/unknown outcomes. Uncertain actions are not retried automatically.

## Lifetime, registration and trust

The relay uses plain Win32 COM and static linking, built with Windhawk's bundled
MinGW clang. It is not a Windhawk mod. Its own address space separates plugin
execution from mstsc, but does not create isolation from hostile same-user code.
The mods use C++/WinRT for their taskbar UI; a separate executable does not
require a different compiler toolchain. Signing is separate distribution work;
the published relay is unsigned.

COM plugin/channel/factory references govern shutdown. Channel handling tolerates
reentrant close/write without holding state locks across external writes. Routine
poll success is silent. Logs are bounded to a current file and backup under
`%LOCALAPPDATA%\RDPSessionToolkit`, rather than growing beside the executable.

Registration is per-user HKCU. It separates the quoted LocalServer32 launch
command from the raw ServerExecutable identity, rejects conflicting ownership,
preserves foreign values and scopes removal/rollback. Default removal does not
fall back to HKLM. New machine registration is unsupported; `/machine` applies
only to explicit legacy unregistration. This is not a full deployment transaction
or crash-recovery installer. Use the home guide's existing registered path and
update order rather than registering a second extracted copy.

The client's registered-image check is advisory: WM_COPYDATA's sender HWND is
caller-supplied. It is not authentication against hostile same-user code. The
local panel secret and user-writable snapshots do not establish that boundary
either. See [IMPLEMENTATION_REVIEW.md](../IMPLEMENTATION_REVIEW.md) for limits.

## Build and verify

From the repository root, for a local source build:

```powershell
./dvc-plugin/relay/build.ps1
./probes/runtime/run-checks.ps1
```

The default relay build produces the x64 executable under ignored
`dvc-plugin/relay/bin/`; it does not register or start it. Do not double-click
the EXE or run it without arguments as a build check: that starts a COM server.
In restricted environments set process-local TEMP/TMP to a fresh ignored workspace
directory before clang. The [candidate builder](../scripts/build-refinement-candidate.ps1)
creates a separate ignored bundle without installation or registration.

Use the [runtime checks](../probes/runtime/README.md) and
[packaging probes](../probes/packaging/README.md) for isolated regression coverage.
Actual DVC activation, process exit, mixed-version behavior and unload/reconnect
need the [acceptance matrix](../TOP_THREE_ACCEPTANCE.md). Coordinate runtime
evidence through LOOP-011/LOOP-002 and deployment gates through LOOP-010 in
[OPEN_LOOPS.md](../.raiden/state/OPEN_LOOPS.md). Toolkit setup must reuse
independently installed Windhawk (D-42).
