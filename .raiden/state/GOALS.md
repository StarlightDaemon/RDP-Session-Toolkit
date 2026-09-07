# Goals

- Build a client-side Windhawk mod that extends the existing RDP connection-bar-hiding
  mod with a signal-relay capability.
- Build a remote-side Windhawk mod targeting `explorer.exe` on the RDP session host that
  adds a taskbar widget.
- Build a standalone DVC-relay companion executable that receives an RDP Dynamic Virtual
  Channel signal on the client machine and relays it to the client-side mod via a window
  message.
- All three components communicate to give the client-side taskbar a live signal from
  the remote session, without merging the added trust/complexity into the original
  Hide RDP Connection Bar repo.

All three goals are built and have been live-tested by the operator. Current
verification and remaining hardening work are recorded in `CURRENT_STATE.md`
and `OPEN_LOOPS.md`.
