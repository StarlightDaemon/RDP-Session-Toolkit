# Open Loops

This is the current loop ledger. The
[pre-reconciliation snapshot](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md)
preserves the earlier narrative verbatim, including superseded statements.
Resolve relative links inside that archive from this directory, its original
location; use the dispositions below for current work.

- **LOOP-011 — Diagnostics and session reliability candidate acceptance — OPEN
  2026-10-16.** The operator authorized the top-three additions from
  `BEST_NEXT_ADDITIONS.md`, including prerequisite lifetime, logging and stale
  state fixes. Source implementation and isolated regression checks are
  complete; real Explorer/RDP acceptance remains. Start with
  [HOME_UPDATE_TEST_ROLLBACK.md](../../HOME_UPDATE_TEST_ROLLBACK.md), then the
  extended [TOP_THREE_ACCEPTANCE.md](../../TOP_THREE_ACCEPTANCE.md): matched component
  controls, feedback and expiry, two mstsc instances including an unconnected
  dialog, changed/old identities, mixed versions without action fallback,
  relay disappearance, COM process exit, clipboard recovery and unload during
  work. Verify normal dispatcher cleanup plus the inert-module retention
  fallback for hung/disconnected UI; measure actual resource use and check
  multiple monitors, DPI and themes. No production install, registry mutation,
  live mod reload or two-endpoint verification occurred in this pass.
  The pre-home-test review corrections are described in
  [IMPLEMENTATION_REVIEW.md](../../IMPLEMENTATION_REVIEW.md) under D-45. Include
  multiple frames within one process, foreign-frame refusal, uncertain shortcut
  and clipboard outcomes, and restart after retained-code unload in acceptance.

- **LOOP-010 — Toolkit packaging readiness — OPEN 2026-09-18.**
  Registration/path/scope refinements and runtime/session source changes for
  passes 1–3 are implemented and locally regression-checked; see
  [REFINEMENT_PLAN.md](../../REFINEMENT_PLAN.md). Manual deployment and runtime
  acceptance remain to be performed; LOOP-011 carries the new runtime gates.
  The remaining packaging work is passes 4–5: validate an exact Windhawk CLI
  adapter in a disposable Windows environment, implement the deployment
  transaction, and exercise install/upgrade/repair/rollback/uninstall, including
  ownership, failure recovery and different-admin credentials. The manual
  candidate and its published ZIP do not establish installer readiness.
  [PACKAGING_RESEARCH.md](../../PACKAGING_RESEARCH.md) records the deployment
  findings and acceptance matrix. D-42 governs the prerequisite boundary.

