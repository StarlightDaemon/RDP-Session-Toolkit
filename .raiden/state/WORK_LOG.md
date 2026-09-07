# Work Log

## 2026-08-20

- Bootstrapped repo: `git init`, MIT LICENSE (StarlightDaemon), RAIDEN Instance
  install (Edict v2.0.0), directory scaffold for `taskbar-integration/` and
  `dvc-plugin/` with placeholder READMEs, top-level README.md.
- No code written yet.

## 2026-08-20 (later session)

- Ported the client-side Windhawk mod from Hide RDP Connection Bar v1.1.9
  (commit `dc82b10d4c8713d71f5e556649b114a6d43dad04`) into
  `taskbar-integration/rdp-session-toolkit-taskbar-client.wh.cpp` as a fork
  (no shared history; provenance in the file header). New mod id
  `rdp-session-toolkit-taskbar-client`, fresh version line: v0.1.0 = pure port
  (compile-verified), v0.2.0 = relay receiver.
- v0.2.0 additions: `CitadelRdpTaskbarRelay` message-only receiver window
  (own thread, created at mod load), WM_COPYDATA command-byte protocol with
  minimize (0x01) implemented, sender-PID logging, marked TODO deferring
  sender validation until the relay plugin exists (D-5); shared
  `MinimizeRdpFrame` path between thumb-bar button and relay;
  `debugRelayTestMinimize` setting to exercise the receiver locally.
- Thumbnail toolbar (Minimize/Restore/Disconnect) arrived with the port —
  already built in V1 at v1.1.9, but never live-tested: V1's own test records
  show the last actual operator live test passed at v1.1.1, before the
  thumbnail toolbar existed. Disconnect stays WM_CLOSE to the mstsc frame
  (D-4).
- Added `taskbar-integration/compile-check.ps1` (Windhawk-bundled clang, mirrors
  Windhawk's own mod compile flags); both v0.1.0 and v0.2.0 compile cleanly
  (one pre-existing unused-function warning inherited from V1).
- Docs: taskbar-integration/README.md rewritten for the built mod + relay
  protocol; DECISIONS.md D-4/D-5/D-6; CURRENT_STATE/OPEN_LOOPS updated; root
  README status refreshed. Nothing committed.

## 2026-08-20 (host-mod session)

- Reorganized `taskbar-integration/` into `client/` and `host/` subfolders
  before first commit of the mod files — pure `git mv` of the client mod
  source (no content change); `compile-check.ps1` now scans recursively.
- Built the host-side mod `host/rdp-session-toolkit-taskbar-host.wh.cpp`
  (mod id `rdp-session-toolkit-taskbar-host`, fresh version line at v0.1.0):
  targets `explorer.exe` on the RDP session host, injects a widget into the
  taskbar's own XAML tree (`Grid#RootGrid` under `Taskbar.TaskbarFrame`) via
  the `TaskListButton::UpdateVisualStates` symbol hook — technique adapted
  from the sibling native-taskbar-media-controller mod (D-7).
- Widget shows WTSClientName + WTSConnectState (queried against
  `WTS_CURRENT_SERVER_HANDLE`/`WTS_CURRENT_SESSION`), refreshed via a
  dedicated-thread hidden window registered for WM_WTSSESSION_CHANGE plus a
  30 s safety-net timer (D-8). Disconnect button calls WTSDisconnectSession;
  gated (enabled-state + click-time re-check) on WTSClientProtocolType == RDP
  so it can never disconnect the physical console session (D-8).
- Minimize/DVC/cross-machine signaling explicitly deferred; single marked
  `TODO(minimize-trigger)` placeholder in `BuildWidget()` (D-9).
- Both mods compile cleanly via `compile-check.ps1` (the one pre-existing
  unused-function warning inherited from V1 remains in the client mod).
- Docs: taskbar-integration/README.md restructured for the two subfolders +
  host mod section; DECISIONS.md D-7/D-8/D-9; CURRENT_STATE/OPEN_LOOPS
  updated; root README gains the host-widget section. Nothing committed.

## 2026-08-21 (relay-plugin + minimize-pipe session)

- Built the **production DVC relay plugin** at `dvc-plugin/relay/` (new,
  permanent component; the probe is left untouched as historical reference).
  Adapted the verified probe's out-of-process COM `LocalServer32` model
  verbatim — per-user HKCU-only registration (no admin), plain Win32 COM, built
  `-static` with the Windhawk clang. Own identity (D-10): channel
  `dvc::taskbar::relay`, CLSID `{6FC96481-9467-496E-BA33-A202ED052F39}` (freshly
  minted), AddIn `RdpSessionToolkitDvcRelay`. On `OnDataReceived` it validates
  the payload is non-empty and forwards its **first byte, as-is** to the client
  mod's `CitadelRdpTaskbarRelay` window via `WM_COPYDATA` (FindWindowExW
  discovery), passing its own hidden message-only window as `wParam` for sender
  identity. Builds clean via `build.ps1` (zero warnings); correct registry/
  channel strings verified embedded in the EXE.
