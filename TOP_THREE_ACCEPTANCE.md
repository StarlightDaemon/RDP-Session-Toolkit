# Diagnostics and session reliability candidate acceptance

For the ordered two-machine backup, update, test and recovery procedure, start
with [HOME_UPDATE_TEST_ROLLBACK.md](HOME_UPDATE_TEST_ROLLBACK.md). This document
provides the extended acceptance scenarios.

This candidate implements the requested diagnostics, action feedback and
multi-session ambiguity guard. Match **client 0.11.1, host 0.5.1 and relay
0.3.1**. Its protocol v3 intentionally refuses legacy commands; mixed versions
must fail without acting, rather than silently use unsafe targeting.

The pre-home-test corrections are described in
[IMPLEMENTATION_REVIEW.md](IMPLEMENTATION_REVIEW.md). Use this replacement
candidate rather than the initial reliability bundle.

Local compile/link and isolated regression checks do not establish real RDP,
Explorer injection or unload behavior. Keep the working installation and test
the candidate with local access or a recoverable pair of Windows test machines.
No installation or registry change is performed by the build/check scripts.

## Candidate contents and update order

1. Back up both existing mod sources/settings, the registered relay executable
   and its registration values. Keep the original relay location. Close the
   client RDP sessions before changing their components.
2. Save the candidate client source as the existing client mod ID, preserving
   settings. Save the candidate host source as the existing host mod ID on the
   remote machine. In a bundle these are the two files under `mods/`.
3. Replace the relay at its existing registered location only when its old
   process is no longer running. The preceding relay may remain after
   disconnect; if necessary reboot locally before replacement. Do not register
   a second extracted copy at another path.
4. As the intended connecting user, run the replacement executable at its
   existing path with `/register`. Require exit 0. Its registration format is
   the same as the previous registration-refinement candidate: quoted default
   launch command and raw `ServerExecutable` identity. An ownership conflict
   must stop rather than overwrite another location.
5. Start a fresh mstsc process. Already-running processes can retain older code.
   Windhawk is installed/updated separately; this candidate does not upgrade it.

The bundle contains sources, a Windows x64 relay, diagnostics script, source
copies, build manifest and SHA-256 checksums. It is a manual test bundle, not an
installer. The development executable is unsigned. Do not double-click it:
without arguments it starts a COM server.

## Diagnostics

From the repository, or the bundle's `scripts/` directory:

```powershell
./scripts/Get-ToolkitDiagnostics.ps1
./scripts/Get-ToolkitDiagnostics.ps1 -Json
./scripts/Get-ToolkitDiagnostics.ps1 -WindhawkRoot 'C:\Your Windhawk folder' -Json
./scripts/Get-ToolkitDiagnostics.ps1 -Json -OutputPath 'C:\Your reports\rdp-diagnostics.json'
```

Choose a new export filename; existing files are never overwritten. JSON is
also printed for inspection. Collection never reads the shared command secret,
command lines, `.rdp` contents or clipboard, and never starts the relay, invokes
Windhawk, repairs registration or uploads a report. Default output omits
endpoint names and redacts user-profile paths. `-IncludeConnectionMetadata`
explicitly includes the snapshot hostname and unredacted paths.

Verify separate findings for missing Windhawk at the selected root, missing
CLI, inconsistent registration, absent receiver, incompatible/stale snapshot,
and a snapshot belonging to another process lifetime. A missing CLI means
automatic installation is unavailable, not that an already-loaded mod is
broken. Loaded versions and taskbar compatibility remain unknown unless
verified separately. The script describes this Windows user's local endpoint;
it does not interrogate the remote host.

## One-session controls and feedback

| Scenario | Expected result |
|---|---|
| Client Minimize and Restore | Correct window changes state; panel distinguishes accepted from observed state. |
| Client fullscreen toggle | One click, one toggle; state change observed on a later fresh snapshot. No retry. |
| Client reconnect disabled | Reconnect stays absent; requests cannot bypass the setting. |
| Client reconnect/disconnect | Acceptance is reported without claiming a replacement has connected. |
| Host Minimize/fullscreen | Correlated v3 reply; correct client window; accepted or observed result; later polling updates mode. |
| Host with client mod/relay absent | Peer controls unavailable with explanation; host-local Clipboard/Disconnect remain available over RDP. |
| Clipboard restart | Queued/result feedback; only the verified helper in this session is affected. Clipboard data can be discarded; successful launch does not prove copy/paste works. |
| Console session | Host RDP-only actions remain disabled, including their click-time checks. |
| Receiver hang or reply timeout | Outcome unknown/timed out; never an automatic retry or false success. |
| Relay disappears after a known mode | Host repaints unavailable/unknown rather than keeping a stale fullscreen label. |
| Feedback retention expires | Normal state returns after about ten seconds; long details remain available while feedback is shown. |

