# taskbar-integration

**Not built yet.** Placeholder for two future Windhawk mods.

## What will eventually live here

Two separate Windhawk mod source files:

1. **Client-side mod** — extends the existing RDP connection-bar-hiding mod (from the
   [Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar)
   repo) with a signal-relay capability. It runs inside `mstsc.exe` on the local machine
   and listens for a window message from [`dvc-plugin`](../dvc-plugin/README.md), then
   updates a taskbar widget accordingly.
2. **Remote-side mod** — targets `explorer.exe` on the remote RDP session host and adds
   the taskbar widget itself, pushing state changes out over an RDP Dynamic Virtual
   Channel to the client.

## Why two separate mod files, not one

Windhawk mods are injected per target process, and these two mods run in different
processes on different machines: the client-side mod is injected into `mstsc.exe` on the
local machine, while the remote-side mod is injected into `explorer.exe` on the remote
session host. A single mod file cannot target both processes, on both sides of an RDP
connection, at once — so the functionality has to be split into two mods that
communicate over the RDP Dynamic Virtual Channel rather than sharing process memory.
