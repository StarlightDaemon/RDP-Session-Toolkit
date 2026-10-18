# Home update, test, and rollback guide

Follow this guide for the reviewed Windows x64 candidate: **client 0.11.1,
host 0.5.1, relay 0.3.1**. All three use protocol v3. This is a manual test
bundle, not an installer; its relay is unsigned. Windhawk is installed and
updated separately.

| Machine | Role | Components to update |
|---|---|---|
| Client | The PC where you launch Remote Desktop (`mstsc.exe`) | Client mod and relay |
| Host | The PC you connect to | Host mod |

If a PC serves both roles, update both mods there. Install the relay on machines
that launch mstsc. Keep the existing mod IDs and relay installation path.

The source review and remaining limits are in
[IMPLEMENTATION_REVIEW.md](IMPLEMENTATION_REVIEW.md). The extended acceptance
matrix is in [TOP_THREE_ACCEPTANCE.md](TOP_THREE_ACCEPTANCE.md). This guide is
for the reviewed candidate; `HOME_TEST.md` in the source repository describes
an earlier registration-only candidate.

## 1. Prepare both PCs and verify the bundle

1. Save your work. Have physical access to both PCs for this first update.
2. Before changing anything, confirm ordinary RDP, clipboard sharing and your
   current toolkit controls work. Record any existing failures.
3. Copy and extract the candidate ZIP on both PCs. Use the ZIP identified by the
   latest repository WORK_LOG entry, or build a fresh copy from the reviewed
   source as described below; older candidate ZIPs are retained.
4. In PowerShell, set `$bundle` to the extracted folder containing `mods`,
   `relay`, `scripts`, and `SHA256SUMS.txt`. The path below is an example:

   ```powershell
   $ErrorActionPreference = 'Stop'
   $bundle = 'C:\RDPToolkit-Test\candidate'

   Get-Content -LiteralPath "$bundle\SHA256SUMS.txt" | ForEach-Object {
       $entry = $_ -split '  ', 2
       $file = Join-Path $bundle $entry[1]
       if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $entry[0]) {
           throw "Checksum mismatch: $file"
       }
   }
   'All candidate checksums match'
   ```

   Stop if a file is missing or mismatched. Check `build-manifest.json` lists
   client 0.11.1, host 0.5.1 and relay 0.3.1. Checksums detect changed files;
   they do not make this unsigned build publisher-authenticated.

PowerShell variables do not survive closing the shell or rebooting. Record the
actual bundle and backup paths; set them again in each new shell.

If starting from a Git checkout at home, run these commands from its repository
directory before step 1.3:

```powershell
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw 'Pull failed; resolve it before building' }
.\scripts\build-refinement-candidate.ps1
```

The builder prints the new candidate directory and ZIP under `out\candidates`.
Copy that ZIP to the other PC and continue at step 1.3. It uses the existing
Windhawk compiler at `C:\Program Files\Windhawk\Compiler\bin\clang++.exe`;
pass `-Compiler 'C:\Your compiler folder\bin\clang++.exe'` for another compatible
installation. Building does not install/register components or modify Windhawk.
Generated ZIPs and executables are ignored by Git and do not arrive with a pull.

## 2. Back up each machine before editing its mods

On **both PCs**:

1. Copy each current toolkit mod's complete saved source from Windhawk into a
   backup file. Preserve unsaved editor changes separately if any exist.
2. Record all toolkit settings and whether each mod is enabled. Screenshots
   are sufficient for restoring individual settings through Windhawk.
3. Preserve the `.rdp` files used for reconnect testing. Keep them private.
4. Record the Windows build and Windhawk version.

For a standard installed Windhawk configuration, run this additional backup:

```powershell
$ErrorActionPreference = 'Stop'
$backup = Join-Path $env:USERPROFILE (
    'RDPToolkit-Backups\' + $env:COMPUTERNAME + '-' +
    (Get-Date -Format 'yyyyMMdd-HHmmss')
)
New-Item -ItemType Directory -Path $backup -Force | Out-Null

Copy-Item -LiteralPath 'C:\Program Files\Windhawk\windhawk.ini' `
    -Destination "$backup\windhawk.ini"

