# dvc-plugin / relay

The **production** DVC relay plugin — the real component the throwaway
[`probe/`](../probe/) existed to de-risk. It is a client-side, out-of-process
COM `LocalServer32` plugin that `mstsc` activates from the per-user `AddIns`
registry (no admin). On receiving a Dynamic Virtual Channel signal from the
host-side Windhawk mod, it relays the payload's **first byte, as-is** to the
client-side mod's `CitadelRdpTaskbarRelay` message-only window via
`WM_COPYDATA`. It performs no protocol logic of its own — a pure pass-through
from the DVC channel to the existing window-message contract.

> The probe is kept in this repo unchanged as historical reference documenting
> how the underlying activation + DVC-delivery mechanism was verified
> (`dvc-plugin/TESTING.md`, VERIFIED 2026-08-21). The probe registration was
> already removed; this plugin is a new, independent registration with its own
> identity.

## Layout

```
relay/
  common/RelayIds.h     shared IDs (channel name, CLSID, AddIn name, target window class, log name)
  plugin/               CLIENT-side out-of-process COM LocalServer plugin
    Main.cpp            LocalServer + IClassFactory + /register /unregister
    RdpRelayPlugin.*    IWTSPlugin / IWTSListenerCallback / IWTSVirtualChannelCallback
                        + the WM_COPYDATA forward and the identity window
    RegistryHelper.h    HKCU {CLSID}+LocalServer32 registration (no admin)
  build.ps1             builds the plugin with the Windhawk-bundled clang
```

There is **no trigger EXE** here (unlike the probe). In production the send side
lives inside the host-side Windhawk mod
([`taskbar-integration/host`](../../taskbar-integration/host)), which opens this
channel with `WTSVirtualChannelOpenEx` and writes the command byte.

## Identifiers (distinct from the probe — see `DECISIONS.md` D-10)

- DVC channel: `dvc::taskbar::relay`   (probe was `dvc::taskbar::probe`)
- CLSID: `{6FC96481-9467-496E-BA33-A202ED052F39}`  (freshly minted; distinct from the probe's)
- AddIn name: `RdpSessionToolkitDvcRelay`   (probe was `HideRdpBarTaskbarDvcProbe`)
- Target window class (client mod's receiver): `CitadelRdpTaskbarRelay`

The channel name and target window class are the two contracts this plugin
shares with the other components: the channel name must match the host mod's
`WTSVirtualChannelOpenEx` call, and the target window class must match the
client mod's relay receiver.

## Sender identity for validation

WM_COPYDATA carries this plugin's own hidden message-only window as `wParam`, so
the client mod's receiver can resolve this process's id and confirm its image
path is exactly the EXE registered under
`HKCU\Software\Classes\CLSID\{6FC96481-…}\LocalServer32`. Messages from any other
sender are rejected (log-and-ignore). See `DECISIONS.md` D-11.

## Register / build

```powershell
cd dvc-plugin\relay
.\build.ps1                          # -> .\bin\dvc-relay-plugin.exe
.\bin\dvc-relay-plugin.exe /register # per-user (HKCU), no admin
```

Registration writes exactly the same two per-user entries the probe did, under
this plugin's own AddIn name / CLSID. `/unregister` removes them. Restart
`mstsc` after registering (the `AddIns` registry is read when the client starts
a connection).

## Provenance

Adapted directly from the verified probe (`../probe/`), which was itself adapted
from [`microsoft/rdp-dvc-plugin-samples`](https://github.com/microsoft/rdp-dvc-plugin-samples)
(`Simple/cpp`). The COM/DVC lifecycle is unchanged from the probe; the only
behavioral change is the observable action (forward a byte via `WM_COPYDATA`
instead of writing a log line). Built and confirmed to compile+link cleanly with
**Windhawk's bundled clang** (`-static`, plain Win32 COM).