- **Client mod → sender validation (D-11, closes D-5's TODO; v0.2.1 → 0.3.0).**
  The relay receiver now accepts WM_COPYDATA only from (1) this mod's own
  process — the self-test, updated to pass a same-process window as `wParam` so
  it authenticates — or (2) the process whose image path equals the relay
  CLSID's registered HKCU `LocalServer32` EXE. Everything else is
  logged-and-ignored (fail-closed if the relay is unregistered).
- **Host mod → minimize send (D-12; v0.1.0 → 0.2.0).** Replaced
  `TODO(minimize-trigger)` with a real Minimize button (RDP-gated like
  Disconnect) that opens `dvc::taskbar::relay`, writes one `0x01` byte, and
  closes — on a short-lived **detached** background thread (not the persistent
  readiness-event pattern; it's a one-shot open-write-close), drained by a
  bounded wait in `Wh_ModUninit`. Channel-open failure (no relay listening) is
  logged and dropped — no dialog, no explorer.exe interruption.
- Docs: DECISIONS.md D-10/D-11/D-12; OPEN_LOOPS LOOP-002 + LOOP-003 marked
  resolved (LOOP-003 is the passed hardware/pipe test — the driving task said
  "LOOP-001", but that tracks the still-unrun client-mod live test), new
  LOOP-006 for the full end-to-end minimize feature test; CURRENT_STATE
  refreshed; `.gitignore` ignores `dvc-plugin/relay/bin/`.
- Relay plugin + both mods compile/build cleanly (one pre-existing V1
  unused-function warning remains in the client mod). **Not** yet register/
  live-tested end-to-end (LOOP-006). Nothing committed.

## 2026-08-21 (overnight feature session — client mod v0.3.0 → 0.4.0)

- **LOOP-006 marked resolved** on the operator's direct observation (host
  Minimize → client mstsc window minimized, two machines). No transcript
  captured it; OPEN_LOOPS is the durable record. LOOP-001 annotated: the
  client mod has now been loaded live at least once, but the thumbnail
  toolbar/overlay still lack a specific check. New **LOOP-007** for
  live-testing tonight's five features, with the exact log lines to watch.
- **Fullscreen/windowed toggle** (D-14): thumb-bar button; brings the frame to
  the foreground (SetForegroundWindow + AttachThreadInput fallback) and sends
  Ctrl+Alt+Break via SendInput; never sends if the foreground was not
  acquired. `IMsRdpClient::FullScreen` (DISPID 104) identified as a direct
  alternative and deliberately not used.
- **Session duration / local idle** (D-15): start stamped at the frame-detection
  hook (re-stamped by OnConnected when the sink is live); idle =
  GetLastInputInfo (client-machine input idle, by design). Shown on a new
  overlay row (overlay grows 80×56 → 96×72) and in the new thumb-bar status
  icon's tooltip (minute-granular, diff-throttled).
- **Connection quality** (D-16): real hook point found and built — combase
  `CoCreateInstance` hooked, RDP control recognized via
  `IConnectionPointContainer::FindConnectionPoint(DIID_IMsTscAxEvents)`, a
  second IDispatch sink advised; DISPIDs resolved at runtime from the MSTSCLib
  type library (a scratch probe compiled with the Windhawk clang showed
  OnNetworkStatusChanged = **32** on this machine, contradicting memory).
  Status icon colored by level 1–4; grey + explicit tooltip text until a real
  report arrives. Whether mstscax fires the event to a second sink in
  mstsc.exe is the one thing still unverified (LOOP-007).
- **Quick reconnect** (D-17): shared helper — relaunch command line = own
  command line minus display switches (+ `/v:<host>` from the cached title if
  needed) + `/f` | `/w: /h:` | `/multimon` per new settings; parked as
  pending, WM_CLOSE posted, launched from the frame's WM_DESTROY; once-per-
  process launch guard; plain Disconnect clears a pending plan.
- **Stuck-session detection** (D-18): watchdog thread polling IsHungAppWindow
  (1 s, threshold setting), own topmost alert window with Force reconnect /
  Dismiss (thumb buttons can't be clicked while the frame thread is hung);
  force path = park plan → WM_CLOSE → 3 s grace → launch + TerminateProcess.
  No auto-reconnect anywhere.
- **Thumb bar restructured** (D-19): table-driven slots added up front (6/7),
  diff-based `SyncThumbButtons`; color glyph renderer.
- Settings added: showSessionInfo, showConnectionQuality, showReconnectButton,
  reconnectDisplayMode, reconnectWindowWidth/Height, stuckDetection,
  stuckThresholdSeconds, showFullscreenToggle. Version 0.4.0; Wh_Log banner
  and settings-reloaded line carry the new flags. compilerOptions gain
  `-loleaut32 -lshell32`.
- Compile-checked clean after every task (same single pre-existing V1
  unused-function warning). Host mod and relay plugin untouched; taskbar-
  embedded client mode (D-13) untouched. Nothing committed.

## 2026-08-21 (overnight session — taskbar-embedded client widget)

- **New third mod** `taskbar-integration/client-embedded/rdp-session-toolkit-taskbar-client-embedded.wh.cpp`
  v0.1.0 (`explorer.exe` on the client machine; D-21): one compact status
  icon injected into `Grid#RootGrid` via the host mod's own injection code;
  click opens a Win32 `WS_POPUP` GDI flyout (not a WUX Popup, per D-7) with
  host / session clock / idle / quality / hung lines and Minimize, Restore,
  fullscreen toggle, Reconnect, Disconnect rows, enabled/relabelled from the
  session's iconic/fullscreen state. Own status thread (queue-first readiness
  handshake) polls the status file every 1 s and owns the flyout; teardown in
  `Wh_ModUninit` joins everything.
- **Client mod v0.4.1 → v0.5.0** (D-22/D-23): `LocalWidgetStatus` snapshot
  written to `<Wh_GetModStoragePath>\local-widget-status.dat` from the existing
  overlay status tick *and* the watchdog tick through one throttled writer (no
  new timer); "inactive" record forced on frame `WM_DESTROY`, file deleted at
  uninit. New separate `CitadelRdpTaskbarLocalWidget` message window + thread
  with its own `WM_COPYDATA` validation: packed magic/version payload carrying
  a 32-byte `BCryptGenRandom` secret stored as
  `HKCU\Software\RDPSessionToolkit\LocalWidgetSecret` (get-or-create under a
  named mutex, whichever mod starts first), re-read and constant-time-compared
  per command, fail-closed. Commands route to `MinimizeRdpFrame`, the new
  shared `RestoreRdpFrame` (thumb bar + overlay now use it too),
  `ToggleFullscreen`, `ReconnectSessionClean`, `DisconnectSession`.
  `-lbcrypt` added. Banner bumped.
- **CitadelRdpTaskbarRelay, its validation, `dvc-plugin/relay`, and the host
  mod untouched** (D-24).
- Docs: D-21–D-24; D-13 annotated as built; ROADMAP item moved to built (with
  three new open questions: UIPI stance, writer gap, host+widget overlap);
  LOOP-008 opened; CURRENT_STATE and both READMEs updated. Compile-checked
  clean after each task (same single pre-existing V1 warning). Nothing
  committed.

## 2026-08-21 (consolidation session — client mod v0.5.0 → v0.6.0, two-process mod)

- **Merged the standalone embedded mod into the client mod (D-26).** The client
  mod now has two `@include` targets — `mstsc.exe` and `explorer.exe` — plus
  `@architecture x86-64` and a union of both compiler-option link libraries
  (`-lruntimeobject -luser32 -lwindowsapp` added). Version bumped **0.5.0 →
  0.6.0**.
- **Process detection (Task 1).** `Wh_ModInit` calls `DetectHostProcess()`
  (`GetModuleFileNameW(nullptr, …)`, case-insensitive filename compare) once
  into a `HostProcess` global and dispatches `Wh_ModInit` /
  `Wh_ModSettingsChanged` / `Wh_ModUninit` to `client::` or `embedded::`. A load
  into neither process logs plainly and returns `TRUE` as an inert no-op. The
  proven v0.5.0 mstsc code is moved verbatim into `namespace client`; the WinRT
  `using` directives are confined to `namespace embedded`.
- **Folded the explorer branch (Task 2).** The v0.2.0 widget's XAML injection,
  system-tray margin anchoring, six-element panel, status reader, and command
  sender moved into `namespace embedded` unchanged in behavior — except the
  **status-file path-guessing is gone**: the explorer branch calls
  `Wh_GetModStoragePath` directly (same mod id → same directory the mstsc branch
  writes), so `ResolveModsWritableDir` / `kClientModStorageNames` / the `local@`
  dance are deleted.
- **Reconciled settings (Task 3).** Both blocks merged; the five embedded
  settings renamed to the `embedded…` group and the mstsc overlay's
  `buttonPosition` `$name` clarified, so no two settings can be confused across
  surfaces. Re-ran `compile-check.ps1`'s `Test-ModSettingsYaml` (the D-20
  landmine check) against the merged block — clean (verified it still flags a
  known-bad line).
