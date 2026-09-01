# Project Handoff — RDP Session Toolkit
## Briefing for a brainstorming & actuation partner

> **Who you are (the agent reading this):** a thinking partner in a plain chat —
> no filesystem, no repo, no build tools. This document is your entire context.
> Your job is to help **brainstorm the design** and **drive actuation** (decide
> and sequence the next concrete steps). You do not need to write code; you need
> to reason well about a Windows systems-integration project and ask sharp
> questions. Everything you need to be useful is below. Where this document says
> something is **undecided**, that's an invitation to help decide it — not a gap
> to paper over.

---

## 1. The one-paragraph goal

Put a **live widget on the Windows taskbar of the RDP *client* machine** that
reflects **state from inside the remote RDP session**, wired together over an
**RDP Dynamic Virtual Channel (DVC)**. This is the more ambitious companion to an
existing, shipped Windhawk mod ("Hide RDP Connection Bar") that simply hides the
RDP connection bar overlay inside `mstsc.exe`. This project is
**personal-use-primarily**, Windows 11, and deliberately higher-trust than the
original mod.

---

## 2. Why this is its own repo (the trust boundary)

The original "Hide RDP Connection Bar" is a narrow, easily-audited UI patch that
only tweaks window behavior inside `mstsc.exe`. This project is different in kind:
it introduces a **native standalone executable that registers as a COM server and
can act on a live RDP session** — a materially larger attack surface. It was split
into its own repository specifically to keep that trust distinction visible rather
than quietly expanding the footprint of the simple mod. Keep this framing in mind:
**"is this worth the added trust cost?" is a legitimate recurring question**, not a
settled matter.

---

## 3. Target architecture (planned — three parts + a wire)

```
   REMOTE SESSION HOST                         CLIENT MACHINE
   (what you RDP *into*)                        (where mstsc runs)

  ┌───────────────────────┐                  ┌────────────────────────┐
  │ Remote-side Windhawk   │                  │ Client-side Windhawk   │
  │ mod (in explorer.exe)  │                  │ mod (in mstsc.exe)     │
  │  - detects state       │                  │  - listens for a       │
  │    change              │                  │    window message      │
  │  - pushes it out ──────┼───── DVC ───────►│  - updates a taskbar   │
  │    over the channel    │   (dynamic       │    widget              │
  └───────────────────────┘   virtual        └───────────▲────────────┘
                               channel)                   │ window message
                                                          │
                                              ┌───────────┴────────────┐
                                              │ DVC relay plugin        │
                                              │ (standalone COM EXE)    │
                                              │  - receives DVC signal  │
                                              │  - relays to the mod    │
                                              └─────────────────────────┘
```

1. **Client-side Windhawk mod** (`taskbar-integration/`, in `mstsc.exe`) — listens
   for a window message and updates a taskbar widget. *Not built.*
2. **Remote-side Windhawk mod** (`taskbar-integration/`, in `explorer.exe` on the
   session host) — detects state and pushes it out over the DVC. *Not built.*
3. **DVC relay plugin** (`dvc-plugin/`, standalone EXE on the client) — receives
   the DVC signal and hands it to the client-side mod via a window message.
   *A proof-of-mechanism probe of this is built; the real relay is not.*

Plain-language flow: *something changes on the remote desktop → remote mod sends
it down the DVC → DVC plugin on the client picks it up → hands it to the client
mod via a window message → client mod updates the local taskbar.*

---

## 4. Ground truth — what is actually built right now

Only **one** thing is built, and it is deliberately a **throwaway proof**, not a
piece of the final product:

**A DVC "probe"** (`dvc-plugin/probe/`) whose sole purpose is to answer the single
riskiest unknown in the whole plan:

> Does `mstsc` actually **activate** an out-of-process plugin that's registered
> under the client's `AddIns` key in the `{CLSID}` / `LocalServer32`
> (out-of-process) form, and does that plugin actually **receive a DVC signal**
> sent from inside the RDP session?

It has three parts:
- a **client plugin** — an out-of-process COM LocalServer EXE implementing the DVC
  plugin interfaces; on receiving *any* bytes it appends a timestamped line to a
  log file (the simplest observable proof);
- a **server-side trigger** — a tiny standalone EXE that opens the DVC and sends a
  trivial ASCII payload;
- a **TESTING.md** with the exact operator procedure.

It was **adapted from Microsoft's official reference**,
`microsoft/rdp-dvc-plugin-samples` (the `Simple/cpp` client-plugin / server-trigger
pair), which uses the same out-of-process COM LocalServer model this project chose.

**The probe payload is meaningless on purpose.** It proves the pipe works, not what
travels through it.

---

## 5. Proven vs. unproven — be precise about this

**Verified locally (single machine, no admin):**
- Compiles and links cleanly with the target toolchain.
- `/register` writes the correct **per-user (HKCU)** registry entries from a
  **non-elevated** prompt; `/unregister` cleans them up. (The "no admin needed"
  claim from earlier investigation was re-confirmed against the actual build, not
  assumed.)
