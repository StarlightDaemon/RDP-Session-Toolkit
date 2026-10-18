# Production DVC relay

The client-side, out-of-process COM LocalServer that mstsc activates through its
per-user AddIns registration. It receives host-widget requests over the toolkit
Dynamic Virtual Channel (DVC), forwards bound actions to the client Windhawk mod
and returns correlated results and client window state.

## Published candidate

Relay **0.3.1** requires matching client **0.11.1** and host **0.5.1** for controls.
Download the [Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
from [Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1).
It includes the built relay, exact relay source, mod sources, diagnostics,
manifest and acceptance material. No relay rebuild is needed. Follow the
[home update, test and rollback guide](../../HOME_UPDATE_TEST_ROLLBACK.md), also
available as a [standalone asset](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md),
and verify the [release checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).

This is a manual, unsigned prerelease. Local regression and compile/link checks
passed; candidate two-machine and lifecycle acceptance remains required. The
earlier live-use report is scoped to the preceding baseline in
[D-41](../../.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).
The tag identifies the implementation revision; refreshed assets include the
later guide/download-link revision. Exact provenance and asset verification live
in the ZIP manifest and dated [work log](../../.raiden/state/WORK_LOG.md).

## Transport and state

The host mod opens the DVC, sends a complete protocol v3 Request and reads a
correlated Reply before closing. Requests carry a command, version, request ID
and target PID/creation identity; replies carry capabilities, explicit outcomes
and validated client state. The relay forwards a complete BoundCommand through
a bounded WM_COPYDATA send to the client's `CitadelRdpTaskbarRelay` receiver.

| Command value inside a v3 request | Behavior |
|---|---|
| 0x01 | Minimize the bound client window |
| 0x02 | Toggle fullscreen/windowed on the bound client window |
| 0x03 | Query client state locally in the relay; no action forwarded |

These are fields within a strict frame. Legacy one-byte requests receive an
unknown-state legacy reply with no action; malformed v3 frames are rejected.
New hosts use a marker that old components cannot interpret as an action.
There is no fallback to unbound legacy commands.

Actions require a sole mstsc process in this Windows session, a matching
PID/creation-time snapshot and receiver, and an active client session. Extra
mstsc processes, including unconnected dialogs, multiple RDP frames or
conflicting receivers pause peer controls. The relay does not choose a session.

Client state comes from `local-widget-status-v3.dat`, located through
`HKCU\Software\RDPSessionToolkit\ClientModStoragePath`. The relay validates exact
layout, canonical fields, writer identity and freshness. Records older than four
seconds are stale. Missing, stale, invalid or changed-identity state never becomes
a guessed target. Replies distinguish acceptance from observed state and report
unavailable/ambiguous/changed targets, rejection, incompatibility, busy and
timeout/unknown outcomes. Acceptance is not proof that the requested final state
was reached; uncertain actions are never retried automatically.

## Layout

```
relay/
  common/RelayIds.h     production IDs and client snapshot mirror
  common/ToolkitRuntime.h  canonical protocol, targeting and validation helpers
  plugin/               CLIENT-side out-of-process COM LocalServer plugin
    Main.cpp            LocalServer + IClassFactory + /register /unregister
    RdpRelayPlugin.*    IWTSPlugin / IWTSListenerCallback / IWTSVirtualChannelCallback
                        + the WM_COPYDATA forward and the identity window
    RegistryHelper.h    scoped registration/removal and best-effort value rollback
  build.ps1             builds the plugin with the Windhawk-bundled clang
```

There is **no trigger EXE** here (unlike the probe). In production the send side
lives inside the [host-side Windhawk mod](../../taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp),
which opens this channel with WTSVirtualChannelOpenEx and exchanges v3 frames.

## Production identifiers

- DVC channel: `dvc::taskbar::relay`   (probe was `dvc::taskbar::probe`)
- CLSID: `{6FC96481-9467-496E-BA33-A202ED052F39}`
- AddIn name: `RdpSessionToolkitDvcRelay`   (probe was `HideRdpBarTaskbarDvcProbe`)
- Target window class (client mod's receiver): `CitadelRdpTaskbarRelay`

The identifiers live in [RelayIds.h](common/RelayIds.h); D-10 in the
[decision record](../../.raiden/state/DECISIONS.md) records their origin. The channel and receiver
class must match the mods, as must the protocol and snapshot layout. The marked
block in [ToolkitRuntime.h](common/ToolkitRuntime.h) is mirrored verbatim in both
standalone mod files; regression checks enforce equality and snapshot field offsets.

## Sender identity for validation

WM_COPYDATA carries this plugin's own hidden message-only window as `wParam`, so
the client mod's receiver can resolve this process's id and confirm its image
path matches `ServerExecutable` under
`HKCU\Software\Classes\CLSID\{6FC96481-…}\LocalServer32`. The current client also
accepts supported legacy defaults when this value is absent. This is a path
guard, not proof against a hostile process supplying another window's HWND.
The local panel secret and user-writable snapshots do not isolate hostile
same-user code either. See D-45 in the [decision record](../../.raiden/state/DECISIONS.md)
and the [implementation review](../../IMPLEMENTATION_REVIEW.md).

## Lifetime and logging

Plugin/channel objects and factory locks use COM's server-process reference
functions. The bootstrap reference keeps the server available until activation
or the startup timeout; final release allows shutdown. The main server thread
owns the identity window. Channel state locks are released before external Write
calls so reentrant close/write does not deadlock on them.

Logs are capped at 1 MiB current plus 1 MiB backup under
`%LOCALAPPDATA%\RDPSessionToolkit`; routine polling is silent. The previous
executable-adjacent log is left untouched. These source changes require real
activation, reconnect and process-exit acceptance in addition to isolated tests.

## Build and registration

For a source build, run from the repository root:

```powershell
./dvc-plugin/relay/build.ps1
```

The default build produces an x64 EXE under ignored `dvc-plugin/relay/bin/`.
It uses Windhawk's bundled MinGW clang, plain Win32 COM and static linking; the
mods use the same compiler with C++/WinRT for their UI. Building does not register
or start the relay. In restricted environments set process-local TEMP/TMP to a
fresh ignored workspace directory before clang. Toolkit setup must reuse
independently installed Windhawk (D-42).

Use the home guide for registration at the existing registered path, with both
machines backed up and sessions closed as directed. Do not register a second
extracted/build copy. CLI modes in [Main.cpp](plugin/Main.cpp) are:

| Mode | Effect |
|---|---|
| /help | Print usage without running the server or changing registration |
| No arguments or -Embedding | Run the COM server; not a build/health check |
| /register | Write owned per-user HKCU registration values |
| /unregister | Remove owned HKCU values only |
| /unregister /machine | Explicit owned legacy HKLM cleanup; requires appropriate access |

Registration writes the AddIn `Name`, quoted `LocalServer32`
default, and unquoted `ServerExecutable`. `/unregister` removes only owned
values in HKCU; it retains empty keys and unrelated data. Conflicting paths,
identities, or malformed values stop before mutation. Failures restore earlier
values where possible and report incomplete rollback separately. This is not
a crash-atomic transaction; serialize deployment operations.

`/register /machine` is unsupported because the client resolves relay identity in
HKCU. `/unregister /machine` is explicit legacy cleanup only. Exit codes are
0 success, 1 operation failure, 2 invalid arguments, and 3 incomplete rollback.
An executable at a different path cannot silently overwrite or remove the
existing registration. This is not an installer or a complete deployment
transaction. Do not double-click the EXE: without arguments it starts a COM server.

## Verification

From the repository root:

```powershell
./probes/runtime/run-checks.ps1
```

The [runtime checks](../../probes/runtime/README.md) exercise contracts, actual
relay lifetime code, reentrant close/write and bounded logs without COM
registration/activation or commands to a live receiver. Use the
[packaging probes](../../probes/packaging/README.md) for registration/path fixtures.
Real DVC activation, mixed versions, target ambiguity, relay disappearance and
process exit belong to the [acceptance matrix](../../TOP_THREE_ACCEPTANCE.md)
and LOOP-011/LOOP-002 in [OPEN_LOOPS.md](../../.raiden/state/OPEN_LOOPS.md).
Deployment gates are tracked through LOOP-010 there.

## Provenance

Adapted from the verified [probe](../probe/), which records its Microsoft
Simple/cpp sample origins. The [probe test record](../TESTING.md) documents the
historical activation/signal proof; its registration was removed afterward.
Production uses distinct identifiers, bound controls, explicit replies, scoped
registration and COM reference-based shutdown. The probe remains reference
material and is not part of current deployment.