- **Reconciled shared constants (Task 4).** Channel name, command byte values,
  the status struct + magic, the command payload + magic, the secret registry
  location, and the identical helpers (`ReadLocalWidgetSecret`,
  `EnsureLocalWidgetSecret`, `QualityLabel`, `FormatClock`,
  `GetLocalWidgetStatusFilePath`) are now single definitions above the two
  branch namespaces, used by both. `dvc-plugin/relay` keeps its own copy per
  D-10 (separate build).
- **Retired the standalone mod (Task 5).** `taskbar-integration/client-embedded/`
  file and folder deleted (never committed; every line now lives in
  `namespace embedded`).
- **Docs & banners (Task 6).** New DECISIONS **D-26**; ROADMAP and OPEN_LOOPS
  (LOOP-008 re-scoped to the merged architecture) updated; CURRENT_STATE and
  both READMEs corrected (the `taskbar-integration/README.md` "why separate
  mods" section reversed its now-false premise). Wh_Log init/settings banners
  label each branch (`[mstsc.exe branch]` / `[explorer.exe branch]`) at v0.6.0.
- **Verification.** Compile-checked after each task and finally via
  `compile-check.ps1` (client + host) — clean, only the single pre-existing
  `GetRdpMonitorRect` unused-function warning. Settings block validates.
  Nothing committed or pushed.
- **Open, explicitly not confirmed:** the "no active session shown" symptom is
  *structurally* eliminated by the shared-mod-id status path, but this is a
  strong expectation, **not** a live-verified result — it still needs a real
  pass under the merged mod (LOOP-008).

## 2026-08-22 (overnight session — client mod v0.6.0 → v0.7.0: fullscreen-toggle foreground fix, reconnect gating + launch-time safety check, storage-path cache, quality-sink investigation)

- **Task 1 — fullscreen toggle foreground (D-27).** The explorer branch's
  command handler now calls `AllowSetForegroundWindow` on the mstsc receiver's
  pid (resolved from the `CitadelRdpTaskbarLocalWidget` window with
  `GetWindowThreadProcessId`) right before sending `LWCMD_FULLSCREEN_TOGGLE` —
  only that command. mstsc-side `BringFrameToForeground` / `ToggleFullscreen`
  untouched (comment only); the `AttachThreadInput` fallback stays.
- **Task 2 — reconnect gating + safety check (D-28).** `showReconnectButton`
  → **`enableReconnect`, default false**, gating the thumb-bar slot, the local
  widget's Reconnect (receiver refuses; panel collapses its button), and the
  alert's Force reconnect (alert still appears; Dismiss only). `ReconnectPlan`
  now records the `.rdp` argument; `LaunchReconnect` re-checks it on disk at
  launch time and treats a missing file as a failed reconnect (logged, guard
  released). Force reconnect still terminates a hung client when the relaunch is
  refused, now saying so in the log. Readme block deliberately not edited.
- **Task 3 — storage-path cache.** `GetModStorageDir()` (shared, thread-safe
  static init, primed from both `ModInit`s) replaces the per-second
  `Wh_GetModStoragePath` calls in the mstsc writer and explorer reader;
  `GetButtonPosFilePath` uses it too. One live call site remains (the cache).
- **Task 4 — `@description`** replaced with the operator's text verbatim
  (joined to one metadata line). Readme block unchanged.
- **Task 5 — connection-quality sink (D-29).** Binary inspection of this
  machine's mstsc.exe: no import from mstscax.dll; `"mstscax.dll"` +
  `"DllGetClassObject"` strings adjacent in .rdata; two MsRdpClient CLSIDs
  nearby; ole32's delay-loaded `CoCreateInstance` forwards to combase (hook
  target was right). Conclusion: mstsc takes the control from mstscax's class
  factory directly, bypassing `CoCreateInstance`. **Fix built**: load mstscax in
  `Wh_ModInit`, hook `DllGetClassObject` and the factory's `CreateInstance`
  (vtable slot 3) as Windhawk-managed hooks; `CoCreateInstance` path retained.
  **Diagnostics added** at every step (hook install results, first-fire, per-call
  CLSIDs, QI / FindConnectionPoint / Advise HRESULTs, two fixed-moment summary
  lines). Not live-confirmed — the proof lines to watch are listed in D-29.
