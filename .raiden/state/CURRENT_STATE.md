# Current State

## Repository purpose

Personal-use RDP taskbar controls, diagnostics and recovery helpers, companion
to [Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar).
The toolkit combines Windhawk mods on the connecting client and session host
with a client-side out-of-process COM Dynamic Virtual Channel relay. See the
root [README](../../README.md) for architecture and usage.

## Current implementation

| Component | Source version | Runs on |
|---|---|---|
| [Taskbar Client](../../taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp) | 0.11.1 | Client PC: mstsc.exe and local explorer.exe |
| [Taskbar Host](../../taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp) | 0.5.1 | Session host: explorer.exe |
| [DVC relay](../../dvc-plugin/relay/common/RelayIds.h) | 0.3.1 | Client PC: COM LocalServer activated by mstsc |

The client mod owns the primary taskbar panel and the optional floating
fullscreen overlay. The overlay is off by default. The former thumbnail
toolbar and separate embedded-panel mod are retired; their history is in
DECISIONS and WORK_LOG. Reconnect and Force reconnect remain opt-in.

Diagnostics, action feedback and conservative ambiguity refusal are implemented
with their prerequisite registration and runtime reliability fixes. The current
design is recorded in D-43 through D-45 in [DECISIONS.md](DECISIONS.md):

- Commands use protocol v3 with correlation and target PID/creation identity.
  Extra mstsc processes, including connection dialogs, and multiple RDP frames
  pause peer controls. This does not provide multi-session selection.
- Snapshot v3 uses `local-widget-status-v3.dat`, freshness/identity validation and
  complete-record replacement. Legacy actions have no unsafe fallback.
- Feedback distinguishes acceptance, observed state and uncertain outcomes;
  uncertain actions are not retried. Host-local Clipboard and Disconnect remain
  independent of relay readiness and retain their RDP-session gates.
- Host work uses an owned bounded queue. Client workers and taskbar/native
  callbacks are accounted for during unload. Unresolved callback teardown can
  retain code until process exit; re-enabling then requires a process restart.
  Relay lifetime follows COM references, and relay logs are bounded in user
  storage. These source fixes still require real Windows lifecycle acceptance.
- Registration separates the quoted COM launch command from its raw executable
  identity, preserves foreign values and scopes rollback/removal. It is not a
  complete deployment transaction or crash-recovery installer.

[Get-ToolkitDiagnostics.ps1](../../scripts/Get-ToolkitDiagnostics.ps1) collects
read-only local observations and can export to an explicitly selected new file.
It omits endpoint names by default and does not collect command secrets,
command lines, connection-file contents or clipboard contents. File/source
presence does not prove loaded versions or successful taskbar injection.

The WM_COPYDATA sender-window/image check is advisory identity checking, not
authentication against hostile same-user code. The existing personal-use trust
boundary and its limits are described in the
[implementation review](../../IMPLEMENTATION_REVIEW.md).

## Published home-test candidate

[Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
is published as a prerelease. The
[Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip)
contains the built relay, mod sources, diagnostics, source/build provenance,
acceptance material and the home update/test/rollback guide. The guide and
release checksum file are also standalone release assets. The relay is unsigned;
this is a manual candidate, not an installer or a live-accepted release.

The release tag identifies the implementation revision; refreshed downloadable
assets include the later documentation revision and download links. Exact
revisions, asset hashes, publication details and verification results belong to
the dated [WORK_LOG.md](WORK_LOG.md) entries and the ZIP's build manifest.

Start with [HOME_UPDATE_TEST_ROLLBACK.md](../../HOME_UPDATE_TEST_ROLLBACK.md).
Use [TOP_THREE_ACCEPTANCE.md](../../TOP_THREE_ACCEPTANCE.md) for the extended
runtime matrix. A local source build is available through
[build-refinement-candidate.ps1](../../scripts/build-refinement-candidate.ps1).
Generated binaries and candidate ZIPs remain ignored by Git; download the
published asset or build locally after pulling.

## Verification and next work

The earlier operator-verified baseline is recorded in D-41 and the September 6
WORK_LOG entry. That report predates the current registration/protocol/lifetime
candidate. Local regression and compile/link checks, an implementation review,
and publication/download verification have been recorded for the candidate;
none establishes its real two-machine or Explorer acceptance. This development
work has not changed the installed runtime or production registration.

[OPEN_LOOPS.md](OPEN_LOOPS.md) is authoritative for loop dispositions and
version-scoped evidence. Use these entries to coordinate the next work:

- **LOOP-011:** perform the home update and runtime acceptance matrix, including
  controls/feedback, ambiguity and changed identities, mixed versions, clipboard
  recovery, unload/retained-code restart, foreground input, monitors/DPI/themes
  and resource measurements.
- **LOOP-002:** validate conservative target refusal within that candidate pass.
  A multi-session chooser would require a separate product decision.
- **LOOP-010:** follow the deployment/installer gates in
  [REFINEMENT_PLAN.md](../../REFINEMENT_PLAN.md) and
  [PACKAGING_RESEARCH.md](../../PACKAGING_RESEARCH.md). A toolkit setup must reuse
  an independently installed Windhawk; D-42 governs that boundary.

Historical loop narratives are retained in the ledger's linked snapshot. Do not
reuse old unverified-build text as a current baseline failure or apply the
earlier operator verification to this newer candidate. Dated test counts and
doctor results stay in WORK_LOG; no new live test or doctor run is implied here.

## Planning and history

[BEST_NEXT_ADDITIONS.md](../../BEST_NEXT_ADDITIONS.md) records the offering
comparison and ranked feature proposals; its selected additions are implemented
in this candidate. Broader proposals in that report and
[REPOSITORY_REVIEW.md](../../REPOSITORY_REVIEW.md) remain planning material.
[ROADMAP.md](../../ROADMAP.md) and [REFINEMENT_PLAN.md](../../REFINEMENT_PLAN.md)
provide the associated sequence. The earlier registration-only HOME_TEST guide
is historical; use HOME_UPDATE_TEST_ROLLBACK for this prerelease.

Development and earlier live-test history are preserved in Git,
[DECISIONS.md](DECISIONS.md), [WORK_LOG.md](WORK_LOG.md) and the reconciled loop
ledger rather than repeated as current build notes here.
