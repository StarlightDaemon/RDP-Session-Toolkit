# Expansion Research — where this toolkit goes next

**Date:** 2026-09-01
**Status:** research and recommendation only. No mod code was written, changed, or
fixed for this document; nothing was committed or pushed.
**Purpose:** re-open the parked feature backlog with the *actual current
architecture* in hand, fold in external research, and hand the operator a ranked
recommendation rather than a menu.

Baseline read for this assessment: `DECISIONS.md` D-1 through D-33 in full,
`ROADMAP.md`, `OPEN_LOOPS.md`, and both mod sources end to end — the client mod
(`taskbar-integration/client/…client.wh.cpp`, v0.9.1, 5 454 lines, `mstsc.exe` +
`explorer.exe` branches) and the host mod
(`taskbar-integration/host/…host.wh.cpp`, v0.2.0, `explorer.exe`), plus
`dvc-plugin/relay/` for the wire contract.

---

## 1. External research and what it actually changes

Four findings were supplied. Two settle open worries, one is a concrete feature
lead, and one is a scope boundary. None of them invalidates a decision on record.

### 1.1 Microsoft retired the separate "Remote Desktop" client in March 2026 — mstsc is unaffected

**What it means here: nothing needs to change, and one background worry can be
closed.** The retired client is the MSI/Store app used for Azure Virtual Desktop,
Windows 365 and Dev Box. The built-in `mstsc.exe` this toolkit hooks is explicitly
out of scope for that retirement and remains supported.

That matters because essentially the entire client side is bound to `mstsc.exe`
specifically: the `@include mstsc.exe` branch, the `TscShellContainerClass` frame
latch in `CreateWindowExW_Hook`, the `BBarWindowClass` hide, the Ctrl+Alt+Break
synthesis (D-14), the `mstscax.dll` factory hooks (D-29), and the reconnect
helper's `mstsc` command-line reconstruction (D-17). A retirement of that binary
would have been an extinction event for the client mod. It is not happening.

**The boundary worth writing down:** the replacement client for cloud scenarios
(the *Windows App*) is a different program with **no `mstsc.exe` process at all**.
Nothing in this toolkit's client side can be made to work with it — not by
porting, not by adding an `@include`. If the operator ever moves a workflow to the
Windows App, the client mod does not follow it. That is a scope statement, not a
gap to fill. (The Windows App's own recent investment — dynamic display
resolution, better multi-monitor — is in the same category: interesting, not
actionable here.)

### 1.2 API hooking to extend an RDP client is an established commercial technique

Devolutions' Remote Desktop Manager has publicly referenced doing exactly this.
**Also no action — but it retires a second worry.** D-16 and D-29 spent
considerable effort justifying the hooking approach and confining it to public,
documented COM (the `IConnectionPointContainer` / `IMsTscAxEvents` advise), and
D-29 went as far as parsing `mstsc.exe`'s import tables to prove the control's
real creation path. That care was correct and should be kept. What this finding
adds is that the *category* of approach is mainstream rather than fringe, so the
remaining risk on that work is ordinary engineering risk, not "are we doing
something nobody sane does". As of 2026-09-01 that risk has shrunk further: the
hook demonstrably fires and the sink is advised with a real cookie; what is left
open is only whether mstscax delivers the event to a second subscriber
(LOOP-007a).

### 1.3 rdpclip.exe wedging is a common, well-documented failure with a manual fix

**This is the real lead, and it is the strongest new candidate in this document.**
`rdpclip.exe` is the clipboard-redirection process that runs *inside the session
on the host*. When it wedges, copy-and-paste between client and session dies while
the rest of the session stays perfectly responsive. There is no in-box UI to fix
it; the universal remedy across many independent sources is manual — kill
`rdpclip.exe` and start it again, inside the session.

The host mod's `explorer.exe` is already running **in that exact session, as that
exact user**. The repair is therefore a purely local host-side action with the
same shape as the existing `WTSDisconnectSession` button (D-8): re-check the
protocol at click time, act on the current session only, never touch anything
outside it. Full assessment in §4.1.

### 1.4 The Windhawk ecosystem is active; taskbar-embedded widgets are an established category

