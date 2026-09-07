# dvc-plugin

The production relay is built at [`relay/`](relay/). It is a client-side,
out-of-process COM `LocalServer32` plugin that relays host-widget commands over
an RDP Dynamic Virtual Channel to the client Windhawk mod, then returns the
client's current session/fullscreen state on the same channel. The operator has
live-tested the complete two-machine path.

## Components

- [`relay/`](relay/) — the production DVC relay companion executable.
- [`probe/`](probe/) — the retired activation probe that established the
  out-of-process DVC architecture; retained as historical reference.

## This is not a Windhawk mod

This is a separate kind of software from the two mods in `taskbar-integration/`, with
its own build and code-signing requirements. The relay is a standalone executable that
registers as a DVC plugin (a COM server), not code injected into an existing process.

It has to run **out-of-process** from `mstsc.exe` rather than as an in-process Windhawk
mod, for crash isolation — this matches Microsoft's own documented rationale for
out-of-process Dynamic Virtual Channel plugins: a fault in the plugin should not be able
to take down the RDP client itself. Because it is a separate process registered as a COM
server, it also needs its own build pipeline and code-signing setup distinct from the
Windhawk mod toolchain used for `taskbar-integration/`.