Copy-Item -LiteralPath "$env:ProgramData\Windhawk\ModsSource" `
    -Destination "$backup\ModsSource" -Recurse

if (Test-Path -LiteralPath "$env:ProgramData\Windhawk\Engine\ModsWritable") {
    Copy-Item -LiteralPath "$env:ProgramData\Windhawk\Engine\ModsWritable" `
        -Destination "$backup\ModsWritable" -Recurse
}

reg.exe export 'HKLM\SOFTWARE\Windhawk' "$backup\windhawk.reg"
if ($LASTEXITCODE -ne 0) { throw 'Windhawk settings backup failed' }

$backup
```

If `windhawk.ini` specifies different `AppDataPath`, `RegistryKey` or install
locations, substitute those locations. For portable Windhawk, exit Windhawk
and preserve its entire directory. Do not continue after a failed backup.

Keep backups private and retain a separate backup directory for each PC.
Ordinary rollback below restores toolkit settings through Windhawk; importing
the entire `windhawk.reg` could revert unrelated mods.

## 3. Back up the registered relay on the client

Run as the Windows user who normally launches RDP. Keep that same user for
registration and testing; HKCU belongs to the user running the command.

```powershell
$relayKey = Get-Item -LiteralPath (
    'HKCU:\Software\Classes\CLSID\' +
    '{6FC96481-9467-496E-BA33-A202ED052F39}\LocalServer32'
)

$relayKey.GetValue('')
$relayKey.GetValue('ServerExecutable')
```

Record the existing relay EXE location. Older registration may have only the
default value. If registration is absent, points somewhere unexpected, or the
values disagree, resolve that before proceeding. Do not execute the registry's
default string as a shell command.

Set `$relayExe` to the actual EXE file path, **without command-line quotes or
arguments inside the value**. Set `$backup` to this client's backup directory
from step 2 if using a new shell:

```powershell
$relayExe = 'C:\Your existing toolkit folder\dvc-relay-plugin.exe'

if (-not (Test-Path -LiteralPath $relayExe -PathType Leaf)) {
    throw 'Registered relay executable not found'
}

$relayExe | Set-Content -LiteralPath "$backup\registered-relay-path.txt"
Copy-Item -LiteralPath $relayExe -Destination "$backup\dvc-relay-plugin.exe"

reg.exe export `
    'HKCU\Software\Classes\CLSID\{6FC96481-9467-496E-BA33-A202ED052F39}' `
    "$backup\relay-class.reg"
if ($LASTEXITCODE -ne 0) { throw 'Relay class backup failed' }

reg.exe export `
    'HKCU\Software\Microsoft\Terminal Server Client\Default\AddIns\RdpSessionToolkitDvcRelay' `
    "$backup\relay-addin.reg"
