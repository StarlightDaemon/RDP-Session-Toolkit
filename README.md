# RDP Session Toolkit

Personal-use RDP taskbar controls, diagnostics and recovery helpers for Windows 11.
The toolkit puts controls in the connecting PC's taskbar and the remote session's
taskbar, with a Dynamic Virtual Channel (DVC) relay between them.

**Home-test download:** [Windows x64 candidate ZIP](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/RDP-Session-Toolkit-session-reliability-rc.1-windows-x64.zip).
Start with the [home update, test and rollback guide](HOME_UPDATE_TEST_ROLLBACK.md),
also available as a [standalone release asset](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/HOME_UPDATE_TEST_ROLLBACK.md).
The [RC1 prerelease page](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/tag/session-reliability-rc.1)
has release notes and [asset checksums](https://github.com/StarlightDaemon/RDP-Session-Toolkit/releases/download/session-reliability-rc.1/SHA256SUMS.txt).

The ZIP includes the built relay, Windhawk mod sources, diagnostics, acceptance
material and the guide. No relay rebuild is needed; Windhawk compiles the mod
sources during installation. This is a manual, unsigned home-test candidate.
Use independently installed Windhawk; toolkit setup must reuse it rather than
install or upgrade it.

## Components and current candidate

| Component | Version | Runs on |
|---|---|---|
| [Taskbar Client](taskbar-integration/client/rdp-session-toolkit-taskbar-client.wh.cpp) | 0.11.1 | Connecting PC: mstsc.exe and local explorer.exe |
| [Taskbar Host](taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp) | 0.5.1 | Remote session host: explorer.exe |
| [DVC relay](dvc-plugin/relay/common/RelayIds.h) | 0.3.1 | Connecting PC: COM LocalServer activated by mstsc |

This candidate adds diagnostics, action feedback and conservative ambiguity
refusal, with registration, worker, callback and relay-lifetime fixes. Use the
matched components: protocol v3 refuses legacy actions without an unsafe fallback.

Local regression and compile/link checks passed; real two-machine, Explorer,
visual and resource acceptance is still required. The operator's earlier live-use
report applies to the preceding baseline, scoped in
[D-41](.raiden/state/DECISIONS.md#d-41-operator-verification-closes-the-current-live-test-batch),
not this candidate. See the [implementation review](IMPLEMENTATION_REVIEW.md)
and [extended acceptance matrix](TOP_THREE_ACCEPTANCE.md).

The release tag identifies the implementation revision. Refreshed assets include
the later guide/download-link revision; the ZIP manifest and dated
[work log](.raiden/state/WORK_LOG.md) record provenance and verified checksums.
Later repository documentation does not imply rebuilt release assets.

## Client taskbar controls

One client mod targets mstsc.exe and local explorer.exe. Its primary presentation
is an always-visible Windows 11 taskbar panel. The optional floating fullscreen
overlay is off by default. The thumbnail toolbar and separate embedded-panel mod
are retired.

- **Status:** host name, client session/window state and an unresponsive-window
  warning. Hover the text for this PC's keyboard/mouse idle time and connection
  quality, bandwidth and round-trip time when mstsc reports them. Missing quality
  data stays unknown; local idle time is not remote-session activity. There is no
  session-duration clock.
- **Minimize / Restore:** change the local RDP window without disconnecting.
- **Fullscreen / windowed:** foregrounds the session and sends Remote Desktop's
  Ctrl+Alt+Break shortcut. Failure or uncertainty is reported.
- **Disconnect:** uses the RDP window's normal close path; the remote session
  keeps running.
- **Reconnect / Force reconnect:** opt-in and off by default. Reconnect reopens
  the connection in the selected display mode. Force reconnect can end an
  unresponsive client after a grace period. Every reconnect requires a click.

Windhawk settings also offer connection-bar hiding, an optional disconnect hotkey,
overlay positioning/fading and per-feature toggles.

## Host taskbar controls

The host mod embeds a widget in the remote session's Windows 11 taskbar. Windows
Terminal Services (WTS) supplies that session's state and connecting client name.

- **Minimize the client / Fullscreen toggle:** send commands through the DVC
  relay. Replies and periodic queries return client state; unavailable or stale
  state is shown as unknown.
- **Disconnect:** detaches the current RDP session without signing it out.
- **Restart Clipboard:** restarts verified System32 rdpclip.exe processes in
  this session. This discards shared clipboard contents; feedback includes
  failure or uncertainty.
- **Clipboard helper not running:** reports absence. A running helper does not
  prove copy/paste health, so the widget makes no health claim.

Disconnect and clipboard recovery act locally on the host, are RDP-gated and
remain independent of relay readiness.

## Communication and trust

The client's mstsc branch writes a versioned status snapshot in its mod storage.
Its local Explorer panel reads it and sends bounded WM_COPYDATA commands back.
The cross-machine path is **host widget → DVC → client COM relay → mstsc
receiver**, with correlated replies read before closing the channel. The relay
also reads the client snapshot for state replies. This carries controls and
client state, not remote-app mirroring or arbitrary remote telemetry.

Protocol v3 binds actions to process ID and creation time. Extra mstsc processes
in the same Windows session, including unconnected dialogs, or multiple RDP
frames in one process pause peer controls. There is no session chooser.
Snapshots require valid fields, matching identity and freshness. Feedback
separates acceptance from observed changes; uncertain actions are never retried
automatically.

This retains a personal-use, same-user trust boundary. The shared secret and
sender-window/image checks do not authenticate against hostile same-user code;
WM_COPYDATA's sender window is caller-controlled. See the
[security and compatibility review](IMPLEMENTATION_REVIEW.md).

## Update and diagnose

Use the [home guide](HOME_UPDATE_TEST_ROLLBACK.md) for both machines' backups,
update order, existing-path relay registration, tests and rollback. Load each
mod through Windhawk's **New mod** editor using the appropriate source file;
these mods are not published in its mod catalog. Taskbar panels require Windows
11; the candidate targets Windows x64. The client source requires Windhawk 1.6
or later. Do not register a second extracted relay copy or double-click its EXE:
without arguments it starts a COM server.

For a read-only local report, run from the repository or extracted bundle root:

```powershell
./scripts/Get-ToolkitDiagnostics.ps1
./scripts/Get-ToolkitDiagnostics.ps1 -Json
```

Diagnostics omit endpoint names by default and do not collect command secrets,
command lines, connection-file contents or clipboard contents. They neither
repair components nor upload data. File/source presence does not prove loaded
versions or successful taskbar injection.

## Development and context

- [Handoff](HANDOFF.md) and [roadmap](ROADMAP.md): source entry points and next work.
- [Runtime checks](probes/runtime/README.md) and
  [packaging probes](probes/packaging/README.md): verification and boundaries.
- [Candidate builder](scripts/build-refinement-candidate.ps1): creates an ignored
  bundle without installing or registering it.
- [Current state](.raiden/state/CURRENT_STATE.md),
  [loop ledger](.raiden/state/OPEN_LOOPS.md),
  [decisions](.raiden/state/DECISIONS.md) and
  [work log](.raiden/state/WORK_LOG.md): continuity and historical evidence.
- [Repository review](REPOSITORY_REVIEW.md),
  [ranked additions](BEST_NEXT_ADDITIONS.md) and
  [packaging research](PACKAGING_RESEARCH.md): rationale and proposals.

The client component began as a fork of
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar),
which remains independently maintained. This repository keeps the broader
taskbar/COM relay trust footprint separate from that narrow UI mod. The retired
[DVC probe](dvc-plugin/probe/) remains reference material; its activation test is
historical evidence, not an experiment still waiting to be run.

## License

MIT — see [LICENSE](LICENSE).
