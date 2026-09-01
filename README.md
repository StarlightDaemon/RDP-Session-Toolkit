# RDP Session Toolkit

**Status: early.** Two components are built and ready to try — the client-side
taskbar widget and the host-side taskbar widget, described below. Each works
entirely on its own machine; everything involving cross-machine signaling is
still in progress and not yet usable; see
[What's not yet functional](#whats-not-yet-functional).

## What this is

This is the companion repository to
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar),
a small Windhawk mod that hides the RDP connection bar overlay. That repo is a simple,
low-risk UI mod: it patches window behavior inside `mstsc.exe` and nothing else.

This repository is a toolkit of components built around the same idea, aimed at a more
ambitious, personal-use-primarily goal: a live taskbar widget on the client machine
that eventually reflects state from inside the remote RDP session, wired together over
an RDP Dynamic Virtual Channel (DVC). Right now, the toolkit's first component — a
client-side taskbar widget — works entirely on its own, with no remote-session wiring
yet.

This toolkit's client-side taskbar component specifically began as a fork of
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar).
That original mod still exists and continues to be independently maintained at its own
repository, separate from this fork's ongoing changes.

## Why a separate repository

The two repos are split because they carry different trust levels, not just different
feature sets. `Hide RDP Connection Bar` is a single UI mod with a narrow, easily-audited
scope. This repository is where a **native standalone executable that registers as a COM
server and can act on a live RDP session** will eventually live — a much larger attack
surface and a much higher bar for review before anyone other than the author should run
it. Keeping it in its own repo keeps that distinction visible instead of quietly
expanding the trust footprint of the original mod.

## What works today: the client-side taskbar widget

`taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp` is a Windhawk mod that
runs inside `mstsc.exe` (the Windows Remote Desktop client) on your local machine. It's
a fork of Hide RDP Connection Bar, extended with session controls and a status panel that
the mod embeds in your own taskbar (from `explorer.exe`). Everything it does is entirely
local to the client machine — it does not talk to the remote session host in any way yet.

It's implemented and compiles cleanly against the Windhawk toolchain. It has run under
Windhawk on a real session at least once (the relay-driven minimize was observed
working), but the taskbar-embedded panel, the overlay, and all of the session controls
below have not had their own live pass yet — treat them as ready to try, not as already
confirmed working out of the box.

### Installation

