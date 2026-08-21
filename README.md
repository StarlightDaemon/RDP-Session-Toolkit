# RDP Session Toolkit

**Status: nothing in this repository is built yet. Everything described below is
planned, not implemented.**

## What this is

This is the companion repository to
[Hide RDP Connection Bar](https://github.com/StarlightDaemon/Hide-RDP-Connection-Bar),
a small Windhawk mod that hides the RDP connection bar overlay. That repo is a simple,
low-risk UI mod: it patches window behavior inside `mstsc.exe` and nothing else.

This repository is the more complex, personal-use-primarily extension of that idea: it
adds a live taskbar widget on the client machine that reflects state from the remote RDP
session, wired together over an RDP Dynamic Virtual Channel (DVC).

## Why a separate repository

The two repos are split because they carry different trust levels, not just different
feature sets. `Hide RDP Connection Bar` is a single UI mod with a narrow, easily-audited
scope. This repository includes a **native standalone executable that registers as a COM
server and can act on a live RDP session** — a much larger attack surface and a much
higher bar for review before anyone other than the author should run it. Keeping it in
its own repo keeps that distinction visible instead of quietly expanding the trust
footprint of the original mod.

## Architecture (planned)

Two new Windhawk mods plus one standalone plugin, communicating over an RDP Dynamic
Virtual Channel:

1. **Client-side Windhawk mod** (`taskbar-integration/`) — extends the existing
   bar-hiding mod, running inside `mstsc.exe` on the local machine, with a signal-relay
   capability: it listens for a window message and updates a taskbar widget.
2. **Remote-side Windhawk mod** (`taskbar-integration/`) — runs inside `explorer.exe` on
   the remote RDP session host, and pushes state out over a Dynamic Virtual Channel.
3. **DVC relay plugin** (`dvc-plugin/`) — a standalone executable (not a Windhawk mod)
   that runs on the client machine, receives the DVC signal from the remote-side mod, and
   relays it to the client-side mod via a window message.

In plain terms: something changes on the remote desktop → the remote-side mod sends it
down the DVC → the DVC plugin on the client picks it up and hands it off with a window
message → the client-side mod updates the local taskbar.

## Structure

- [`taskbar-integration/`](taskbar-integration/README.md) — the two Windhawk mods
  (client-side and remote-side)
- [`dvc-plugin/`](dvc-plugin/README.md) — the standalone DVC relay executable

## License

MIT — see [LICENSE](LICENSE).