- **Task 6.** D-27/D-28/D-29 written; version **0.7.0**; both branches' init
  banners (and the inert-process banner) bumped; explorer banner now also
  reports `reconnect=`. LOOP-007 updated; CURRENT_STATE updated.
- **Verification.** `compile-check.ps1` run after each task (1 → 2 → 3 → 4+5 →
  6): clean every time, only the single pre-existing `GetRdpMonitorRect`
  unused-function warning; settings block validates (the renamed
  `enableReconnect` entry passed the D-20 landmine check). Nothing committed or
  pushed.
- **Post-session follow-up (same night).** The operator's Windhawk editor
  buffer — and then the repo file itself (overwritten 01:55) — ended up with
  every comment marker inverted (comment lines bare, code lines commented),
  producing "Couldn't find a metadata block" and a wall of clang errors on
  prose lines. The repo file was rebuilt exactly from the pre-session baseline
  copy plus the session diff (`patch`), verified, and compile-checked clean. A
  stricter check than `compile-check.ps1` (a full `-shared` link with the
  `@compilerOptions` libs) surfaced one real toolchain fragility:
  `SecureZeroMemory` expands under mingw-w64 to the ntdll import
  `RtlSecureZeroMemory`, which neither the documented flags nor `-lntdll`
  resolve. Replaced its four uses with a local volatile-wipe `WipeSecret`
  (shared section); full link now succeeds. `compile-check.ps1` still only
  compiles (`-c`) — a link step would be a worthwhile follow-up.

