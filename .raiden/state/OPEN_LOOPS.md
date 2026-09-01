# Open Loops

- **LOOP-001 — Live-test the ported client mod (open, operator; narrowed
  2026-09-01):** the ported `rdp-session-toolkit-taskbar-client.wh.cpp` v0.2.1
  compiles cleanly but has not been loaded under Windhawk. V1's last actual
  operator live test passed at v1.1.1. Beyond the fork's rename (new mod id →
  new settings/storage scope) and the relay receiver + debug toggle, the
  **floating overlay button** still needs its first manual pass on a real RDP
  session — hide-the-connection-bar, the four rows, drag-and-persist, the
  hotkey.

  **Narrowed 2026-09-01 (client mod v0.9.0, D-33).** This loop also tracked the
  **thumbnail toolbar**, which was added upstream at v1.1.9, was never once
  live-tested here, and has now been **removed** rather than tested — the
  taskbar-embedded panel already covered the same five actions. That half of
  this loop is closed by deletion, not by verification, and nothing is owed on
  it. Everything the toolbar used to present is now LOOP-008's business.

  **Checked against the 2026-09-01 live-test evidence — still fully open.** That
  evidence covers the host mod, the fullscreen-toggle foreground path, the
  connection-quality advise, and the thumbnail-toolbar removal. None of it
  touches the **floating overlay button**, which is the whole of what this loop
  still owes. Note that the overlay has been off by default since D-30
  (`showOverlay`), so a live pass has to turn it on deliberately — it will not
  be exercised incidentally by any other loop's testing.
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
  plain, untimed `SendMessageW`, unlike `Wh_ModUninit`'s frame-thread teardown
  send, which uses `SendMessageTimeoutW`. (That send was the thumb-bar teardown
  message until v0.9.0; it is now `g_msgSinkTeardown`, carrying the event
  sink's Unadvise alone — D-33. The comparison is unchanged.) Worth aligning in
  a future cleanup pass; not urgent.
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

  **Narrowed 2026-09-01 (live-test evidence) — most of it is confirmed, two
  items are not.** Confirmed on real hardware across multiple separate test
  sessions, not just once: the widget's **status display** (which also proves
  the XAML injection, the walk to `Grid#RootGrid`, and therefore that the
  `TaskListButton::UpdateVisualStates` symbol resolves on this host's OS build —
  none of that can produce a visible widget if it fails); the **Disconnect**
  button's `WTSDisconnectSession`; and the **minimize-send path** (LOOP-006
  proved the whole cross-machine pipe once; this is repeated confirmation of the
  host end of it). Also confirmed, though this loop never itemised it: the
  widget's **taskbar / system-tray overlap fix** — the margin-based
  right-anchoring inherited from D-7/D-21 — works. Recording it here so it is
  not left as folklore.

  **Still open, because the evidence does not cover it:**
  - **Console-vs-RDP gating of the Disconnect and Minimize buttons.** Every
    observation so far is of the RDP case, where the buttons are *supposed* to
    work. The property that actually matters is the negative one D-8 was written
    for — that the mod can never disconnect the physical console session and
    kick a locally-signed-in user to the lock screen — and that needs a
    deliberate console-session pass. A working button over RDP is no evidence
    about it.
  - **Whether `WTSRegisterSessionNotification` actually succeeds inside
    `explorer.exe`.** A status display that tracks connect/disconnect cycles
    looks identical whether the event registration works or the 30 s safety-net
    timer is quietly carrying it. This needs the registration's own log line, or
    an observed refresh faster than 30 s — not the display alone.
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

  **Reconfirmed 2026-09-01.** The minimize path has since been observed working
  on real hardware across multiple separate test sessions, not only the single
  occasion this entry was written from. The "recorded on the operator's direct
  report, no transcript" caveat above no longer carries the whole weight of the
  claim.
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

  **Update 2026-09-01 (client mod v0.9.0, D-33) — still open, re-scoped again.**
  The thumbnail toolbar is removed, so every sub-item that named it now applies
  to the **taskbar-embedded panel** instead; none of the underlying mechanisms
  changed, and none of the uncertainties are resolved by the removal:
  - (a) connection quality is unaffected — the sink, the two creation-path
    hooks, and all the `RdpEvents:` diagnostics are untouched. What changed is
    only where a report is *shown*: the panel's status text and its tooltip,
    not a thumb-bar icon colour. Watch the same log lines.
  - (b) the fullscreen toggle is now driven exclusively from the panel, i.e.
    from an explorer.exe click with `AllowSetForegroundWindow` (D-27) — the
    thumbnail-button click path this item also covered no longer exists, so
    this is now a single path to verify rather than two.
  - (e) Reconnect-is-opt-in now means one button to watch (the panel's) plus
    the alert's Force reconnect, not two buttons plus the alert.
  - New: `showFullscreenToggle` now collapses the panel's fullscreen button
    (it used to hide a thumb-bar slot) — confirm it disappears with the setting
    off and returns with it on, exactly like Reconnect.
  - New: confirm `Wh_ModUninit` still tears the event sink down cleanly now
    that it travels under its own message (`WH_RdpstkClient_SinkTeardown`,
    D-33) rather than riding along with the thumb-bar teardown. Disable the mod
    while a session is open and look for `RdpEvents: unadvised (mod unload)`;
    then close a session with the mod loaded and look for `RdpEvents:
    unadvised (frame WM_DESTROY)`. Both paths must still log.

  **Update 2026-09-01 (live-test evidence) — (b) resolved, (a) narrowed to half
  its size; the loop stays open.**
  - **(b) fullscreen-toggle foreground — RESOLVED.** D-31's fix is confirmed
    working live, repeatedly, and across *two* different underlying success
    paths: the no-current-foreground direct-trust path D-31 was built for, and a
    plain first-call `SetForegroundWindow` success that needed no fallback at
    all. The foreground is acquired and the Ctrl+Alt+Break chord is therefore
    delivered rather than suppressed — which is exactly and only what this item
    asked. D-27's grant and D-31's direct path both stand as built; nothing is
    owed here.
  - **(a) connection quality — NARROWED; the advise is done, the event is not.**
    Confirmed across multiple separate test sessions: D-29's factory-path hook
    fires, the DISPIDs resolve for real from the registered type library, and
    `Advise` succeeds returning a real cookie. The sink is genuinely subscribed,
    so D-29 is live-confirmed and this item's original phrasing — "whether
    `OnNetworkStatusChanged` actually fires to a second sink … if the first
    [`RdpEvents: advised`] never appears the control was created by a path the
    hook does not see" — is settled in the affirmative on the hook half.
    **What remains open is strictly narrower and is a different question:** no
    `OnNetworkStatusChanged` callback has ever been observed arriving with real
    data. Every diagnostic checkpoint has read `quality=0`, including well past
    the `[diag @ 10 s into session]` line. The open question is no longer "does
    the hook fire" but "does mstscax broadcast this event to a *second*
    subscriber at all" — D-16's original untested assumption, now isolated as
    the only unproven link. Until a real `RdpEvents: network status` line
    appears, the only honest thing any surface can show is "waiting for Remote
    Desktop's first report" (D-16); confirming the panel actually says that is a
    LOOP-008 item.
  - **Untouched by this evidence and still fully open:** (c) the WM_DESTROY
    reconnect relaunch; (d) `/w:` + `/h:` producing a windowed session against a
    fullscreen `Default.rdp`; (e) the `enableReconnect` off/on gating of the
    panel button and the alert's Force reconnect; (f) the deleted-`.rdp` refusal
    at launch time; and both 2026-09-01 additions — `showFullscreenToggle`
    collapsing the panel's fullscreen button, and both `UnadviseRdpEvents`
    teardown paths (`RdpEvents: unadvised (mod unload)` and
    `… (frame WM_DESTROY)`). The teardown paths in particular are *not* covered
    by the confirmed thumbnail-toolbar removal: what was confirmed there is the
    absence of thumb-bar log lines, not the presence of the sink's own.
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
  *Residual from D-22 — RESOLVED 2026-09-01 (client mod v0.9.2, D-34).* The
  `LocalWidget` message-only window (always alive regardless of settings) now
  carries its own unconditional 1 s `SetTimer` calling
  `WriteLocalWidgetStatus()`, so a session with both `showButton` and
  `stuckDetection` off no longer goes silent — see D-34 for why this became
  necessary once D-33 removed the thumbnail toolbar's fallback surface. Still
  to confirm live: with both settings off, the panel shows an active session
  (previously it would have shown "no session"). *LOOP-004 note:* the new channel's sender uses
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

  **Update 2026-09-01 (client mod v0.9.0, D-33) — still open, widened.** The
  thumbnail toolbar is removed and the panel is now the toolkit's only
  full-featured client surface, so this loop is the one that matters most. Two
  new things to check on top of everything above, both purely presentational
  (no channel or state change is involved):
  - **The migrated status tooltip.** Hover the panel's host-name / status text
    with a session open. Expect up to three lines, rebuilt on this side from
    the status snapshot rather than read from mstsc: `Session <coarse> · this
    PC idle <coarse>` (with *Show session duration and idle time* on),
    `Quality n/4 (label) · bandwidth … · rtt … ms` (with *Show connection
    quality* on), and, while the watchdog reports a hang, `NOT RESPONDING for
    N s …`. The quality line must say `waiting for Remote Desktop's first
    report` — never a fabricated level — until a real `RdpEvents: network
    status` line appears in the mstsc log; it is the same honesty rule as D-16,
    now enforced on the reader side. Turn each of the two settings off and
    confirm the corresponding line disappears. With **no** session, the tooltip
    must explain itself rather than go blank: "no session", "last update N s
    ago", or "no mstsc.exe running this mod", depending on what is actually
    true. Cross-check the numbers against the mstsc-side log so the reader is
    not quietly showing stale or mis-scaled values.
  - **The five action tooltips.** Hover each button and confirm a short
    plain-language description appears, and specifically that the fullscreen
    one flips wording with the session's real state (it is rewritten alongside
    the label and the glyph on every poll) — a tooltip describing the wrong
    direction would be worse than none.
  Also worth one look: `showFullscreenToggle` off should remove the fullscreen
  button from the row entirely (`Visibility::Collapsed`), the same way
  `enableReconnect` off removes Reconnect — not merely dim it.

  **Update 2026-09-01 (live-test evidence) — narrowed, still open, and still the
  loop that matters most.**

  *Confirmed live:*
  - **D-33's removal is clean on the mstsc side** — no thumbnail-toolbar log
    lines appear post-removal.
  - **The panel renders and shows the migrated status tooltip.** So the
    explorer branch injects, the panel is alive, and it is reading and
    reformatting a snapshot rather than throwing or blanking.
  - **The command channel carries a command end-to-end.** Since D-33 the
    fullscreen toggle is driven *exclusively* from the panel (and it was already
    the panel path in D-27/D-31), so LOOP-007(b)'s repeated live confirmation
    necessarily means an explorer-side click reached the mstsc branch: the
    shared secret was present and validated in both processes,
    `SendLocalWidgetCommand` was handled, and `ToggleFullscreen` ran. That
    substantially retires step (b) — the secret demonstrably works, even though
    nobody has read the "generated"/"present" log lines themselves — and the
    fullscreen row of step (d). It is also the first practical evidence that
    D-26's merged-mod plumbing works, for the command half.

  *Still open — and it is most of the loop:*
  - **The status half is not confirmed.** Nobody has watched
    `LocalWidget: status snapshots → <path>` on the mstsc side against
    `Status: session ACTIVE` on the panel, or cross-checked the panel's numbers
    against the mstsc log. A tooltip that renders is not proof the record behind
    it is fresh, correctly scaled, or the one the mstsc branch just wrote — so
    D-26's "the sibling-path guessing is structurally eliminated" remains a
    strong expectation, not a checked result. Step (a) stands.
  - **The other four command rows** — minimize, restore, reconnect, disconnect —
    are unexercised from the panel.
  - **D-34's new unconditional 1 s writer has never been run.** Its whole point
    is that the panel stays live with both `showOverlay` and `stuckDetection`
    off, which is precisely the combination nobody has tried; this replaces the
    D-22 residual noted above as the thing to confirm here.
  - **Everything presentational from the 2026-09-01 widening**: the tooltip's
    three lines and their two setting gates, the no-session wording, the quality
    line saying "waiting for Remote Desktop's first report" (which per
    LOOP-007(a) is the only thing it can honestly say today, and therefore the
    exact string to look for), the five action tooltips, and the fullscreen
    tooltip flipping direction with real session state.
  - **The remaining mechanics**: the ~4 s staleness dimming after an unclean
    mstsc kill, `showFullscreenToggle` collapsing the fullscreen button, and
    clean disable/re-enable of the mod in both processes.

### LOOP-SWEEP-20260831-001 — RDP Session Toolkit doctor WARN — state_unrecognized

- Status: open
- Gate: operator
- Success Condition: `raiden_updater.cli doctor --instance <path to RDP Session Toolkit>` no longer reports WARN for check `state_unrecognized` (next fleet sweep shows it OK).
- Check: doctor:RDP Session Toolkit:state_unrecognized
