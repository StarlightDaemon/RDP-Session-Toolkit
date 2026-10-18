# First refinement candidate: home-test checklist

Candidate: client **0.10.1**, relay **0.2.1**, host **0.4.0** (unchanged).
This candidate changes registration and executable-path handling. It is built
and locally checked, but its new runtime behavior is not yet verified in RDP.
Keep the currently working installation until you have local access.

## Before changing anything at home

1. Save the current client/host mod sources and settings from Windhawk and a
   copy of the currently registered relay EXE. Keep the original relay path.
   Check that your usual RDP controls work before changing the baseline.
2. Read the current per-user registration without changing it:

   ```powershell
   $relayKey = 'HKCU:\Software\Classes\CLSID\{6FC96481-9467-496E-BA33-A202ED052F39}\LocalServer32'
   Get-ItemProperty -LiteralPath $relayKey
   ```

   The default value is the old executable path (possibly quoted). Record it;
   do not execute this registry string as a shell command. If entries disagree
   with your expected installation, stop and investigate.
3. Check the candidate's `SHA256SUMS.txt`. `build-manifest.json` records the
   source revision, working-tree status, compiler, and candidate versions.
   Hashes detect changes; this development build is unsigned.
4. Close your RDP sessions on the client before replacing the relay. The current
   relay may remain running after disconnection; if its file is still in use,
   reboot locally and do the replacement before starting RDP again. Do not
   force-stop a session you still depend on.

## Update order

1. In the existing Windhawk client mod, install/save the candidate
   `mods/rdp-session-toolkit-taskbar-client.wh.cpp` as the **same mod ID**.
   Preserve settings. Do not create a duplicate mod. This updated client accepts
   both the old and new relay registration formats.
2. With sessions closed and the old relay no longer running, back up and replace
   the relay EXE **at its existing registered path** with the candidate
   `relay/dvc-relay-plugin.exe`. Do not register it from the extracted candidate
   folder: a different path is deliberately treated as an ownership conflict.
3. From a normal shell as the intended client user, run the replacement at that
   existing path with `/register`, using PowerShell's call operator and a quoted
   file path. Example only; substitute the actual path you recorded:

   ```powershell
   & 'C:\Your existing toolkit folder\dvc-relay-plugin.exe' /register
   ```

   Require exit code 0. Re-read the registry: the default value must be a quoted
   executable path and `ServerExecutable` the same path without quotes. Repeat
   `/register`; it should succeed without changing the effective registration.
4. Keep host 0.4.0 installed. The included host source is unchanged, provided to
   make the candidate's matching component set explicit. No Windhawk upgrade
   is needed for this first manual test.
5. Start a fresh mstsc connection. Existing sessions may retain loaded code;
   do not treat an already-open session as proof of the new candidate.

## Runtime checks

| Check | Expected result |
|---|---|
| Client panel and host widget | Same appearance and settings as the baseline |
| Host Minimize, then restore locally | Correct client window minimizes and restores |
| Host fullscreen toggle, repeated | Works repeatedly; state label follows |
| Disconnect/reconnect | Fresh connection activates relay and controls still work |
| Relay log | New server identifies itself as 0.2.1 |
| Client logs | New initialization identifies 0.10.1; no relay image-path mismatch |
| Legacy compatibility, before re-registering | Updated client still accepts the old relay's registration |

Test one RDP session first. Multi-session routing and unload-during-work fixes
are later passes; this candidate does not claim those limitations are resolved.

## Removal and recovery checks (after the runtime check)

With sessions closed, run the **replacement EXE at its registered path** with
`/unregister`. Require exit 0. The three owned values should be absent;
empty keys may remain. HKLM and unrelated values must be unchanged. Repeat
removal to check idempotence, then `/register` again and reconnect.

If an operation reports failure, preserve its Windows error number. Exit 3
means rollback was incomplete: inspect the registration before retrying. Do not
run several installers or edit these registry values concurrently. This pass
restores values after ordinary failures; it is not power-loss/crash recovery.
`/machine` is only supported with `/unregister` for deliberate legacy cleanup;
ordinary testing does not need it.

To revert: close sessions, use the **new relay** to unregister its owned values,
then restore the backed-up old EXE at that same path and register it. Restore
the older client mod only **after** restoring the old registration format.
An older client cannot read the new quoted default. Restore saved settings if
needed. If the new removal fails, resolve that error before changing versions.

Record Windows builds, Windhawk version, candidate folder name, each check's
result, and relevant errors (without clipboard contents or credentials). A
passing home test validates pass 1; installer and newer-Windhawk validation
remain separate in [REFINEMENT_PLAN.md](REFINEMENT_PLAN.md).