1. Install [Windhawk](https://windhawk.net/) if you don't already have it.
2. This mod isn't published to the Windhawk Marketplace yet, so you won't find it by
   searching in the app. Instead, load it the same way this fleet's other unpublished
   Windhawk mods are tested: open Windhawk, choose **New mod**, and paste in the entire
   contents of
   [`taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp`](taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp).
3. Save and enable the mod. Open (or reopen, if one is already running) a Remote Desktop
   connection to see it.

### Usage

Once enabled, a status panel appears in your own taskbar with all five actions as
always-visible buttons, each with a tooltip explaining what the click does. An optional
floating on-screen button (off by default; corner and offset are configurable in the
Windhawk mod settings) offers Minimize, Restore, and Disconnect on the RDP monitor
itself. The controls are:

- **Connection status and name display** — the button shows the remote host's name, so
  you can tell at a glance which session it belongs to. It's a passive display, not a
  click target.
- **Minimize** — sends the fullscreen RDP session down to the taskbar without
  disconnecting it.
- **Restore** — brings a minimized session back. Works from the taskbar panel or from
  the same on-screen button, which stays visible while minimized.
- **Disconnect** — cleanly closes the RDP session, the same as closing the window
  normally.
- **Session time and idle time** — a row on the on-screen button (fullscreen sessions)
  and the tooltip of the taskbar panel's status text (all sessions) show how long the
  session has been open and how long *this computer's* keyboard and mouse have been
  idle. That is deliberately local idle time, not remote-session activity.
- **Connection quality** — the same tooltip carries the quality level Remote Desktop
  itself reports (4 levels), with bandwidth and round-trip time. It says it is waiting
  until the first report arrives, and says so plainly if none can come.
- **Fullscreen / windowed** — one panel button switches the live session between
  fullscreen and windowed without disconnecting, by sending Remote Desktop's own
  Ctrl+Alt+Break shortcut to the session window.
- **Reconnect** — cleanly disconnects and reopens the same connection in your preferred
  display mode (fullscreen, windowed at a chosen or the current size, or all monitors),
  configurable in the mod settings.
- **Stuck-session alert** — if the session window stops responding for a configurable
  number of seconds, a small alert appears offering **Force reconnect** (ends the stuck
  client after a short grace period and reopens the connection). The mod never
  reconnects on its own; every reconnect starts with your click.

The mod also has other configurable options (hiding the native connection bar, an idle
fade effect, a disconnect hotkey, per-feature toggles for everything above) — see the
mod's own settings in Windhawk for the full list.

### Alternative presentation: a status panel in your own taskbar

As of **v0.6.0** the client mod also targets `explorer.exe` on the *client* machine
(a single Windhawk mod can target more than one process). When loaded there, its
**explorer.exe branch** puts a wide, always-visible RDP status panel directly into the
Windows 11 taskbar — the same taskbar-XAML injection technique the host-side widget
below uses. The panel shows the remote host name, the session duration and connection
quality (or an honest "quality n/a" / "not responding" line), and five directly
clickable buttons: Minimize / Restore / Fullscreen-toggle / Reconnect / Disconnect —
for fullscreen and windowed sessions alike. It does nothing to the session on its own:
it reads a status file the mstsc.exe branch writes once a second and sends its actions
back over a local, secret-guarded message channel. Because both surfaces are now one
mod id, the panel finds that status file directly (no sibling-directory guessing). To
use only the taskbar panel, turn the mstsc side's *Show disconnect button* off and
leave *Detect stuck sessions* on. It compiles cleanly but has not had a live pass under
the merged architecture yet.

(This was previously a third, separate mod, `rdp-session-toolkit-taskbar-client-embedded`;
it was folded into the client mod at v0.6.0 — see
[`.raiden/state/DECISIONS.md`](.raiden/state/DECISIONS.md) D-26.)

## What works today: the host-side taskbar widget

`taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp` is a second Windhawk
mod that runs inside `explorer.exe` on the machine you connect *to* — the RDP session
host. It injects a small widget directly into the Windows 11 taskbar of the remote
session (using the native taskbar-XAML injection technique proven by the sibling
[Native Taskbar Media Controller](https://github.com/StarlightDaemon/Native-Taskbar-Media-Controller)
mod), showing:

- **Connecting client name and connection state** — which machine is currently
  attached to the session, and the session's live state (Active, Disconnected, …).
- **Disconnect** — detaches the RDP client cleanly, same as the Start menu's
  Disconnect; the session keeps running. The button disables itself when the session
  is at the physical console, so it can't kick a locally-signed-in user.

Like the client-side mod, everything it does is entirely local to its own machine —
it detects and acts on the session it runs in, and does not talk to the client
machine in any way yet. Install it the same way as the client mod (Windhawk →
**New mod** → paste the file contents), but on the session host.

It compiles cleanly against the Windhawk toolchain but hasn't had its first live
pass loaded under Windhawk yet — treat it as ready to try, not as already confirmed
working out of the box.

## What's not yet functional

- **True cross-machine control** — for example, triggering a minimize on the client from
  something happening on the session host. Nothing today sends a real signal across the
  RDP connection.
- The client-side mod does already contain a hidden message-window "relay receiver"
  stub, wired up in advance for a future signal to arrive at. Don't read anything into
  its presence: nothing currently sends it a real signal, so it doesn't do anything yet.
  It only responds to a manual, local debug toggle that fires a fake test signal to
  itself — that's a developer aid, not cross-machine control.
- A separate, throwaway proof-of-mechanism experiment (`dvc-plugin/probe/`) exists to
  test whether the underlying DVC signaling mechanism works at all. It hasn't been run
  through its required two-machine live test yet, and it isn't the real relay — it's a
  gating experiment for a decision that hasn't been made.

For the full target architecture, the reasoning behind these decisions, and current
open questions, see [`HANDOFF.md`](HANDOFF.md) and
[`.raiden/state/DECISIONS.md`](.raiden/state/DECISIONS.md) rather than duplicating that
detail here.

## Structure

- [`taskbar-integration/`](taskbar-integration/README.md) — the two Windhawk mods:
  the client-side mod (`client/`, targeting both `mstsc.exe` and — since v0.6.0 —
  `explorer.exe` for the taskbar-embedded panel) and the host-side mod (`host/`),
  both built, see above
- [`dvc-plugin/`](dvc-plugin/README.md) — the planned standalone DVC relay executable
  (a throwaway activation probe exists; the real relay is not started)

## License

MIT — see [LICENSE](LICENSE).