**No action; confirms the presentation direction.** The taskbar-embedded panel
(D-21 → D-25 → D-26 → D-33) is now the toolkit's only full-featured client
surface, and D-33 removed the thumbnail toolbar partly on the strength of that
bet. This finding says the bet is on a popular, well-trodden pattern rather than a
fragile one-off. It does not, however, say anything about the two specific
techniques this project actually depends on — the
`TaskListButton::UpdateVisualStates` symbol hook and the `Grid#RootGrid`
injection — which remain build-version-sensitive and are exactly what LOOP-005 /
LOOP-008 exist to verify.

---

## 2. What a new feature can actually build on

The single most important input to the assessments below is an honest inventory of
what is *already there to reuse* versus what would be new infrastructure.

### 2.1 Cheap to extend (a new feature is a few dozen lines in an existing pattern)

| Asset | Where | What extending it costs |
|---|---|---|
| **Status snapshot** `LocalWidgetStatus` | shared contracts section; written by the mstsc branch ~1/s, read by the explorer branch 1/s | Add a field, bump `kLocalWidgetStatusVersion`, set it in `WriteLocalWidgetStatus`, read it in `ApplyStateToWidget` / `FormatEmbeddedStatusTooltip`. Both ends are **one file, one mod id** since D-26 — no hand-synchronised copies. |
| **Local command channel** `CitadelRdpTaskbarLocalWidget` | client mod, both branches | Add a `LocalWidgetCommand` byte, a `case` in `LocalWidgetWndProc`, an entry in `LabelForCommand` / `CommandEnabledForState`, and one `MakeActionButton(...)` call. The 32-byte HKCU secret, the constant-time compare, the fail-closed validation and the off-the-UI-thread send (D-23) all apply automatically. |
| **Shared action functions** | `namespace client` | `MinimizeRdpFrame`, `RestoreRdpFrame`, `ToggleFullscreen`, `DisconnectSession`, the reconnect helpers. A new action written beside them is reachable from every surface at once — D-33 explicitly preserved this. |
| **Existing 1 s ticks** | overlay `STATUS_TIMER_ID`, watchdog `WATCHDOG_TIMER_ID` | A new periodic client-side check rides an existing tick. D-22's "no new timer" discipline is a real constraint and has been honoured throughout. |
| **Watchdog alert window** | `AlertWndProc`, mstsc | A fully built pattern for *detect a condition on a tick → raise a topmost GDI alert on the session's own monitor → offer two click zones → never act unasked*. A second condition reusing this shape is cheap. |
| **Host WTS state machine** | host mod `QueryWtsState` + `WtsWndProc` | Already event-driven on `WM_WTSSESSION_CHANGE`, with a 30 s safety-net timer, change detection (`changed`), and `WTSClientName` / `WTSConnectState` / `WTSClientProtocolType` in hand. A new host-side reaction to connect/disconnect is a few lines inside one existing function. |
| **Host widget button** | host mod `BuildWidget` | Disconnect and Minimize are the template: build the button, gate `IsEnabled`/`Opacity` on RDP in `ApplyStateToWidget`, **re-check the protocol at click time**, and run anything with latency on the detached one-shot thread with the `g_MinimizeSendsInFlight`-style drain in `Wh_ModUninit` (D-12). |
| **Host → client DVC pipe** | host mod → relay plugin → `CitadelRdpTaskbarRelay` | One new `RelayCommand` byte plus one `case` in `RelayWndProc`. This is the *only* leg proven end-to-end on real hardware (LOOP-006), but each addition still needs a two-machine test. |

### 2.2 Genuinely new infrastructure (do not let anyone call these cheap)

- **Client → host signalling does not exist, in any form.** The relay plugin holds
  an `IWTSVirtualChannel*` it could in principle `Write` to, but three things are
  missing and all three are real work: (i) nothing in the client mod can talk to
  the relay plugin — there is no reverse IPC and no reverse validation model, and
  D-11 / D-13 are emphatic that the relay channel's narrow exact-binary-path
  guarantee must not be widened; (ii) the channel only exists while the host has
  it open, and the host mod's send is an **open-write-close per click** — there is
  no persistent channel to write back on; (iii) the host mod never calls
  `WTSVirtualChannelRead` and has no reader at all. Any feature that needs the
  client to tell the host something is a new channel-lifetime design, a new
  host-side read loop, a new validation model, and a fresh two-machine live test.