## 2026-09-01 — Host mod v0.3.0: restart rdpclip + clipboard presence line (D-35)

Built the root `EXPANSION_RESEARCH.md`'s #1 recommendation, as specified there rather
than redesigned: §4.1's restart action and §4.2's presence line, both on the
host widget. No new channel, no new IPC, no new thread model, no client-side
change, and no `@compilerOptions` change.

- **Task 1 — find / verify / restart.** `ScanForRdpClipInSession` is the single
  shared find-and-verify function (used by both the action and the status line,
  so they cannot disagree): `WTSEnumerateProcessesW`, name-matched
  case-insensitively, scoped to `ProcessIdToSessionId(GetCurrentProcessId())`
  — *not* the active console session — then each candidate's
  `QueryFullProcessImageNameW` path required to equal
  `<GetSystemWindowsDirectoryW>\System32\rdpclip.exe` via
  `CompareStringOrdinal(..., TRUE)`, D-11's exact call and style. Skips are
  logged with their specific reason. `GetSystemWindowsDirectoryW` is the
  TS-correct call here; `GetWindowsDirectoryW` can return a per-user private
  Windows directory on a Terminal Services host and would fail every check
  closed. **Zero matches** → nothing to terminate, proceed to launch as a fresh
  start. **Multiple matches** → terminate every verified one, launch exactly one
  replacement. Each termination is followed by a bounded 2 s wait on the process
  handle, then a 250 ms settle — investigated and documented in D-35: rdpclip's
  format-listener registration *and* its delayed-render clipboard ownership are
  both keyed to its windows, and the USER-side rundown that destroys them is not
  ordered before the process handle signals. Then a re-scan (per §4.1, some
  builds respawn it) and `CreateProcessW` on the verified path.
