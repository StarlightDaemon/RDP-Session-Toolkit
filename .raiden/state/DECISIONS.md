# Decisions

## D-1: Separate repo from Hide RDP Connection Bar

This work lives in its own repository rather than inside `Hide RDP Connection Bar`
because it carries a different trust level: it includes a native standalone executable
(`dvc-plugin/`) that registers as a COM server and can act on a live RDP session, versus
the simple, low-risk UI mod in the original repo.

## D-2: Two separate Windhawk mod files, not one

`taskbar-integration/` will eventually hold two distinct Windhawk mods — a client-side
mod (extends the existing bar-hiding mod with signal-relay capability) and a remote-side
mod (targets `explorer.exe` on the session host to add a taskbar widget) — rather than a
single mod, because Windhawk mods are injected per target process and the two mods run
in entirely different processes on potentially different machines (local client vs.
remote session host).

## D-3: DVC relay plugin is not a Windhawk mod

`dvc-plugin/` is a standalone executable, not a Windhawk mod, because it must run
out-of-process from `mstsc.exe` for crash isolation, per Microsoft's own documented
rationale for out-of-process Dynamic Virtual Channel plugins. It has its own build and
code-signing requirements distinct from the Windhawk mod toolchain.