- The COM server starts, `CoRegisterClassObject` succeeds, and it writes its log.
- The trigger's send path executes down to `WTSVirtualChannelOpenEx` (it fails with
  a benign error *only* because there's no RDP session on a local console — the code
  path itself is exercised).

**NOT yet verified (this is the gating unknown):**
- That `mstsc` *actually activates* the plugin, and that the DVC signal *actually
  arrives*. This **inherently requires a live two-machine RDP session** and cannot
  be tested in a dev sandbox. Until an operator sees the "signal received" line in
  the log on real hardware, the mechanism is **"built and plausible," not
  "proven."** Everything downstream in the plan rests on this.

**Treat step 1 of any roadmap as: confirm the probe on real hardware.** If it
fails, the architecture — not just the code — may need to change.

---

## 6. Locked-in technical facts & decisions (so we don't relitigate them)

- **Out-of-process, not in-process.** The DVC plugin runs as a separate process
  from `mstsc.exe` (COM `LocalServer32`), not injected in-process, for crash
  isolation — matching Microsoft's own documented rationale: a fault in the plugin
  must not take down the RDP client. (This also gives it its own build/signing
  pipeline, distinct from the Windhawk mods.)
- **Registration is HKCU, per-user, no admin.** The plugin registers itself under
  `HKCU\Software\Microsoft\Terminal Server Client\Default\AddIns\<name>` with the
  **bare `{CLSID}`** value (not a `path.dll:{CLSID}` string), plus
  `HKCU\Software\Classes\CLSID\{…}\LocalServer32` → EXE path. The bare-`{CLSID}` form
  is what makes `mstsc` use normal COM activation → `LocalServer32` → a separate
  process. Confirmed working non-elevated.
- **Toolchain reality:** the compiler in use is **Windhawk's bundled clang**, which
  is a **mingw-targeting clang** (`x86_64-w64-windows-gnu`), **not MSVC**.
  Consequences that will recur:
  - Its sysroot **does** ship the needed DVC/WTS/COM headers
    (`tsvirtualchannels.h`, `wtsapi32.h`, etc.), so native C++ DVC/COM code builds
    with it directly — no Visual Studio / Windows SDK required.
  - It has **no C++/WinRT** (`winrt::implements<>`, `winrt::com_ptr<>`), so
    Microsoft's WinRT-flavored sample had to be re-expressed as **plain Win32 COM**
    (hand-rolled `IUnknown` refcounting). The final relay plugin should stay on
    plain Win32 COM for the same reason.
  - EXEs **must be built `-static`**, or they fail to launch (missing clang runtime
    DLLs) when COM/`mstsc` starts them from a foreign working directory.
- **Windhawk mods are C++** compiled by that same clang — so the whole stack (mods
  + relay plugin) is one language and one compiler. The sibling
  "Hide RDP Connection Bar" repo already builds successfully this way and already
  contains reusable taskbar-side patterns (e.g. a thumbnail toolbar with
  Minimize/Restore rows, drag-to-reposition UI).
- **Probe identifiers** (may or may not carry into production): DVC channel name
  `dvc::taskbar::probe`, CLSID `{3194520D-DE59-4432-95B5-D5CB4FAFC30E}`, AddIn name
  `HideRdpBarTaskbarDvcProbe`. Client and trigger share one header so these can't
  drift.

---

## 7. The brainstorming agenda — open design questions

This is the heart of the handoff. These are genuinely open and worth thinking hard
about. Grouped by area.

### A. The relay hop: DVC plugin → client-side mod
The plugin (its own process) has to get the signal into the mod (living inside
`mstsc.exe`). The plan says "a window message." Things to reason about:
- **Which mechanism?** `RegisterWindowMessage` + a targeted `PostMessage`, or
  `WM_COPYDATA` (which can carry a payload, not just a ping), or step outside window
  messages entirely (named pipe, shared memory + event, memory-mapped file).
  Window messages are simplest but carry the least data and have delivery caveats.
- **How does the plugin find the mod's window?** The mod would need to create a
  hidden message-only window with a discoverable class/title, or register a known
  name. Race: what if the signal arrives before the mod's window exists?
- **Integrity levels / UIPI.** `mstsc` and the plugin may run at different integrity
  levels; Windows blocks cross-level window messages by default. Does the receiver
  need `ChangeWindowMessageFilterEx`? Does an out-of-process plugin dodge or worsen
  this vs. an in-process one?
- **Coupling of identity:** how does the plugin know *which* `mstsc` window/session
  a given signal belongs to, if more than one RDP session is open?

### B. The payload & wire protocol
- **This is the biggest genuinely-undecided product question: what state does the
  widget actually reflect?** The READMEs only say "state from the remote session."
  Concretely — is it the remote foreground window? A count of remote notifications?
  Remote taskbar contents mirrored? A specific app's status? **Nothing downstream can
  be firmly designed until this is pinned down.** Push on it.
