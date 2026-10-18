# RDP Session Toolkit best next additions

Review snapshot: October 16, 2026. Scope: the current working tree on `main`,
including the uncommitted refinement candidate, against base commit `13430fd`.
Product comparisons use the linked first-party documentation and repositories.
This is an architecture and feature review; it does not add runtime acceptance
evidence or replace a security audit.

**Recommendation:** first make the toolkit explain its failures and target the
right session, then add clearer session identity and useful display presets.
The strongest small host utility is an opt-in request to stay awake during an
active RDP session. Keep the taskbar interface compact and preserve the native
Microsoft client workflow.

## What the toolkit offers

The toolkit is a companion to Windows Remote Desktop Connection (`mstsc.exe`).
It puts session controls in the local taskbar and in the remote desktop's
taskbar, with a small Dynamic Virtual Channel (DVC) relay between them. Its
advantage is convenient control of the session already in use, including
controlling the client window while working inside the remote desktop.

The local panel works without installing the remote widget or relay. The host
widget's Disconnect and Restart Clipboard actions also work locally on the
host. Its Minimize and fullscreen controls depend on the client relay and mod.
This partial-install usefulness should survive expansion.

### Repository and architecture

| Area | Responsibility and implications |
|---|---|
| [Client mod](taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp) | One single-file Windhawk mod with separate `mstsc.exe` and local `explorer.exe` branches. Session actions, connection-bar suppression, optional overlay, hang detection, state publication, and taskbar presentation live here. |
| [Host mod](taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp) | Injects into remote `explorer.exe`; queries its own WTS session, supplies local recovery/control, sends DVC requests, and displays the client's reported window mode. |
| [Relay implementation](dvc-plugin/relay/plugin/RdpRelayPlugin.cpp) and [server entry point](dvc-plugin/relay/plugin/Main.cpp) | Separate COM LocalServer executable on the client. Activated by RDP; forwards host commands to the client mod and returns a small status reply. |
| [Wire contracts](dvc-plugin/relay/common/RelayIds.h) | One-byte Minimize, fullscreen-toggle, and status-query requests; versioned status reply. Some definitions are mirrored in the mods because installation uses standalone source files. |
| [Registration helper](dvc-plugin/relay/plugin/RegistryHelper.h) | Candidate code checks ownership, separates the quoted launch command from executable identity, scopes removal, and attempts rollback after ordinary failures. |
| [Build checks](taskbar-integration/compile-check.ps1), [relay build](dvc-plugin/relay/build.ps1), [candidate builder](scripts/build-refinement-candidate.ps1) | Mod compile/link and narrow settings checks; separate native relay build; isolated manual candidate bundles with source provenance and hashes. The candidate builder does not run the complete verification suite. |
| [Packaging probes](probes/packaging/README.md) | Mocked registration/path regression checks and preliminary prerequisite classification. These do not establish live COM, taskbar, installer, or two-machine behavior. |
| [Activation probe](dvc-plugin/probe/README.md) | Retained historical experiment, separate identifiers and registration; no additional production dependency. |
| RAIDEN Instance state and research documents | Development continuity and planning. They are not runtime components. |

The source mods target Windows x64; taskbar integration depends on Windows 11
XAML internals and symbol hooks. The relay build script's other architecture
options do not establish end-to-end ARM64 or 32-bit toolkit support.

The local state path is `mstsc branch -> versioned snapshot file -> Explorer
panel`. The local command path is `Explorer panel -> shared-secret command
message -> mstsc branch`. The network path is `host widget -> DVC -> relay ->
client mod`, with a client-state reply on that same short-lived channel.

There is **no general client-to-host command service** and no generic host
health feed into the local panel. A return status reply is not that service.
Adding a local button to restart the remote clipboard helper, for example,
needs a new host command receiver and session-bound protocol design.

### Existing capability baseline