if ($LASTEXITCODE -ne 0) { throw 'Relay AddIn backup failed' }
```

Confirm the backup contains the original relay EXE, saved path, both relay
registry exports, original mod sources and settings records before continuing.

## 4. Update the host PC

At the host's physical console:

1. Disable the existing **RDP Session Toolkit — Taskbar Host** mod in Windhawk.
2. Open that existing mod's source editor; do not create a duplicate mod.
3. Replace its complete source with the bundle's
   `mods\rdp-session-toolkit-taskbar-host.wh.cpp`.
4. Save/compile. Require successful compilation and source version **0.5.1**.
5. Preserve the existing settings and enable the mod.
6. Reboot the host so acceptance starts with a fresh Explorer process.

At the physical console, the host's RDP-only buttons should be disabled.

## 5. Update the client PC and relay

1. Close **all** Remote Desktop connections and connection dialogs.
2. Disable the existing **RDP Session Toolkit — Taskbar Client** mod.
3. In that existing mod's source editor, replace the complete source with
   `mods\rdp-session-toolkit-taskbar-client.wh.cpp` from the bundle.
4. Save/compile. Require successful compilation and source version **0.11.1**.
5. Preserve settings and enable the mod. This one client mod supplies both the
   mstsc behavior and local Explorer taskbar panel.
6. Reboot the client. **Do not open Remote Desktop yet.**
7. Open PowerShell as the intended RDP user. Restore your actual paths:

   ```powershell
   $ErrorActionPreference = 'Stop'
   $bundle = 'C:\RDPToolkit-Test\candidate' # Your extracted bundle
   $backup = 'C:\Users\YourName\RDPToolkit-Backups\YOUR-CLIENT-BACKUP'
   $relayExe = Get-Content -LiteralPath "$backup\registered-relay-path.txt"

   Get-Process -Name mstsc,dvc-relay-plugin -ErrorAction SilentlyContinue |
       Select-Object Id, ProcessName, Path
   ```

   There should be no mstsc or old relay process before replacement. If the
   EXE remains locked, investigate or reboot again. Do not force-stop an RDP
   connection you depend on.

8. Replace the relay **at its existing registered location**, then register:

   ```powershell
   Copy-Item -LiteralPath "$bundle\relay\dvc-relay-plugin.exe" `
       -Destination $relayExe -Force

   & $relayExe /register
   if ($LASTEXITCODE -ne 0) {
       throw "Registration failed: exit $LASTEXITCODE"
   }
   ```

   Use a normal shell as the intended RDP user. Do not register a second copy
   from the extracted bundle, use `/machine`, or double-click the EXE. Without
   arguments, the EXE starts a COM server.

9. Re-read the registry values using step 3. Expect the default to be a quoted
   executable path and `ServerExecutable` the same path without quotes.
10. Run `& $relayExe /register` once more. Require exit **0** again.

## 6. Test one fresh connection

Launch one fresh mstsc connection to the host and allow about **10 seconds**
for status and polling. Test in this order:

| Test | Expected result |
|---|---|
| Client panel and host widget | Both appear with preserved settings. |
| Client panel Minimize, then Restore | Only the intended RDP window changes state. |
| Client fullscreen toggle | Exactly one toggle per click; wait about five seconds between clicks. |
| Host Minimize | Client RDP window minimizes; restore it locally. |
| Host fullscreen toggle | Client changes mode and host label follows. Foreground refusal reports rejection rather than injecting into another application. |
| Action feedback | Acceptance and observed state are distinguished. No automatic retries. |
| Clipboard restart | Queued/result feedback appears. Re-copy fresh harmless text and verify copying in both directions. |
| Host Disconnect | Connection closes while the remote Windows session remains running. |
| Fresh connection afterward | Controls work again. |

Use harmless clipboard text such as `TOOLKIT-CLIENT-1` and `TOOLKIT-HOST-1`.
Restarting the helper can discard existing clipboard contents. A successful
helper launch does not prove copy/paste works; actually check both directions.

Initially leave Reconnect disabled if that was your existing setting. After
the basic tests pass, optionally enable it and test one clean reconnect. Do
not deliberately hang or force-terminate the client during this first pass.

## 7. Test ambiguity, unavailable peers and lifecycle

With the first connection active:

1. Open a second `mstsc.exe` **connection dialog without connecting**.
2. Wait approximately **5–10 seconds**.
3. Confirm the client panel's peer controls and host Minimize/fullscreen
   controls pause as ambiguous.
4. Confirm host-local Disconnect and Clipboard remain available over RDP.
5. Close only the extra dialog.
6. Confirm peer controls return after fresh state arrives.

A second real connection is optional. Use a different test endpoint or a
configuration supporting concurrent sessions: connecting again to the same
host/account can displace the original connection.

With physical access available, test missing-component handling:

1. Disable the client mod while connected.
2. Within roughly **15 seconds**, host peer controls should become unavailable.
3. Host-local Disconnect and Clipboard should remain usable.
4. Re-enable the client mod and start a fresh connection.

If a log says code was **retained**, restart the affected mstsc or Explorer
process, or reboot that PC, before re-enabling. The retained module deliberately
refuses to reuse stopped state. A channel-open operation can delay unload;
record a persistent hang instead of assuming a worker has stopped.