- **A flyout or any pop-out UI on either taskbar panel.** D-7's inherited landmine
  stands: a WUX `Popup` does not render inside the taskbar's XAML Island
  compositor target. The Win32 `WS_POPUP` GDI flyout that used to solve this was
  **deliberately deleted** in D-25. Re-introducing one is not a tweak; it is
  undoing a decision. Tooltips on panel elements *do* work and are the cheap way
  to show more text (proven by D-33's tooltip migration).
- **Retaining and using the RDP ActiveX control pointer.** The event sink
  (D-16 / D-29) reaches the control at creation but keeps only the connection
  point; **it never holds an `IMsRdpClient*`** — and that, not the state of the
  hook, is the real blocker. D-29's creation-path hook is **live-confirmed** as
  of 2026-09-01 (the factory fires, `FindConnectionPoint` succeeds, `Advise`
  returns a real cookie, across multiple sessions), so the control object *is*
  reached; the only thing still unproven on that leg is whether mstscax delivers
  `OnNetworkStatusChanged` to a second subscriber — the event has never arrived
  with data (LOOP-007a). Anything built on a retained control pointer therefore
  needs the mod to start retaining and lifetime-managing one on mstsc's UI
  thread, which is new work regardless of how the event question lands.
- **Anything on the host that needs to observe the host's own desktop activity**
  (notifications, foreground app, user input inside the session). The host mod
  today reads WTS session facts and nothing else.

### 2.3 One structural observation

Every low-cost, low-risk candidate in this document lands on the **host mod**.
That is not a coincidence. The client mod is at v0.9.1 after three sessions of
feature work and carries a long unverified-live tail (LOOP-007, LOOP-008); the
host mod has sat at v0.2.0 since 2026-08-21 with two buttons and a WTS query. The
cheap surface area is on the host side because that is where the least has been
built.

The honest counterweight: the host mod is also the **least live-verified**
component. LOOP-005 has never been run; the only host-side behaviour ever observed
working is the Minimize → DVC send (LOOP-006). Adding features there means their
first live test is entangled with LOOP-005's own first pass. That argues for
sequencing, not for avoiding the host mod — see §6.

---

## 3. Re-evaluated backlog — the six parked ideas

These six were parked as "maybe later" in an earlier brainstorming session, before
the status file, the local command channel, the watchdog, the shared action
functions, or the taskbar-embedded panel existed. **They are not recorded anywhere
in this repository's state files** — they lived only in that session's context.
This document is now their durable record.

Each is assessed on the three axes asked for: real value, real complexity *given
the current code*, and real risk.

### 3.1 Clipboard safety net — **re-scope it; the useful version is §4.1**

The parked scope is ambiguous, and the ambiguity matters because the two readings
have opposite verdicts.

**Reading (a): preserve clipboard contents across a disconnect/reconnect.**

- *Value:* low-to-moderate. Addresses a rare annoyance.
- *Complexity:* **high, and none of the built architecture helps.** The clipboard
  is a shared, format-rich, ownership-based resource: `CF_HDROP`, delayed
  rendering, and formats whose owner disappears when the session does.
  Snapshotting arbitrary formats faithfully is a known-hard problem, not a bounded
  one. It needs a clipboard-format-listener window and a new ownership model — new
  infrastructure in a process (`mstsc.exe`) that is already contending with
  rdpclip for the same resource.
- *Risk:* the highest of any candidate here — silently corrupting or duplicating
  clipboard content is worse than the problem being solved, and it fails
  invisibly.
- **Verdict: do not build as scoped.**

**Reading (b): make the clipboard *work again* when it breaks.** This is exactly
the rdpclip restart in §4.1, and it is the top recommendation of this document. It
is cheaper, safer, and addresses the failure people actually hit.

**Recommendation: retire "clipboard safety net" as a backlog entry and replace it
with the rdpclip restart action.**

### 3.2 Keyboard-shortcut capture toggle — **blocked on unproven work; revisit later**

Switch whether Windows-key combinations go to the session or stay local — the
control's documented `IMsRdpClientAdvancedSettings::KeyboardHookMode` (0 local /
1 remote / 2 fullscreen-only).

- *Value:* moderate. Real convenience, but most people set it once in the `.rdp`
  file and never touch it again. It is not a mid-session need the way
  minimize / fullscreen / disconnect are.