- **Gating and threading.** RDP-only, re-read from `g_WtsState` at click time
  (D-8), never actionable from a console session. D-12's detached one-shot with
  `g_ClipboardRestartsInFlight` drained by a bounded 5 s wait in
  `Wh_ModUninit`; plus `g_ClipboardRestartBusy`, a compare-exchange latch so a
  double click cannot kill the instance the first click just launched.
- **Task 2 — the button.** `Restart Clipboard`, Segoe MDL2 Paste glyph
  (0xE77F), column 1 (Minimize and Disconnect shifted to 2 and 3), same
  transparent/white/disabled-at-0.35-opacity treatment as its neighbours.
  First tooltip on the host widget — `SetElementTooltip` copied verbatim from
  the client mod so the two surfaces match — stating the data loss outright:
  "…Whatever is currently on the shared clipboard is discarded." Disconnect and
  Minimize deliberately left untooltipped; that is a change to live-confirmed
  UI this task did not ask for.
- **Task 3 — the presence line.** A tri-state `ClipboardHelper` field on
  `WtsState`, filled in `QueryWtsState` (which the existing 30 s safety-net
  timer already drives — **no new timer**), so the struct's existing change
  detection refreshes the widget for free. Shows "Clipboard helper not running"
  on `NotRunning` and **nothing at all** otherwise — no "clipboard OK", ever,
  because a wedged-but-present rdpclip is indistinguishable (§4.2, and D-16's
  never-invent-a-value rule). A failed enumeration and a console session both
  stay `Unknown`. The restart worker calls `QueryWtsState` on completion so the
  line updates immediately rather than up to 30 s later.
- **Task 4 — docs and version.** D-35 written (citing D-8, D-11, D-12 and
  §4.1/§4.2 as the design basis); ROADMAP gains a "Built 2026-09-01 (host mod
  v0.3.0), pending live test" section moving this from recommended to built;
  LOOP-005 widened with what the new feature owes and the note that its live
  test is *expected* to exercise most of that loop — explicitly **not** marked
  resolved, since that needs a real test and not a build. Version 0.2.0 →
  0.3.0, `@description`, readme block and `Wh_ModInit` banner all bumped.
  Incidental: default `PanelWidth` 220 → 260, since a third button and a third
  text line do not fit legibly at 220.
