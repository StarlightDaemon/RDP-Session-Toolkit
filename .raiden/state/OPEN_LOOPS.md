# Open Loops

- **LOOP-001 — Live-test the ported client mod (open, operator):** the ported
  `rdp-session-toolkit-taskbar-client.wh.cpp` v0.2.1 compiles cleanly but has not
  been loaded under Windhawk. V1's last actual operator live test passed at
  v1.1.1, before the thumbnail toolbar existed; the toolbar itself was added at
  v1.1.9 and has never been live-tested. So beyond the fork's rename (new mod id
  → new settings/storage scope) and the new relay receiver + debug toggle, the
  ported thumbnail toolbar itself also still needs its first manual pass on a
  real RDP session.
- **LOOP-002 — Relay sender validation — RESOLVED 2026-08-21 (validation);
  residual: instance targeting still open.** The `CitadelRdpTaskbarRelay`
  receiver now validates every WM_COPYDATA sender: it accepts only this mod's
  own process (the self-test) or the registered DVC relay plugin's exact
  registered EXE image path, and logs-and-ignores anything else (see D-11 and
  the relay plugin, D-10). The former marked TODO in the mod source is closed.
  *Residual (still open):* if two `mstsc.exe` sessions are both running, a
  sender locating the relay window by class name has no way to target a
  specific instance. Not a problem for the current single-signal design (each
  client's own relay plugin forwards within that client), but any future
  multi-session protocol will need instance targeting.
- **LOOP-003 — DVC probe two-machine live test — RESOLVED 2026-08-21.** The
  gating hardware test passed: `dvc-plugin/TESTING.md` is marked **VERIFIED
  (2026-08-21)** — `IWTSPlugin::Initialize`, `CreateListener`, and the
  `*** DVC SIGNAL RECEIVED ***` lines all appeared on the client, proving mstsc
  activates an out-of-process `AddIns` `{CLSID}`/`LocalServer32` plugin and
  delivers it a real DVC signal (the "pipe-activation" test). The probe
  registration was unregistered afterward. This is the architecture-gating pipe
  proof the whole DVC leg rested on; the production relay plugin (D-10) is built
  on this now-verified model. **Note:** the driving task named this "LOOP-001",
  but LOOP-001 here tracks the client-mod Windhawk live-test (never run — see
  below); the hardware/pipe test that actually passed is this loop, LOOP-003,
  which is the one resolved. See LOOP-006 for the still-untested full feature.
- **LOOP-004 — Debug self-test path uses an untimed SendMessageW (low
  priority, cosmetic):** `SendRelayTestMinimize` sends its WM_COPYDATA via a
  plain, untimed `SendMessageW`, unlike the thumb-bar teardown path
  (`Wh_ModUninit`), which uses `SendMessageTimeoutW`. Worth aligning in a
  future cleanup pass; not urgent.
- **LOOP-005 — Live-test the host-side mod (open, operator):**
  `host/rdp-session-toolkit-taskbar-host.wh.cpp` v0.2.0 compiles cleanly but
  has never been loaded under Windhawk. Needs its first manual pass on a real
  Windows 11 session host: widget injection into the taskbar XAML tree,
  WTSClientName/WTSConnectState display across connect/disconnect cycles,
  console-vs-RDP gating of the disconnect *and new minimize* buttons (the
  minimize send itself is covered end-to-end by LOOP-006), and the
  `TaskListButton::UpdateVisualStates` symbol resolving on the host's OS
  build. Also verify WTSRegisterSessionNotification succeeds inside
  explorer.exe (the 30 s timer is the fallback if it does not).
- **LOOP-006 — Full end-to-end minimize feature live test — RESOLVED
  2026-08-21 (operator-observed).** The whole cross-machine pipe — host mod
  Minimize button → `WTSVirtualChannelWrite('dvc::taskbar::relay', 0x01)` →
  client-side DVC relay plugin (`dvc-plugin/relay`) → `WM_COPYDATA` → client
  mod `CitadelRdpTaskbarRelay` receiver → `SW_MINIMIZE` on the mstsc frame —
  was exercised for real on two machines in a prior 2026-08-21 session (after
  the relay-plugin + minimize-pipe session that opened this loop, and before
  the overnight feature session that closed it): the operator clicked Minimize
  on the host and **directly watched the client's `mstsc` window minimize**.
  That is the proof this loop asked for — real command byte, real relay plugin
  forwarding, real receiver sender-validation accepting the relay's image
  path, real minimize. The feature is now "proven," not merely "built and
  plausible." Recorded on the operator's direct report; no transcript or state
  file in this repo captured the observation at the time, which is why this
  entry is the durable record of it. *Consequence for LOOP-001:* this also
  means the client mod **has** now been loaded under Windhawk in `mstsc.exe`
  at least once (the relay receiver ran live), so LOOP-001's "never loaded"
  framing is stale — the thumbnail toolbar and overlay, however, still have
  not been specifically checked and LOOP-001 stays open for those.
- **LOOP-007 — Live-test the v0.4.0 client-mod features (open, operator):**
  the five features added in the 2026-08-21 overnight session — fullscreen/
  windowed toggle (Ctrl+Alt+Break injection), session duration / idle display
  (overlay row + status-icon tooltip), connection-quality indicator (second
  `IMsTscAxEvents` sink advised on the RDP control via the `CoCreateInstance`
  hook), quick reconnect (WM_CLOSE → relaunch from the frame's WM_DESTROY),
  and stuck-session detection (`IsHungAppWindow` watchdog + alert popup with
  Force reconnect) — all compile cleanly but none has been loaded under
  Windhawk. Things to watch specifically, in rough order of uncertainty:
  (a) whether `OnNetworkStatusChanged` actually fires to a second sink in
  `mstsc.exe` (the Wh_Log line `RdpEvents: advised` must appear at control
  creation, then `RdpEvents: network status` lines during the session; if the
  first never appears the control was created by a path the hook does not
  see); (b) whether `SetForegroundWindow` succeeds from a thumbnail-button
  click so the Ctrl+Alt+Break chord is actually delivered (the log says
  `Fullscreen toggle: foreground not acquired` if not — the chord is then
  deliberately *not* sent); (c) whether the reconnect relaunch fires from
  WM_DESTROY before mstsc exits; (d) whether `/w:`/`/h:` alone reliably
  produce a windowed session when Default.rdp says fullscreen.

  **Update 2026-08-22 (client mod v0.7.0, D-27/D-28/D-29) — still open,
  re-scoped.** Live tests since confirmed two of these: (a) *no* `RdpEvents`
  line ever appeared, and (b) the panel's fullscreen toggle logged `foreground
  not acquired`. Both are now addressed and need re-testing. (a) Binary
  inspection found mstsc creates the control through mstscax's own class
  factory (`DllGetClassObject`), bypassing `CoCreateInstance` (D-29); the mod
  now hooks that path. Watch for, in order: `RdpEvents: hook mstscax!
  DllGetClassObject … Wh_SetFunctionHook=1` and `hook MsRdpClient (version 12)
  factory CreateInstance … =1` at init; `mstscax!DllGetClassObject #1 clsid=
  {1DF7C823-…} … factory's CreateInstance is the hooked one` when the session
  starts; `mstscax factory CreateInstance #1 …`; `FindConnectionPoint
  (IMsTscAxEvents) hr=0x00000000 — this object IS the RDP control`; `advised
  IMsTscAxEvents sink`; then `RdpEvents: network status …` during the session.
  If nothing fires, the `[diag @ frame created]` / `[diag @ 10 s into session]`
  summary lines say which counters stayed at zero. (b) The explorer branch now
  grants mstsc the foreground right (`AllowSetForegroundWindow`) before sending
  the toggle (D-27); watch for `Command Fullscreen toggle: AllowSetForeground
  Window(pid=…) granted` in explorer and `frame … is foreground, sending
  Ctrl+Alt+Break` in mstsc. New in scope: (e) Reconnect is now opt-in
  (`enableReconnect`, default off, D-28) — confirm the thumb-bar slot, the
  panel button, and the alert's Force reconnect all disappear with it off and
  return with it on; (f) with it on, start mstsc on a temp `.rdp`, delete the
  file, click Reconnect — expect `NOT launched: the connection file … no longer
  exists at launch time` and no replacement client.
- **LOOP-008 — Live-test the taskbar-embedded client widget (open, operator):**
  `client-embedded/rdp-session-toolkit-taskbar-client-embedded.wh.cpp` v0.1.0
  and the client mod's v0.5.0 additions (status snapshot,
  `CitadelRdpTaskbarLocalWidget` receiver, shared secret) compile cleanly but
  have never been loaded under Windhawk. Suggested order, with the log lines
  to watch: (a) client mod `LocalWidget: status snapshots → <path>` (first
  write) and widget `Status: watching <ModsWritable>\{...}` then
  `Status: session ACTIVE` — if the widget never goes active while a session
  is open, the storage-dir derivation (D-22: sibling of the widget's own
  `Wh_GetModStoragePath`, `local@` prefix or not) is the first suspect;
  (b) `… — local widget secret generated` in exactly one of the two mods and
  `… present` in the other; (c) click the icon — `Flyout shown at …` — and
  confirm click-outside dismisses it (if it stays open, `SetForegroundWindow`
  from the status thread was refused); (d) each row: widget
  `Command <label> (0x0N) → … handled=1` paired with client
  `LocalWidget: command 0x0N from pid=<explorer pid>` and the action's own
  log line; the fullscreen row shares LOOP-007(b)'s foreground-acquisition
  uncertainty, now from an explorer click rather than a thumbnail click;
  (e) close mstsc uncleanly (kill it) and confirm the icon dims within ~4 s
  (staleness, D-22); (f) `showButton` off + `stuckDetection` on still shows
  status (the watchdog tick is the writer then); (g) disable/re-enable each
  mod in Windhawk and confirm clean unload (no explorer or mstsc hang).
  *Residual from D-22:* with both `showButton` and `stuckDetection` off the
  widget shows "no session" by design; decide whether a dedicated writer is
  wanted after all. *LOOP-004 note:* the new channel's sender uses
  `SendMessageTimeoutW`; the relay self-test's untimed `SendMessageW` is
  unchanged (relay code is off-limits per D-24).

  **Update 2026-08-21 (client mod v0.6.0 consolidation, D-26) — still open,
  re-scoped.** The standalone embedded mod is retired: its `explorer.exe` code
  is now the `explorer.exe` branch of `rdp-session-toolkit-taskbar-client`
  (one mod, two `@include` targets). This loop must be re-run against the
  **merged** mod — enable the single mod and confirm Windhawk injects both
  branches (a `[mstsc.exe branch] … initialized` line from mstsc and an
  `[explorer.exe branch] … initialized` line from explorer). The specific
  thing to re-verify, and the reason this loop is emphatically **not** closed
  by the merge: the storage-path derivation that step (a) called the first
  suspect for "widget never goes active" is now structurally different — the
  explorer branch calls `Wh_GetModStoragePath` directly and, because both
  branches share one mod id, gets the *same* directory the mstsc branch writes
  to (no sibling search, no `local@` guessing; D-26). This is a **strong
  expectation** that the "no active session shown" symptom is fixed, **not a
  confirmed result** — it has not been watched working live under the merged
  architecture, and the standalone mod was never confirmed working live either.
  Watch for `LocalWidget: status snapshots → <path>` (mstsc) and the widget's
  `Status: session ACTIVE` with a real host while a session is open; if it
  still never goes active, the shared status path is again the first place to
  look. All other sub-steps (b)–(g) carry over unchanged. The settings keys the
  widget reads are renamed to the `embedded…` group (D-26); confirm the panel
  still honors position/width/font/offset/show-when-no-session after the merge.

### LOOP-SWEEP-20260831-001 — RDP Session Toolkit doctor WARN — state_unrecognized

- Status: open
- Gate: operator
- Success Condition: `raiden_updater.cli doctor --instance <path to RDP Session Toolkit>` no longer reports WARN for check `state_unrecognized` (next fleet sweep shows it OK).
- Check: doctor:RDP Session Toolkit:state_unrecognized