- *Complexity given the current code:* **deceptively high.** The presentation half
  is trivial (a command byte, a `case`, a button — §2.1). The mechanism half is
  not:
  - It needs the mod to **retain an `IMsRdpClient*`, which it currently never
    does** — the sink takes the connection point and drops the object. That is
    the hard dependency, and it is real work: pointer retention plus lifetime
    management on mstsc's UI thread, under the same "this is an STA object, and
    it may only be released on its home thread" constraint the sink already
    lives with (D-16). It is **not** blocked on the hook being unproven — as of
    2026-09-01 D-29's factory hook is **live-confirmed**: it fires and the sink
    is advised with a real cookie across multiple sessions, so the code path
    that would hand over an `IMsRdpClient*` demonstrably runs. (Separately, and
    not a dependency for this feature: no `OnNetworkStatusChanged` event has
    ever arrived with data — LOOP-007a. That is an event-delivery question;
    setting a property on the control does not depend on it.)
  - Whether `mstsc.exe`'s container honours a *mid-session* change to that
    property is unverified and cannot be verified here.
- *Interaction with a decision on record:* **D-14 must be acknowledged, not
  bypassed.** D-14 identified the control's documented `IMsRdpClient::FullScreen`
  property as a candidate direct path and *deliberately declined it*, preferring
  to synthesise Microsoft's own Ctrl+Alt+Break chord, because the property's
  interaction with mstsc's container (`ContainerHandledFullScreen`,
  `OnRequestGoFullScreen`) could not be verified. `KeyboardHookMode` is a
  different property with weaker container coupling — it negotiates no window
  state — so this is not a straight contradiction. But it *is* the same class of
  move, and unlike the fullscreen case there is **no Microsoft-provided chord to
  reuse**: Windows exposes no keyboard shortcut for this setting, so the escape
  hatch that made D-14 comfortable does not exist here. Setting the property
  directly is the only mechanism.
- *Risk:* moderate. Getting keyboard capture wrong mid-session means keystrokes
  land on the wrong machine, which is confusing and briefly unrecoverable (you may
  not be able to type the combination that fixes it).
- **Verdict: defer — but for the accurate reason.** The deferral stands; only the
  reasoning needed correcting. The blockers are that the mod **holds no
  `IMsRdpClient*` at all** and would have to grow one, and that whether mstsc's
  container honours a *mid-session* change to the property cannot be verified
  here. It is **no longer** gated on D-29 being proven live — that happened on
  2026-09-01. If the mod ever retains a working control pointer for some other
  reason, this becomes genuinely cheap and is a good second-wave candidate.

### 3.3 Host connection history log — **cheap now; modest value**

Record connect/disconnect events on the session host.

- *Value:* modest but real, and it is the one question the toolkit currently
  cannot answer at all. The host widget shows who is connected *now*; nothing
  anywhere answers "who connected, from where, and when, while I was not looking".
- *Complexity given the current code:* **genuinely small — the hard half is
  already built.** `QueryWtsState` already fires on every `WM_WTSSESSION_CHANGE`,
  already has `WTSClientName` / `WTSConnectState` / `WTSClientProtocolType`, and
  already computes `changed` and logs a state-transition line. The delta is:
  - a storage path — the host mod does **not** currently call
    `Wh_GetModStoragePath` at all, so it needs the client mod's `GetModStorageDir`
    / `BuildModStorageFilePath` pattern copied across (~30 lines of
    already-proven code);
  - a timestamp — `GetLocalTime` plus formatting; the host mod has no time
    handling today, so this is new but trivial;
  - append-on-change instead of log-on-change.

  No new thread, no new timer, no new window, no IPC, no channel.
- *Presentation is the more expensive half, and should probably be skipped.* The
  widget is a 220 px panel with two text lines and two buttons, and there is no
  flyout available (§2.2). A tooltip carrying the last few events on the
  client-name column is achievable — the client branch's `SetElementTooltip` proves
  tooltips work in this injection context, though the host mod would need that
  ~15-line helper — but a scrollable history in the taskbar is not on the table.
  **Recommendation: write the file, do not build a viewer.** A plain text file the
  operator opens is the right fidelity for this.
- *Risk:* low. One consideration worth naming rather than discovering later: the
  file is a plaintext record of client machine names and connection times, under
  the user's profile. Not sensitive in a single-operator setup, but it is the
  first thing this toolkit would persist that is about *who*, not about *state*.
  Bounded size (append with a cap, or roll) should be in scope from the start.
- **Verdict: build it. Small, self-contained, no new mechanisms.**

### 3.4 Prevent host sleep during a session — **cheap now; prevents an expensive failure**

Keep the session host awake while an RDP session is connected.

