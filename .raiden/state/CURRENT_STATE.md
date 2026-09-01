# Current State

## Repo Purpose

Companion repo to [Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar),
housing the more complex, personal-use-primarily taskbar-integration work that does not
belong in the simple-UI-mod trust boundary of that repo. See root `README.md` for the
full architecture summary.

## Status

Repository bootstrapped 2026-08-20; first components landed the same day.

- `taskbar-integration/` — reorganized into `client/` and `host/` subfolders
  (pure move, pre-commit). **Client-side mod built** as
  `client/rdp-session-toolkit-taskbar-client.wh.cpp`, now **v0.9.2** and
  targeting both `mstsc.exe` and `explorer.exe` (D-26; the embedded panel bullet
  below). The client-side pair now presents through **two independent
  surfaces**: the **taskbar-embedded panel** (`explorer.exe`, D-21/D-26) is the
  primary, always-visible, full-featured surface; the **floating overlay
  button** (`mstsc.exe`, fullscreen-only) is off by default (`showOverlay`,
  D-30). The **taskbar thumbnail toolbar**, the former third surface, was
  **removed outright in v0.9.0 (D-33)** — its rich status tooltip was rebuilt on
  the panel's own status text, and the panel's five action buttons gained
  plain-language tooltips (v0.9.1). v0.9.2 (D-34) gave the panel's own
  message-only window an unconditional 1 s status-write tick, closing the D-22
  gap where a session with both `showOverlay` and `stuckDetection` off left the
  panel — now the toolkit's only full-featured surface — showing "no session."
  v0.7.0 (2026-08-22 overnight session): the panel's fullscreen toggle
  hands mstsc the foreground right before the command (D-27); Reconnect and
  Force reconnect are one opt-in setting, off by default, and a relaunch
  re-checks its `.rdp` file on disk at launch time (D-28); the mod storage path
  is resolved once instead of every second; and the connection-quality sink
  now also hooks the control's real creation path — mstscax's own class factory,
  identified from the binary — with step-by-step diagnostics (D-29; fix built,
  and **live-confirmed 2026-09-01: the sink now advises for real. The event
  itself, `OnNetworkStatusChanged`, has still never arrived with data** — that
  is D-16's separate open assumption). Originally v0.4.0: a fork of
  Hide RDP Connection Bar v1.1.9 (commit `dc82b10d…`, see D-6) carrying the
  taskbar thumbnail toolbar — grown to six slots: Status icon, Minimize,
  Restore, Fullscreen/windowed toggle, Reconnect, Disconnect (D-19) — **since
  removed outright in v0.9.0 (D-33)**, its status tooltip migrated to the
  taskbar-embedded panel; see the surfaces summary above. Also carries the
  `CitadelRdpTaskbarRelay` message-window relay receiver with real sender
  validation accepting only this mod or the registered relay plugin EXE
  (WM_COPYDATA, minimize command; see D-5/D-11). v0.4.0 (2026-08-21 overnight
  session) added the five roadmap features: Ctrl+Alt+Break fullscreen toggle
  (D-14), session clock / local idle on the overlay and in the status tooltip
  (D-15), a connection-quality indicator driven by a second `IMsTscAxEvents`
  sink on the RDP control (D-16), a shared reconnect helper with
  settings-backed display mode (D-17), and an `IsHungAppWindow` watchdog with a
  manual Force-reconnect alert (D-18). Of the v0.4.0 features: the fullscreen
  toggle's foreground path is **live-confirmed** (D-31, LOOP-007 (b) closed);
  the quality sink's advise is **live-confirmed** but its event
  (`OnNetworkStatusChanged`) has never arrived with data, narrowing rather than
  closing LOOP-007 (a); the reconnect and watchdog paths are still untested
  (LOOP-007 (c)–(f)). **Host-side mod built**
  as `host/rdp-session-toolkit-taskbar-host.wh.cpp` v0.2.0 (own version line):
  native taskbar-XAML-injected widget in `explorer.exe` on the session host
  showing WTSClientName + WTSConnectState with a console-gated
  WTSDisconnectSession button (see D-7/D-8), plus a Minimize button that sends
  one byte over the `dvc::taskbar::relay` DVC on a detached background thread
  (see D-12; replaces the former D-9 TODO). Both compile-checked with the
  Windhawk clang (`compile-check.ps1`). The full host→client minimize pipe has
  been **observed working live by the operator**, and **reconfirmed
  2026-09-01** across multiple further sessions (LOOP-006 resolved
  2026-08-21). Beyond that pipe, live-test coverage as of 2026-09-01: the host
  mod's status display, Disconnect button, and minimize-send path are
  **confirmed**, with console-vs-RDP button gating and whether
  `WTSRegisterSessionNotification` actually registers still open (LOOP-005
  narrowed); the client mod's fullscreen-toggle foreground fix is **confirmed**
  and the quality sink's advise is **confirmed** (its event is not — see
  above), with the reconnect/watchdog paths and the floating overlay button
  still untested (LOOP-001/007); the taskbar-embedded panel itself is
  confirmed rendering with a working command channel, but its status half and
  four of its five command rows remain unexercised (LOOP-008, "the loop that
  matters most" — see below).
- `dvc-plugin/` — throwaway activation **probe** (out-of-process COM
  LocalServer + server-side trigger, `dvc-plugin/probe/`) — its gating
  two-machine live test **PASSED** (`dvc-plugin/TESTING.md`, VERIFIED
  2026-08-21; LOOP-003 resolved) and its registration was removed; kept as
  historical reference. The **production relay plugin is now built** at
  `dvc-plugin/relay/` (v0.1.0): same verified out-of-process COM LocalServer
  model, own CLSID/AddIn/channel (D-10), builds clean with `build.ps1`. It
  forwards a received DVC byte to the client mod's relay window via
  WM_COPYDATA. End-to-end minimize feature **observed working** (LOOP-006
  resolved 2026-08-21).

- **Taskbar-embedded client widget — consolidated into the client mod (D-26).**
  Originally built (second 2026-08-21 overnight session) as a third, separate
  mod `rdp-session-toolkit-taskbar-client-embedded` (`explorer.exe`, D-21),
  reaching v0.2.0 with a wide always-visible panel (D-25). It is now **folded
  into the client mod as its `explorer.exe` branch**: the client mod at
  **v0.6.0** carries two `@include` targets (`mstsc.exe` + `explorer.exe`),
  `Wh_ModInit` detects the host process and runs the matching branch, the four
  shared contracts (status struct, command channel, secret location, command
  bytes) are single definitions used by both, and the explorer branch reads the
  status file directly via `Wh_GetModStoragePath` — same mod id, same directory,
  so the former sibling-directory guessing is gone. The standalone
  `taskbar-integration/client-embedded/` file and folder are **deleted**
  (never committed; nothing lost). Merged mod compile-checks clean and its
  settings block validates. **Live-confirmed 2026-09-01** (LOOP-008,
  narrowed): D-26's merged-mod plumbing works for the command half — the panel
  renders, shows the migrated status tooltip, and its fullscreen row carries a
  command end-to-end through the shared-secret channel. **Still open and it is
  most of the loop:** the status half (nobody has cross-checked the panel's
  numbers against the mstsc-side snapshot write), the other four command rows
  (minimize, restore, reconnect, disconnect), D-34's new unconditional 1 s
  writer, and the presentational details (tooltip lines, setting gates,
  staleness dimming, button collapse). The status writer (D-22, since extended
  by D-34), command channel (D-23), and `CitadelRdpTaskbarRelay` (D-24) are
  unchanged in behavior.

## Active Work

Everything through the client mod's v0.9.0 thumbnail-toolbar removal (D-33)
and v0.9.1 action-button-tooltip pass is committed. This session lands two
more things: the **v0.9.2 code fix** — an unconditional 1 s status-write tick
on the client mod's `LocalWidget` message window (D-34), closing the D-22
residual gap that D-33's removal made serious (the panel is now the toolkit's
only full-featured client surface, so it can no longer be allowed to go
silently blank) — and a **documentation reconciliation pass**: correcting the
connection-quality claim (D-16/D-29 were written before the sink's advise was
live-confirmed) across DECISIONS/ROADMAP/CURRENT_STATE, folding the
2026-09-01 live-test evidence into every open loop's disposition in
OPEN_LOOPS (some resolved, some narrowed, some still fully open — no longer a
blanket "pending"), and adding `EXPANSION_RESEARCH.md`, a research-only
assessment (no code changes) of where the toolkit goes next given the
now-actual architecture. Next natural steps: the live passes OPEN_LOOPS still
calls for, in the order it lays out — LOOP-008 (the taskbar-embedded panel)
matters most, since D-33 left it the toolkit's sole full-featured surface and
most of that loop is still unexercised.

## Notes

- GitHub remote: `git@github.com:StarlightDaemon/RDP-Session-Toolkit.git`
  (origin). No longer local-only.