| Capability | Disposition for this review |
|---|---|
| Connection-bar hiding; local taskbar panel; optional floating overlay | Already implemented. Preserve the panel as the main surface and the overlay as optional. |
| Minimize, Restore, fullscreen/windowed, Disconnect | Already implemented. New work is clearer targeting and outcomes, not additional copies of these buttons. |
| Reconnect display mode and manual Force reconnect | Already implemented, opt-in. Existing choices include fullscreen, windowed dimensions, and all monitors. Named presets and selected monitors would be additions. |
| Window-hang warning | Already implemented using `IsHungAppWindow`. It detects a client window that stops responding, not every network stall, black screen, or frozen remote application. |
| Local idle time | Already implemented and intentionally measures input inactivity on the connecting computer. It is not remote-session idle time. |
| Connection-quality tooltip | Event sink and presentation exist. Missing reports are shown as unavailable/waiting. Do not count another quality icon as a new feature or assume numeric telemetry is consistently available. |
| Host client-name/state display and console gating | Already implemented through WTS queries and notifications. |
| Host Restart Clipboard and missing-helper warning | Already implemented. Presence does not establish clipboard health. This was the earlier expansion review's strongest recommendation and is no longer new work. |
| DVC replies and host polling of client window mode | Already implemented. This answers a narrow client-state question, not arbitrary remote diagnostics. |
| Per-monitor widgets, live theme changes, tooltips and accessible button names | Already implemented. These need preservation and regression coverage rather than being proposed again. |
| Safer registration and executable-path parsing | Implemented in the refinement candidate, awaiting its acceptance process. Follow [REFINEMENT_PLAN.md](REFINEMENT_PLAN.md), [HOME_TEST.md](HOME_TEST.md), and [LOOP-010](.raiden/state/OPEN_LOOPS.md). |
| Thumbnail toolbar and session-duration display | Deliberately removed under D-33 and D-36. Do not reintroduce them as feature parity work. |

The operator's baseline verification is recorded in
[D-41](.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).
The candidate design is recorded in D-43. Older build notes and uncommitted
source must not be conflated with newly verified runtime behavior.

## Source findings that affect the next additions

These findings remain relevant in the current candidate; its registration
repairs do not address the session or runtime-lifetime design.

| Finding | Source evidence | Consequence |
|---|---|---|
| Displayed session and command target are not bound together | Client `GetLocalWidgetStatusFilePath` selects one shared file; `WriteLocalWidgetStatus` publishes a PID; `SendLocalWidgetCommand` selects a receiver by class name. Relay `ForwardFirstByte` also picks the first matching message window. | Multiple instances can produce state/action disagreement. Add an ambiguity guard before a chooser or more cross-machine commands. |
| A valid relay reply does not prove action success | `ForwardFirstByte` returns no result to `OnDataReceived`; the relay builds a status reply even after a failed forward. Host `SendRelayCommandAsync` calls a parsed reply “delivery confirmed.” Fullscreen handlers can return handled without establishing a state change. | Distinguish relay receipt, receiver acceptance, and observed outcome. A timeout can leave the outcome unknown. |
| Expiration may not repaint the host label | Host `GetClientView` computes staleness when read, but failed `PollClientStateAsync` requests do not schedule a UI refresh. Unchanged WTS state also need not repaint. | Test loss of the relay after a known mode; explicitly refresh the known-to-unknown transition. |
| Normal relay traffic has unbounded persistent logging | `RelayLog` appends next to the EXE; routine channel creation, query, reply and teardown all log. No rotation is implemented. | Bound logs before increasing telemetry or adding retained history. A writable per-user destination is preferable for distribution. |
| Relay process lifetime is incomplete | `RunServer` waits indefinitely for a shutdown event; releasing plugin objects does not signal it. The factory lock counter does not implement last-client shutdown. | Finish COM lifetime work before dependable update/repair/removal. |
| Worker lifetime needs stronger ownership | Host relay and clipboard jobs detach; unload waits a fixed interval and can return while counters remain nonzero. XAML removal is also dispatched asynchronously. | Require demonstrable completion of workers and queued callbacks before unload; audit client stop paths as part of the same work. |
| Internal Windows hooks create compatibility cost | Both mods hook `TaskListButton::UpdateVisualStates` and traverse taskbar XAML. | Surface unsupported combinations and record tested Windows/Windhawk builds. Successful compilation is insufficient. |
| UI settings still carry historical advice | Client embedded README/settings suggest keeping the watchdog on to maintain snapshots, although `LocalWidgetWndProc` now has its unconditional timer. | Correct this small documentation inconsistency in a later implementation pass; the watchdog should be chosen for hang detection. |