- *Value:* high relative to cost. A host that sleeps mid-session drops the session
  and is genuinely hard to recover — you cannot wake it remotely without
  Wake-on-LAN. The usual workaround is setting the host to never sleep at all,
  permanently; this feature makes that conditional on a session actually being
  connected, which is strictly better behaviour.
- *Complexity given the current code:* **small, and it reuses the existing state
  machine wholesale.** `QueryWtsState` already knows the exact transitions that
  should arm and disarm it (`connectState` + `protocol`, with change detection
  already computed). The implementation is: arm on becoming RDP-and-active, disarm
  otherwise, release in `Wh_ModUninit`. Prefer `PowerCreateRequest` /
  `PowerSetRequest` / `PowerClearRequest` over `SetThreadExecutionState` — the
  request carries a **reason string visible in `powercfg /requests`**, which makes
  a stuck request diagnosable instead of mysterious, and it is not tied to a
  specific thread's lifetime. Both are `kernel32`, so `@compilerOptions` does not
  change (relevant given D-32's real link step and D-33's care about that block).
  `ES_SYSTEM_REQUIRED` semantics only — there is no reason to keep the host's
  physical display awake.
- *Risk:* low but sharp, and it is the whole risk surface: **a mod that keeps a
  machine awake and fails to release the request keeps it awake forever.**
  Mitigations are all cheap and should be non-negotiable — release on disconnect,
  release in `Wh_ModUninit`, log both arm and release, and use the named-request
  API so `powercfg /requests` identifies the culprit.
- *One honest caveat.* Windows often already avoids sleeping while a session is
  active, because remote input arrives as real input and resets the idle timer.
  The scenario this actually fixes is narrower: hosts with a short or scheduled
  sleep policy, and sessions that are *connected but idle* — you have walked away
  while a long job runs. That is a real scenario and an expensive failure, but the
  feature is a **guarantee**, not a fix for a universally-observed bug. It should
  be described that way in its setting text.
- *Default:* off, consistent with D-28's posture that anything which can surprise
  you is opt-in.
- **Verdict: build it.**

### 3.5 Host-initiated notification badge — **the transport is cheap; the source is unbuilt**

Something happens on the host; a badge appears on the client's taskbar panel.

This is the candidate whose cost is most easily understated, so it is worth
separating the two halves.

- **The transport is largely built and cheap.** Host mod → `dvc::taskbar::relay` →
  relay plugin → `CitadelRdpTaskbarRelay` → a flag in the client mod → the status
  snapshot → the panel. Every hop exists, and this is the one direction the pipe
  already runs. Adding badge-set / badge-clear is two `RelayCommand` bytes, two
  `case`s, one snapshot field, one panel visual.
- **The source is entirely unbuilt, and there is no cheap documented mechanism.**
  The host mod knows WTS session facts and nothing else. "A notification happened
  on the host" requires observing the Windows notification platform:
  `UserNotificationListener` requires a packaged app identity and explicit user
  consent, and is not available to an unpackaged `explorer.exe` injection in any
  documented way. The alternatives — hooking `FlashWindowEx`, watching taskbar
  button visual states, polling for a specific process — are each a research
  project in their own right with no guarantee of a general answer. **This is new
  infrastructure of an open-ended kind, and it is the expensive half.**
- **Two further design gaps.** The relay protocol is one byte, one shot, and
  entirely stateless; a badge needs state and a clear rule (cleared by what? does
  it survive a reconnect? what about two sessions?). And the badge would inherit
  D-22's residual gap — with both the floating overlay and `stuckDetection` off,
  nothing writes the snapshot, so the badge would silently never appear.
- *Risk:* moderate, mostly as scope creep. It also multiplies traffic on the
  least-tested leg of the system: LOOP-006 proved exactly one byte on one path,
  and nothing else has ever crossed it.
- **Verdict: not now, as scoped.** If it returns, it should return with **one
  concrete named trigger** ("this process exited", "this file changed") rather
  than "notifications" — a specific trigger is buildable in an afternoon, and
  general Windows 11 notification observation from an unpackaged process is not.
- *A tractable subset worth naming:* a **manual "ping the client" button** on the
  host widget — you click it on the host, the client's panel flashes. That is the
  badge with the source problem deleted: the trigger is a click, exactly like
  Minimize, and it reuses the entire proven pipe. It is cheap. Its *value*,
  though, is questionable in a single-operator setup where the same person is at
  both ends and is the one doing the clicking. Mechanism cheap, value thin.