- **LOOP-001 — Client-mod live test — RESOLVED 2026-09-06 (operator-reported).**
  - Status: Closed — resolved for the historical client 0.10.0, host 0.4.0,
    relay 0.2.0 combination on the existing operator report.
  - Evidence: D-41 and the September 6 verification summary explicitly cover
    this loop. Earlier unverified-build notes are superseded, not new findings.
  - Acceptance boundary: this is not acceptance of the later registration,
    protocol, lifetime or diagnostic candidates; LOOP-010 and LOOP-011 retain
    those separate gates. No new runtime or two-machine test was performed.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-002 — Relay sender validation and instance-targeting residual — OPEN.**
  - Status: Open — historical sender-validation work is resolved; targeting
    residual and candidate guard acceptance are distinct.
  - Verified history: the original path validation was recorded complete on
    2026-08-21. It is advisory identity checking, not hostile same-user isolation.
  - Current scope: D-44/D-45 bind process and creation identity and refuse
    ambiguous processes/frames. They do not implement a multi-session chooser.
    Candidate runtime validation belongs to LOOP-011 and remains pending.
  - Scope boundary: a multi-session selector is a separate future product
    decision, not an unimplemented promise of this loop. Conservative refusal
    is the current design; its live validation is the remaining gate here.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-003 — DVC probe two-machine live test — RESOLVED 2026-08-21.**
  - Evidence: [dvc-plugin/TESTING.md](../../dvc-plugin/TESTING.md) records the
    successful `IWTSPlugin::Initialize`, `CreateListener` and received-signal
    hardware test. The throwaway probe registration was removed afterward.
  - Scope: activation and signal delivery for the out-of-process COM model.
    Historical production minimize verification is LOOP-006; candidate
    protocol/lifetime acceptance remains under LOOP-011.
  - Naming note: the original driving task called this LOOP-001, but that ID
    belongs to the client-mod live test in this ledger. The probe is LOOP-003.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-004 — Debug self-test timed-send cleanup — RESOLVED 2026-10-16
  (source and compile/link check).** `SendRelayTestMinimize` now uses a bound
  PID/creation-time request and `SendMessageTimeoutW` with a bounded wait.
  The untimed-send source finding is fixed; no further source cleanup is
  promised by this loop. Candidate runtime acceptance remains under LOOP-011.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-005 — Host-mod live test — RESOLVED 2026-09-06 (operator-reported).**
  - Status: Closed — resolved for the historical client 0.10.0, host 0.4.0,
    relay 0.2.0 combination on the existing operator report.
  - Evidence: D-41 and the September 6 verification summary explicitly cover
    this loop. Earlier unverified-build notes are superseded, not new findings.
  - Acceptance boundary: this is not acceptance of the later registration,
    protocol, lifetime or diagnostic candidates; LOOP-010 and LOOP-011 retain
    those separate gates. No new runtime or two-machine test was performed.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-006 — Full end-to-end minimize feature live test — RESOLVED
  2026-08-21 (operator-observed).**
  - Evidence: the operator observed a host Minimize click traverse the DVC,
    production relay and client receiver and minimize the correct mstsc frame.
    It was reconfirmed across further sessions on 2026-09-01; D-41 and the
    September 6 work-log entry record the later verified baseline.
  - Scope: historical production behavior, not acceptance of the current v3
    protocol or lifetime changes. D-37 subsequently repaired the delivery race;
    the candidate's new end-to-end validation remains under LOOP-011.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-007 — Client-mod feature live test — RESOLVED 2026-09-06 (operator-reported).**
  - Status: Closed — resolved for the historical client 0.10.0, host 0.4.0,
    relay 0.2.0 combination on the existing operator report.
  - Evidence: D-41 and the September 6 verification summary explicitly cover
    this loop. Earlier unverified-build notes are superseded, not new findings.
  - Acceptance boundary: this is not acceptance of the later registration,
    protocol, lifetime or diagnostic candidates; LOOP-010 and LOOP-011 retain
    those separate gates. No new runtime or two-machine test was performed.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

- **LOOP-008 — Taskbar-embedded client widget live test — RESOLVED 2026-09-06 (operator-reported).**
  - Status: Closed — resolved for the historical client 0.10.0, host 0.4.0,
    relay 0.2.0 combination on the existing operator report.
  - Evidence: D-41 and the September 6 verification summary explicitly cover
    this loop. Earlier unverified-build notes are superseded, not new findings.
  - Acceptance boundary: this is not acceptance of the later registration,
    protocol, lifetime or diagnostic candidates; LOOP-010 and LOOP-011 retain
    those separate gates. No new runtime or two-machine test was performed.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

### LOOP-SWEEP-20260831-001 — RDP Session Toolkit doctor WARN — state_unrecognized

- Status: RESOLVED 2026-09-06
- Resolution: moved the non-schema `EXPANSION_RESEARCH.md` and `ROADMAP.md`
  documents from `.raiden/state/` to the repository root, preserving their
  tracked content while restoring the v2 state-file set.
- Verification: `raiden_updater.cli doctor --instance <path to RDP Session Toolkit>`:
  10 OK, 0 WARN, 0 FAIL.
- Check: doctor:RDP Session Toolkit:state_unrecognized

- **LOOP-009 — September 1 second-session batch live test — RESOLVED 2026-09-06 (operator-reported).**
  - Status: Closed — resolved for the historical client 0.10.0, host 0.4.0,
    relay 0.2.0 combination on the existing operator report.
  - Evidence: D-41 and the September 6 verification summary explicitly cover
    this loop. Earlier unverified-build notes are superseded, not new findings.
  - Acceptance boundary: this is not acceptance of the later registration,
    protocol, lifetime or diagnostic candidates; LOOP-010 and LOOP-011 retain
    those separate gates. No new runtime or two-machine test was performed.
  - History: [verbatim pre-reconciliation ledger](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md).

## References

- [Historical verification summary and full original entries](SNAPSHOTS/OPEN_LOOPS-before-ledger-reconciliation-2026-10-17.md#verification-update--2026-09-06)
- [Operator verification decision](DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch)
- [September 6 operator verification and reconciliation record](WORK_LOG.md#2026-09-06-operator-verification-and-documentation-reconciliation)
