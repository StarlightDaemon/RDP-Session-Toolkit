# Roadmap

## Current planning proposal

The operator has authorized the top-three additions from
[BEST_NEXT_ADDITIONS.md](BEST_NEXT_ADDITIONS.md). Diagnostics, action feedback,
ambiguity refusal and the prerequisite runtime fixes are implemented as a
candidate under the [refinement plan](REFINEMENT_PLAN.md). Follow
[TOP_THREE_ACCEPTANCE.md](TOP_THREE_ACCEPTANCE.md) for runtime acceptance.
Deployment validation and the installer remain later work. Broader feature
proposals below retain their prior planning status.

The [repository review](REPOSITORY_REVIEW.md) contains the proposed mission,
source-based expansion constraints, ranked candidates, overhead limits, and
acceptance gates. Its section 8 is the proposed next-work sequence. These are
recommendations for discussion, not newly agreed implementation commitments.

The sections below preserve earlier delivery history and design questions.
Historical testing language is superseded where applicable by D-41 and the
authoritative entries in [OPEN_LOOPS.md](.raiden/state/OPEN_LOOPS.md).

Backlog of agreed-but-not-yet-built features and scoped-but-open ideas. Unlike
`DECISIONS.md` (committed architectural decisions with rationale) and
`OPEN_LOOPS.md` (verification/test work on code that already exists), this
file tracks things nobody has started building yet.

## Built 2026-09-01, second session (client v0.10.0, host v0.4.0, relay v0.2.0) — live-verified 2026-09-06

One large session; every item compiles/links clean and was subsequently
live-verified by the operator over several days (LOOP-009 resolved):

- **Session duration removed outright; idle time stands alone** (D-36).
  `LocalWidgetStatus` wire version 1 → 2; *Show idle time* (the rescoped
  `showSessionInfo`) now controls exactly the idle display.
- **Host Minimize fixed** (D-37): the open-write-close send raced asynchronous
  DVC delivery (the live-proven probe drained 500 ms before closing; that
  drain was dropped in the D-12 adaptation). The send is now
  open-write-**read**-close; the read is the delivery confirmation.
- **Client → host state channel** (D-38): the relay plugin answers every
  command with a `RelayStatusReply` (magic 'RSRP', v1) built from the client
  mod's status snapshot, located via the registry-published storage path —
  §2.2's missing piece, built on the existing channel. The host polls a
  no-side-effect status query every 3 s and caches the client's fullscreen
  state with a 10 s staleness window.
- **Host fullscreen/windowed toggle button** (D-38): sends the new relay
  command 0x02 to the client's shared `ToggleFullscreen`; relabels itself
  from the cached client state or says the state is unknown.
