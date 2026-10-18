# dvc-plugin / relay

**0.3.1 session-reliability candidate:** protocol v3 accepts strict framed
requests and returns correlated results and target identity. Actions require
a sole mstsc process in this Windows session and a matching PID/creation-time
snapshot and receiver. Legacy action requests receive unknown legacy state
without executing anything; the matching host reports incompatible peers.
Use client 0.11.1 and host 0.5.1. See
[acceptance and rollback](../../TOP_THREE_ACCEPTANCE.md).

The server now accounts for plugin/channel objects and factory locks through
COM's server-process reference functions and exits after the final release.
Its identity window belongs to the main server thread. Logs are capped at
1 MiB current plus 1 MiB backup under `%LOCALAPPDATA%\RDPSessionToolkit`;
routine polling is silent. The old executable-adjacent log is left untouched.

The following notice and transport description preserve the preceding
registration-refinement candidate's history. Protocol v3's current contracts
are in `common/ToolkitRuntime.h`; its snapshot mirror is in `common/RelayIds.h`.

**0.2.1 refinement candidate:** registration behavior changed; runtime transport
is unchanged. Use client 0.10.1 before writing the new registration format.
The candidate is compiled and regression-tested, awaiting the
[home test](../../HOME_TEST.md).

The **production** DVC relay plugin — the real component the throwaway
[`probe/`](../probe/) existed to de-risk. It is a client-side, out-of-process
COM `LocalServer32` plugin that `mstsc` activates from the per-user `AddIns`
registry (no admin). On receiving a Dynamic Virtual Channel signal from the
host-side Windhawk mod, it relays the payload's **first byte, as-is** to the
client-side mod's `CitadelRdpTaskbarRelay` message-only window via
`WM_COPYDATA` — with one deliberate exception since **v0.2.0** (D-38): the
status-query byte `0x03` is answered locally and never forwarded, so the host
can poll with no side effect on the client. After processing **any** command,
the plugin writes a small `RelayStatusReply` (magic + version + the client's
session/fullscreen state) back on the still-open channel before the host
closes it. The state comes from the client mod's once-a-second status
snapshot, located via the `HKCU\Software\RDPSessionToolkit\ClientModStoragePath`
value the client mod publishes (no directory guessing); a missing, stale, or
version-mismatched snapshot is answered as *state unknown*, never guessed.
The reply is also the host's delivery confirmation — its read leg is what
fixed the Minimize button's dropped-command race (D-37).

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
path matches `ServerExecutable` under
`HKCU\Software\Classes\CLSID\{6FC96481-…}\LocalServer32`. Client 0.10.1 also
accepts supported legacy defaults when this value is absent. This is a path
guard, not proof against a hostile process supplying another window's HWND.
See `DECISIONS.md` D-11 and the packaging research's trust discussion.

## Register / build

```powershell
cd dvc-plugin\relay
.\build.ps1                          # -> .\bin\dvc-relay-plugin.exe
.\bin\dvc-relay-plugin.exe /register # per-user (HKCU), no admin
```

Registration writes three values: the AddIn `Name`, quoted `LocalServer32`
default, and unquoted `ServerExecutable`. `/unregister` removes only owned
values in HKCU; it retains empty keys and unrelated data. Conflicting paths,
identities, or malformed values stop before mutation. Failures restore earlier
values where possible and report incomplete rollback separately. This is not
a crash-atomic transaction; serialize deployment operations.

`/register /machine` is unsupported because the client authenticates against
HKCU. `/unregister /machine` is explicit legacy cleanup only. Exit codes are
0 success, 1 operation failure, 2 invalid arguments, and 3 incomplete rollback.
An executable at a different path cannot silently overwrite or remove the
existing registration. For the refinement candidate, follow the linked home
checklist rather than registering a second copy from a build folder.

## Provenance

Adapted directly from the verified probe (`../probe/`), which was itself adapted
from [`microsoft/rdp-dvc-plugin-samples`](https://github.com/microsoft/rdp-dvc-plugin-samples)
(`Simple/cpp`). The COM/DVC server lifetime still follows the probe; production
adds command forwarding, status replies, and the 0.2.1 registration changes.
Built and confirmed to compile+link cleanly with
**Windhawk's bundled clang** (`-static`, plain Win32 COM).
