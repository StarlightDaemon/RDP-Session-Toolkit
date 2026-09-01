# DVC probe — testing the activation mechanism

> **Status: VERIFIED (2026-08-21).** The procedure below was run end-to-end and
> produced a clean success — `IWTSPlugin::Initialize`, `CreateListener`, and all
> three `*** DVC SIGNAL RECEIVED ***` lines appeared in `dvc-probe.log` on the
> client. The mechanism this probe exists to test is confirmed working; see
> "What this proves" at the bottom. The probe registration was unregistered
> afterward (it's a throwaway test artifact, not the real relay plugin — see
> that section for what still isn't built).

This document is the operator procedure for the **one unproven mechanism** in the
whole taskbar-integration plan:

> Does an out-of-process plugin, registered under the RDP client's `AddIns` key
> in the `{CLSID}` / `LocalServer32` (out-of-process) form, actually get
> **activated by `mstsc`**, and does it actually **receive a Dynamic Virtual
> Channel signal** sent from inside the RDP session?

Everything here lives in [`probe/`](probe/). It is a throwaway proof, not the
real relay plugin. It sends a trivial payload and, on receipt, writes one
timestamped line to a log file. Nothing more. If that line appears, the pipe is
real and the rest of the plan can be built on it.

The probe was adapted from Microsoft's official reference,
[`microsoft/rdp-dvc-plugin-samples`](https://github.com/microsoft/rdp-dvc-plugin-samples)
(`Simple/cpp`), which uses the same out-of-process COM LocalServer model. See
[`probe/README.md`](probe/README.md) for what changed and why.

---

## What you need

- **Two machines** (or one machine + one VM) in an RDP relationship:
  - the **CLIENT** — where you run `mstsc.exe` and where the plugin runs;
  - the **REMOTE / server** — the RDP session host you connect *to*, where the
    trigger runs.
- The probe built for both. It builds with the **Windhawk-bundled clang**
  (`C:\Program Files\Windhawk\Compiler`) — no Visual Studio or Windows SDK
  required. See "Build" below. (This was already confirmed to compile and link
  cleanly with that toolchain.)

No administrator rights are needed anywhere in this procedure. Registration is
**per-user (HKCU)** only. (Verified: `/register` succeeds from a non-elevated
prompt and writes only under `HKEY_CURRENT_USER`.)

---

## Build

On each machine (or build once and copy the matching-arch `bin\` across):

```powershell
cd dvc-plugin\probe
.\build.ps1                # x86_64 by default -> .\bin\
```

Outputs:

- `bin\dvc-probe-plugin.exe`  — the CLIENT-side plugin (also self-registers)
- `bin\dvc-probe-trigger.exe` — the REMOTE-side trigger

The build uses `-static`, so each EXE is standalone — it does **not** depend on
clang's `libc++.dll` / `libunwind.dll`. This matters because `mstsc` launches the
plugin from its own working directory, not from `bin\`.

> The CLIENT needs the plugin EXE. The REMOTE needs the trigger EXE. You can
> copy just the one file each side needs.

---

## Step 1 — Register the plugin (CLIENT, one time)

On the **client** machine, from a normal (non-admin) PowerShell:

```powershell
.\bin\dvc-probe-plugin.exe /register
```

Expected output: `Registration succeeded (HKCU/per-user)`.

This writes exactly two per-user registry entries:

| Key | Value |
|---|---|
| `HKCU\Software\Microsoft\Terminal Server Client\Default\AddIns\HideRdpBarTaskbarDvcProbe` → `Name` | `{3194520D-DE59-4432-95B5-D5CB4FAFC30E}` |
| `HKCU\Software\Classes\CLSID\{3194520D-...}\LocalServer32` → *(default)* | full path to `dvc-probe-plugin.exe` |

You can confirm with:

```powershell
Get-ItemProperty "HKCU:\Software\Microsoft\Terminal Server Client\Default\AddIns\HideRdpBarTaskbarDvcProbe"
Get-ItemProperty "HKCU:\Software\Classes\CLSID\{3194520D-DE59-4432-95B5-D5CB4FAFC30E}\LocalServer32"
```

The **bare `{CLSID}` form** (a GUID string, *not* a `path.dll:{CLSID}` string)
is what makes `mstsc` use normal COM activation. Because the CLSID resolves to a
`LocalServer32` (an EXE) rather than an `InprocServer32` (a DLL), COM launches
the plugin **as its own process** instead of loading it inside `mstsc`. That
out-of-process launch is the specific thing this test proves.

> **Do not** keep the plugin EXE open/registered from a temp folder you will
> delete — the `LocalServer32` value is the absolute path recorded at
> registration time. If you move the EXE, re-run `/register`.

---

## Step 2 — Start an RDP session

From the **client**, launch `mstsc.exe` and connect to the **remote** host as you
normally would. Log in and get to the remote desktop.

There is nothing to configure in the RDP session dialog for this — the plugin is
discovered from the registry automatically when `mstsc` starts the connection.

> If `mstsc` was already running before Step 1, close **all** `mstsc.exe`
> instances and reconnect. The `AddIns` registry is read when the client
> starts a connection.

---

## Step 3 — Run the trigger (REMOTE, inside the session)

Inside the RDP session (i.e. in a shell **on the remote host**, seen through the
RDP window), run:

```powershell
.\dvc-probe-trigger.exe
```

Expected output on success:

```
[trigger] pid=... session=...
[trigger] opening DVC 'dvc::taskbar::probe' (WTS_CHANNEL_OPTION_DYNAMIC)...
[trigger] channel opened. Sending payload(s)...
[trigger] sent #1: "PROBE-PING #1" (13 bytes)
[trigger] sent #2: "PROBE-PING #2" (13 bytes)
[trigger] sent #3: "PROBE-PING #3" (13 bytes)
[trigger] channel closed. Done.
[trigger] Now check dvc-probe.log next to the plugin EXE on the CLIENT.
```

`/once` sends a single payload instead of three.

---

## Step 4 — Check the result (CLIENT)

On the **client**, look next to the plugin EXE:

```powershell
Get-Content .\bin\dvc-probe.log
```

### ✅ SUCCESS looks like this

A `dvc-probe.log` file exists and contains the activation sequence **and** the
data-arrival lines, e.g.:

```
2026-08-20 14:20:47.392  pid=9776  plugin instance constructed (this=0x...)
2026-08-20 14:20:47.394  pid=9776  IWTSPlugin::Initialize called -- mstsc HAS ACTIVATED THIS PLUGIN
2026-08-20 14:20:47.395  pid=9776  CreateListener(dvc::taskbar::probe) succeeded -- now listening
2026-08-20 14:20:51.101  pid=9776  OnNewChannelConnection[Ch1] -- server opened the DVC
2026-08-20 14:20:51.101  pid=9776  [Ch1] channel callback created
2026-08-20 14:20:51.102  pid=9776  [Ch1] *** DVC SIGNAL RECEIVED: 13 bytes, preview="PROBE-PING #1" ***
```

The line that proves the whole mechanism is the **`*** DVC SIGNAL RECEIVED ***`**
line. If you see it, `mstsc` activated an out-of-process plugin from the `AddIns`
`{CLSID}`/`LocalServer32` registration **and** delivered a real DVC signal to it.

You can also watch these lines live (without opening the file) using
[DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) on the
client — every log line is also emitted via `OutputDebugString`.

### ❌ FAILURE modes and what they mean

| Symptom | Meaning | Where to look |
|---|---|---|
| Trigger prints `WTSVirtualChannelOpenEx FAILED, GetLastError=31` | You are **not inside an RDP session** (running on the local console), *or* the client plugin never started listening. `31` is the normal error when there is no DVC infrastructure — e.g. running the trigger on a plain desktop. | Confirm the shell is really inside the `mstsc` window; confirm Steps 1–2. |
| No `dvc-probe.log` at all, trigger says it sent OK | `mstsc` never activated the plugin — it never called `CreateListener`, so the remote's open request had nothing to connect to. | The plugin was not registered, was registered from a different user, the EXE path is wrong/moved, or `mstsc` was already running before registration. Re-run `/register`; restart `mstsc`. |
| Log has `Initialize`/`CreateListener` lines but **no** `OnNewChannelConnection` / `SIGNAL RECEIVED` | The plugin activated but the signal never crossed. Channel-name mismatch, or the trigger ran in a different session than the one `mstsc` connected. | Confirm both sides use `dvc::taskbar::probe` (they share `probe/common/ProbeIds.h`). Confirm the trigger runs in the RDP session, not a second local login. |
| `Get-ItemProperty` in Step 1 shows nothing | Registration did not happen or wrote to a different hive. | Re-run `/register` (no `/machine`); confirm you read `HKCU`. |

The `dvc-probe.log` timestamps come from the **client's** local clock. The
trigger's console output is on the **remote**. They will differ if the two
machines' clocks differ — that is fine; the probe does not measure timing.

---

## Cleanup

On the client:

```powershell
.\bin\dvc-probe-plugin.exe /unregister
```

This removes the `AddIns` and `CLSID` keys from both `HKCU` and `HKLM` (the
`HKLM` deletes are no-ops unless you used `/machine`). Verified idempotent. You
can also delete `bin\dvc-probe.log`.

---

## What this proves, and what it does NOT

**Proves (once you see `SIGNAL RECEIVED`):**

- `mstsc` reads the per-user `AddIns` `{CLSID}` registration and activates the
  plugin **out-of-process** via COM `LocalServer32`.
- The plugin's `IWTSPlugin` / `IWTSListenerCallback` / `IWTSVirtualChannelCallback`
  implementation is wired correctly and receives real DVC data.
- The remote-side `WTSVirtualChannelOpenEx(..., WTS_CHANNEL_OPTION_DYNAMIC)` +
  `WTSVirtualChannelWrite` path successfully delivers bytes to the client plugin.
- All of the above works with **HKCU-only, no-admin** registration.

**Does NOT cover (deliberately out of scope for this probe):**

- Relaying the signal onward to the client-side Windhawk mod via a window
  message — not built yet.
- Any real payload / protocol / framing — the payload here is a throwaway
  ASCII string.
- Integrating the send side into the remote-side Windhawk mod — the trigger is
  a standalone EXE for now.