Try both themes, mixed DPI, secondary monitors and a computer carrying both
mods. Full feedback is in the status tooltip when the compact line is clipped.
The quality indicator still depends on actual RDP reports; this work does not
invent connection-quality data.

## Ambiguity and changing identities

1. With one session active, open a second mstsc **connection dialog** without
   connecting. Both local and relay-dependent controls must pause as ambiguous.
   A second real connection must produce the same refusal. This is deliberately
   conservative; a session selector is not implemented.
2. Attempt actions from both host widgets and from local panels. Neither may
   operate on whichever receiver happened to be enumerated first. Direct
   in-process overlay controls remain tied to their own window.
3. Close the extra dialog/session. Controls should return only when the sole
   receiver's PID and creation time agree with a fresh v3 snapshot.
4. Close/relaunch the client while a prior snapshot is present. A reused PID,
   old process creation time, future tick, stale record, malformed record or
   missing receiver must not authorize a command.
5. If a single client process has multiple RDP frame windows, peer controls
   must also pause. Frames belonging to other applications must never become
   this client's fallback target.

## Mixed versions

| Combination | Expected result |
|---|---|
| New host with old relay | New marker is an unknown command to the old stack. No action; host reports upgrade required. |
| Old host with new relay | Legacy request receives unknown legacy state; no action is forwarded. Old host cannot display the new detailed explanation. |
| New relay with old client | No trustworthy v3 snapshot/bound receiver; command unavailable, no legacy fallback. |
| New panel with old client, or old panel with new client | New local command version and separate snapshot filename prevent unsafe cross-version actions. |
| All new components | Guarded actions and correlated feedback work. |

Do not use production registrations to create these combinations while remote.
Use a disposable pair or perform the controlled acceptance with local access.

## Lifetime and overhead

- Repeatedly connect/disconnect and close all client instances. The new relay
  should exit after its final plugin/channel/factory reference is released.
  A retained mstsc/plugin object can keep it alive; disconnection alone is not
  proof that COM released all references.
- Disable/re-enable each mod while a status request or clipboard job is active.
  The host joins its worker and drains queued taskbar callbacks before unload.
  Waiting for OS channel-open completion can delay unload; a deadline alone
  would permit code to unload beneath a running worker.
- Verify client unload during a force-reconnect grace period cancels that
  worker. If its event-sink STA is hung, code can remain pinned and inert until
  mstsc exits; close/reopen the client before further acceptance.
- A stopped/disconnected XAML dispatcher can also cause inert code to remain
  pinned. Normal unload must remove controls, finish callbacks and leave no
  jobs executing. These Explorer-specific cases still require live testing.
  After a retention log, restart the affected Explorer or mstsc process before
  re-enabling: the retained module deliberately refuses to reuse stopped state.
- Hold a modifier while clicking fullscreen. It must refuse input injection.
  An incomplete shortcut has an unknown outcome and must never be retried
  automatically. Failed clipboard termination must report failure, not recovery.
- Inspect `%LOCALAPPDATA%\RDPSessionToolkit\dvc-relay.log` and `.log.old`.
  Each is capped at 1 MiB; the zero-length `.log.lock` file serializes writers.
  Poll success is silent. The previous EXE-adjacent log is historical and is
  neither read nor deleted by this candidate.
- Compare CPU, private memory, handles, threads, log growth and click response
  against the backed-up baseline. Guards now enumerate session clients and the
  host owns one worker rather than spawning detached jobs; measure actual cost.

Record Windows builds, Windhawk version, bundle name, results and relevant error
codes. Runtime acceptance is tracked in LOOP-011; packaging acceptance remains
a separate track under LOOP-010 in the repository state.

## Rollback

With sessions closed and relay processes exited, restore the backed-up matched
component set. If reverting to a pre-registration-refinement relay/client,
first use the new relay at its registered path with `/unregister`; restore and
register the old relay there, then restore the old client mod. A client that
expects an unquoted default cannot consume the newer registration format.
Preserve settings. If removal or rollback reports an error, inspect the exact
owned values before changing locations or retrying. Empty keys are permitted;
foreign values are retained.
