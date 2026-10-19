# First refinement candidate: historical home-test checklist

This document preserves the registration-only candidate's manual test plan.
For the published session-reliability candidate, start with
[HOME_UPDATE_TEST_ROLLBACK.md](HOME_UPDATE_TEST_ROLLBACK.md).

## Current toolkit guidance

Use [Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
and the [Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip).
It includes the built relay, mod sources, diagnostics and the home guide; no
relay rebuild is needed. The release also provides the
[standalone home guide](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md)
and [asset checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).

RC1 uses matching **client 0.11.1, host 0.5.1 and relay 0.3.1** with protocol v3.
Use the current home guide for both machines' backups, update order, testing and
rollback, then [TOP_THREE_ACCEPTANCE.md](TOP_THREE_ACCEPTANCE.md) for the extended
matrix. In particular, the historical instruction below to retain host 0.4.0
does not apply to RC1. Neither the old component set nor this registration-only
checklist validates the later protocol, ambiguity guard, feedback or lifetime work.

RC1 is a manual, unsigned prerelease with local checks recorded and real home
acceptance still required. The release tag identifies the implementation revision;
refreshed assets include the later guide/download-link revision. Exact provenance
and asset verification live in the manifest and dated
[work log](.raiden/state/WORK_LOG.md). Current acceptance/deployment work is
coordinated through LOOP-011/LOOP-002/LOOP-010 in
[OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md). Toolkit setup must reuse independently
installed Windhawk (D-42).

## Historical registration-candidate scope

The first-pass candidate was **client 0.10.1, relay 0.2.1, host 0.4.0 unchanged**.
It changed registration and executable-path handling while retaining the earlier
host and transport. At preparation it was built and locally checked; the guide
was a plan for home validation, not a record of completed live acceptance. Its
bundle used the earlier `refinement-1-*` layout described in the historical
pass-1 section of [REFINEMENT_PLAN.md](REFINEMENT_PLAN.md).

The remaining steps and expected version/log values apply only to that older
candidate. Do not apply this abbreviated update/rollback sequence to RC1 or mix
its components with that set. Keep the working installation until local access
and use the current home guide for today's update. No new live test is implied
by this documentation review.

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
4. Close your RDP sessions on the client before replacing the relay. The earlier
   baseline relay may remain running after disconnection; if its file is still
   in use, reboot locally and do the replacement before starting RDP again. Do not
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

Test one RDP session first. This first-pass candidate did not include ambiguity
refusal or unload-during-work refinements. Later source implements conservative
target refusal and lifecycle changes, not a multi-session chooser; use the
current home guide and acceptance matrix to validate those changes. The
historical checks above do not cover them.

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
passing run of these checks would provide evidence for that first-pass candidate
only. It would not accept RC1 or establish installer readiness. The subsequent
source/deployment sequence is in [REFINEMENT_PLAN.md](REFINEMENT_PLAN.md); use
the current home guide for current results and rollback.
