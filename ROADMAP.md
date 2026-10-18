# Roadmap

The toolkit centers on RDP taskbar controls, small session diagnostics and recovery
helpers. This roadmap separates implemented source, acceptance/deployment work
and ideas needing a product decision. [OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md)
owns formal loop dispositions; [DECISIONS.md](.raiden/state/DECISIONS.md) owns
architectural decisions.

## Implemented foundation

The client taskbar panel, optional overlay, host widget and out-of-process DVC
relay are built. Controls include minimize/restore, fullscreen switching,
disconnect, opt-in reconnect/force reconnect and host clipboard-helper restart.
Local idle time and reported connection quality supplement session state. The
separate embedded-client mod and thumbnail toolbar were retired; session duration
was removed.

The operator authorized the top additions from
[BEST_NEXT_ADDITIONS.md](BEST_NEXT_ADDITIONS.md). Current source implements:

- **Diagnostics:** read-only local observations, structured output and explicit
  local export, with endpoint names omitted by default.
- **Action feedback:** correlated results, distinct acceptance and observation,
  unavailable/rejected/busy/unknown outcomes, and no automatic uncertain retry.
- **Ambiguity guard:** process/creation identity and fresh snapshots, with
  conservative refusal for extra mstsc processes or multiple frames. This is
  single-target control without a session chooser.

Prerequisites include scoped registration/rollback, owned bounded work, callback
accounting and unload retention when necessary, COM reference-based relay
shutdown, atomic snapshot replacement and bounded user-local logs. See
[README.md](README.md) for versions and [IMPLEMENTATION_REVIEW.md](IMPLEMENTATION_REVIEW.md)
for review corrections.

## Next: accept the published candidate at home

Use [Session reliability RC1](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
and the [home update, test and rollback guide](HOME_UPDATE_TEST_ROLLBACK.md), then
the [extended acceptance matrix](TOP_THREE_ACCEPTANCE.md). Publication and local
checks do not establish live acceptance. The earlier operator-verified baseline
is scoped in [D-41](.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).

Collect real two-machine evidence for changed controls, feedback, mixed-version
refusal, changed identities, relay disappearance, clipboard outcomes and
unload/restart behavior. Include extra connection dialogs, multiple frames,
foreground/input handling, monitors/DPI/themes and resource measurements.
Coordinate through LOOP-011 and LOOP-002 in the
[ledger](.raiden/state/OPEN_LOOPS.md); resolve uncertain or failed cases before
treating the candidate as the new baseline.

## Then: validate deployment before an installer

[REFINEMENT_PLAN.md](REFINEMENT_PLAN.md) records the sequence. Registration and
runtime source passes are implemented; deployment passes require more than a
manual ZIP or successful build.

1. Validate an exact-version Windhawk CLI adapter in a disposable Windows
   environment, including prerequisite discovery and different-admin credentials.
2. Implement the toolkit deployment transaction and operation journal; exercise
   install, upgrade, repair, rollback, uninstall and interrupted recovery.
3. Build a toolkit-only installer after those gates, with capability selection
   and prerequisite guidance; complete fresh-machine and two-endpoint tests.

Use LOOP-010 and [PACKAGING_RESEARCH.md](PACKAGING_RESEARCH.md) for detailed gates.
[D-42](.raiden/state/DECISIONS.md#d-42-toolkit-setup-must-not-install-or-upgrade-windhawk)
requires reuse of independently installed Windhawk. Current registration preserves
foreign values and scopes rollback, but is not a crash-recovery deployment
transaction. Signing and broader distribution need separate work.

## Deferred ideas and design limits

Remaining recommendations in [BEST_NEXT_ADDITIONS.md](BEST_NEXT_ADDITIONS.md),
[REPOSITORY_REVIEW.md](REPOSITORY_REVIEW.md) and
[EXPANSION_RESEARCH.md](EXPANSION_RESEARCH.md) are planning material, not added
implementation commitments. Examples include host sleep prevention and connection
history. Reassess utility, overhead and privacy cost after candidate acceptance;
the original ranking is not authorization to build the remaining proposals.

Further decisions would be needed for:

- **A session chooser:** explicit routing and UI beyond today's ambiguity refusal.
- **Elevated-client control:** the local channel preserves UIPI refusal without
  opening a message filter. A different access-control design requires review.
- **Panel coexistence:** host and client panels on the same PC can overlap at
  their default position; configure separate positions today. Automatic layout
  coordination is not implemented.
- **Broader telemetry or remote-app mirroring:** outside current controls and
  session-state scope; the original brainstorm is not an implementation promise.

## Delivery history

Earlier chronological build notes are preserved in Git, the decision record and
dated [WORK_LOG.md](.raiden/state/WORK_LOG.md) entries. The ledger links its
pre-reconciliation snapshot for historical test narratives. Earlier unverified
build claims do not reopen baseline tests, and baseline verification does not
accept this newer candidate. [HANDOFF.md](HANDOFF.md) is the current technical
continuation guide.
