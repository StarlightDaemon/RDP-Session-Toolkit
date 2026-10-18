# RDP Session Toolkit

**Diagnostics and session reliability candidate:** client 0.11.1, host 0.5.1
and relay 0.3.1 add local diagnostics, action feedback and a conservative
multiple-client guard, with owned workers, bounded relay logs and safer unload.
Protocol v3 requires matching components and refuses legacy actions. See
[candidate acceptance and rollback](TOP_THREE_ACCEPTANCE.md),
[runtime regression checks](probes/runtime/README.md), and the
[implementation plan](REFINEMENT_PLAN.md). This source is locally checked;
installed-runtime acceptance remains separate.
Use the [home update, test and rollback guide](HOME_UPDATE_TEST_ROLLBACK.md)
for the step-by-step procedure on both machines; it is included in candidate ZIPs.
The [implementation review](IMPLEMENTATION_REVIEW.md) describes the corrected
bugs, security boundaries and remaining home-test gates.

Run `./scripts/Get-ToolkitDiagnostics.ps1` for a read-only local explanation,
or add `-Json` for structured output. The script does not start or repair
components, read secrets/clipboard contents, or upload anything.

The registration-refinement notice below describes the preceding candidate.

**Refinement candidate available:** client 0.10.1 and relay 0.2.1 add coordinated
path handling and safer registration/removal; host 0.4.0 is unchanged. This
candidate has passed local checks but has not been live-tested. Follow the
[refinement plan](REFINEMENT_PLAN.md) and [home-test checklist](HOME_TEST.md).
The verified-runtime statements below describe the preceding component set.

**Status: working, personal-use toolkit.** The client and host taskbar widgets
and the Dynamic Virtual Channel relay have been live-tested by the operator
over several days, including the cross-machine control and status paths. The
repository remains intentionally conservative about its trust boundary and is
not yet packaged for general distribution.

For a repository explanation, proposed mission, and sequenced feature plan, see
[Repository review and proposed direction](REPOSITORY_REVIEW.md). The proposal
keeps the taskbar controls central while allowing small session diagnostics and
recovery utilities beyond the toolbar.

[Packaging feasibility research](PACKAGING_RESEARCH.md) assesses a toolkit-only
setup program that reuses a separately installed Windhawk, with reproducible
probes, deployment findings, and the remaining validation gates.

## What this is

This is the companion repository to
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar),
a small Windhawk mod that hides the RDP connection bar overlay. That repo is a simple,
low-risk UI mod: it patches window behavior inside `mstsc.exe` and nothing else.

This repository is a toolkit of components built around the same idea, aimed at a more
ambitious, personal-use-primarily goal: a live taskbar widget on the client machine
that reflects state from inside the remote RDP session, wired together over an RDP
Dynamic Virtual Channel (DVC).

This toolkit's client-side taskbar component specifically began as a fork of
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar).
That original mod still exists and continues to be independently maintained at its own
repository, separate from this fork's ongoing changes.

## Why a separate repository

The two repos are split because they carry different trust levels, not just different
feature sets. `Hide RDP Connection Bar` is a single UI mod with a narrow, easily-audited
scope. This repository is where a **native standalone executable that registers as a COM
server and can act on a live RDP session** lives — a much larger attack
surface and a much higher bar for review before anyone other than the author should run
it. Keeping it in its own repo keeps that distinction visible instead of quietly
expanding the trust footprint of the original mod.

## What works today: the client-side taskbar widget

`taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp` is a Windhawk mod that
runs inside `mstsc.exe` (the Windows Remote Desktop client) on your local machine. It's
a fork of Hide RDP Connection Bar, extended with session controls and a status panel that
the mod embeds in your own taskbar (from `explorer.exe`). Everything it does is entirely
local to the client machine. Its local controls work independently; with the relay
installed, it also receives host-widget commands and publishes client state that
the relay returns to the host.

It compiles cleanly against the Windhawk toolchain and has been live-tested by the
operator across the panel, overlay, session controls, and relay-backed host actions.

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

Once enabled, a status panel appears in your own taskbar with session action
buttons, each with a tooltip explaining what the click does. Reconnect is hidden
by default and appears when enabled in settings. An optional
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
- **Idle time** — a row on the on-screen button (fullscreen sessions) and the tooltip
  of the taskbar panel's status text (all sessions) show how long *this computer's*
  keyboard and mouse have been idle. That is deliberately local idle time, not
  remote-session activity. (The session-duration clock that used to sit beside it was
  removed at v0.10.0.)
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

