# Refinement plan and first home-test candidate

Started 2026-09-18. The operator requested remote-safe refinement toward a
candidate to test at home. Keep the installed Windhawk, loaded mods, registered
relay, and active RDP sessions untouched while preparing source and artifacts.
The prerequisite boundary remains D-42: toolkit setup never installs or
upgrades Windhawk.

| Pass | Deliverable | Completion gate |
|---|---|---|
| 1 — Registration reliability | Coordinated client/relay path handling, scoped ownership checks, recovery from failed registry writes, explicit command errors, candidate bundle | Mocked failure/compatibility tests; both mods and relay compile/link; manual home-test guide |
| 2 — Runtime lifecycle and overhead | Correct COM server shutdown, owned host workers, bounded diagnostics | Teardown and failure tests; Windows unload/reconnect checks at home |
| 3 — Session and protocol reliability | Ambiguity guard, explicit action outcomes, mixed-version compatibility | Two-session routing and mixed-component tests before adding new controls |
| 4 — Deployment experiment | Exact-version Windhawk adapter, prerequisite discovery, toolkit operation journal | Disposable-VM install/upgrade/repair/rollback/uninstall, including different-admin credentials |
| 5 — Toolkit-only installer | Capability selection, clear prerequisite guidance, component repair/removal | Previous gates pass; fresh-machine and two-endpoint acceptance |

## Current implementation increment

The operator authorized diagnostics, action feedback, a multi-session ambiguity
guard and their prerequisite reliability fixes. Source for passes 2 and 3 is
now implemented in the session-reliability candidate; the new
[acceptance guide](TOP_THREE_ACCEPTANCE.md) defines runtime validation. These
changes do not establish completed home acceptance or installer readiness.
Passes 4 and 5 remain later deployment work.

- Owned host worker with a bounded queue; joined client message/force workers;
  tracked and drained taskbar callbacks. A hung/disconnected UI can require
  retaining inert module code until process exit.
- COM server lifetime follows plugin/channel/factory references. Relay logs
  move to bounded per-user storage; routine polling is silent.
- Protocol v3 binds commands to PID plus creation time and correlates results
  with requests. Multiple mstsc processes, including dialogs, pause controls.
  Old protocols have no action fallback; snapshot v3 uses a separate filename.
- Panel feedback separates accepted from observed state, unknown outcomes are
  never retried, and failed host polls repaint unavailable state.
- `scripts/Get-ToolkitDiagnostics.ps1` provides read-only local observations
  and explicit local export with default endpoint-name omission.

Use `scripts/build-refinement-candidate.ps1` for a new `session-reliability-*`
bundle of the current sources. Earlier `refinement-1-*` bundles are retained.
The following pass-1 acceptance section is the historical candidate record.

The first candidate retains
the existing host and wire protocol. It targets the existing Windhawk workflow
so home testing does not require adopting an alpha release. Do not advertise it
as installer-ready or deploy it into the active remote session.

## Pass 1 acceptance

**Implemented, awaiting home acceptance:** client 0.10.1, relay 0.2.1, unchanged
host 0.4.0. Local verification: 31 registration regression checks, 20 client
path-reader checks, five prerequisite fixtures, six safe CLI argument checks,
and both mod compile/link/settings checks passed. The relay candidate also
compiled and linked. No registry mutation, mod reload, or relay-server launch
was performed. See [HOME_TEST.md](HOME_TEST.md) for deployment and rollback order.

The pass-1 candidate was built with the earlier candidate builder.
Its output is a separate `out/candidates/refinement-1-*` folder and ZIP, with
mod sources, relay EXE/source, build manifest, and SHA-256 checksums. This build
script does not run all verification automatically; use the commands in the
[probe guide](probes/packaging/README.md) plus `taskbar-integration/compile-check.ps1`.

- New relay registration writes a quoted launch command and a separate raw
  executable identity; the updated client reads the identity and continues to
  accept the old registration format. Malformed identities fail closed.
- Registration and removal inspect all owned values before changing them;
  conflicting paths or AddIn identities stop without overwriting them.
- A failed operation restores previously changed values where possible and
  reports rollback failure separately. Empty registry containers can remain;
  foreign values and subkeys are never recursively removed.
- Default removal affects HKCU only. Machine registration is unsupported in
  this candidate; explicit machine removal remains available for old entries.
- A different executable location cannot silently take ownership. Use the
  registered location for the replacement and migration, with sessions closed
  at home. Moving locations remains deliberate future deployment work.
- Candidate files go to a separate output directory; no build step registers,
  launches, or replaces the production relay. Include source mods, hashes,
  build provenance, and a home-test checklist.

The complete rationale and remaining architectural risks are in
[PACKAGING_RESEARCH.md](PACKAGING_RESEARCH.md). Live acceptance remains tracked
by LOOP-010 in [OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md).
