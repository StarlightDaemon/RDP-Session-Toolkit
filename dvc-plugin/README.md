# dvc-plugin

**Not built yet.** Placeholder for the standalone DVC relay companion executable.

## What will eventually live here

A standalone executable that runs on the RDP client machine. It receives an RDP Dynamic
Virtual Channel (DVC) signal sent by the remote-side Windhawk mod (see
[`taskbar-integration`](../taskbar-integration/README.md)) and relays it to the
client-side Windhawk mod via a Windows window message.

## This is not a Windhawk mod

This is a separate kind of software from the two mods in `taskbar-integration/`, with
its own build and code-signing requirements. It is a standalone executable that
registers as a DVC plugin (a COM server), not code injected into an existing process.

It has to run **out-of-process** from `mstsc.exe` rather than as an in-process Windhawk
mod, for crash isolation — this matches Microsoft's own documented rationale for
out-of-process Dynamic Virtual Channel plugins: a fault in the plugin should not be able
to take down the RDP client itself. Because it is a separate process registered as a COM
server, it also needs its own build pipeline and code-signing setup distinct from the
Windhawk mod toolchain used for `taskbar-integration/`.
