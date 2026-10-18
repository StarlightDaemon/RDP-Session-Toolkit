# DVC probe — historical activation test

> **Status: VERIFIED (2026-08-21).** The procedure below was run end-to-end and
> produced a clean success — `IWTSPlugin::Initialize`, `CreateListener`, and all
> three `*** DVC SIGNAL RECEIVED ***` lines appeared in `dvc-probe.log` on the
> client. The mechanism this probe exists to test is confirmed working; see
> "What this proves" at the bottom. The probe registration was unregistered
> afterward (it's a throwaway test artifact, not the real relay plugin — see
> that section for the limits of this evidence).

## Current toolkit testing

Use [Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
and the [Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
for current deployment. The ZIP includes the built relay, mod sources and the
[home update, test and rollback guide](../HOME_UPDATE_TEST_ROLLBACK.md); no relay
rebuild is needed. The release also provides the
[standalone guide](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md)
and [asset checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).
Use [TOP_THREE_ACCEPTANCE.md](../TOP_THREE_ACCEPTANCE.md) for the current runtime
matrix. The manual, unsigned prerelease still requires its own home acceptance;
neither this probe result nor the later baseline report in
[D-41](../.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch)
accepts RC1. Release/asset provenance is recorded through the manifest and
[work log](../.raiden/state/WORK_LOG.md).

The [production relay](relay/README.md) and both mods are implemented. Do not
register the retired probe or run its trigger for a normal toolkit update/test.
Formal evidence and follow-up work are in LOOP-003, LOOP-011 and LOOP-002 in
[OPEN_LOOPS.md](../.raiden/state/OPEN_LOOPS.md).

## Historical question and procedure

The remaining sections preserve the original experiment and its examples. They
are archival instructions for a deliberate replay in a disposable test
environment, not the current toolkit installation or rollback procedure. The
probe uses its original unscoped registry helper and executable-adjacent log;
the cleanup caveats below matter if replaying it. No probe registration, live
test or cleanup was performed for this documentation review.

The experiment addressed this then-unproven mechanism:

> Does an out-of-process plugin, registered under the RDP client's `AddIns` key
> in the `{CLSID}` / `LocalServer32` (out-of-process) form, actually get
> **activated by `mstsc`**, and does it actually **receive a Dynamic Virtual
> Channel signal** sent from inside the RDP session?

Everything here lives in [`probe/`](probe/). It is a throwaway proof, not the
production relay plugin. It sends a trivial payload and logs its receipt.
The recorded result established activation and byte delivery for that probe;
it did not establish production control or lifecycle behavior.

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

The recorded test used **per-user HKCU registration from a non-elevated prompt**.
The retained probe also supports machine registration, and its cleanup attempts
both hives regardless of registration scope. Do not infer a general no-admin
or safe-removal guarantee from the original HKCU success.

---

## Build

For an intentional historical replay, build once and copy the matching-arch
files, or build on each test machine. Start at the repository root:

```powershell
cd dvc-plugin\probe
.\build.ps1                # x86_64 by default -> .\bin\
```

Outputs:

- `bin\dvc-probe-plugin.exe`  — the CLIENT-side plugin (supports explicit `/register`)
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
> registration time. The old probe overwrites its registration on `/register`;
> it lacks the production relay's ownership/rollback checks. Keep the replay's
> test path stable. Use the current home guide for production path handling.

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

Expected output on success (example, not a newly captured run):

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

The old trigger's printed `session=...` value comes from
WTSGetActiveConsoleSessionId, not the trigger process's session. It is not proof
that the shell is inside RDP. The channel open itself uses WTS_CURRENT_SESSION;
run the trigger in the intended remote session as specified above.

---

## Step 4 — Check the result (CLIENT)

On the **client**, look next to the plugin EXE:

```powershell
Get-Content .\bin\dvc-probe.log
```

### ✅ SUCCESS looks like this

A `dvc-probe.log` file exists and contains the activation sequence **and** the
data-arrival lines. This retained example illustrates the format; its timestamps
are not the date of the August 21 verification:

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

### ❌ Probe troubleshooting observations

These are diagnostic leads, not proof of a single cause. In particular, no file
can also mean that the executable-adjacent log could not be written.

| Symptom | Meaning | Where to look |
|---|---|---|
| Trigger prints `WTSVirtualChannelOpenEx FAILED, GetLastError=31` | Channel open failed. The historical console check observed this error; it alone does not identify a cause. | Confirm the intended RDP session, registration and listener activation in the disposable replay environment. |
| No `dvc-probe.log` at all, trigger says it sent OK | Activation or logging may have failed; the missing file alone cannot distinguish them. | Inspect the probe registration/user/path, executable-directory write access and OutputDebugString output before changing registration or restarting the test client. |
| Log has `Initialize`/`CreateListener` lines but **no** `OnNewChannelConnection` / `SIGNAL RECEIVED` | Listener activation was logged, but arrival was not. Possible causes include a channel/session mismatch or subsequent logging failure. | Confirm both sides use `dvc::taskbar::probe`, that the trigger runs in the intended RDP session, and that logging remains writable. |
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

This is the **old probe cleanup**, retained for the historical replay. Its helper
attempts recursive deletion of the probe's AddIns and CLSID trees in **both HKCU
and HKLM**, without ownership checks. HKLM attempts are not conditional on having
used `/machine`; access can fail or pre-existing probe-key data can be removed.
Its boolean result can report success when only one hive's cleanup succeeded.
Inspect/back up those exact probe keys before deliberate reuse and verify both
hives afterward rather than treating the exit status as proof of complete cleanup.
The recorded test's probe registration was removed; this review did not repeat it.
The test log can be removed after preserving any evidence needed.

Do not use this command for current relay removal/rollback; follow
[HOME_UPDATE_TEST_ROLLBACK.md](../HOME_UPDATE_TEST_ROLLBACK.md). The production
relay has different identifiers and scoped value removal.

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

- Relaying the signal onward to a Windhawk mod via a window message. That path
  is now implemented in the separate production components; this probe never
  exercised it.
- Any real payload / protocol / framing — the payload here is a throwaway
  ASCII string.
- Integrating the send side into the remote-side Windhawk mod. This probe keeps
  its standalone trigger; the production host mod implements its own sender.
- Production protocol v3, bound targeting, explicit feedback, registration
  rollback, COM reference-based shutdown, taskbar behavior or current candidate
  compatibility/security. Use the current acceptance matrix for those gates.
