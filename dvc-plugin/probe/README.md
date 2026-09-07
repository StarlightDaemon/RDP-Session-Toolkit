# dvc-plugin / probe

A **proof-of-mechanism** DVC probe. Not the real relay plugin — a throwaway that
answers exactly one question: does `mstsc` activate an out-of-process AddIn
plugin and deliver it a Dynamic Virtual Channel signal? Full operator procedure
is in [`../TESTING.md`](../TESTING.md).

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
- CLSID: `{3194520D-DE59-4432-95B5-D5CB4FAFC30E}` (freshly minted for this repo)
- AddIn name: `HideRdpBarTaskbarDvcProbe`
- Client and trigger share `common/ProbeIds.h`, so the channel name / CLSID
  cannot drift between the two sides.

## Provenance — adapted from Microsoft's sample

Starting point:
[`microsoft/rdp-dvc-plugin-samples`](https://github.com/microsoft/rdp-dvc-plugin-samples),
`Simple/cpp` — the minimal client-plugin / server-trigger pair, and the variant
that uses the same **out-of-process COM LocalServer** activation model this
project already decided on.

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

- **Removed the C++/WinRT dependency.** The sample implements its plugin with
  `winrt::implements<>` / `winrt::com_ptr<>` (`#include <winrt/base.h>`). That
  projection is a Visual Studio / Windows-SDK facility and is **not** available
  in the Windhawk-bundled mingw clang toolchain
  (`i686-w64-windows-gnu` / `x86_64-w64-windows-gnu`). The plugin here is
  therefore hand-rolled **plain Win32 COM**: explicit `IUnknown` ref counting
  via `InterlockedIncrement/Decrement`, exactly as the sample's own
  `IClassFactory` already does. The DVC COM interfaces and IIDs come from the
  toolchain's own `tsvirtualchannels.h`.
- **Dropped the ping/echo protocol.** No framing, no reassembler, no RTT, no
  echo-back. The trigger writes a short ASCII string; the plugin logs its
  arrival. The point is proving the pipe, not what travels through it.
- **The observable action is a log line.** On any `OnDataReceived`, the plugin
  appends a timestamped line to `dvc-probe.log` next to the plugin EXE (and
  mirrors every lifecycle event to `OutputDebugString`).

## Toolchain note

Built and confirmed to compile+link cleanly with **Windhawk's bundled clang**
(`C:\Program Files\Windhawk\Compiler\bin\clang++.exe`, clang 20, mingw target) —
the same compiler used successfully in the sibling `Hide RDP Connection Bar`
repo. `-static` is required so the EXEs don't depend on clang's runtime DLLs,
since `mstsc` launches the plugin from its own working directory. This is C++
native COM, so it fits that compiler directly — no separate MSVC/SDK setup.

See [`../TESTING.md`](../TESTING.md) to actually run it.