Repeat a few disconnect/reconnect cycles. Check both taskbars' light/dark
themes, secondary monitors and DPI placement. Hold a modifier while clicking
the client fullscreen control: it should refuse shortcut injection.

Deliberate mixed-version, hung-process, and force-reconnect cancellation trials
belong to the extended matrix in `TOP_THREE_ACCEPTANCE.md`, after this matched
component pass succeeds. Passing this guide alone does not close those gates.

## 8. Collect results

On the **client**, with `$bundle` and `$backup` set to the recorded paths:

```powershell
& "$bundle\scripts\Get-ToolkitDiagnostics.ps1" -Json `
    -OutputPath "$backup\after-update-client.json"
```

Use a new output filename for every run; exports refuse overwrites. If Windhawk
is installed elsewhere, pass `-WindhawkRoot 'C:\Your Windhawk folder'`.

Check client receiver/snapshot identity and freshness. `LoadedVersion='Unknown'`
is expected: source presence does not prove loaded code. Confirm component
initialization versions in Windhawk/relay logs as well.

The collector can run on the host, but missing **local mstsc/client registration**
findings are expected on a host-only machine. Do not treat those as failed host
mod acceptance. A missing Windhawk CLI alone also does not mean the loaded mod
is broken.

Preserve:

- Windows build and Windhawk version on each PC, plus bundle name.
- Each test's result and any exact error/exit code.
- Windhawk initialization/error logs.
- Client `%LOCALAPPDATA%\RDPSessionToolkit\dvc-relay.log` and `.log.old`.

Default diagnostics omit endpoint names and do not collect command secrets,
command lines, connection-file contents or clipboard contents. The optional
`-IncludeConnectionMetadata` switch includes endpoint names and unredacted
paths. Inspect reports/logs before sharing them. Keep registry and `.rdp`
backups private.

If an action reports an unknown outcome, inspect the actual window before
clicking again.

## 9. Roll back if anything fails

Restore the complete backed-up matched component set. On each PC use its own
backup, not the other machine's settings or sources.

1. Save logs and failure details.
2. Close all mstsc windows and connection dialogs.
3. Disable the toolkit mods on both machines.
4. Ensure relay processes have exited; reboot locally if necessary. After
   reboot, do not launch RDP before completing relay restoration.
5. On the client, set `$backup` to its recorded backup directory and load the
   original relay location. **Before overwriting the candidate relay**, use
   the new EXE still at that registered location to unregister:

   ```powershell
   $ErrorActionPreference = 'Stop'
   $backup = 'C:\Users\YourName\RDPToolkit-Backups\YOUR-CLIENT-BACKUP'
   $relayExe = Get-Content -LiteralPath "$backup\registered-relay-path.txt"

   & $relayExe /unregister
   if ($LASTEXITCODE -ne 0) {
       throw "Unregistration failed: exit $LASTEXITCODE"
   }
   ```

6. Restore the backed-up old relay at that same location and register it:

   ```powershell
   Copy-Item -LiteralPath "$backup\dvc-relay-plugin.exe" `
       -Destination $relayExe -Force

   & $relayExe /register
   if ($LASTEXITCODE -ne 0) {
       throw "Old relay registration failed: exit $LASTEXITCODE"
   }
   ```

7. **After restoring old relay registration**, paste the saved old client source
   into its existing Windhawk mod editor, save/compile, and restore its recorded
   settings. An older client may not understand the new quoted registration
   format, so do not reverse this order.
8. Restore the saved old host source and settings through its existing mod
   editor on the host PC.
9. Restore the recorded enabled states, reboot both PCs, and repeat the original
   baseline RDP/control checks.

The registry exports are reference/recovery backups. Importing a `.reg` file
merges values and does not necessarily remove newer values; it is not a
substitute for the ordered unregister/restore/register procedure above.

If unregistration fails, or exit **3** reports incomplete rollback, stop before
replacing binaries or importing registry exports. Preserve the exact error and
registration backups. Keep the toolkit mods disabled and use ordinary RDP while
resolving registration. Do not delete whole registry trees, use `/machine` for
routine recovery, or repeatedly retry different versions.