### 3.6 Idle-timeout awareness with an auto-action — **split it; build neither half as scoped**

- **The awareness half is largely shipped already.** `GetLocalIdleMs()` exists
  (D-15), `localIdleMs` is already in the status snapshot, and the panel's status
  tooltip already renders "this PC idle 4m" (D-33). The delta between what ships
  today and "idle awareness" is a threshold plus an alert.
- **That delta is cheap** — the watchdog thread already ticks once a second and
  already owns an alert window with a two-zone action row, and the pattern *detect
  on a tick → alert → wait for a click → never act unasked* is exactly this
  feature's shape. A second condition on that tick and a second alert variant is a
  small amount of code.
- **But the value of the alert is thin, and for a specific reason.** The signal is
  *this computer's* local input idle. An alert raised on the client's own screen
  because you have walked away from the client is an alert nobody is present to
  see. The place idle-awareness would actually pay off is the **host** ("nobody has
  touched this session in two hours") — and the host mod has no idle measurement
  and no alert surface, so that version is new work, not a cheap extension.
- **The auto-action half should not be built, on two independent grounds.**
  1. *It reverses the toolkit's central safety posture.* D-18 states the mod
     **never auto-reconnects** — detection shows the alert and waits. D-28 made
     reconnect opt-in and off by default because "relaunching a connection is the
     one action here that can do something unexpected". D-8, D-12 and D-19 all
     re-check state at click time and all gate on an explicit operator click. An
     auto-disconnect on idle would be **the first action in this toolkit that
     happens with no click at all.** That is a change in kind, not a new setting,
     and it should not be slipped in as one.
  2. *The signal does not mean what the action needs it to mean.* D-15 pinned the
     semantics deliberately: idle is local input idle, explicitly **not** a measure
     of activity inside the remote session — "a 'have I walked away from this
     machine' signal". Walking away from the client while the remote session runs a
     long job is precisely the case where the session must **not** be dropped — and
     local-input idle is exactly the signal that would fire then. The auto-action
     would be driven by a measurement that is wrong for it.
- **Verdict: build neither half now.** If the operator wants idle handling anyway,
  the only honest shape is: alert first with a visible countdown, act only on no
  response, and let the action be **minimize or lock — never disconnect, and never
  reconnect**.

---

## 4. New candidates

### 4.1 Restart rdpclip.exe from the host widget — **the strongest candidate in this document**

One button on the host taskbar widget that kills and relaunches the session's
`rdpclip.exe`, restoring copy-and-paste between client and session.

- *Value:* **high.** This is the most common RDP annoyance with no in-box fix. The
  current remedy is opening Task Manager or a terminal inside the session and
  doing it by hand — thirty seconds of fiddling at exactly the moment you are
  mid-workflow, every time. A one-click button on a taskbar you are already
  looking at is a genuine, repeated win, and it is the kind of session-repair
  action this toolkit is *for*.
- *Complexity given the current code:* **low, and it reuses an existing pattern end
  to end.** It is the same shape as the Disconnect button (D-8):
  - **Find it:** `WTSEnumerateProcessesW` (wtsapi32 is **already linked** by the
    host mod) gives process names with session IDs directly; match `rdpclip.exe`
    against `ProcessIdToSessionId(GetCurrentProcessId())`. Session matching is
    mandatory — a multi-session host runs one rdpclip per session, and touching
    another user's is both wrong and, unelevated, not permitted anyway.
  - **Verify it:** resolve the candidate's image path with
    `QueryFullProcessImageNameW` and require it to be
    `%SystemRoot%\System32\rdpclip.exe` before terminating anything. A name match
    alone could hit a planted process. This is the same discipline D-11 applied to
    the relay sender, and it costs a dozen lines.
  - **Restart it:** `TerminateProcess`, then `CreateProcessW` on the System32 path
    — it inherits our session and token. Re-check before launching, since some
    builds restart it themselves.
  - **Gate it:** `WTSClientProtocolType == kProtocolRdp`, re-checked at click time,
    exactly as Disconnect and Minimize already do. On a console session there is no
    rdpclip and the button is meaningless.
  - **Thread it:** the detached one-shot with the in-flight counter drained in
    `Wh_ModUninit`, copied from `SendMinimizeToClientAsync` (D-12).

  No new channel, no new IPC, no new thread model, no client-side change at all,
  and **no change to `@compilerOptions`** (everything needed is wtsapi32 — already
  linked — plus kernel32).
- *Risk:* low, with three things to name honestly rather than discover later:
  1. **Terminating rdpclip discards whatever the host clipboard currently holds.**
     Acceptable, because the button is only used when the clipboard is *already*
     broken — but the tooltip must say so plainly, in the plain-language style D-33
     established for the panel's buttons.
  2. **This is the toolkit's first action that terminates a process other than
     mstsc itself.** Precedent exists — the force-reconnect path calls
     `TerminateProcess` (D-18) — but that was self-termination. Killing a named
     system component is a new class of action, which is exactly why the
     session-ID match, the RDP gate and the System32 path check are all
     non-negotiable rather than nice-to-have.
  3. *Scope:* the toolkit exists to control and observe an RDP session from the
     taskbar, and the host widget already carries a session-level action
     (Disconnect). A clipboard repair is a session-level action of the same kind.
     This is not scope creep.

### 4.2 "Clipboard redirection is not running" line on the host widget — small add-on to 4.1

Worth doing *with* 4.1, and worth being strict about what it can honestly claim.

- **Cheap and honest:** "is `rdpclip.exe` running in this session at all?" — one
  process check, definitive when the answer is no, and it can ride the host mod's
  **existing 30 s WTS safety-net timer** with no new timer (D-22's discipline).
- **Not honestly detectable:** "is rdpclip running but *wedged*?" There is no
  side-effect-free test. Writing to the clipboard to probe it is destructive, and
  `GetClipboardSequenceNumber` proves nothing about the remote half.
- **So ship only the first, and never imply the second** — the same
  never-invent-a-value rule D-16 set for connection quality and D-33 enforced on
  the reader side. The line should say "clipboard helper not running" when that is
  true and say nothing otherwise; it must not say "clipboard OK".

### 4.3 No action required from findings 1.1, 1.2 and 1.4

Recorded in §1 so they are not re-litigated. The one thing worth carrying forward
as a written scope boundary is that the Windows App is out of reach for the client
side, permanently and by construction.

---

## 5. Comparison

| Candidate | Value | Complexity *given this codebase* | Risk | Verdict |
|---|---|---|---|---|
| **Restart rdpclip (host)** §4.1 | High — common failure, no in-box fix | **Low** — Disconnect-button shape, D-12 one-shot thread, wtsapi32 already linked | Low, with named guards (session ID, System32 path, RDP gate) | **Build — #1** |
| **Prevent host sleep** §3.4 | High vs. cost — prevents an unrecoverable drop | **Low** — arms/disarms inside the existing `QueryWtsState` | Low but sharp: a leaked power request never releases | **Build — #2** |
| **Host connection history** §3.3 | Modest — answers a question nothing answers today | **Low** — event source and change detection already exist; needs a storage-path helper | Low; bound the file size, note it records *who* | **Build — #3** |
| rdpclip presence line §4.2 | Small | Very low — rides the 30 s timer | Very low, if it never overclaims | Build with #1 |
| Idle *awareness* §3.6 | Thin — the alert lands where nobody is | Low — rides the watchdog tick and alert window | Low | Hold |
| "Ping the client" §3.5 | Thin for one operator | Low — reuses the proven pipe | Low | Hold |
| Keyboard capture toggle §3.2 | Moderate | **Higher than it looks** — the mod retains no `IMsRdpClient*` at all and would have to grow one, with STA lifetime rules (D-29's hook itself is live-confirmed) | Moderate — keystrokes to the wrong machine | **Defer** — on the retained-pointer work, *not* on D-29 |
| Notification badge §3.5 | Speculative until a trigger is named | **High** — the source is unbuilt with no documented mechanism | Moderate — scope creep, loads the least-tested leg | **Not now** |
| Idle *auto-action* §3.6 | — | Low to code | **High** — reverses D-18 / D-28 posture; D-15's signal is wrong for the action | **Recommend against** |
| Clipboard preservation §3.1(a) | Low–moderate | **High** — clipboard formats/ownership; new infrastructure | **Highest here** — silent corruption | **Recommend against; re-scope to §4.1** |

---

## 6. Recommended top three

### #1 — Restart rdpclip.exe from the host widget (§4.1), with the presence line (§4.2)

Best value-to-effort ratio in the set by a clear margin. It solves a failure the
operator will actually hit repeatedly, it is a copy of the Disconnect button's
already-working shape plus the D-12 one-shot thread, it introduces no new channel,
no new IPC and no client-side change, and it does not touch a single piece of the
unverified client-side machinery. The one genuinely new thing — this toolkit
terminating a process it does not own — is bounded by three cheap, specific guards
that follow existing precedent (D-11's path-identity discipline, D-8's click-time
protocol re-check).

### #2 — Prevent host sleep while an RDP session is connected (§3.4)

Reuses the host mod's existing WTS state machine wholesale — the arm/disarm
conditions are already computed, in one function, for another purpose. One
documented API, no new thread, window, timer or channel. It prevents a failure
that is expensive and hard to recover from, and it replaces the usual blunt
workaround (never sleep, ever) with something conditional. Ship it off by default
per D-28's posture, use the named `PowerSetRequest` form so a stuck request is
visible in `powercfg /requests`, and be honest in the setting text that it is a
guarantee rather than a fix for a universal bug.

### #3 — Host connection history log (§3.3)

The cheapest of the three. `QueryWtsState` already fires on every session change,
already holds the client name and state, and already computes whether anything
changed — the feature is "append that to a file with a timestamp". It answers the
one question the toolkit currently cannot answer at all. Write the file; do not
build a viewer in the taskbar (§2.2 — there is no flyout available, and D-25
deleted the one that existed).

### Sequencing

**Do #1 first, on its own, and live-test it before starting #2.** Its test forces a
full host-side pass — widget injection, the `TaskListButton::UpdateVisualStates`
symbol resolving on the host's build, `WTSRegisterSessionNotification` inside
`explorer.exe`, the RDP-vs-console gating — which is most of **LOOP-005**, open and
never run. Landing one feature and closing LOOP-005 as a side effect is worth more
than landing three features onto a widget nobody has watched work.

All three are host-mod changes and none touches the client mod, so none of them can
regress LOOP-007 or LOOP-008.

---

## 7. Cheaper than any of the above: two things already on the books

Neither is a new feature, so neither competes in the ranking — but a careful reading
of the code says both are better value than most of this document.

- **The D-22 residual — CLOSED 2026-09-01 (D-34); built, not yet live-tested.**
  With both the floating overlay and `stuckDetection` off, nothing wrote the
  status snapshot and the panel silently showed "no session" — a considerably
  more serious failure mode once D-33 made the panel the toolkit's *only*
  full-featured client surface. The call this section urged was made: client mod
  v0.9.2 gives the always-alive `CitadelRdpTaskbarLocalWidget` window its own
  unconditional 1 s `WriteLocalWidgetStatus` tick, reading neither settings flag
  (D-34). Compile-checked, never run — confirming it with both settings off is
  now a LOOP-008 item, and `ROADMAP.md` no longer lists this as an open design
  question.
- **The verification debt — materially smaller than when this was written
  (2026-09-01).** Of the three pieces of built work listed here as waiting on a
  live pass, two have since been run and passed: **D-29's factory-hook fix** (the
  sink advises for real — though no `OnNetworkStatusChanged` event has ever
  arrived, which is D-16's separate and still-open assumption, not D-29's) and
  **D-31's foreground fix** (the panel's fullscreen toggle works, repeatedly, on
  both success paths). **D-33's tooltip migration** is confirmed rendering but
  has not been checked line by line. The host mod's main surfaces are confirmed
  too, leaving LOOP-005 owing only the console-session gating pass and the
  `WTSRegisterSessionNotification` question. What remains is concentrated in
  **LOOP-008** — the panel's status half, its other four command rows, D-34's
  new writer, and the presentational detail — plus LOOP-007's reconnect and
  settings-gating items and the unbuilt-here overlay pass (LOOP-001).

---

## 8. Explicitly not recommended, and why

- **Clipboard content preservation across reconnect** (§3.1a) — high complexity, no
  architectural leverage, and the failure mode is silent data corruption.
  Superseded by §4.1.
- **Idle auto-action** (§3.6) — would be the first no-click action in a toolkit
  whose safety posture is explicit and repeatedly reaffirmed (D-18, D-28), and it
  would be driven by a signal D-15 deliberately defined as meaning something else.
- **Host notification badge as scoped** (§3.5) — the transport is nearly free and
  the source has no documented mechanism; bring it back only with one concrete
  named trigger.
- **Any client → host feature**, until someone budgets for it properly (§2.2) — it
  is a new channel-lifetime design, a new host-side reader, a new validation model,
  and a fresh two-machine test. It is not an extension of the existing pipe.
