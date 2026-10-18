# Packaging regression checks and prerequisite probe

These checks support [REFINEMENT_PLAN.md](../../REFINEMENT_PLAN.md). The earlier
five-defect reproduction was converted into regression checks as the first
refinement pass repaired the production code. Historical findings remain in
[PACKAGING_RESEARCH.md](../../PACKAGING_RESEARCH.md).

Run from the repository root:

```powershell
./probes/packaging/run-registry-probe.ps1
./probes/packaging/run-client-path-probe.ps1
./probes/packaging/check-prerequisite-cases.ps1
./probes/packaging/prerequisite-probe.ps1 -Json
./taskbar-integration/compile-check.ps1
```

The two C++ runners accept `-Compiler` for another compatible MinGW clang.
They build and run tests under ignored `out/packaging-probe/`.

- **Registry helper:** 31 checks execute the production `RegistryHelper.h`
  against in-memory registry mocks. Covers fresh/legacy registration, each
  write/deletion failure, rollback failure, denied access, conflicting paths
  and types, explicit scope, foreign data preservation, and overlong paths.
- **Client path reader:** 20 checks compile the exact marked functions from the
  production mod, extracted by the runner. Covers modern and legacy formats,
  malformed values, read errors, oversized paths, and output bounds. It does
  not load the mod or exercise the entire message receiver.
- **Prerequisites:** five fixture cases cover missing root, missing CLI,
  missing tools, an empty compiler setting, and a custom portable path. Inert
  fixture EXEs are never executed. File presence never claims readiness.

Exit 0 now means the regression expectations passed. Neither C++ test accesses
Windows registry data. These tests do not establish real COM activation or
crash-safe deployment.

`prerequisite-probe.ps1` reads file presence, version metadata, and selected
storage configuration from one root. It does not invoke Windhawk, inspect mod
settings, read the command secret, or write registry data. Use `-WindhawkRoot`
for a selected non-default installation. It is not complete installer discovery.

The retained local research observation was Windhawk 1.7.3, compiler present,
CLI absent: `AutomaticInstallUnavailable`. No automatic install was attempted.

The candidate relay also passed six safe CLI checks: `/help` exits 0;
`/unknown`, `/register /unregister`, `/register /machine`, `/machine`, and
`-Embedding /register` exit 2 without starting the server or modifying registry.
Both mods passed compilation, linking, and the existing settings-block checks.
The relay candidate compiled/linked; its Authenticode status is `NotSigned`.
Live acceptance remains the [home checklist](../../HOME_TEST.md).
