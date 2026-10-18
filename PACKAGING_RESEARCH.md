# Toolkit-only setup: feasibility and architecture review

> Historical research baseline: P1-P4 have since received source changes and
> regression checks in refinement pass 1. See [REFINEMENT_PLAN.md](REFINEMENT_PLAN.md)
> and [HOME_TEST.md](HOME_TEST.md). They still require live acceptance; the
> original evidence below describes the versions inspected before refinement.

Research date: 2026-09-18. Scope: the existing client v0.10.0, host v0.4.0,
relay v0.2.0, local Windhawk installation, current upstream sources, and
Microsoft documentation. This is research and non-invasive probing, not a
validated installer or a runtime migration.

## Verdict

**Proceed with a toolkit-only setup program while retaining Windhawk.** The
official CLI provides a credible way to install our unpublished mod sources.
There is no architectural finding here that justifies replacing Windhawk's
injection engine merely to simplify distribution. There are, however, concrete
deployment defects and untested prerequisites to resolve before shipping.

The operator's boundary is explicit: setup installs toolkit components only.
It must not install or upgrade Windhawk. Missing or incompatible prerequisites
produce a clear explanation, an official download link, a folder-selection
option where appropriate, and **Check again**. Check these before making changes;
do not describe an unusable installation as complete. See D-42 in the
[decision record](.raiden/state/DECISIONS.md).

A setup executable can exit when finished. No additional service, resident
manager, cloud account, or general plugin framework is needed. The taskbar
controls remain central; future session utilities can be independent tools
when they do not need injection. The proposed mission remains in the
[repository review](REPOSITORY_REVIEW.md), pending adoption.

## What the evidence establishes

| Evidence | Result | What it does not prove |
|---|---|---|
| Read-only inspection of the selected local Windhawk root | Version 1.7.3; development compiler present; no `windhawk-cli.exe` | Automatic installation on another Windhawk version |
| Tagged upstream CLI source | Local source-file installation exists in 2.0.0-alpha.5 | Successful deployment of these two mods on that version |
| Existing mod compile-check script | Both production mods compile/link; settings checks pass | Injection, visuals, or safe unload |
| Fresh relay build into ignored probe output | Production relay compiles/links; Authenticode reports `NotSigned` | COM activation or fresh-machine policy acceptance |
| Mocked production registration helper | Five problematic behaviors reproduced, below | Actual Windows COM launch behavior or a demonstrated exploit |
| Prerequisite detector fixture checks | Five classification cases pass | Complete discovery, trusted executables, compatible APIs, or permissions |
| Earlier operator verification, D-41 | Existing three-component combination worked over several days | New Windhawk versions, installation, upgrade, rollback, or uninstall |

The new [packaging probes](probes/packaging/README.md) are reproducible. They
do not install mods, register the relay, upgrade dependencies, or invoke a
Windhawk executable. The registration probe replaces registry operations with
in-memory mocks. The newly built production relay was not run. No new physical
two-machine or disposable-VM test was performed.

## Upstream support and its limits

