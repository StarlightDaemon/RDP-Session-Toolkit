# dvc-plugin / probe

A retired **proof-of-mechanism** DVC probe. The
[historical test record and procedure](../TESTING.md) document the successful
2026-08-21 activation and signal-delivery test; the probe registration was removed
afterward. Its question was whether mstsc activates an out-of-process AddIn and
delivers a DVC signal. It does not implement the production relay or establish
acceptance of later toolkit versions.

## Current toolkit guidance

Use [Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
and its [Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
for the current toolkit. Follow the [home update, test and rollback guide](../../HOME_UPDATE_TEST_ROLLBACK.md),
also available as a [standalone asset](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md),
and verify the [release checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).
The ZIP contains the built relay and mod sources; no relay rebuild is needed.
This is a manual, unsigned prerelease with its own pending home acceptance in
[TOP_THREE_ACCEPTANCE.md](../../TOP_THREE_ACCEPTANCE.md).

The [production relay](../relay/README.md) uses distinct identifiers, protocol v3,
bound controls, explicit replies, scoped registration and COM reference-based
shutdown. Do not register this probe or run its trigger for an RC1 update/test.
The release tag and refreshed asset documentation revisions are recorded through
the manifest and [work log](../../.raiden/state/WORK_LOG.md).

The rest of this document describes the retained experiment. Its unframed ASCII
payloads, executable-adjacent logs and registry helper are historical behavior,
not current relay guidance. In particular, probe unregistration attempts
recursive deletion of its keys in both HKCU and HKLM without ownership checks;
it is not the current relay's removal or rollback mechanism. Any deliberate
replay belongs in a disposable test environment using the archived procedure.

## Layout

```
probe/
  common/ProbeIds.h     shared IDs (channel name, CLSID, AddIn name, log name)
  plugin/               CLIENT-side out-of-process COM LocalServer plugin
    Main.cpp            LocalServer + IClassFactory + /register /unregister
    RdpProbePlugin.*    IWTSPlugin / IWTSListenerCallback / IWTSVirtualChannelCallback
    RegistryHelper.h    HKCU {CLSID}+LocalServer32 registration (no admin)
  trigger/Trigger.cpp   REMOTE-side sender: WTSVirtualChannelOpenEx + Write
  build.ps1             builds both with the Windhawk-bundled clang
```

## Identifiers

- DVC channel: `dvc::taskbar::probe`
- CLSID: `{3194520D-DE59-4432-95B5-D5CB4FAFC30E}`
- AddIn name: `HideRdpBarTaskbarDvcProbe`
- Client and trigger share [common/ProbeIds.h](common/ProbeIds.h) for their
  identifiers; these are separate from the production relay's identifiers.

## Provenance — adapted from Microsoft's sample

Starting point:
[`microsoft/rdp-dvc-plugin-samples`](https://github.com/microsoft/rdp-dvc-plugin-samples),
`Simple/cpp` — the minimal client-plugin / server-trigger pair, and the variant
that uses the **out-of-process COM LocalServer** activation model chosen for
the experiment.

### What was kept

- The COM LocalServer lifecycle: `CoRegisterClassObject` →
  `IClassFactory::CreateInstance` → `IWTSPlugin::Initialize` → `CreateListener`
  → `OnNewChannelConnection` → `OnDataReceived`.
- The registration shape: per-user (`HKCU`) `AddIns\<name>\Name = {CLSID}` plus
  `CLSID\{...}\LocalServer32 = <exe path>`. No admin. (Straight from the
  sample's `RegistryHelper`, which is already plain Win32.)
- The server open/write path: `WTSVirtualChannelOpenEx` with
  `WTS_CHANNEL_OPTION_DYNAMIC`, then `WTSVirtualChannelWrite`.

### What changed, and why

- **Removed the sample's C++/WinRT dependency for the experiment.** The probe
  uses hand-rolled **plain Win32 COM** with explicit IUnknown reference counting
  via InterlockedIncrement/Decrement. Its DVC interfaces and IIDs come from the
  toolchain's tsvirtualchannels.h. This describes the probe's implementation
  choice, not a toolchain-wide absence of C++/WinRT: the current taskbar mods
  use C++/WinRT with Windhawk's bundled MinGW clang.
- **Dropped the ping/echo protocol.** No framing, no reassembler, no RTT, no
  echo-back. The trigger writes a short ASCII string; the plugin logs its
  arrival. The point is proving the pipe, not what travels through it.
- **The observable action is a log line.** On any `OnDataReceived`, the plugin
  appends a timestamped line to `dvc-probe.log` next to the plugin EXE (and
  mirrors every lifecycle event to `OutputDebugString`).

## Toolchain note

The original build was recorded as compiling/linking with **Windhawk's bundled
clang** (`C:\Program Files\Windhawk\Compiler\bin\clang++.exe`, clang 20, MinGW
target). This is historical build evidence, not a claim about today's installed
compiler. `-static` is required so the EXEs don't depend on clang's runtime DLLs,
since `mstsc` launches the plugin from its own working directory. This is C++
native COM, so it fits that compiler directly — no separate MSVC/SDK setup.

See [TESTING.md](../TESTING.md) for the archived procedure and its evidence
boundary. No probe rebuild or live rerun is implied by this documentation review.
