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
  `client/rdp-session-toolkit-taskbar-client.wh.cpp`, now **v0.7.0** and
  targeting both `mstsc.exe` and `explorer.exe` (D-26; the embedded panel bullet
  below). v0.7.0 (2026-08-22 overnight session): the panel's fullscreen toggle
  hands mstsc the foreground right before the command (D-27); Reconnect and
  Force reconnect are one opt-in setting, off by default, and a relaunch
  re-checks its `.rdp` file on disk at launch time (D-28); the mod storage path
  is resolved once instead of every second; and the connection-quality sink
  now also hooks the control's real creation path — mstscax's own class factory,
  identified from the binary — with step-by-step diagnostics (D-29; fix built,
  not yet live-confirmed). Originally v0.4.0: a fork of
  Hide RDP Connection Bar v1.1.9 (commit `dc82b10d…`, see D-6) carrying the
  taskbar thumbnail toolbar — now six slots: Status icon, Minimize, Restore,
  Fullscreen/windowed toggle, Reconnect, Disconnect (D-19) — plus the
  `CitadelRdpTaskbarRelay` message-window relay receiver with real sender
  validation accepting only this mod or the registered relay plugin EXE
  (WM_COPYDATA, minimize command; see D-5/D-11). v0.4.0 (2026-08-21 overnight
  session) added the five roadmap features: Ctrl+Alt+Break fullscreen toggle
  (D-14), session clock / local idle on the overlay and in the status tooltip
  (D-15), a connection-quality indicator driven by a second `IMsTscAxEvents`
  sink on the RDP control (D-16 — built, least verified), a shared
  reconnect helper with settings-backed display mode (D-17), and an
  `IsHungAppWindow` watchdog with a manual Force-reconnect alert (D-18). None
  of the v0.4.0 features has been live-tested (LOOP-007). **Host-side mod built**
  as `host/rdp-session-toolkit-taskbar-host.wh.cpp` v0.2.0 (own version line):
  native taskbar-XAML-injected widget in `explorer.exe` on the session host
  showing WTSClientName + WTSConnectState with a console-gated
  WTSDisconnectSession button (see D-7/D-8), plus a Minimize button that sends
  one byte over the `dvc::taskbar::relay` DVC on a detached background thread
  (see D-12; replaces the former D-9 TODO). Both compile-checked with the
  Windhawk clang (`compile-check.ps1`). The full host→client minimize pipe has
  been **observed working live by the operator** (LOOP-006 resolved
  2026-08-21), so both mods have run under Windhawk at least once; their
  remaining surfaces are still unchecked (LOOP-001/005/007).
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
  settings block validates; **not yet loaded under Windhawk in the merged form**
  — LOOP-008 re-scoped, the "no active session shown" fix is a strong
  expectation pending live confirmation. The status writer (D-22), command
  channel (D-23), and `CitadelRdpTaskbarRelay` (D-24) are unchanged in behavior.

## Active Work

Three 2026-08-21 sessions' worth of work is drafted and uncommitted:
client mod v0.4.0/v0.4.1 (five roadmap features, LOOP-007); the
taskbar-embedded client widget v0.1.0→v0.2.0 + client mod v0.5.0 (LOOP-008);
and now the **v0.6.0 consolidation** — the standalone embedded mod folded into
the client mod as its `explorer.exe` branch, one mod id with two `@include`
targets, shared contracts unified, status-file path guessing eliminated, the
`client-embedded/` folder deleted (D-26). Merged mod compile-checks clean and
its settings block validates. DECISIONS D-14–D-26, ROADMAP, OPEN_LOOPS, and the
READMEs updated. On top of that, the **2026-08-22 overnight session (v0.7.0,
D-27–D-29)** fixed the two failures the first live tests surfaced (fullscreen
toggle foreground; the never-advised quality sink — cause found in the mstsc
binary, fix built, pending confirmation), gated all reconnects behind an
off-by-default setting with a launch-time `.rdp` existence check, and removed
the per-second storage-path polling; compile-checked clean after each task.
Next natural steps: operator review of the judgment calls in LOOP-007 (now
listing the exact log lines that prove or disprove D-29 and D-27) and
(re-scoped) LOOP-008, then the live passes, then commit.

## Notes

- No GitHub remote configured; this repo is intentionally local-only for now.