The local secret and relay image-path guard are useful checks within the
existing personal-use design. They are not a strong boundary against hostile
processes running as the same user. More powerful remote operations would need
a fresh trust review; the relay's sender HWND is supplied by the sender.

## Comparable offerings and useful lessons

These are feature comparisons, not hands-on benchmarks. Open-source project
default branches can contain work beyond a stable release. Commercial feature
availability can depend on edition and companion products. Royal TS V26's
external-process and monitor-selection items below are specifically documented
as beta features.

| Offering and model | Relevant documented features | Lesson for this toolkit |
|---|---|---|
| [MsRdpEx](https://github.com/Devolutions/MsRdpEx), open source, MIT | Extends Microsoft RDP through launchers/API hooks; extra RDP settings; opt-in diagnostic logging with configurable level/path; ActiveX integration. | Closest architectural reference for client extension and diagnostics. Borrow bounded, explicit diagnostics; authentication hooks and alternative hosting are substantial separate work. |
| [Remote Desktop Plus](https://www.donkz.nl/), freeware wrapper with no public source linked on the product page | Wraps `mstsc`; display sizing and monitor placement; custom titles/icons; favorites and tray switching; connection logs; credentials and kiosk options. | Closest workflow comparison. Labels, display presets, and small file-based shortcuts transfer well. Credential automation and kiosk enforcement do not suit the current purpose. |
| [RDCMan](https://learn.microsoft.com/en-us/sysinternals/downloads/rdcman), Microsoft freeware | Grouped connections, live thumbnails, inherited settings, credential profiles, favorites/recent groups, configurable hotkeys, reconnect workflows. | Session organization matters when many machines are used. A small correctly targeted chooser could help later; rebuilding its connection tree would change the product's scope. |
| [mRemoteNG](https://github.com/mRemoteNG/mRemoteNG), open source, GPL-2.0 | Tabbed manager supporting RDP, VNC, SSH, web and other connection types. | Learn from easy switching and visible session identity. A multi-protocol manager is a different product, with little reuse of the toolkit's current session controls. |
| [Royal TS](https://docs.royalapps.com/r2023/royalts/reference/connections/rdp.html), proprietary commercial product | Connection colors/icons, display sizing and smart reconnect, diagnostic logging, credentials and session administration. Its [V26 beta](https://royalapps.com/blog/royal-ts-v26-a-closer-look-at-whats-new-for-rdp) adds external `mstsc` mode and selected monitors. | Strong reference for identity cues and display workflows. Advanced ActiveX capabilities are not automatically available to this mod. |
| [Devolutions Remote Desktop Manager](https://devolutions.net/remote-desktop-manager/features/remote-connection-management/), proprietary product with free and paid offerings | Consolidates connections; quick connect; credential vault/injection; broad protocol integrations; shared access and role-based control. | A convenient entry point is useful. Vaults, collaboration and access governance would add a much larger product and operational burden. |
| [Remmina](https://www.remmina.org/remmina-features/), open-source client | Configurable host-key actions for fullscreen, minimize, disconnect and keyboard grab; connection/view preferences; RDP redirection settings. | A clear escape/focus workflow and remembered presentation are valuable. Replacing the RDP engine or porting the toolkit to Linux is not an incremental extension. |
| [Apache Guacamole](https://guacamole.apache.org/), open-source browser gateway | [Connection switching/tiling, clipboard UI and file transfer](https://guacamole.apache.org/doc/gug/using-guacamole.html); configurable [session recording](https://guacamole.apache.org/doc/gug/configuring-guacamole.html). | Learn from explicit connection focus and a secondary menu. Browser delivery, shared gateways, recording and transfer infrastructure carry different deployment requirements. |
| [RDPSoft Remote Desktop Commander Suite](https://www.rdpsoft.com/products/remote-desktop-commander/suite/), proprietary commercial platform | Connection-quality trends, client diagnostics, disconnect investigation, session activity/performance reports and optional recording. | “Why is this session slow?” is useful. Start with an on-demand local diagnostic and known data sources; avoid recreating a monitoring service and reporting platform. |

The comparison supports a small session companion focused on identity,
control, diagnosis, and recovery. None of these sources establishes that the
toolkit's taskbar approach is unique, or that another manager's embedded RDP
sessions will work with the current `mstsc.exe` include target.

## Ranked additive candidates

Each utility and fit score runs from **1 to 10, with 10 highest**. Utility
means benefit to this toolkit's recurring RDP workflow; fit means reuse of its
architecture and consistency with its scope. The overall score is
`0.6 × utility + 0.4 × fit`. Scores are judgment, not measured market demand.
Order within ties favors the smaller dependency burden. Effort includes
cross-version and runtime testing, not just coding.

| Rank | Candidate | Utility | Fit | Overall | Effort | Disposition |
|---:|---|---:|---:|---:|---|---|
| 1 | On-demand toolkit diagnostics and shareable report | 10 | 10 | **10.0** | Small–medium | Best next; extend existing prerequisite work. |
| 2 | Visible action outcomes and peer availability | 10 | 10 | **10.0** | Medium | Best next; protocol and UI work. |
| 3 | Multiple-session ambiguity guard | 9 | 10 | **9.4** | Medium | Best next; safety before convenience. |
| 4 | Session nickname, environment accent and explicit role | 8 | 10 | **8.8** | Small–medium | First visible convenience addition after correct binding. |
| 5 | Named display presets and selected-monitor reconnect | 9 | 8 | **8.6** | Medium | Best next convenience feature; prototype monitor handling. |
| 6 | Host request to stay awake while RDP is active | 8 | 9 | **8.4** | Small–medium | Best next host utility, opt-in. |
| 7 | Bounded session event history | 7 | 10 | **8.2** | Small–medium | Next wave after logging/lifetime repairs. |
| 8 | Explicit client/host panel coexistence and placement | 7 | 9 | **7.8** | Small–medium initially | Useful when this PC is both host and client. |
| 9 | On-demand lag and disconnect investigation | 8 | 7 | **7.6** | Medium–large | Add after basic diagnostics; validate each metric source. |
| 10 | Small launcher for existing `.rdp` files | 7 | 8 | **7.4** | Small–medium | Later convenience; no catalog or credential store. |
| 11 | Correctly bound session chooser and restore shortcut | 9 | 5 | **7.4** | Large | Conditional on real simultaneous-session demand. |
| 12 | Restart remote clipboard from the local panel | 9 | 5 | **7.4** | Large | Good convenience, but needs a new client-to-host command path. |
| 13 | Redirection settings preview and reconnect profiles | 7 | 7 | **7.0** | Medium | Later; show configured intent separately from effective state. |
| 14 | Live keyboard-capture toggle and escape workflow | 7 | 6 | **6.6** | Medium–large | Defer live toggle pending retained-control/lifetime proof. |
| 15 | True host-session idle/lock awareness | 6 | 7 | **6.4** | Medium locally; larger cross-machine | Optional information only; no policy countdown inferred. |
| 16 | One named remote job-completion signal | 6 | 7 | **6.4** | Medium–large | Hold until a specific producer and clearing rule are named. |

The first three add observable product behavior even though they also repair
reliability. COM lifetime, safe unload, and log rotation are enabling engineering
work, not additional feature rows scored to inflate the list.

### The best next additions in concrete terms

**1. Explain why a component is unavailable.** Start with a read-only
PowerShell command producing readable text and optional JSON. Inspect selected
Windhawk root/capabilities, component files, registration consistency, current
processes, snapshot layout/freshness, receiver presence, and known build
compatibility. Report unavailable evidence as unknown, including loaded
versions that cannot be established. Reuse the prerequisite classifier but do
not promote file presence to “ready.” Exclude the shared secret, credentials,
clipboard contents, and full connection files. Preview connection metadata
before an optional local export; no automatic upload or repair.

Acceptance: distinguish missing prerequisite, wrong user/path registration,
missing receiver, incompatible snapshot, stale snapshot, and ambiguous session.
Run without starting a relay, registering anything, or reloading a mod. This
builds on the diagnostic patterns in MsRdpEx and Royal TS while staying useful
for the toolkit's actual failure points.

**2. Make a click's result visible.** Use the existing status line/tooltip for
brief states such as “sent,” “accepted,” “mode changed,” “rejected,” and
“outcome unknown.” Separate local host actions from relay-dependent actions so
a missing peer does not disable a working clipboard remedy. Add capability and
result fields through a deliberately versioned contract; preserve an honest
legacy-peer fallback. Correlate outcomes to requests and use later state
observations where possible. Do not call the immediate pre-refresh snapshot
proof that fullscreen changed.

Acceptance: missing client mod, rejected command, hung receiver, unsupported
peer, timeout and successful state change all produce different outcomes. Test
both old/new directions. Never automatically retry an uncertain fullscreen
toggle: a repeat can undo the first action.

**3. Refuse ambiguous targeting.** Detect competing toolkit receivers/writers;
disable ambiguous panel and relay actions and explain why. Validate that the
fresh snapshot belongs to the receiver before acting; validate process lifetime
as well as PID. A first guard does not need a chooser, favorites database, or
new per-session UI.

Acceptance: two active clients, one active plus one connection dialog,
disconnect/relaunch, stale snapshots, and receiver disappearance cannot cause
an action intended for A to operate on B. Conservatively disabling an ambiguous
action is acceptable; displaying whichever process last wrote is not adequate
target selection. Reliable DVC-to-client binding remains a prerequisite for
full multi-session support.

**4. Make the session's identity easier to recognize.** Offer a local nickname
and a small accent such as “Work,” “Lab,” or “Production,” retaining the actual
target in the tooltip. Label the role as “Connected to” or “Connected from.”
Royal TS's colors/icons and RDP+'s custom titles are useful precedents. Reuse
the panel's existing text/palette and accessible names. Keep text labels so
color is never the only distinction.

Acceptance: long or identical nicknames, light/dark theme, high contrast,
multiple monitors, and a PC carrying both roles remain understandable. Bind
configuration to a reliable target; a window-title-derived hostname alone must
not become a certificate-identity or verified-server claim. Start with manual
configuration rather than network discovery.

**5. Choose a display arrangement without editing connection files by hand.**
Extend existing reconnect planning with named window-size presets, a chosen
monitor, and a selected contiguous monitor set. The meaningful addition is
“remote on these two screens, local work on the third,” rather than another
fullscreen toggle. Microsoft's [RDP properties reference](https://learn.microsoft.com/en-us/azure/virtual-desktop/rdp-properties#selectedmonitors)
documents `selectedmonitors`, the multimon requirement, contiguous displays,
and machine-specific monitor IDs obtained through `mstsc /l`.

Start by launching user-owned preset `.rdp` files. A later editor must preserve
unrelated properties and avoid silently rewriting signed or externally managed
files. Do not assume existing command-line display switches override every
property in a connection file. Label a reconnect requirement explicitly and
respect the reconnect opt-in. Live dynamic resizing is a separate experiment.

Acceptance: monitor unplug/reorder, changed IDs, mixed DPI, invalid/noncontiguous
selection, paths with spaces, preserved gateway settings, and a removed preset
file fail predictably. Royal TS's beta monitor selection demonstrates the
workflow's value, not compatibility of its implementation with this mod.

**6. Request that an idle connected host keep running.** Add an off-by-default
host setting driven by its existing RDP/active WTS state. Use a named system
power request; release on disconnect, setting disable, unload and error
cleanup. Display whether the request is actually held. Keep physical-display
power separate and retain normal machine power settings.

Acceptance: a request appears in `powercfg /requests`, clears on all lifecycle
paths, and never stacks counts on repeated WTS refreshes. Test the relevant
power configuration. Microsoft's [PowerSetRequest documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-powersetrequest)
describes user-initiated sleep and Modern Standby/DC restrictions. This is a
request to prevent automatic idle sleep, not a guarantee of remote availability
or a way to defeat session timeouts. This narrows the stronger guarantee wording
in the earlier expansion research.

### Useful later additions and their limits

- **Event history:** record connection/client/state changes and toolkit action
  outcomes locally, with UTC timestamps and explicit retention. WTS already
  supplies the host event source. Start with a text/JSONL file and an “open
  history” command; skip a database and embedded viewer. It is an operational
  record observed while the toolkit runs, not a complete security audit trail.
- **Coexistence:** first add role labels and documented opposing placements.
  Automatic collision avoidance and one combined panel would require extra
  coordination; do not assume the separate mods already share layout state.
- **Lag/disconnect investigation:** begin with recent relevant local RDP events,
  toolkit state and client process/resource observations. A live numeric graph
  is conditional on a proven source. The current
  [`OnNetworkStatusChanged`](https://learn.microsoft.com/en-us/windows/win32/termserv/imstscaxevents-onnetworkstatuschanged)
  consumer does not establish reliable samples, packet-loss measurement, or
  transport identification. Label an optional ICMP probe as ICMP, never as RDP
  latency or a comprehensive host-health diagnosis.
- **File launcher:** retain a few explicit user-selected `.rdp` paths. Check
  existence and launch through the existing deliberate workflow. No scanning
  other managers' vaults or copying passwords into toolkit configuration.
- **Remote clipboard shortcut:** use the existing host restart implementation
  only after a narrow request/response protocol exists. Host-initiated channel
  traffic and client-state replies do not supply that receiver today.
- **Redirection profiles:** show configured clipboard/drive/audio intent and
  provide a deliberate reconnect route. Policy and negotiation can change the
  effective result; do not show “sharing disabled” from a file value alone.
  Microsoft's [RDP properties](https://learn.microsoft.com/en-us/azure/virtual-desktop/rdp-properties)
  describe redirection and keyboard settings, not proof of a live negotiated
  state in this toolkit.
- **Keyboard workflow:** begin with shortcut help and a reliable local escape
  route. A live capture toggle needs a retained RDP control interface, correct
  apartment ownership and tested restore behavior; advising an event sink did
  not retain a general control interface. Avoid a broad keyboard hook merely
  to expose a switch.
- **Host idle awareness:** [WTSINFOEX_LEVEL1](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/ns-wtsapi32-wtsinfoex_level1_w)
  provides session input and sample times. That can support an honestly labeled
  host-local idle value. A policy countdown needs a separately verified policy
  source; presenting it locally on the client needs a new host-to-client data
  contract. This should not restore the removed duration clock.
- **Completion signal:** a specific opt-in producer such as a named job's exit
  could set a bounded badge. Define ownership, acknowledgment, clearing,
  reconnect and stale-session behavior first. The toolkit cannot infer a
  general Windows-notification feed from the current host widget.

## Reviewed and excluded directions

These are exclusions from the proposed next roadmap, not claims that the
features lack value in the products that offer them. Scores here measure
benefit and fit for this toolkit under the same formula.

| Direction | Utility | Fit | Overall | Reason and narrower alternative |
|---|---:|---:|---:|---|
| Credential vault, password injection and shared access controls | 5 | 1 | 3.4 | Adds sensitive storage and account-management responsibilities. Keep using Windows and established connection managers. |
| Full tabbed, multi-protocol connection manager | 5 | 2 | 3.8 | Rebuilds mature products and displaces the native taskbar workflow. Prefer a small bound session selector only if needed. |
| Screen recording, screenshot streams and productivity monitoring | 4 | 1 | 2.8 | Sustained capture, storage and retention machinery exceed the small companion design. Prefer bounded operational events. |
| Dedicated file-transfer engine and clipboard history/preservation | 6 | 2 | 4.4 | Native redirection already exists; preserving rich clipboard formats introduces ownership and correctness problems. Prefer clear settings and the existing restart remedy. |
| Synthetic input to avoid idle policies | 2 | 1 | 1.6 | Changes session behavior without explaining its cause and mixes power/idle semantics. Prefer an explicit power request for the narrow sleep use case. |
| Automatic toolkit force-reconnect or idle disconnect | 3 | 1 | 2.2 | Conflicts with the deliberate-action design and can act on the wrong signal. Preserve manual recovery and informative warnings. |
| Generic remote shell, machine restart, service management or session reset | 5 | 1 | 3.4 | Changes the trust boundary and failure impact. New recovery tools should address a named session-local problem. |
| General notification interception | 4 | 2 | 3.2 | Missing producer mechanism and unclear badge lifetime. Consider one explicit job-completion producer instead. |
| Wake-on-LAN | 5 | 3 | 4.2 | Requires a route while RDP/DVC is absent plus network/NIC configuration. Revisit only with a specific wake workflow. |
| Replace mstsc, support browser gateways, or promise every RDP client | 4 | 1 | 2.8 | Current hooks and includes are client-specific. Similar protocol names do not establish compatible processes or taskbar structures. |
| Restore thumbnail toolbar or duration clock | 2 | 2 | 2.0 | Reverses deliberate removals without a newly demonstrated need; duplicates controls or restores discarded clutter. |

Toolkit-only setup remains a separate distribution track: **utility 8, fit 8,
overall 8.0, large effort**. It is valuable when sharing the kit, but it is not
the next session feature. Follow the existing packaging gates rather than
building an installer UI around unresolved lifetime and ownership behavior.
[D-42](.raiden/state/DECISIONS.md#d-42-toolkit-setup-must-not-install-or-upgrade-windhawk)
requires reusing a separately installed Windhawk; setup must never install or
upgrade it. This review does not revalidate earlier Windhawk CLI findings.

## Recommended delivery sequence

1. **Finish the current candidate's home acceptance and runtime foundations.**
   Use the existing plan and acceptance record. Address relay shutdown, worker
   and dispatcher completion, log bounds, and host stale-state repaint. These
   enable the feature work; they are not reasons to add another service.
2. **Deliver diagnostics, action outcomes and ambiguity handling.** Basic
   diagnostics can proceed independently. Session guards and protocol outcomes
   should land before adding more controls. Test both mixed component versions
   and the absence of optional components.
3. **Deliver one visible convenience improvement.** Session labels are the
   smaller first increment. Display presets are the stronger new workflow
   feature; prototype file-based presets before building a monitor editor.
4. **Add one small host utility.** Prefer the opt-in awake request, then bounded
   history if retained troubleshooting context is useful. Keep rare actions in
   an explicit command or small secondary menu instead of widening both panels.
5. **Expand only around observed demand.** Simultaneous-session use justifies
   the chooser; repeated lag cases justify deeper diagnostics; a named remote
   job justifies completion signaling. Advance toolkit-only setup through its
   own validated deployment stages.

For each implementation wave, check unload during work, no-relay behavior,
stale/mismatched state, console gating, and correct targeting. Visual changes
also need DPI, multiple monitors and both themes. Measure idle CPU, memory,
handles, threads, log growth and click responsiveness before and after on the
same environment. No overhead or compatibility promise follows from a feature
score or a successful compile.

## Relationship to earlier planning

[EXPANSION_RESEARCH.md](EXPANSION_RESEARCH.md) identified clipboard restart,
host awake requests and connection history. Clipboard recovery has since been
implemented; the other two remain useful with the limits above.
[REPOSITORY_REVIEW.md](REPOSITORY_REVIEW.md) already proposed diagnostics,
feedback and ambiguity handling. The current source supports retaining that
ordering. This comparison adds concrete precedent and higher-detail scopes for
identity accents, selected-monitor presets, redirection previews and local
access to remote recovery.

[REFINEMENT_PLAN.md](REFINEMENT_PLAN.md) remains the home for the active
refinement sequence; [PACKAGING_RESEARCH.md](PACKAGING_RESEARCH.md) remains the
home for deployment findings. This report supplies the requested product
comparison and ranking. It does not adopt a new mission, change the roadmap's
commitments, implement features, or close verification loops.