- **Verification.** `compile-check.ps1` clean end to end — compile *and* the
  real `-shared` link step (D-32) for both mods, settings blocks validate. Zero
  warnings from the host mod; the client's single `GetRdpMonitorRect`
  unused-function warning is pre-existing and untouched. Nothing committed or
  pushed. **Nothing live-tested.**

## 2026-09-01 (second session — duration removal, minimize fix, client→host channel, per-monitor margins, theme)

- **Client mod v0.9.2 → v0.10.0, host mod v0.3.0 → v0.4.0, relay plugin →
  v0.2.0** (its first version constant). Compile-check (compile + real link +
  settings YAML) run after every task; relay rebuilt via its build.ps1.
- **Session duration removed entirely** (D-36): overlay row, panel line,
  tooltip, and the wire field (`LocalWidgetStatus` v1 → v2). Idle time stands
  alone; `showSessionInfo` rescoped to *Show idle time*. The one non-display
  consumer of the start tick — the D-29 10-s diagnostic — got its own
  deadline tick. 192-byte static_assert layout pins added on both the mod and
  the relay mirror.
- **Host Minimize root cause found and fixed** (D-37): the open-write-close
  send closed the DVC immediately after a queued (not delivered) write; the
  live-proven probe trigger drained 500 ms before closing and that drain was
  dropped in the D-12 adaptation. Fix: open-write-READ-close — the relay's
  new reply is the delivery confirmation.
- **Client → host state channel built** (D-38, EXPANSION_RESEARCH §2.2's
  missing piece): client mod publishes its storage dir at
  `HKCU\Software\RDPSessionToolkit\ClientModStoragePath`; relay plugin reads
  the status snapshot and answers EVERY command with a `RelayStatusReply`
  ('RSRP' v1) on the still-open channel; a 0x03 status query is answered
  without forwarding; host polls it every 3 s and caches the client view with
  a 10 s staleness window. Host gained the fullscreen/windowed toggle button
  (relay 0x02 → shared `ToggleFullscreen`), relabeling from the cached view.
- **Per-monitor widget instances in both mods** (D-39): the reported
  secondary-monitor margin gap was the single-global tray binding — whichever
  monitor's tray bound first drove every monitor's margin. One tracked
  instance per taskbar frame now.
- **Visual convergence + theme inversion** (D-40): one shared spec (shared
  `StyleActionButton`, host's two-line status format on both, spacing/radius
  numbers written down); panels render the OPPOSITE of the system theme and
  react live to `WM_SETTINGCHANGE("ImmersiveColorSet")`.
- Host Minimize/Disconnect tooltips added (Task 4), matching the established
  plain-language style; Restart Clipboard's verified code-side (same
  live-confirmed SetElementTooltip mechanism).
- Docs: D-36..D-40, ROADMAP second-session section, LOOP-009 (+ LOOP-008
  re-scope note), README refreshed where this session made it stale.
- **Nothing in this session has run on real hardware** — it spans two
  machines; LOOP-009 is the checklist.

## 2026-09-06 (operator verification and documentation reconciliation)

- The operator reports several days of issue-free use of the current client
  v0.10.0, host v0.4.0, and relay v0.2.0 combination. This closes the manual
  verification work in LOOP-001, LOOP-005, LOOP-007, LOOP-008, and LOOP-009:
  client panel/overlay, host widget, clipboard recovery, DVC command/reply,
  state polling, fullscreen control, per-monitor layout, and live theme
  handling all work in practice.
- Reconciled the public README and component documentation with that verified
  state. The retained DVC activation probe is documented as historical source,
  not a future placeholder.
- Removed the unused client `GetRdpMonitorRect` helper and the ignored,
  unsigned root `nul.exe` artifact. Compile/link verification remains clean.
- Relocated `EXPANSION_RESEARCH.md` and `ROADMAP.md` from `.raiden/state/` to
  the repository root because state schema v2 does not recognize them there.
  The subsequent RAIDEN doctor is clean: 10 OK, 0 WARN, 0 FAIL.