- Once the *what* is known: wire format, versioning, and framing. DVC chunks around
  ~1600 bytes and fragments larger messages (`CHANNEL_FLAG_FIRST/LAST`); the WTS
  read API reassembles automatically but a file-handle path does not. How much data,
  how often, how latency-sensitive?

### C. Remote-side detection (what generates the signal)
- The remote mod runs in `explorer.exe`. **What does it watch** to know state
  changed? (Shell hooks, foreground-window changes, taskbar/notification state,
  specific window events.)
- **Fold the sender into the mod.** The probe's trigger is a standalone EXE; in
  production the send side (`WTSVirtualChannelOpenEx` + write) needs to live inside
  the remote Windhawk mod. Any constraints on calling WTS channel APIs from within
  `explorer.exe`?

### D. Lifecycle & robustness
- The probe plugin currently **lives until the session ends** (no idle shutdown).
  Production wants a defined lifecycle: when to start, when to exit, reconnection
  after a dropped/resumed RDP session, behavior across multiple monitors.
- **Multiple concurrent RDP sessions / multiple `mstsc` windows** — supported or
  explicitly out of scope? This ripples into A and B.
- What happens when the mod isn't loaded but the plugin gets a signal (and vice
  versa)? Graceful degradation, buffering, or drop?

### E. Trust, security, distribution
- The relay plugin is **the high-trust component** — it accepts data *from the
  remote session* and acts on the client. Treat the remote as **semi-trusted input**:
  strict bounds checking, no parsing that can be driven into a bad state by a hostile
  remote.
- **Code signing** the standalone EXE, and what review bar must be cleared before
  anyone other than the author runs it.
- Does out-of-process actually reduce the blast radius meaningfully, or just move it?

### F. Taskbar widget UX (client side)
- What does the widget *look like and do*? Overlay icon, thumbnail-toolbar button,
  jump-list entry, a custom element? The sibling repo's existing thumbnail-toolbar /
  button work is a reuse candidate — worth leaning on rather than inventing.

---

## 8. Actuation roadmap (proposed order — challenge it)

1. **GATING: live-test the probe on real hardware** (two machines, per the project's
   TESTING procedure). Confirm `mstsc` activates the plugin and the signal arrives.
   *Nothing else should be built until this is green* — a failure here can invalidate
   the architecture, not just the code.
2. **Prove the relay hop end-to-end with a trivial payload:** extend the (now
   confirmed) plugin to forward a dummy signal to a stub client-side mod that does one
   visible thing. This validates area **A** in isolation before any real feature.
3. **Decide the actual feature / payload** (area **B/F**) — this is a product
   decision and is currently the critical-path blocker. Everything concrete depends
   on it.
4. **Build remote-side detection** as a Windhawk mod (area **C**), folding in the
   send side.
5. **Build the client-side taskbar widget** (area **F**), reusing sibling-repo
   patterns.
6. **Harden:** lifecycle, reconnection, multi-session, input validation, signing
   (areas **D/E**).

Each numbered step is a natural "prove one thing, then proceed" boundary — the same
philosophy that produced the probe.

---

## 9. Constraints & conventions to respect

- This repo runs a "RAIDEN" control-plane in `.raiden/`; mainline branch is **`main`**
  (never `master`); commits **must not** include `Co-Authored-By` trailers; commit
  only when explicitly asked.
- Windows 11, personal-use-primarily. No admin should be required for the
  client-side install (registration is HKCU) — preserve that property.
- One-language / one-compiler stack (Windhawk's mingw clang, plain Win32 COM) — new
  native pieces should stay compatible with it.

---

## 10. Sharp questions for you (the agent) to put back to us early

- **What exact remote state should the widget reflect?** (Undecided; blocks §7B and
  §7F and roadmap step 3.)
- **One session at a time, or multiple concurrent RDP sessions?** (Ripples through
  §7A, §7B, §7D.)
- **How fresh must the widget be** — instantaneous, or is a second of latency fine?
  (Shapes protocol and detection strategy.)
- **Who besides the author will ever run this?** (Sets the security/signing/review
  bar in §7E.)
- **Is the window-message relay a firm decision, or open to a pipe/shared-memory
  alternative** if UIPI/integrity-level issues bite in §7A?

---

*Glossary (in case it helps): **DVC** = RDP Dynamic Virtual Channel, a bidirectional
data channel inside an RDP connection. **Windhawk** = a Windows mod platform that
injects C++ mods into running processes. **`mstsc.exe`** = the Windows RDP client.
**COM LocalServer** = a COM object served by a standalone `.exe` in its own process
(vs. an in-process DLL). **`IWTSPlugin` / `WTSVirtualChannelOpenEx`** = the client
plugin interface and the server-side channel-open API, respectively, for DVCs.*