- **Per-monitor taskbar instances in both mods** (D-39): fixes the
  secondary-monitor margin bug (shared globals let one monitor's tray width
  drive every monitor's margin).
- **Shared visual spec + theme-inverted coloring in both widgets** (D-40):
  one panel look; light taskbar → dark panel and vice versa; live reaction to
  theme flips via WM_SETTINGCHANGE.
- Host Minimize and Disconnect gained plain-language tooltips (Task 4),
  joining Restart Clipboard's.

## Removed 2026-09-01 (client mod v0.9.0) — the thumbnail toolbar is gone

The **taskbar thumbnail toolbar** — the mstsc.exe-side buttons under the
taskbar hover preview, the third of the toolkit's three client presentation
surfaces — was removed in full
([D-33](.raiden/state/DECISIONS.md#d-33-the-taskbar-thumbnail-toolbar-is-removed-its-rich-status-tooltip-moves-to-the-taskbar-embedded-panel)).
Two surfaces remain: the **taskbar-embedded panel** (`explorer.exe`, the main
one, always visible) and the optional **floating overlay button**
(`mstsc.exe`, off by default).

The panel already offered the same five actions with the same
enable/disable/relabel rules, always visible rather than hover-only, and
covering windowed sessions equally — so the toolbar was a second copy of one UI
to keep in sync, on top of an Apartment-model COM pointer, a one-shot
`ThumbBarAddButtons` API, and hand-rendered glyph icons. Nothing that any other
surface uses was touched: every shared action function (minimize, restore,
fullscreen toggle, the reconnect helpers, disconnect) is unchanged, and the
connection-quality sink's teardown — which used to ride along with the thumb
bar's teardown on both the frame-destroy and the mod-unload path — keeps firing
on both under its own message.

The toolbar's rich status tooltip (session duration, local idle time,
connection quality with bandwidth and round-trip time, not-responding warning)
was **not** dropped: it was rebuilt on the panel's own status text from the
fields the status snapshot already carries. The panel's five buttons also
gained plain-language tooltips. `showThumbbar` is gone; `showConnectionQuality`,
`showSessionInfo` and `showFullscreenToggle` now apply to the panel.

Compiles, links and validates clean. **Partly live-tested since (2026-09-01):**
the removal itself is confirmed on real hardware — no thumbnail-toolbar log
lines appear post-removal — and the panel is confirmed showing the migrated
status tooltip. The rest is still owed: the panel's status half, the other
command rows, and the five new button tooltips. LOOP-008 remains the checklist.

## Built 2026-09-01 (host mod v0.3.0), live-verified 2026-09-06

- **Restart rdpclip.exe from the host widget, with the clipboard presence
  line** — built
  ([D-35](.raiden/state/DECISIONS.md#d-35-restart-rdpclipexe-from-the-host-widget-plus-a-report-the-negative-only-clipboard-presence-line)).
  This was `EXPANSION_RESEARCH.md`'s **#1 recommendation** (§4.1 for the action,
  §4.2 for the line, §6 for the ranking and the sequencing); it moves from
  *recommended* to *built* here, and it is built as that document specified
  rather than redesigned.

  A **Restart Clipboard** button on the host widget finds the `rdpclip.exe`
  processes in *this* session (`ProcessIdToSessionId` of our own pid — not the
  active console session), verifies each one's full image path really is
  `<GetSystemWindowsDirectoryW>\System32\rdpclip.exe` before touching it,
  terminates every verified match, waits on each handle for it to actually
  exit plus a short settle for the window manager to release its clipboard
  bindings, re-checks, and launches exactly one replacement. Zero matches is a
  fresh start, not an error. RDP-gated and re-checked at click time like
  Disconnect and Minimize; run on the D-12 detached one-shot thread with its
  in-flight counter drained at unload. The tooltip states plainly that the
  shared clipboard's contents are discarded.

  A **"Clipboard helper not running"** line, from the same shared find-and-verify
  function, rides the existing 30 s WTS safety-net timer — no new timer. It says
  nothing at all when a helper *is* running: a wedged-but-present rdpclip cannot
  be distinguished, so the widget never claims the clipboard is fine.

  No new channel, no new IPC, no client-side change, no `@compilerOptions`
  change. Compiles, links and validates clean. The operator subsequently
  live-verified the action and presence line; LOOP-005 is resolved.

  *Still recommended, not started:* #2 prevent host sleep during a session
  (§3.4) and #3 host connection history log (§3.3). §6 says to live-test #1
  before starting #2.

## Agreed, not yet built

Nothing in this section at the moment — the five items agreed in the earlier
brainstorm session (2026-08-21 or earlier) were all built in the first
2026-08-21 overnight session, and the taskbar-embedded client mode in the
second; see the next two sections.

## Built 2026-08-21 (client mod v0.4.0), live-verified 2026-09-06

All five moved from "agreed, not built" to built-and-compiling in one session;
none has been loaded under Windhawk yet — **LOOP-007** in `OPEN_LOOPS.md` is
the live-test checklist, ordered by how much each one still has riding on a
real session.

- **Fullscreen/windowed toggle button** — built
  ([D-14](.raiden/state/DECISIONS.md#d-14-fullscreenwindowed-toggle-is-synthesized-ctrlaltbreak-not-an-internal-call)):
  a thumbnail-toolbar button that sends Remote Desktop's own Ctrl+Alt+Break to
  the foregrounded session window. No transition logic of its own; refuses to
  send if the foreground cannot be acquired. *(v0.9.0: the button now lives on
  the taskbar-embedded panel — the toolbar is gone, D-33. The mechanism is
  unchanged.)* *(2026-09-01: **live-confirmed** — the panel's toggle acquires
  the foreground and delivers the chord, repeatedly, on both of D-31's success
  paths. LOOP-007 (b) is closed.)*
- **Session duration / idle-time display** — built
  ([D-15](.raiden/state/DECISIONS.md#d-15-session-duration--idle-display-lives-on-both-surfaces-idle-is-local-input-idle)):
  a new row on the (fullscreen-only) overlay plus the tooltip of a new
  thumbnail-toolbar status icon (windowed coverage, minute granularity). Idle
  is the client machine's own input idle by design. *(v0.9.0: the tooltip half
  moved to the taskbar-embedded panel's status text, rebuilt there from the
  status snapshot — D-33. The overlay row is unchanged.)*
- **Connection quality indicator** — built, the most provisional of the five
  ([D-16](.raiden/state/DECISIONS.md#d-16-connection-quality-comes-from-a-second-imstscaxevents-sink-advised-via-a-cocreateinstance-hook--advise-live-confirmed-via-d-29-the-event-itself-has-never-arrived)):
  a real, documented hook point was found (second `IMsTscAxEvents` sink
  advised on the RDP control captured via a `CoCreateInstance` hook; DISPIDs
  resolved at runtime), so it was built rather than left unbuilt — but whether
  the event actually reaches a second sink inside mstsc.exe is unverified.
  The icon stays neutral and says so until a real report arrives; nothing is
  faked. *(2026-09-01: the **advise half is live-confirmed** — D-29's
  creation-path hook fires and the sink is subscribed with a real cookie, across
  multiple sessions. The **event half is still unverified**: no
  `OnNetworkStatusChanged` has ever arrived with data, so the indicator has
  stayed neutral on every real session — exactly as designed. The report now
  shows on the taskbar-embedded panel, not a thumb-bar icon — D-33.)*
- **Quick-reconnect shortcut with remembered display settings** — built
  ([D-17](.raiden/state/DECISIONS.md#d-17-one-shared-reconnect-helper-capture-a-relaunch-command-line-launch-from-the-frames-wm_destroy)):
  settings-backed preferred mode (fullscreen / windowed at a fixed or the
  current size / all monitors); shared capture-then-relaunch helper; clean
  close via the established WM_CLOSE, relaunch from the frame's WM_DESTROY.
- **Stuck-session detection with force reconnect** — built
  ([D-18](.raiden/state/DECISIONS.md#d-18-stuck-session-detection-runs-on-its-own-thread-alerts-in-its-own-window-and-never-reconnects-unasked)):
  `IsHungAppWindow` watchdog on its own thread, on-screen alert with a manual
  Force reconnect (bounded grace, then `TerminateProcess` + relaunch). Never
  reconnects on its own.

## Consolidated 2026-08-21 (client mod v0.6.0) — one mod, two process branches

The client-side pair became a single mod
([D-26](.raiden/state/DECISIONS.md#d-26-the-two-client-side-mods-are-consolidated-into-one-mstscexe--explorerexe-branches-retiring-the-standalone-embedded-mod)):
the standalone `rdp-session-toolkit-taskbar-client-embedded`
(`explorer.exe`, last v0.2.0) is folded into
`rdp-session-toolkit-taskbar-client` as its `explorer.exe` branch, and
`taskbar-integration/client-embedded/` is **deleted**. One mod file now targets
both `mstsc.exe` and `explorer.exe`; `Wh_ModInit` detects which process it is in
and runs only that branch. The four hand-copied contracts (status struct,
command channel, secret location, command bytes) are now single shared
definitions. The status-file *path-guessing* the standalone mod needed
(sibling-directory search, `local@` prefix) is gone: same mod id → same
`Wh_GetModStoragePath` in both processes. This is a **strong expectation** that
it fixes LOOP-008's "no active session shown" risk, **not** a confirmed result —
it still needs a fresh live pass under the merged architecture. All the
formerly-open items below were resolved as recorded; only the *packaging*
changed (two mods → one), not the runtime design.

- **Taskbar-embedded client presentation mode** — originally built as the
  third, separate Windhawk mod `rdp-session-toolkit-taskbar-client-embedded`
  (`taskbar-integration/client-embedded/`, `explorer.exe` on the client
  machine; [D-21](.raiden/state/DECISIONS.md#d-21-third-mod--the-taskbar-embedded-client-widget-is-its-own-explorerexe-mod-templated-on-the-host-mod));
  **as of v0.6.0 it is the `explorer.exe` branch of the client mod, D-26**):
  one compact status icon injected into the client machine's own taskbar
  with the host mod's injection code, opening a Win32 GDI flyout with the
  status lines and all five actions — full parity with the thumbnail toolbar.
  It covers fullscreen *and* windowed sessions (the floating overlay is
  fullscreen-only; the thumbnail toolbar already covered both). Not loaded
  under Windhawk yet — **LOOP-008** in `OPEN_LOOPS.md` is the live-test
  checklist. How the formerly open items were resolved:
  - **Cross-process control path** — two one-way channels into / out of the
    client mod ([D-22](.raiden/state/DECISIONS.md#d-22-status-channel--the-client-mod-writes-a-fixed-layout-snapshot-to-its-own-storage-path-the-widget-derives-that-path-and-treats-the-record-as-potentially-stale),
    [D-23](.raiden/state/DECISIONS.md#d-23-command-channel--a-second-local-message-window-with-a-shared-secret-under-hkcusoftwarerdpsessiontoolkit-citadelrdptaskbarrelay-untouched)):
    a once-a-second status file in the client mod's storage directory (read
    as potentially stale), and a second message-only window
    `CitadelRdpTaskbarLocalWidget` for commands.
  - **Same-machine validation mechanism** — a 32-byte `BCryptGenRandom`
    shared secret at `HKCU\Software\RDPSessionToolkit\LocalWidgetSecret`,
    generated by whichever mod starts first, carried in every command,
    re-checked per command, fail-closed (D-23).
  - **Command scoping** — the channel accepts exactly the five thumbnail
    actions (minimize, restore, fullscreen toggle, reconnect, disconnect) and
    nothing else; the existing per-feature thumb-bar settings are not applied
    to it (D-23).
  - **Toggle mechanism** — per-mod enable in Windhawk, as already confirmed;
    to retire the overlay turn the client mod's `showButton` off **and keep
    `stuckDetection` on** — with `showButton` off the status file is written
    from the watchdog tick (D-22). *(As of v0.9.0 the thumbnail toolbar is no
    longer one of the surfaces to retire — it was removed outright, D-33.)*
  - **CitadelRdpTaskbarRelay untouched** (D-24).

## Scoped, open design questions

- **UIPI / elevation parity for the local widget channel** — deliberately
  left fail-closed: the new `CitadelRdpTaskbarLocalWidget` window has no
  `ChangeWindowMessageFilterEx` allowance, so an elevated `mstsc.exe` cannot
  be driven from the medium-integrity widget (D-23 gives the reasoning: a
  low-integrity process can read HKCU, so opening the filter would hand it
  the channel's only credential). If elevated sessions matter, the options
  are an ACL'd secret location plus `MSGFLT_ALLOW`, or a different IPC
  primitive with its own access control. Not started.
- ~~**Status writer when both the floating overlay and `stuckDetection` are
  off**~~ — **closed 2026-09-01 (D-34); no longer an open design question.**
  The panel used to show "no session" in that combination because the only two
  1 s ticks driving the writer belonged to the overlay's status timer and the
  watchdog poll (removing the thumbnail toolbar in D-33 did not change that —
  the toolbar never drove the writer). The operator's call on the no-new-timer
  constraint was made in favour of a third tick: the always-alive
  `CitadelRdpTaskbarLocalWidget` window now carries an unconditional 1 s
  `SetTimer` calling `WriteLocalWidgetStatus`, reading neither settings flag
  (D-34, client mod v0.9.2). Built, compile-checked, and subsequently
  live-verified by the operator; LOOP-008 is resolved.
- **Host mod + embedded client widget on the same machine** — both inject at
  the same default taskbar position and would overlap (D-21); only a
  settings workaround exists today.