The release list inspected during this review places **2.0.0-alpha.5** ahead of
the alpha.1 used in the earlier discussion; 1.7.3 remains the stable line shown
there. Alpha.5 is explicitly a prerelease. Its fixes include mod-storage
integrity labels and shutdown deadlocks, both relevant to this toolkit's shared
status file and unload behavior. Select and validate an exact release instead
of promising compatibility with every version starting with 2.0.
[Releases](https://github.com/ramensoftware/windhawk/releases),
[alpha.5 notes](https://github.com/ramensoftware/windhawk/releases/tag/2.0.0-alpha.5).

The tagged CLI supports `mod install --file <source> --disabled`. The local-file
implementation uses a `local@` identity and compiles locally; Windhawk's optional
development tools are therefore required for this route. These mods do not have
to be accepted into the public catalog. Toolkit releases must own their updates
and preserve settings and local edits. Do not substitute catalog `mod update`
for reinstalling toolkit-managed local source.
[Installation implementation](https://github.com/ramensoftware/windhawk/blob/2.0.0-alpha.5/src/windhawk-core/cli/src/commands/mods/install.rs).

CLI contracts are changing: alpha.5's settings setter accepts `key=value`
pairs, unlike the earlier alpha.1 syntax inspected in the initial discussion.
The current data-import default also overwrites existing mods. A broad restore
of Windhawk user data is consequently a poor substitute for a narrowly scoped
toolkit installer. Use explicit component IDs, checked exit codes, structured
output, and a version-specific adapter.
[Current CLI arguments](https://github.com/ramensoftware/windhawk/blob/2.0.0-alpha.5/src/windhawk-core/cli/src/args.rs).

Discovery must respect the selected installation. Upstream resolves an explicit
`--app-root`, an environment-derived root, or the CLI's own directory, and reads
storage configuration from `windhawk.ini`. Installed and portable storage
differ. Do not hardcode internal registry layout, guess the compiler location,
or bring a CLI from a different Windhawk version. The preliminary probe checks
one selected root only; ambiguous or custom installations need user selection.
[App-root resolution](https://github.com/ramensoftware/windhawk/blob/2.0.0-alpha.5/src/windhawk-core/cli/src/app_root.rs),
[storage implementation](https://github.com/ramensoftware/windhawk/blob/2.0.0-alpha.5/src/windhawk-core/windows/src/storage.rs).

An open upstream issue reports that writing CLI commands require elevation,
despite the newer UI's separate broker. Its author tested alpha.1 and alpha.3;
we did not reproduce this on alpha.5. Treat elevation as a required validation
case, not as a promise of silent installation. Running all setup work under
different administrator credentials could register the relay for the wrong
Windows user. Separate the original user's operations from privileged changes.
[Upstream issue #1050](https://github.com/ramensoftware/windhawk/issues/1050).

For the current 1.7.3 installation, retain documented manual installation.
Do not emulate Windhawk's private storage writes or automate editor clicks as
the primary fallback. If the user chooses to update Windhawk independently,
setup can check the resulting installation again.

## Concrete deployment findings

The source for findings P1-P3 is
[RegistryHelper.h](dvc-plugin/relay/plugin/RegistryHelper.h) and the client's
`GetRelayPluginExePath` / `IsAuthorizedRelaySender` implementation in the
[client mod](taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp).
The retained probe executes the actual helper against mocked registry APIs.

| ID | Finding and evidence | Required outcome before packaging |
|---|---|---|
| P1 | Registration writes an unquoted `LocalServer32` executable path, without `ServerExecutable`. The client compares that raw registry value to a process image path. A path with spaces reproduces the registration problem; quoting it alone reproduces a comparison mismatch. | Coordinate both sides: quoted launch command, unquoted executable identity, and intentional legacy-registration handling. Verify real activation from a path containing spaces. |
| P2 | Failure creating the second registration key leaves the first write in place; reproduced with injected access denial. | Preserve previous values and roll back partial writes. Repair must distinguish owned entries from conflicting registration. |
| P3 | Unregister tries both HKCU and HKLM. With HKCU removal denied and HKLM entries absent, it returns success; both behaviors reproduced. | Explicit scope and ownership, accurate partial-failure reporting, and no deletion of another installation's registration. |
| P4 | Source inspection: `/machine` registration exists, while client relay identity lookup uses HKCU. | Prefer an initially supported per-user relay deployment; do not expose machine scope until lookup, user identity, and removal semantics agree. |
| P5 | Source inspection: the relay waits indefinitely for its shutdown event; final COM-reference release does not end the server. | Establish correct server lifetime and a non-disruptive update strategy. An installer must not assume disconnect releases the executable for replacement. |

Microsoft documents the distinct launch-command and executable-identity values
for `LocalServer32`. This supports the P1 repair direction; the mocked probe
does not prove an actual mislaunch or exploitation on this machine.
[Microsoft LocalServer32 reference](https://learn.microsoft.com/en-us/windows/win32/com/localserver32).

Installing a mod is only one step in the toolkit deployment. Upstream has its
own compilation and commit orchestration; this does not make mod installation,
relay file replacement, COM registration, and enablement a single transaction.
Maintain a toolkit-owned operation journal and prior component/settings state.
If restoration fails, report exactly what remains changed. Avoid full Windhawk
data imports that could replace unrelated settings.
[Upstream install orchestration](https://github.com/ramensoftware/windhawk/blob/2.0.0-alpha.5/src/windhawk-core/core/src/services/install/orchestrate.rs).

## Long-term architectural issues

**Windows shell compatibility is the largest continuing maintenance cost.**
The embedded panels depend on private Explorer symbols and a particular XAML
tree. A standalone EXE would still need injection to preserve those embedded
panels, or would need a different UI. Replacing Windhawk would add ownership of
the injection machinery without removing the private-shell dependency. Keep
supported Windows builds explicit and make a missing hook diagnosable.
Windhawk documents missing-symbol and compatibility failures; an installer
cannot eliminate them.
[Windhawk troubleshooting](https://github.com/ramensoftware/windhawk/wiki/Troubleshooting).

**Session identity must precede multi-session features.** Class-name window
lookup and a shared status snapshot do not establish a unique mstsc instance.
Adding a session chooser before fixing routing could send an action to the
wrong window. Initially detect ambiguity and disable affected controls; later
introduce a validated binding between the DVC connection and its local client.
A COM server's parent process is not a reliable substitute for this binding.

**Replies need to distinguish transport from action success.** The relay sends
status even when command forwarding fails. A received reply therefore proves
the reply path worked, not that minimize/fullscreen executed. Before more
commands are added, define versioned request identity and explicit outcomes,
including unavailable, unsupported, and failed. Keep mixed host/client versions
safe because the two PCs will not always update together. Pin field layout as
well as structure sizes; share or generate definitions only in a way compatible
with single-file mod installation.

**Shutdown and bounded overhead need attention.** Host work uses detached
threads and a bounded unload wait, which does not establish that all work has
finished before code unloads. The relay writes routine poll logs without
rotation. Use owned/cancellable work, verified teardown, bounded diagnostic
logs, and change-based logging. Measure idle costs on one and several sessions;
do not invent a memory/CPU budget from compilation results. These fixes improve
the existing toolkit without adding a resident process.
[Windhawk mod lifetime](https://github.com/ramensoftware/windhawk/wiki/Mod-lifetime).

**The transport is legitimate; its edge contracts still need tests.** Microsoft
documents DVCs and client AddIn registration. The repository also retains its
earlier successful out-of-process activation probe. There is no finding here
requiring a replacement TCP service. The host's 256-byte read buffers should be
reviewed against the documented `CHANNEL_PDU_LENGTH` requirement and DVC header
framing. This is a source/API-contract concern, not a newly reproduced failure.
[DVC overview](https://learn.microsoft.com/en-us/windows/win32/termserv/dynamic-virtual-channels),
[client registration](https://learn.microsoft.com/en-us/windows/win32/termserv/virtual-channel-client-registration),
[WTSVirtualChannelRead](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsvirtualchannelread).

**Host/client are capabilities, not permanent machine identities.** Offer
"Connect from this PC", "Connect to this PC", and "Both". An RDP session at
setup time does not determine the machine's future use. Runtime presentation
can use session observations, but automatic coexistence of the two panels is
still separate feature work. Start with Windows 11 x64 and classic `mstsc.exe`
on explicitly tested builds; do not imply Windows App, ARM64, or all Windows
Server versions are covered. Microsoft continues to document mstsc for Windows
11; this is not a guarantee about its indefinite future.
[mstsc reference](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/mstsc).

**Trust and distribution need accurate claims.** Existing window/path checks
are not a hardened boundary against a hostile process running as the same
user; a supplied sender HWND is not proof of the true message sender. Keep
recovery actions narrowly scoped. For release, sign distributed executables
and test fresh-machine policy behavior; a signed installer does not sign the
DLLs that Windhawk compiles locally. Checksums alone are not publisher identity.
The local relay build is unsigned. Do not solve policy failures by silently
weakening Windows protections.
[Microsoft code-signing guidance](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control).

The public mod repository asks for single-file submissions, but this research
does not establish a blanket official prohibition on companion EXEs. Catalog
acceptance remains unconfirmed and is unnecessary for the proposed GitHub
distribution. Packaging can remove setup friction; it cannot establish future
market demand.
[Mod submission requirements](https://github.com/ramensoftware/windhawk-mods#submitting-a-new-mod).

## Proposed setup behavior

1. Explain the selected capabilities and inspect the chosen Windhawk root.
   If Windhawk, a supported CLI, or required development tools are unavailable,
   stop before writes and provide precise next steps. Never install or upgrade
   Windhawk automatically.
2. Detect existing toolkit components, local source edits, registration scope,
   and running sessions. Resolve ownership and capture the prior working state.
3. Stage a matched toolkit release at a stable location. Verify its manifest;
   install selected local mods disabled through the validated CLI adapter.
4. Register the client relay for the intended user, verify the resulting entries,
   and enable only the selected components after deployment checks succeed.
5. Distinguish "components installed" from "runtime verified". Report restart or
   session-reopen needs honestly; never terminate a live RDP session to finish.
6. Repair/update/uninstall only owned components. Preserve unrelated mods,
   Windhawk itself, and user settings unless their removal is explicitly chosen.

## Validation gates and next work

| Gate | Can progress without physical access? | Acceptance |
|---|---|---|
| Prerequisite classification | Yes; preliminary probe complete | Missing root, CLI, tools, and custom paths classify honestly. Add ambiguity, malformed input, unreadable files, and version/capability cases before production use. |
| Registration and lifecycle repairs | Yes; implementation next | P1-P5 addressed with meaningful failure/ownership tests; no production registration required for mock tests. |
| Disposable Windows VM | Yes, when a suitable VM is available | Exact Windhawk candidate installs both real mods; enable/disable, upgrade, settings retention, edited-source detection, rollback, repair, uninstall. Include standard user and different-admin UAC credentials, paths with spaces, and unrelated mods. |
| Fresh-machine trust and resource checks | Requires Windows test environment | Signature/policy outcomes understood; logs bounded; idle costs measured; missing symbols/tools/network produce useful diagnostics. |
| Two-endpoint RDP behavior | Two suitable Windows VMs or physical PCs | Correct user registration, actual DVC activation, action outcomes, mixed versions, reconnect, multiple sessions, panel coexistence, and unload during work. |
| Installer release | After preceding gates | Supported versions documented; no prerequisite installation; recoverable failures; reproducible artifacts and installation guide. |

The recommended next implementation is the small deployment-reliability pass,
followed by the VM experiment, then the installer UI. Physical access is not
needed to repair the proven registration defects or design the compatibility
adapter. Live Windows/RDP validation remains essential before a release claim.
Open verification work is tracked in LOOP-010 in
[OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md).
