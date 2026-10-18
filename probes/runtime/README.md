# Runtime reliability regression checks

Run `./probes/runtime/run-checks.ps1` from the repository root. The runner uses
Windhawk's MinGW clang by default and accepts `-Compiler` for a compatible
compiler. All outputs, temporary files, test logs and child fixture programs
stay in a new ignored `out/runtime-checks/` directory.

The tests exercise the canonical `ToolkitRuntime.h` used by the relay and
require the exact marked block to match both standalone Windhawk sources.
Snapshot layout checks compile the actual extracted client structure against
the relay's mirror, including every field offset, rather than checking only
their total size.

- `runtime-contract.cpp`: strict requests and correlated replies, legacy and
  malformed frames, target mismatch, freshness and observed window state;
  complete snapshot publication under concurrent readers; bounded worker
  admission and stop/join behavior; real message-window/process fixtures for
  one client, a second connection dialog, two receivers, foreign receiver,
  changed creation time and an exited process. Fixture images use a distinct
  test filename; no real mstsc process or RDP connection is started or changed.
  Also covers strict DVC headers, multiple frames within one process, foreign
  frame fallback, occupied temporary files and extracted input cleanup policy.
- `relay-lifetime.cpp`: actual relay factory/plugin/channel implementation and
  COM process-reference functions, without registering or activating a class.
  Tests final-reference shutdown, channel teardown, no-action legacy replies,
  malformed requests, query correlation and retained log bounds. LOCALAPPDATA
  is overridden for this test process only. It does not invoke `/register` or
  send a command to a live receiver.
- `client-lifetime.cpp`: the extracted production COM sink teardown function,
  including failed unsubscription retaining references and the unload signal.
  Also executes the native subclass drain policy with active/completed callbacks.
- `host-clipboard.cpp`: the extracted production clipboard restart function,
  with inert process fixtures covering failed/uncertain termination, wrong
  identity/session, missing helper and automatic respawn. It never touches the
  clipboard or any real helper process.
- `test-diagnostics.ps1`: diagnostic classifications, real binary snapshot
  parsing, freshness/identity handling and default endpoint-name omission.
  The runner also invokes diagnostics in a fresh Windows PowerShell 5.1 process,
  when present, to catch entry-point compatibility errors masked by dot-sourcing.

Snapshot replacement can fail transiently under reader contention. The test
requires complete generations and progress; it does not require every attempted
replacement to succeed. Production retains the previous record on failure and
uses freshness to fail closed if updates stop.

The tests do not load a Windhawk mod or validate Explorer's dispatcher,
cross-machine DVC activation, foreground/input behavior, visual rendering,
or installed-runtime resource use. Those checks are in
[TOP_THREE_ACCEPTANCE.md](../../TOP_THREE_ACCEPTANCE.md).

Also run the existing compile/link/settings check and packaging regressions:

```powershell
./taskbar-integration/compile-check.ps1
./dvc-plugin/relay/build.ps1
./probes/packaging/run-registry-probe.ps1
./probes/packaging/run-client-path-probe.ps1
./probes/packaging/check-prerequisite-cases.ps1
```

In restricted execution environments, set process-local TEMP/TMP to a new
directory under `out/` before invoking clang. Do not change machine settings or
reuse another build's temporary output directory.