### Primary presentation: a status panel in your own taskbar

As of **v0.6.0** the client mod also targets `explorer.exe` on the *client* machine
(a single Windhawk mod can target more than one process). When loaded there, its
**explorer.exe branch** puts a wide, always-visible RDP status panel directly into the
Windows 11 taskbar — the same taskbar-XAML injection technique the host-side widget
below uses. The panel shows the remote host name over a state-and-protocol line
("Active · RDP", or an honest "not responding" line) — the same two-line format the
host widget uses — with idle time and connection quality in its tooltip, and five
directly clickable buttons: Minimize / Restore / Fullscreen-toggle / Reconnect /
Disconnect —
for fullscreen and windowed sessions alike. It does nothing to the session on its own:
it reads a status file the mstsc.exe branch writes once a second and sends its actions
back over a local, secret-guarded message channel. Because both surfaces are now one
mod id, the panel finds that status file directly (no sibling-directory guessing). To
use only the taskbar panel, leave *Show floating overlay button* off (its default).
See the verification record in
[D-41](.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch).

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
- **Minimize the client** — signals the connecting computer, over the toolkit's RDP
  Dynamic Virtual Channel relay, to minimize its Remote Desktop window. The send now
  reads the relay's reply back before closing the channel, which both confirms
  delivery and fixed a race where the command could be dropped.
- **Fullscreen / windowed toggle for the client** — switches the connecting
  computer's Remote Desktop window between fullscreen and windowed over the same
  relay. The relay's replies (and a lightweight every-few-seconds status query)
  carry the client's current mode back, so the button labels its actual direction —
  or says plainly that the state is unknown.
- **Restart Clipboard** — kills and relaunches this session's `rdpclip.exe`, the
  helper that carries copy-and-paste between your machine and the session, to fix
  it when it wedges. It only ever touches a process in this session whose full
  image path really is `%SystemRoot%\System32\rdpclip.exe`. **Restarting it
  discards whatever is currently on the shared clipboard** — the button's tooltip
  says so. Console-gated like Disconnect.
- **Clipboard helper not running** — a line that appears when no clipboard helper
  can be found in the session. When one *is* running the widget says nothing about
  the clipboard at all: a helper that is running but wedged can't be told apart
  from a healthy one, so it never claims the clipboard is fine.

Disconnect and Restart Clipboard act entirely locally on the session host; Minimize
and the fullscreen toggle are the toolkit's cross-machine pipe (host mod → DVC relay
plugin → client mod), which also carries the client's state back to the host. Install
it the same way as the client mod (Windhawk → **New mod** → paste the file contents),
but on the session host.

It compiles cleanly against the Windhawk toolchain and has been live-tested by the
operator, including taskbar injection, RDP gating, clipboard recovery, and relay-backed
client controls.

## Cross-machine control

The cross-machine pipe is live-proven: host mod → DVC relay plugin
(`dvc-plugin/relay/`, a standalone out-of-process COM LocalServer the RDP client
activates) → the client mod's relay receiver. As of host v0.4.0 / relay v0.2.0 the
pipe is two-way per interaction: every command the host sends is answered by a small
status reply carrying the client's session/fullscreen state, read back before the
channel closes. The reply leg, fullscreen command, and host periodic state poll have
also been live-tested. (`dvc-plugin/probe/` is the
retired throwaway experiment that first proved the DVC activation mechanism; it is
kept as reference only.)

For the full target architecture, the reasoning behind these decisions, and current
open questions, see [`HANDOFF.md`](HANDOFF.md) and
[`.raiden/state/DECISIONS.md`](.raiden/state/DECISIONS.md) rather than duplicating that
detail here.

## Structure

- [`taskbar-integration/`](taskbar-integration/README.md) — the two Windhawk mods:
  the client-side mod (`client/`, targeting both `mstsc.exe` and — since v0.6.0 —
  `explorer.exe` for the taskbar-embedded panel) and the host-side mod (`host/`),
  both built, see above
- [`dvc-plugin/`](dvc-plugin/README.md) — the standalone DVC relay executable
  (`relay/`, the production plugin at v0.2.0) plus the retired activation probe
  (`probe/`, kept as reference)

## License

MIT — see [LICENSE](LICENSE).
