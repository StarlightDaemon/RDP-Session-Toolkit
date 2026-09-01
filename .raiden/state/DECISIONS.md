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

## D-4: Client-mod disconnect mechanism is WM_CLOSE to the mstsc frame

The client-side mod's Disconnect actions (thumbnail-toolbar button, overlay button,
hotkey) post `WM_CLOSE` to the main mstsc frame window (`TscShellContainerClass`).
Investigated against the ported V1 source directly: V1 never hooks or invokes any
internal disconnect mechanism of the connection bar it hides — its own established,
live-tested disconnect path *is* `PostMessage(frame, WM_CLOSE)`, which is the same
clean-disconnect path as closing the window normally. The documented default and the
"actual internal mechanism" therefore coincide; no undocumented internals are relied on.

## D-5: Relay receiver protocol (message window class, command byte, deferred sender validation)

The client-side mod creates a hidden message-only window (`HWND_MESSAGE` parent),
class `CitadelRdpTaskbarRelay`, at mod load, as the receiving end of the future
DVC-plugin → mod hop. Protocol: `WM_COPYDATA`, first payload byte = command, further
bytes reserved for future arguments; `0x01` = minimize (same code path as the
thumbnail-toolbar Minimize), `0x00` permanently unassigned. New commands extend the
enum/switch only — the window and class setup is stable. The receiver logs the sending
process ID but does **not** validate the sender yet: the relay plugin that will send
these signals has not been built, so validating against an undetermined future binary
identity would be guesswork. A marked TODO in the source requires sender validation
once the plugin exists with a known, stable identity. Until then, UIPI's default
blocking of lower-integrity senders is the only gate, and minimize is deliberately the
only (low-consequence) implemented command.

## D-6: Client mod is a fork of Hide RDP Connection Bar, not a shared-history migration

`taskbar-integration/rdp-session-toolkit-taskbar-client.wh.cpp` was copied from the
`Hide RDP Connection Bar` repo at commit `dc82b10d4c8713d71f5e556649b114a6d43dad04`
(v1.1.9) without importing that repo's git history. Provenance is recorded in the
ported file's header. The fork gets its own mod id (`rdp-session-toolkit-taskbar-client`,
namespaced under the toolkit so future sibling mods read consistently) and a fresh
version line starting at 0.1.0 (port) / 0.2.0 (relay receiver), per the fleet's
per-change version bump convention.

## D-7: Host mod widget deployment is native taskbar-XAML injection

The host-side mod (`taskbar-integration/host/rdp-session-toolkit-taskbar-host.wh.cpp`,
mod id `rdp-session-toolkit-taskbar-host`, own version line from 0.1.0) inserts its
widget directly into the taskbar's own XAML tree — `Grid#RootGrid` under
`Taskbar.TaskbarFrame`, reached by hooking
`winrt::Taskbar::implementation::TaskListButton::UpdateVisualStates` in
`Taskbar.View.dll` and walking up the visual tree — rather than using an overlay
window. The technique, including the Taskbar.View.dll GetModuleHandle-polling
cold-start path and the WM_SIZE initial-scan trigger, is adapted from the sibling
`native-taskbar-media-controller` mod (its D-1/D-7), which proved it live. That
mod's documented landmine is inherited as a constraint: a WUX `Popup` does not
render inside the taskbar's XAML Island compositor target (its D-2), so if this
mod ever needs a flyout it must be a Win32 `WS_POPUP` HWND — the current widget
deliberately has no flyout. The `taskbar-integration/` folder was reorganized into
`client/` and `host/` subfolders (pure move, before first commit of the mod files)
so the two sibling mods sit side by side.

## D-8: Host mod session facts come from WTS APIs against the current session

The host mod reads everything it displays via `WTSQuerySessionInformation` on
`WTS_CURRENT_SERVER_HANDLE` / `WTS_CURRENT_SESSION`: `WTSClientName` (connecting
client's machine name), `WTSConnectState` (live connection state), and
`WTSClientProtocolType` (console vs. RDP). Refresh is event-driven — a hidden
window on a dedicated thread registered via `WTSRegisterSessionNotification`
(`NOTIFY_FOR_THIS_SESSION`) re-queries on every `WM_WTSSESSION_CHANGE` — with a
30 s timer as a safety net and as the registration retry path. A hidden normal
top-level window is used instead of a message-only window because session-change
delivery to `HWND_MESSAGE` windows is not reliable. Disconnect calls
`WTSDisconnectSession(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, FALSE)` —
the same clean detach as the Start menu's Disconnect. The disconnect control is
gated on `WTSClientProtocolType == 2` (RDP), both in the button's enabled state
and re-checked at click time, so the mod can never disconnect the physical console
session and kick a locally-signed-in user to the lock screen.

## D-9: Host mod minimize trigger deferred — placeholder only

The host mod's v0.1.0 deliberately implements no minimize functionality, no DVC
channel code, and no cross-machine send/receive of any kind. All of that is gated
on the DVC probe's two-machine live test (LOOP-003) and the not-yet-built relay
plugin. A marked `TODO(minimize-trigger)` comment in `BuildWidget()` records where
the future control would hook in (a DVC-relayed signal to the client mod's
`CitadelRdpTaskbarRelay` receiver, command `0x01`) and is the only artifact —
mirroring the client side's D-5 deferral discipline.

## D-10: Production relay plugin identity (CLSID, AddIn name, channel name)

The production DVC relay plugin (`dvc-plugin/relay/`) is a new, permanent
component, separate from the throwaway probe (`dvc-plugin/probe/`, kept
unchanged as historical reference). It reuses the probe's verified model
verbatim — out-of-process COM `LocalServer32`, per-user HKCU-only registration
(no admin), plain Win32 COM built `-static` with the Windhawk-bundled clang —
and changes only the observable action: on `OnDataReceived` it relays the
payload's **first byte, as-is** (no translation, no protocol logic) to the
client mod's `CitadelRdpTaskbarRelay` message-only window via `WM_COPYDATA`,
discovered with `FindWindowExW(HWND_MESSAGE, …)` exactly as the client mod's own
self-test does. It is a pure pass-through; the client mod's receiver is the sole
interpreter of the byte.

Its identity is deliberately **distinct from the probe's** (the probe was
already unregistered; this is an independent registration):

| | Probe (retired) | **Relay (production)** |
|---|---|---|
| DVC channel | `dvc::taskbar::probe` | **`dvc::taskbar::relay`** |
| CLSID | `{3194520D-DE59-4432-95B5-D5CB4FAFC30E}` | **`{6FC96481-9467-496E-BA33-A202ED052F39}`** (freshly minted) |
| AddIn name | `HideRdpBarTaskbarDvcProbe` | **`RdpSessionToolkitDvcRelay`** |

These live in `dvc-plugin/relay/common/RelayIds.h`. Three of them are shared
contracts with other components and this decision is the single source of truth
that keeps the copies in sync: the **channel name** must match the host mod's
`WTSVirtualChannelOpenEx` call (`RELAY_DVC_CHANNEL`), the **target window class**
(`CitadelRdpTaskbarRelay`) must match the client mod's relay receiver, and the
**CLSID** is duplicated in the client mod for sender validation (D-11). There is
no trigger EXE in `relay/` (unlike the probe): the send side now lives inside
the host mod (D-12).

## D-11: Client relay receiver now validates the sender (closes D-5's TODO)

D-5 deferred sender validation because the relay plugin did not yet exist with a
stable identity. It does now (D-10), so the `CitadelRdpTaskbarRelay` receiver
authenticates every `WM_COPYDATA` sender before acting. `wParam` (the
WM_COPYDATA sender-window handle) is resolved to a pid via
`GetWindowThreadProcessId`, and exactly two senders are accepted:

1. **This mod's own process** — the built-in local self-test
   (`SendRelayTestMinimize`), which now passes the relay window itself (a
   same-process window) as `wParam`, so `senderPid == GetCurrentProcessId()`.
   This is what keeps the self-test working under validation.
2. **The registered relay plugin** — a separate process whose full image path
   (`QueryFullProcessImageNameW`) equals the EXE registered as the relay CLSID's
   `HKCU\Software\Classes\CLSID\{6FC96481-…}\LocalServer32` value — i.e. the
   exact binary mstsc activates. Read back at validation time via `RegGetValueW`.

Everything else — unidentifiable sender (pid 0), relay not registered
(fail-closed), or image-path mismatch — is logged and ignored. The relay CLSID
is duplicated as a literal in the client mod (not a shared header) because the
mod and the plugin are separate components with separate build systems; D-10 is
the reconciling source of truth. Compared against a signature check: an image
path tied to the HKCU LocalServer32 registration is the tightest binding
available without a signing pipeline, which the toolkit does not yet have.

## D-12: Host mod minimize send — implementation and threading

The host mod's `TODO(minimize-trigger)` (D-9) is replaced with a real Minimize
button in `BuildWidget()`, beside Disconnect, enabled only over RDP (re-checked
at click time, same discipline as Disconnect). Unlike Disconnect it does not act
locally: it opens the production DVC channel (`dvc::taskbar::relay`) with
`WTSVirtualChannelOpenEx(WTS_CHANNEL_OPTION_DYNAMIC)`, writes exactly one byte
(`0x01`, the value the client mod's receiver reads as minimize), and closes —
adapted directly from the probe's trigger open-write-close path.

**Threading:** a short-lived, **detached one-shot** background thread per click,
**not** the persistent-thread-with-readiness-event pattern used for
`StopWtsThread`/`StopRelayThread`. That pattern earns its complexity by owning a
long-lived window + message loop that must be torn down in order; this send owns
no window, no message loop, and no state past its own return — it is a single
open-write-close. It must run off the XAML UI / click thread because
`WTSVirtualChannelOpenEx` can have real latency and explorer.exe's message loop
must never block on it. Unload safety is handled with an atomic in-flight
counter (`g_MinimizeSendsInFlight`) that `Wh_ModUninit` drains with a bounded
(3 s) wait, so no detached thread is still running the DLL's code at unload.

If no relay is listening (the channel fails to open) the failure is logged via
`Wh_Log` and nothing further happens — no blocking dialog, no interruption to
explorer.exe, per the task's degradation requirement.

## D-13: Taskbar-embedded client mode gets its own local message window, not an extension of CitadelRdpTaskbarRelay (design only, not built)

The 2026-08-21 scoping session for the taskbar-embedded client presentation
mode (see `ROADMAP.md`) identified a cross-process authorization problem: that
future third mod would live in the client machine's own `explorer.exe`, and
needs some way to signal the existing client mod (in `mstsc.exe`) — but
`CitadelRdpTaskbarRelay`'s `IsAuthorizedRelaySender` only recognizes two
identities (its own process, and the DVC relay plugin's exact registered
binary path — D-11). `explorer.exe` is a ubiquitous, high-value system process
shared by every shell extension and every other Windhawk mod on the box, not
a narrow, purpose-built binary with one registered identity, so it cannot be
authenticated the same way.

Resolved: when this mode is built, it gets a **second, separate local message
window** with its own validation scoped to the same-machine threat model,
rather than extending or generalizing `CitadelRdpTaskbarRelay`'s validation to
also accept this new local sender.

D-11's validation is deliberately narrow — tight, exact-binary-path matching —
for the cross-machine case, where the sender is a purpose-built relay EXE with
exactly one registered identity. That channel is also the foundation other
future roadmap features are expected to build on as they land. Weakening
`CitadelRdpTaskbarRelay`'s guarantee to accommodate an unrelated same-machine
use case would lower the security bar for everything built on that channel
later, not just this one feature. A separate channel lets the local case get
its own appropriately-scoped validation design without touching that
guarantee.

This is a design decision, not an implementation: the local channel, its
validation mechanism, and the new mod itself are **not yet built**. What
"appropriately scoped" validation looks like for the same-machine case
(extra handshake/shared secret, a different IPC mechanism, command scoping,
UIPI/elevation handling) remains open — see `ROADMAP.md`.

**Update 2026-08-21:** built. The mod is D-21, the status channel D-22, the
local window and its shared-secret validation D-23; D-24 records that
`CitadelRdpTaskbarRelay` was left exactly as it was.

## D-14: Fullscreen/windowed toggle is synthesized Ctrl+Alt+Break, not an internal call

The client mod's Fullscreen/windowed toggle (thumbnail-toolbar button, v0.4.0)
sends the session window the exact key chord Remote Desktop documents for this —
Ctrl+Alt+Break — via `SendInput`, after bringing the RDP frame to the foreground
(`SetForegroundWindow`, with the established `AttachThreadInput` fallback because
a thumbnail click arrives via `explorer.exe` and the process may not hold
foreground rights). It reimplements no fullscreen-transition logic.

Investigated first: nothing in the code this mod already hooks exposes the
transition (`CreateWindowExW`/`ShowWindow`/`SetWindowPos`/`SetWindowTextW` see
its *effects*, not a trigger). Two candidate direct paths were identified and
deliberately **not** used: (a) the RDP ActiveX control's documented
`IMsRdpClient::FullScreen` property (DISPID 104 in this machine's registered
MSTSCLib, reachable through the same control pointer D-16 captures) — public COM,
but its interaction with mstsc.exe's container (`ContainerHandledFullScreen`,
`OnRequestGoFullScreen`) cannot be verified without a live session; (b) mstscax
internals, which would be an undocumented-symbol dependency. The chord reuses
Microsoft's own already-working toggle, matches the operator's stated safe
default, and has one hard safety rule: if the frame cannot be confirmed as the
foreground window, the chord is **not** sent (Ctrl+Break delivered to some other
window — a console, say — is a real interrupt; a logged no-op is the better
failure). Fullscreen state is detected from window style/geometry (caption gone,
or rect == monitor), not from any mstsc internal. Limitation: the chord only
works while the session is connected and mstsc's keyboard handling is active,
exactly like pressing the keys yourself.

## D-15: Session duration / idle display lives on both surfaces; idle is local-input idle

Session start is stamped at the existing connection-detection hook point —
`CreateWindowExW_Hook` latching the `TscShellContainerClass` frame — and
re-stamped by the control's `OnConnected` event when D-16's sink is live (a
tighter origin than "window created"). Idle time is `GetLastInputInfo`: this
computer's own physical keyboard/mouse idle, whether or not that input is aimed
into the RDP session. That is the intended meaning, not a limitation — it is a
"have I walked away from this machine" signal, deliberately *not* a measure of
activity inside the remote session.

Placement decision: **both** surfaces, because each covers what the other
cannot. The floating overlay can render arbitrary live text but only exists for
fullscreen sessions (the connection bar it replaces is fullscreen-only), so it
gets a new second-granular status row ("1:23:45 · idle 0:42") and grows from
80×56 to 96×72 to fit it. The thumbnail toolbar works for windowed sessions too
but can show only icon state and tooltips, so its new leftmost **status icon**
carries the same data in its tooltip at minute granularity ("Session 1h 23m ·
this PC idle 5m"). Resulting limitations, stated plainly: windowed sessions get
the information only on hover, only to the minute; the tooltip refresh crosses
into `explorer.exe` (`ThumbBarUpdateButtons`) and is therefore throttled to
text changes (≈ once a minute) by a diff in the frame-thread sync, with the
1 s tick living on the existing helper-thread overlay window. The row is blank
(space reserved) when the feature is off, matching the hostname-row convention.

## D-16: Connection quality comes from a second IMsTscAxEvents sink, advised via a CoCreateInstance hook — built, pending live verification

Investigation result: `mstsc.exe` hosts the public MsRdpClient ActiveX control
(`mstscax.dll`) and creates it with `CoCreateInstance` — checked against the
binary: `CoCreateInstance` is imported (from `ole32.dll`, which forwards to
`combase.dll`); no `CoGetClassObject`, `OleCreate`, or ATL `AtlAxCreateControl`
imports; window class `TscShellAxHostClass` confirms an ActiveX host. That is a
real, public hook point: detour `combase!CoCreateInstance` (resolved via
`GetProcAddress` so every in-process caller is seen), and for each
non-aggregated object created, `QueryInterface` for the documented
`IConnectionPointContainer` and `FindConnectionPoint(DIID_IMsTscAxEvents)`. Only
the RDP control answers; the mod then `Advise`s its own minimal `IDispatch` sink
as a *second* subscriber — mstsc's own sink is untouched. Everything used is
public, documented COM; no mstsc/mstscax internals are called or assumed.

DISPIDs are resolved **at runtime** from the registered MSTSCLib type library
(`LoadRegTypeLib` → `GetTypeInfoOfGuid(DIID_IMsTscAxEvents)` → `GetIDsOfNames`),
because a probe compiled with the Windhawk clang against this machine's
mstscax showed `OnNetworkStatusChanged` is DISPID **32** — not the value memory
or older references suggest; a hard-coded guess would have matched nothing and
silently produced a dead indicator. The read values are kept as a logged
fallback if the type library cannot be loaded. Per Microsoft's documentation the
level is 1–4, higher is better (1 = <512 KBps, 2 = 512–1,999, 3 = 2,000–9,999,
4 = ≥10,000 KBps); the documentation gives no unit for `bandwidth`, so the
tooltip shows it as reported, and `rtt` as milliseconds.

Honesty rules built in: the status icon is neutral grey and the tooltip says
"waiting for Remote Desktop's first report" until a real event arrives; if the
control was created before the mod loaded (mod enabled mid-session) the tooltip
says the control is not hooked and to reopen the session; nothing is ever
hard-coded or inferred. `Unadvise` runs only on the sink's home (STA) thread —
the frame subclass does it on the existing teardown message and on the frame's
`WM_DESTROY` (mstsc's one UI thread owns both frame and control); anywhere else
it refuses and leaks rather than make a cross-apartment call. **Unverified
live:** whether mstscax actually fires `OnNetworkStatusChanged` to a second
sink inside mstsc.exe (it fires it to its own sink — the connection bar's signal
bars depend on it — and COM connection points broadcast to all subscribers, but
that has not been watched on a real session). LOOP-007 names the exact log
lines that confirm or refute it.

## D-17: One shared reconnect helper: capture a relaunch command line, launch from the frame's WM_DESTROY

Quick reconnect (button) and force reconnect (watchdog, D-18) share one helper
with two trigger-agnostic steps. **Capture** (`BuildReconnectPlan`): the relaunch
is this process's own command line with any display switches removed (`/f`,
`/multimon`, `/span`, `/w:`, `/h:`) — so `/v:`, an `.rdp` file path, `/admin`,
`/restrictedAdmin`, gateway options and anything else carry over unchanged —
plus `/v:<host>` from the cached window-title hostname when the command line
named no target (mstsc launched bare and the host typed in), plus the display
switches for the settings-backed preferred mode: `/f`, or `/w:W /h:H`
(settings values, or 0 = the session window's current client size at that
moment — the "remembered" size), or `/multimon`. Capture reads only cached
state, the command line, and `GetClientRect` (no cross-thread messaging), so it
also works while the frame thread is hung. **Launch** (`LaunchReconnect`):
`CreateProcessW` of that line, at most once per process behind an atomic
exchange, however many paths race to it.

Sequencing for the clean trigger: the plan is parked as *pending* (60 s TTL),
then the established `WM_CLOSE` (D-4) is posted, and the frame subclass launches
the pending plan on the frame's **`WM_DESTROY`** — the last moment this process
is reliably alive (mstsc exits right after its frame goes), and reached only if
the session really closed; if the operator cancels mstsc's close prompt the plan
simply expires, and every plain Disconnect (overlay, thumb bar, hotkey) drops a
pending plan explicitly so an ordinary later close can never relaunch by
surprise. Rejected alternative: launch first, then close — two concurrent
clients to one host race the server's session takeover and leave a modal error
on the old client. Known judgment call (LOOP-007): `/w:`/`/h:` alone are relied
on to yield a windowed session even when `Default.rdp` says fullscreen; mstsc
has no explicit "windowed" switch.

## D-18: Stuck-session detection runs on its own thread, alerts in its own window, and never reconnects unasked

A dedicated watchdog thread (same queue-first readiness handshake as the helper
and relay threads) polls `IsHungAppWindow` on the RDP frame once a second and
counts consecutive hung seconds against the `stuckThresholdSeconds` setting
(default 10; note `IsHungAppWindow` itself only reports a hang ~5 s after the
thread stopped pumping, so the alert lands about that much later than the
setting). It is independent of `showButton` and runs whenever `stuckDetection`
is on.

The **action surface is a small topmost alert window owned by the watchdog
thread** (top-center of the RDP monitor; "Remote Desktop is not responding
(N s)" / **Force reconnect** / **Dismiss**), not a thumbnail-toolbar button,
because `THBN_CLICKED` is delivered to the frame's own — hung — thread: a thumb
button could never be clicked exactly when it matters. The thumb bar's status
icon and tooltip, and the overlay's status row, still switch to a red warning
state; the thumb bar only catches up once the frame thread is alive to process
the refresh, which is accepted. Per the caution already established around
disconnect-style actions (D-4, D-8, D-12), the mod **never auto-reconnects**:
detection only shows the alert and waits.

Force reconnect reuses D-17's helper with a different termination: capture the
plan → park it → post `WM_CLOSE` → wait a bounded 3 s grace on a detached
one-shot thread (a frame that turns out responsive closes cleanly and relaunches
via the normal `WM_DESTROY` path) → if the frame is still there, launch the
replacement from the watchdog side and `TerminateProcess` this client.
`TerminateProcess`, not `ExitProcess`, because a genuinely hung process may never
run the clean close path and DLL-detach code could wedge on the loader lock. Both
launch paths funnel into the once-per-process guard, so the grace-period race
cannot double-launch. The one-shot thread is drained with a bounded wait in
`Wh_ModUninit`, as in D-12.

## D-19: Thumbnail toolbar slots are all added up front (6 of 7 used) and pushed through one diff-based sync

`ThumbBarAddButtons` is one-shot per taskbar-button lifetime (no add/remove/
reorder afterwards, 7 maximum). v0.4.0 therefore adds every button the mod can
ever show in one call, in a fixed slot order — Status, Minimize, Restore,
Fullscreen toggle, Reconnect, Disconnect (6 of 7; one slot is left for the
future) — and expresses per-feature settings as `THBF_HIDDEN` on individual
slots. Every state push goes through `SyncThumbButtons`, which computes the
desired flags/icon/tooltip of all slots from live state and settings
(`ComputeThumbButtons`, the single source of truth) and issues
`ThumbBarUpdateButtons` only for slots that differ from what the taskbar was
last told. That makes every driver — `WM_SIZE`, the overlay's poll, settings
changes, fullscreen events, status ticks — free to call it without chattering
into `explorer.exe`. Status-icon colors are rendered per tone (neutral, four
quality tones, alert) and cached so HICON handles stay stable for the diff;
the glyph renderer was generalized from two-tone to arbitrary color.

## D-20: Settings-block landmine — an unescaped "word: word" pattern in a value breaks YAML parsing

The client mod's `reconnectDisplayMode` option `windowed: Windowed (/w: and /h:)`
(added in v0.4.0, D-19's settings batch) has the same defect class as
`debugRelayTestMinimize`'s `$name` (quoted for exactly this reason when it was
added): an unquoted YAML scalar value containing a colon immediately followed
by a space is parsed as a nested mapping key, not literal text, and breaks the
settings block. `/w: and` inside the unquoted value is exactly that pattern.
Fixed in v0.4.1 by quoting the value: `windowed: "Windowed (/w: and /h:)"`.

**Rule for this file going forward:** any `$name`, `$description`, or
`$options` value containing a literal `": "` (colon-space) — not just the
field's own key delimiter — must be wrapped in double quotes. This is a real,
repeat-offender landmine in this specific settings block, not a one-off typo:
it has now broken parsing twice from two unrelated settings additions.
`compile-check.ps1` gained a lightweight check for this exact pattern (plus
mixed tab/space indentation) so it is caught by the existing compile-check
step instead of requiring a manual load into Windhawk's settings editor to
discover.

## D-21: Third mod — the taskbar-embedded client widget is its own explorer.exe mod, templated on the host mod

The taskbar-embedded client presentation mode scoped in D-13 is built as a
third, separate Windhawk mod:
`taskbar-integration/client-embedded/rdp-session-toolkit-taskbar-client-embedded.wh.cpp`,
mod id `rdp-session-toolkit-taskbar-client-embedded` (toolkit-prefixed sibling
naming, like `-client` and `-host`), `@include explorer.exe` on the **client**
machine, own version line starting at 0.1.0, own `client-embedded/` subfolder
so `compile-check.ps1` picks it up unchanged. It injects into `Grid#RootGrid`
under `Taskbar.TaskbarFrame` with the host mod's already-adapted, live-run
code (symbol hook, tree walk, Taskbar.View.dll polling, WM_SIZE initial scan,
system-tray margin tracking) copied from this repo rather than re-derived from
the external reference mod; the Right/Left/Center position and offset settings
are the host's.

Visual design: **one compact status icon** (Segoe MDL2 Wifi / Warning glyph in
a flat XAML `Button`, coloured with the same tones as the client mod's
thumb-bar status icon; dimmed, or hidden by setting, while no session is
active), not up to six always-visible elements. Clicking it opens a flyout
with the status lines and the five action rows. The flyout is a **Win32
`WS_POPUP` window painted with GDI**, owned by the mod's own status thread —
not a WUX `Popup`, which does not render inside the taskbar's XAML Island
compositor target (D-7's inherited landmine; not rediscovered). It takes
activation when shown and dismisses on deactivation or Escape; an icon click
within 400 ms of such a dismissal counts as "close", not "reopen". The widget
is presentation only: it reads state (D-22) and sends commands (D-23) and never
touches Remote Desktop itself. Scope is full parity with the thumbnail
toolbar — status display, minimize, restore, fullscreen toggle, reconnect,
disconnect — with the same enable/disable/relabel rules from the session's
iconic/fullscreen state. Single-session only, like the rest of the toolkit.
Known overlap: a machine that is both an RDP client and a session host runs
the host mod's widget and this one in the same `explorer.exe`; both inject at
the same default position and would overlap — different position settings are
the workaround; not solved in code.

## D-22: Status channel — the client mod writes a fixed-layout snapshot to its own storage path; the widget derives that path and treats the record as potentially stale

Client mod v0.5.0 publishes `LocalWidgetStatus` (magic `'RSTS'` = 0x53545352,
version 1, fixed fields: writer pid, session active, session duration, local
idle, quality level / bandwidth / rtt plus "sink advised", hung + hung seconds,
iconic, fullscreen, `writeTick` = `GetTickCount64`, hostname) to
**`<Wh_GetModStoragePath of the client mod>\local-widget-status.dat`** — the
same magic+version+fixed-fields pattern as `PersistedButtonPos`. On this
machine that is
`C:\ProgramData\Windhawk\Engine\ModsWritable\<client mod name>\local-widget-status.dat`.

**No new timer.** The write is driven by the two *existing* 1 s ticks — the
overlay's `STATUS_TIMER_ID` (helper thread) and the watchdog's
`WATCHDOG_TIMER_ID` — through one writer (`WriteLocalWidgetStatus`) that
throttles to one write per ~900 ms with an atomic compare-exchange, so both
ticks may call it freely. Finding during implementation: the overlay's status
timer lives on the overlay window, which exists only in **fullscreen sessions
with `showButton` on** — piggybacking on it alone would have left the new mode
(whose whole point is `showButton` off) with no status at all. The watchdog's
tick covers that whenever `stuckDetection` (default on) is enabled, and keeps
publishing while the frame thread is hung. **Residual gap, stated plainly:**
with both `showButton` and `stuckDetection` off, nothing writes and the widget
shows "no session". A dedicated writer would close it but was not added, per
the no-new-timer instruction. Write discipline: `OPEN_ALWAYS` plus one
`WriteFile` of the whole record at offset 0 — never `CREATE_ALWAYS` — so a
reader can never observe a truncated file. The frame's `WM_DESTROY` forces one
"inactive" record; `Wh_ModUninit` deletes the file.

Widget side: `Wh_GetModStoragePath` is per mod id, so the widget cannot read
"its own" storage path and find the client's file. It derives the client's
directory as a **sibling of its own storage directory** (their common parent is
Windhawk's `ModsWritable`), trying both `rdp-session-toolkit-taskbar-client`
and `local@rdp-session-toolkit-taskbar-client` — Windhawk prefixes mods
created in its own editor with `local@` (this machine's `ModsSource` shows
exactly that for the host mod) — and takes the record with the newest
`writeTick`. Polled every 1000 ms. **Staleness:** a record is treated as **no
session** when `writeTick` is more than 4 s old (1 s interval + 3 s slack) or
lies in the future (a file left from a previous boot), so an uncleanly closed
client can never leave a phantom session on the taskbar. `GetTickCount64` is
the comparison clock because it is system-wide across processes on one
machine, which is the only case here. The widget also reports, in its flyout,
whether the client's command window currently exists.

## D-23: Command channel — a second local message window with a shared secret under HKCU\Software\RDPSessionToolkit; CitadelRdpTaskbarRelay untouched

Per D-13, the client mod gained a **second, entirely separate** message-only
window: class `CitadelRdpTaskbarLocalWidget`, own thread (`LocalWidgetThread`,
same queue-first readiness handshake and `StopLocalWidgetThread` teardown as
the relay / helper / watchdog threads), own `WM_COPYDATA` handler, own
validation function (`ValidateLocalWidgetCommand`). Nothing routes through
`CitadelRdpTaskbarRelay`, and its validation was not extended in any way
(D-24).

Protocol: packed fixed-layout `LocalWidgetCommandPayload` — magic `'RWLC'` =
0x434C5752, version 1, command byte, 3 reserved bytes, 32-byte secret; five
commands 0x01 minimize, 0x02 restore, 0x03 fullscreen toggle, 0x04 reconnect,
0x05 disconnect; 0x00 permanently unassigned. Each routes to the thumbnail
toolbar's own shared action function: `MinimizeRdpFrame`, `RestoreRdpFrame`,
`ToggleFullscreen`, `ReconnectSessionClean`, `DisconnectSession`. Restore had
no shared function (the thumb bar and the overlay each inlined `SW_RESTORE`),
so **`RestoreRdpFrame` was added beside `MinimizeRdpFrame`** and both existing
surfaces now call it — the one touch to pre-existing action code. No mutual
exclusion against the other UI surfaces: the actions check live state and are
idempotent, which is the already-working pattern. The per-feature thumb-bar
settings (`showFullscreenToggle`, `showReconnectButton`) are **not** consulted
for widget commands — they describe thumbnail buttons; the widget is a separate
surface and its rows are explicit operator clicks. The reconnect display-mode
settings do apply, inside `ReconnectSessionClean`.

Sender validation — not an "is explorer.exe" check. **Shared secret:** 32
bytes from `BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG)`, stored as a
`REG_BINARY` value **`LocalWidgetSecret`** under
**`HKEY_CURRENT_USER\Software\RDPSessionToolkit`** — a project-level, per-user
key, deliberately not derived from either mod's `Wh_GetModStoragePath` (those
differ per mod and are not a shared location). Generated once by whichever mod
initializes first (`EnsureLocalWidgetSecret`, identical in both mods;
get-or-create serialized by the session-local named mutex
`Local\RDPSessionToolkit.LocalWidgetSecret` so two simultaneous first starts
cannot mint two different secrets); the other finds it. The receiver re-reads
the value on **every** command, requires exactly 32 `REG_BINARY` bytes,
compares in constant time, and fails closed when the key or value is absent,
malformed, or different; the sender also reads it fresh at send time, so mod
start-up order does not matter. A malformed value (wrong size / type) is
replaced at the next mod start — same-user self-healing, since anyone who can
write HKCU already is the user. This mirrors the relay's HKCU-based discovery
(the CLSID `LocalServer32` registration, D-11) applied to the same-machine
case. The widget sends with `SendMessageTimeoutW(SMTO_ABORTIFHUNG |
SMTO_BLOCK, 2 s)` from its status thread, never the XAML UI thread, so a hung
mstsc cannot wedge explorer.exe.

**UIPI:** no `ChangeWindowMessageFilterEx` on the new window. If mstsc runs
elevated, the medium-integrity widget's `WM_COPYDATA` is blocked by UIPI and
the channel does nothing (the widget logs the send as not handled) — chosen
over letting the message through, because a low-integrity process can read
HKCU and would then hold the only credential the channel checks. The
ROADMAP's elevation-parity item stays open as a conscious trade-off.

## D-24: CitadelRdpTaskbarRelay confirmed unmodified in the 2026-08-21 widget session

Explicit confirmation of the hard boundary: `RELAY_CLASS`, `RelayCommand`,
`RELAY_PLUGIN_CLSID`, `GetRelayPluginExePath`, `GetProcessImagePath`,
`IsAuthorizedRelaySender`, `RelayWndProc`, `RelayThread`,
`StartRelayThread` / `StopRelayThread`, `SendRelayTestMinimize`,
`dvc-plugin/relay/`, and the host mod were not edited. The session diff
(reconstructed against the v0.4.1 source, since the mod files are only
intent-to-add in git) contains no hunk inside the "Toolkit relay receiver" or
"Relay sender validation" sections; the new receiver section is inserted after
`SendRelayTestMinimize` and before the RDP control event sink. The host mod's
`SendMinimizeToClientAsync` → relay plugin → `CitadelRdpTaskbarRelay` pipe
(LOOP-006, proven live) is therefore exactly as it was.

## D-25: Widget visual design corrected to a wide always-visible panel, superseding D-21's icon-and-flyout

D-21's "one compact status icon, click opens a Win32 flyout with five action
rows" visual design is replaced with a wide, always-visible panel: the host
mod's exact XAML injection technique (same `Grid#RootGrid` injection, same
margin-based right-anchoring against the system tray) rendered as six visible
elements — a status element (remote host name plus a quality/duration
indicator, in the same visual role as the host mod's client-name/state
column) and five directly clickable action buttons (Minimize, Restore,
Fullscreen toggle, Reconnect, Disconnect), each enabling, disabling, or
relabeling itself from the same status-file state the retired flyout read.
The Win32 `WS_POPUP` GDI flyout is removed entirely — with every action
element always visible, it has no remaining purpose.

This supersedes D-21's visual design outright: the operator's specification
was to match the host mod's actual presentation, not to introduce a new
icon-and-flyout pattern.

Unchanged, by explicit instruction: the status-file mechanism (D-22, both the
client mod's writer and this mod's `ReadWidgetState`/`ReadStatusFile`), the
local command channel and its shared-secret validation (D-23,
`SendLocalWidgetCommand`/`EnsureLocalWidgetSecret`/`ValidateLocalWidgetCommand`
in the client mod), and the status-thread lifecycle pattern (queue-first
readiness handshake, `StartStatusThread`/`StopStatusThread`). The status
thread's hidden window is retained — it is still the poll timer's owner and
the sender identity for outgoing commands — only the flyout window it used to
also own is gone. Commands now reach it via a single `WM_EMBED_SEND_COMMAND`
thread message (replacing the retired `WM_EMBED_TOGGLE_FLYOUT`/
`WM_EMBED_HIDE_FLYOUT` pair), posted from each button's XAML click handler and
re-checked against live state (`CommandEnabledForState`) before the existing
`SendLocalWidgetCommand` call goes out — preserving D-23's "runs on the status
thread, never the XAML UI thread" rule.

Mod version bumped to 0.2.0. Recompiled clean via `compile-check.ps1`.

## D-26: The two client-side mods are consolidated into one (mstsc.exe + explorer.exe branches), retiring the standalone embedded mod

The taskbar-embedded client widget (`rdp-session-toolkit-taskbar-client-embedded`,
`explorer.exe`, last at v0.2.0 — D-21..D-25) is folded into the client mod
(`rdp-session-toolkit-taskbar-client`, `mstsc.exe`) as a second process branch.
The result is one mod file, mod id `rdp-session-toolkit-taskbar-client`, at
**v0.6.0**, with two `@include` lines (`mstsc.exe`, `explorer.exe`) and one
`@architecture x86-64`. The standalone embedded mod file and its
`client-embedded/` folder are deleted.

**Why one mod, not two.** D-2 committed to "two separate Windhawk mod files,
not one" for the *client vs. host* split — those genuinely run on different
machines / different processes and share no code path. That reasoning does not
apply to the client-side pair: a single Windhawk mod already supports multiple
`@include` targets, and Windhawk compiles the file once and injects the right
branch into each matching process. The embedded widget and the mstsc client are
two ends of the *same* same-machine feature (one writes the status file and
receives commands, the other reads it and sends them), sharing four hand-copied
contracts. Splitting them into two mod ids bought nothing and cost a
manually-synchronized duplication surface — the exact class of bug D-20 warns
about, in data form. This does **not** revisit D-2's client/host split, which
stands: the host mod remains its own mod on the session host.

**Structure.** `Wh_ModInit` resolves the host process once
(`GetModuleFileNameW(nullptr, …)` compared case-insensitively against
`mstsc.exe` / `explorer.exe`) into a `HostProcess` global, then dispatches to
`client::ModInit` or `embedded::ModInit`; `Wh_ModSettingsChanged` /
`Wh_ModUninit` dispatch the same way. Each branch's machinery (hooks, threads,
windows) is started only in its own process, so a function written for one
branch can never run in the other. A load into neither process logs plainly and
returns `TRUE` as an inert no-op. The WinRT-heavy explorer code lives in
`namespace embedded` (which alone carries the `using namespace winrt…`
directives); the Win32 mstsc code lives in `namespace client`; the four shared
contracts sit above both. The mstsc branch is the proven v0.5.0 code moved
verbatim into `namespace client`; the explorer branch is the v0.2.0 widget
moved verbatim into `namespace embedded`.

**Shared contracts unified (was D-22/D-23's two copies).** The status-file
struct + magic/version/filename (`LocalWidgetStatus`,
`kLocalWidgetStatus*`), the command channel's window class + registry-secret
location + payload + command bytes (`LOCAL_WIDGET_CLASS`, `LOCAL_WIDGET_REG_*`,
`LocalWidgetCommandPayload`, `LocalWidgetCommand`), and the identical helpers
(`ReadLocalWidgetSecret`, `EnsureLocalWidgetSecret`, `QualityLabel`,
`FormatClock`, `GetLocalWidgetStatusFilePath`) are now **single definitions**
used by both branches, not two hand-synchronized copies. `dvc-plugin/relay`
keeps its own copy of the *relay* channel's identifiers per D-10 — that is a
genuinely separate build system (out-of-process COM, its own toolchain) and is
deliberately untouched here.

**The status-file path-guessing bug is structurally eliminated (pending live
confirmation).** D-22 had the widget derive the client mod's storage directory
as a *sibling* of its own `Wh_GetModStoragePath` — trying both
`rdp-session-toolkit-taskbar-client` and `local@rdp-session-toolkit-taskbar-client`,
since Windhawk prefixes editor-created mods with `local@`. That guess was the
prime suspect for LOOP-008's "widget never goes active" risk. Now that both
branches are the **same mod id**, `Wh_GetModStoragePath` returns the identical
directory in both processes, so the explorer branch reads the very file the
mstsc branch writes — no sibling search, no `local@` guessing. The
`ResolveModsWritableDir` / `kClientModStorageNames` logic is removed entirely.
This *should* fix the "no active session shown" symptom, but that is a
structural expectation, not an observed result: it still needs a live pass
under the merged architecture (the standalone mod was never confirmed working
live either). Watched in OPEN_LOOPS LOOP-008.

**Settings.** The two settings blocks are merged. To keep it unambiguous which
surface each setting drives, the five embedded-widget settings are renamed with
an `embedded` prefix (`embeddedWidgetPosition`, `embeddedOffsetX`,
`embeddedPanelWidth`, `embeddedFontSize`, `embeddedShowWhenNoSession`) and the
mstsc overlay's `buttonPosition` `$name` now reads "Overlay button position
(mstsc.exe)". The merged block was validated with `compile-check.ps1`'s
`Test-ModSettingsYaml` (D-20's landmine check) — a settings-block merge is
exactly the change most likely to reintroduce that failure class — and passes.

**Not committed loss.** The retired embedded mod was never committed (intent-to-add
only), and every line of it now lives in `namespace embedded`, so deleting the
file removes nothing of value.

## D-27: Fullscreen toggle from the taskbar-embedded panel — explorer.exe grants mstsc the foreground right per click

**Bug.** The panel's Fullscreen/windowed button sent `LWCMD_FULLSCREEN_TOGGLE`
to the mstsc branch, whose `ToggleFullscreen` must first make the RDP frame the
foreground window (D-14's safety rule: no foreground, no Ctrl+Alt+Break). Live
tests logged `foreground not acquired … Ctrl+Alt+Break NOT sent`.

**Root cause.** `SetForegroundWindow` only succeeds for a process that currently
holds the foreground right, and `AllowSetForegroundWindow(pid)` lets a process
that *does* hold it pass it on — but the grantor must hold it at that moment.
The click on the panel is input delivered to **explorer.exe**, so explorer holds
the right when it handles the click; mstsc, a background process that merely
received a `WM_COPYDATA`, does not. Nothing ever performed the grant.

**Fix (v0.7.0).** In the explorer branch's command handler (`OnSendCommand`, on
the status thread, right before `SendLocalWidgetCommand`), for
`LWCMD_FULLSCREEN_TOGGLE` **only**, `GrantForegroundRightToReceiver` locates the
`CitadelRdpTaskbarLocalWidget` window, resolves its owning pid with
`GetWindowThreadProcessId` (the same way the mstsc-side sender validation
resolves pids from `WM_COPYDATA`'s wParam), and calls
`AllowSetForegroundWindow(pid)`, logging the result (an `ERROR_ACCESS_DENIED`
means explorer no longer held the right — the mstsc side then falls back as
before). Minimize, Restore, Reconnect, and Disconnect need no foreground right
and are deliberately not granted one — the grant is the narrowest thing that
fixes the bug. The mstsc side (`BringFrameToForeground` / `ToggleFullscreen`) is
untouched: its plain `SetForegroundWindow` is now expected to succeed on this
path, and the `AttachThreadInput` fallback remains as the safety net for the
thumbnail-toolbar path and anything else. Pending live confirmation (LOOP-007 b).

## D-28: Reconnect and Force reconnect are one opt-in setting (off by default), and a relaunch re-checks its .rdp file at launch time

**Gating.** `showReconnectButton` (default true) only controlled the thumb bar's
Reconnect slot; the local widget's `LWCMD_RECONNECT` and the stuck-session
alert's Force reconnect shared the relaunch helper (D-17) but were not gated.
Renamed to **`enableReconnect`**, default **false**, and widened to every
relaunch surface: the thumb bar hides its slot; the mstsc receiver refuses
`LWCMD_RECONNECT` (and the explorer panel — same mod id, same setting —
collapses its Reconnect button, so the two agree); and the stuck-session alert
still appears and still says "Remote Desktop is not responding (N s)" but its
action row offers **Dismiss only** (full width) — `ForceReconnectSession`
additionally refuses on the setting, as defense in depth. The status-icon
tooltip drops its "use the on-screen alert to force reconnect" hint when the
setting is off. No backward-compatibility shim for the old key: this repo has no
other users. Why off by default: relaunching a connection is the one action
here that can do something unexpected (below), so it is opt-in.

**Pre-launch safety check.** `BuildReconnectPlan` now records the bare
connection-file argument (the `.rdp` path mstsc was started with), and
`LaunchReconnect` — the single launch point for both the WM_DESTROY path and
Force reconnect — checks with `GetFileAttributesW` that the file still exists
(and is not a directory) **at the moment of the actual launch**, not just when
the plan was built. A missing file is logged plainly ("NOT launched: the
connection file the plan references no longer exists at launch time") and
treated exactly like a failed `CreateProcessW`: `false` to the caller and the
once-per-process guard handed back. This targets the failure already observed:
an external launcher started mstsc on a temp `.rdp` file it had deleted or
regenerated by the time the relaunch fired, so the replacement client opened on
nothing. Relative paths resolve against the process's current directory, which
`CreateProcessW` inherits, so the check and the launch see the same file. The
plan's log line now also reports whether the file existed at build time.

**Judgment call — Force reconnect when the relaunch is refused.** The operator
clicked "Force reconnect" on a *hung* client; ending it is half of what was
asked, and the half that does not depend on the file. `ForceReconnectThread`
therefore still terminates the hung process when `LaunchReconnect` fails, but
now logs explicitly that the replacement was NOT launched and must be reopened
by hand — a reconnect is never implied. Reversible if the operator prefers
"refuse to terminate too".

**Readme left alone, by instruction.** The `==WindhawkModReadme==` block still
describes the alert as offering Force reconnect unconditionally and the
Reconnect button as present; the task's instruction not to change that block was
followed, so those two paragraphs are now slightly stale — flagged for the
operator rather than edited.

## D-29: Connection-quality sink — the CoCreateInstance hook watched the wrong door; mstsc creates the control through mstscax's own class factory (cause identified from the binary; fix built; diagnostics added; live confirmation pending)

**Problem.** Across every live test no `RdpEvents` line ever appeared, so it was
unknown whether D-16's `CoCreateInstance` hook fired at all, fired and failed to
advise, or something else.

**Investigation (this machine: mstsc.exe / mstscax.dll 10.0.26100.8875, Windows
11 25H2 build 26200).** The hook *installation* is sound: `ole32!CoCreateInstance`
— which mstsc reaches only as a **delay-load** import — is a forwarder to
`api-ms-win-core-com-l1-1-0`, i.e. `combase!CoCreateInstance`, exactly the
function the mod hooks. The *assumption* behind it is not: parsing mstsc.exe's
import and delay-import tables shows it imports **nothing from mstscax.dll**,
while mstscax.dll exports only `DllGetClassObject`, `DllGetTscCtlVer`,
`DllCanUnloadNow`, `Dll(Un)RegisterServer`. mstsc.exe's read-only data holds
the UTF-16 string `"mstscax.dll"` (file offset `0x11EFD8`) immediately followed
by the ASCII string `"DllGetClassObject"` (`0x11EFF0`) — a name that is not an
import of mstsc.exe and therefore exists only to be passed to `GetProcAddress`
— with the two MsRdpClient CLSIDs mstsc embeds ("version 12"
`{1DF7C823-B2D4-4B54-975A-F2AC5D7CF8B8}` and "version 2"
`{7CACBD7B-0D99-468F-AC33-22E495C0AFE5}`) a few hundred bytes away in the same
region. That is the hand-load-the-control-DLL pattern: `LoadLibrary`,
`GetProcAddress("DllGetClassObject")`, `IClassFactory::CreateInstance`. An
object created that way **never passes through `CoCreateInstance`**, which is a
specific, concrete reason the sink could never be advised. (What is *not*
proven from static inspection alone: that mstsc never *also* CoCreateInstances
the control on some path — the diagnostics below settle that live.)

**Fix (v0.7.0).** `InstallMstscaxFactoryHooks`, called from `Wh_ModInit`:
loads `mstscax.dll` (mstsc loads the same System32 module moments later; the
refcount goes up — the reference is intentionally never released, as mstsc
keeps the DLL for the process lifetime), hooks its `DllGetClassObject` export,
then calls it once per embedded control CLSID to obtain the class factory,
reads `CreateInstance` from the factory's vtable (slot 3), releases the factory
(no control is created), and hooks each distinct `CreateInstance`
implementation (two slots; normally one shared). The `CreateInstance` hook
offers every object the factory produces to the existing `TryAdviseRdpControl`
— the same sink, the same documented `IConnectionPointContainer` advise, reached
at the control's real birthplace. All hooks are set in `Wh_ModInit` and applied
by Windhawk with the rest: no `Wh_ApplyHookOperations` at DLL-load time, no
work under the loader lock, and — the reason a `GetProcAddress`-hook-returning-
a-detour design was rejected — no pointer into this mod handed to mstsc that
would dangle if the mod were disabled while mstsc keeps running. The
`CoCreateInstance` hook stays as the second watched path.

**Diagnostics (regardless of outcome).** Logged: `Wh_SetFunctionHook`'s result
and target for the CoCreateInstance, DllGetClassObject, and factory
CreateInstance hooks; whether mstscax.dll was already loaded at init (then a
pre-existing control cannot be observed — reopen the session); the first call
into the CoCreateInstance hook and the first 40 calls individually (CLSID,
context, HRESULT); every `DllGetClassObject` call (CLSID, riid, HRESULT, and
whether the returned factory's `CreateInstance` is the hooked one — a WARNING if
not); every factory `CreateInstance` (riid, HRESULT); and, per candidate object,
the implementing module (from its vtable), the `QueryInterface(IConnectionPoint-
Container)` HRESULT, the `FindConnectionPoint(IMsTscAxEvents)` HRESULT, and the
`Advise` HRESULT. A summary line (`[diag @ frame created]`, `[diag @ 10 s into
session]`) reports all counters, sink state, and mstscax presence at two fixed
moments so the next live test produces evidence even if nothing else fires.

**Honest status.** A specific cause was identified from binary inspection and a
targeted fix was built on it; this is more than diagnostics-only. It is **not**
live-confirmed: the proof is `RdpEvents: mstscax factory CreateInstance #1 …`
followed by `… FindConnectionPoint(IMsTscAxEvents) hr=0x00000000 — this object
IS the RDP control` and `advised IMsTscAxEvents sink` in the next live test
(LOOP-007 a). If the `[diag @ 10 s]` line shows zero factory calls, the WARNING
variant of the DllGetClassObject line, or a non-zero `FindConnectionPoint`
HRESULT, the log now says exactly where the path stopped.

## D-30: Overlay and thumbnail toolbar are two independent visibility settings (overlay off by default, thumbbar on)

**Date:** 2026-08-22. Client mod v0.8.0.

The single `showButton` setting gated both mstsc-side surfaces at once — the
floating overlay button and the taskbar thumbnail toolbar — so you could not
have the (less intrusive) thumbnail toolbar without also getting the on-screen
overlay, or vice versa. Split into two independent settings:

- **`showThumbbar`** (default **true**) — the taskbar thumbnail toolbar only.
- **`showOverlay`** (default **false**) — the floating overlay button only.

The new defaults deliberately flip the effective behaviour: the toolkit now
shows the unobtrusive thumbnail toolbar out of the box and leaves the overlay
opt-in, where the old combined setting defaulted the pair on together.

**Wiring.** Each surface's code paths are driven by its own flag, nothing
shared:

- The helper thread (which owns the floating overlay window) starts/stops on
  `g_showOverlay` only — in `Wh_ModInit`, in `Wh_ModSettingsChanged` (recycled
  on any change of that flag), and its `WM_CREATE_BTN` / `WM_HIDE_BTN` /
  `WM_REPAINT_BTN` posts (BBar detect, monitor-change reposition, hostname
  repaint, BBar teardown) are all `g_showOverlay`-gated.
- `SyncThumbButtonsEx` visibility, the `TaskbarButtonCreated` create, and the
  `g_msgThumbRefresh` handler use `g_showThumbbar` only. Thumbbar transitions
  ride the existing `g_msgThumbRefresh` post (its handler reads the flag live,
  so it covers both on→off and off→on without a thread restart).
- The RDP-frame subclass is installed when **any** of hide / overlay / thumbbar
  is on (`g_hideBar || g_showOverlay || g_showThumbbar`); the BBar subclass
  when hide or overlay is on (`g_hideBar || g_showOverlay`) — the thumbbar does
  not need the BBar.

**Status display preserved (D-15).** The session-duration / idle / quality /
stuck-status display still appears on both surfaces regardless of which is
enabled — a row on the overlay, a tooltip on the thumbbar's status icon. This
was never special-cased: each surface's own status rendering already runs only
when that surface is drawn (the overlay row on the helper thread, the thumb
tooltip inside the `visible`-gated sync), so it simply follows the surface's own
new flag now instead of the shared one. The watchdog's status-snapshot write
(D-22) remains independent of both flags, as before.

Readme and setting descriptions updated to describe the two surfaces as
independently togglable.

## D-31: Fullscreen-toggle foreground — a no-current-foreground-window direct path, distinct from the attach-to-another-thread fallback

**Date:** 2026-08-22. Client mod v0.8.0. Follows the earlier
`AllowSetForegroundWindow` grant (D-27), which did **not** fix the symptom.

**Symptom (from the prior session's evidence).** For the taskbar-embedded
panel's fullscreen toggle, the explorer.exe branch's `AllowSetForegroundWindow`
grant is confirmed happening (logged as granted), yet `BringFrameToForeground`
still fails identically every time, with `GetForegroundWindow` returning a flat
**null** — not merely a different window.

**Hypothesis checked.** `BringFrameToForeground`'s AttachThreadInput fallback is
guarded by "a foreground thread ID was found" (`fgThread != 0`). When
`GetForegroundWindow()` is null, `fgThread` is 0 and the fallback is skipped
entirely. But per Microsoft's documented `SetForegroundWindow` success criteria,
"there is no foreground window, and the foreground process is not being
debugged" is itself a condition under which the call succeeds unconditionally —
no foreground rights required. That case was being folded into (and thus lost
to) the attach-to-another-thread fallback, which structurally assumes another
foreground window exists to attach to.

**Diagnostics added (regardless of outcome).** `BringFrameToForeground` now logs,
at each step: `GetForegroundWindow` immediately **before** the call; the **BOOL
return** of the first `SetForegroundWindow`; whether the AttachThreadInput
fallback is ENTERED or SKIPPED (with `fg` / `fgThread` / `me`); and, when
entered, the return values of `AttachThreadInput` and the second
`SetForegroundWindow`; plus a final `IsFrameForeground` + `GetForegroundWindow`
line.

**Fix applied — conservative, non-regressing.** When `GetForegroundWindow()` was
null before the call (the confirmed symptom), the no-foreground case is handled
as its own direct path: it **trusts the BOOL** `SetForegroundWindow` returns
rather than re-checking `IsFrameForeground` — whose `GetForegroundWindow` read is
the very thing that is unreliable inside an RDP session's own desktop. A `TRUE`
return is treated as success (the chord is then sent); a `FALSE` return falls
through to the existing path unchanged. This cannot regress the current
behaviour: that scenario is 100% broken today, and a `FALSE` return leaves the
old flow intact.

**Honest status.** The hypothesis's **precondition** (a flat-null foreground) is
confirmed by the prior session's evidence, and the fix follows documented
`SetForegroundWindow` semantics. What is **not** yet confirmed in this session is
the runtime **BOOL** of that first `SetForegroundWindow` in the live RDP session
— this mod cannot be executed here (it injects into `mstsc.exe`). The fix is
therefore applied guarded on that BOOL, and the full diagnostics are retained so
the next live toggle proves it: if the log shows `foreground-before=0000...` and
`SetForegroundWindow=1` followed by the chord being sent, the hypothesis is
confirmed and the path fixed; if it shows `SetForegroundWindow=0` with a null
foreground (or a non-null foreground that IsFrameForeground still rejects), the
cause lies elsewhere (an RDP desktop / window-station specific), the fix
harmlessly no-ops, and the log says exactly where the path stopped — the same
diagnostics-first standard applied to D-29.

## D-32: compile-check.ps1 links a real DLL per mod, not just a -c compile

**Date:** 2026-08-22.

`compile-check.ps1` compiled each `*.wh.cpp` with `-c` only and never linked, so
a link-time defect could pass every "clean" report undetected. This is not
hypothetical: `SecureZeroMemory` expands under mingw-w64 to the ntdll import
`RtlSecureZeroMemory`, which the documented mod flags do not resolve — a defect
found and fixed by hand in the client, invisible to a compile-only check (see
WORK_LOG, 2026-08 client build notes).

The script now performs a full link step for **every** mod file it checks, as
part of its normal pass:

- A new `Get-ModCompilerOptions` parses that mod's own `// @compilerOptions`
  line from its `==WindhawkMod==` metadata header (scanning only the header) and
  splits it into the `-l...` library arguments.
- After the `-c` compile succeeds, the object is linked with
  `-target x86_64-w64-mingw32 -shared` **plus that mod's own
  @compilerOptions**, producing the actual `.dll`. `WH_EDITING` keeps the `Wh_*`
  API self-contained, so no Windhawk engine import library is needed to link.
- A link failure trips the same `if ($LASTEXITCODE -ne 0) { throw }` reporting a
  compile failure uses (`"<mod> failed to link"`).

Verified: both mods (client v0.8.0 and host v0.2.0) compile **and link** clean
through the updated script (exit 0); and an isolated source with an undefined
symbol compiles `rc=0` but links `exit 1`, confirming the new step catches
exactly the class of defect a compile-only check missed.

## D-33: the taskbar thumbnail toolbar is removed; its rich status tooltip moves to the taskbar-embedded panel

**Date:** 2026-09-01. **Client mod v0.8.0 → v0.9.0.**

### What was removed

The whole mstsc.exe-side **taskbar thumbnail toolbar** — the buttons under the
mstsc taskbar hover preview, added in the upstream mod at v1.1.9 and carried
through the fork ever since. Concretely: the `ITaskbarList3` usage
(`CLSID_TaskbarList` / `IID_ITaskbarList3` literals, the `CoInitializeEx` /
`CoUninitialize` balance, `ThumbBarAddButtons` / `ThumbBarUpdateButtons`), the
`ThumbSlot` / `THUMB_ID_*` slot model, the Segoe MDL2 glyph-to-`HICON`
rendering (`CreateGlyphIconColor` and its coverage-as-alpha conversion) and the
`StatusTone` colour table it fed, `IsTaskbarLightTheme`, the diff-based
`SyncThumbButtons` / `ComputeThumbButtons` / `CreateOrRefreshThumbBar` /
`TeardownThumbBar` machinery, the `TaskbarButtonCreated` / `ThumbRefresh` /
`StatusRefresh` registered messages and their `FrameSubclassProc` handlers
(including the `WM_SIZE` re-sync and the `THBN_CLICKED` routing), the
`ChangeWindowMessageFilterEx` UIPI allowance for `TaskbarButtonCreated`, the
`showThumbbar` setting, and `FormatStatusTooltip`.

### Why

The taskbar-embedded panel (D-21/D-26) offers **the same five actions** —
Minimize, Restore, fullscreen toggle, Reconnect, Disconnect — with the **same
enable / disable / relabel rules** driven by the same session state, always
visible rather than hover-only, and covering windowed sessions equally. The
thumbnail toolbar was therefore a second copy of one UI: two surfaces to keep
behaviourally in sync, two sets of state pushes, two places to change for every
future action. Reducing that to one is the point of this change. The toolbar
also carried real cost the panel does not: an Apartment-model COM pointer
pinned to mstsc's UI thread, a one-shot `ThumbBarAddButtons` API whose "buttons
can never be added, removed or reordered" constraint forced the
add-everything-then-hide design, hand-rendered icons, and an explorer-restart
re-establishment path. None of it was ever confirmed working live (LOOP-001,
LOOP-007).

### What was deliberately preserved

- **Every shared action function is untouched**: `MinimizeRdpFrame`,
  `RestoreRdpFrame`, `ToggleFullscreen`, the reconnect helpers
  (`BuildReconnectPlan` / `LaunchReconnect` / `ReconnectSessionClean` /
  `LaunchPendingReconnect` / `ForceReconnectSession`) and `DisconnectSession`.
  The floating overlay and the local-widget command receiver call these
  directly and behave exactly as before.
- **`UnadviseRdpEvents` still fires on both of its paths.** This was the one
  real trap in the removal. The connection-quality sink teardown was invoked
  *alongside* `TeardownThumbBar` in two places: the frame's `WM_DESTROY`
  handler, and the `g_msgThumbTeardown` handler that `Wh_ModUninit` reached
  with a synchronous `SendMessageTimeoutW`. Deleting the thumb-bar teardown
  would have silently taken the mod-unload path's Unadvise with it — the sink
  is an STA object advised on mstsc's UI thread, and `UnadviseRdpEvents`
  refuses to run anywhere else, so no other thread could have covered for it.
  The message is therefore **kept and renamed** to `g_msgSinkTeardown`
  (`WH_RdpstkClient_SinkTeardown`), its handler now calls `UnadviseRdpEvents`
  alone, and `Wh_ModUninit` sends that message with the same timeout and the
  same "a hung frame thread only costs the timeout" reasoning. `WM_DESTROY`
  keeps its call unchanged. The frame subclass survives purely for these two
  jobs, plus the reconnect launch point and the final status write.
- **The connection-quality event sink itself**, the stuck-session watchdog, the
  relay receiver, the local-widget command receiver, the status-snapshot
  writer, and the floating overlay — all unchanged in behaviour.

### The tooltip migration (the detail that would otherwise have been lost)

`FormatStatusTooltip` built the thumb-bar status icon's tooltip — session
duration, this PC's local input idle time, connection quality with bandwidth
and round-trip time, and the not-responding warning. It could not be moved: it
read mstsc-side globals (`g_sessionStartTick`, `GetLastInputInfo`, the `g_net*`
atomics, `g_sessionHung` / `g_hungSeconds`) that do not exist in explorer.exe.

Every one of those values is, however, already published in the
`LocalWidgetStatus` snapshot the panel reads once a second. So an equivalent
`FormatEmbeddedStatusTooltip` was **built in the explorer.exe branch**,
reconstructing the same lines from `sessionDurationMs`, `localIdleMs`,
`quality`, `bandwidth`, `rtt`, `qualityAvailable`, `hung` and `hungSeconds`,
with the same gating and the same never-invent-a-value discipline (D-16). It is
attached to the panel's host-name / status-text column and refreshed in place
on every poll — the existing `ToolTip`'s `TextBlock` is reused, so no XAML
objects are built per tick and a tooltip the user has open is not disturbed.
One line the original had no need for was added: this surface is alive while no
session is, so a missing or stale snapshot now gets an honest explanation
instead of an empty tooltip.

`FormatCoarse` ("1h 23m") moved up to the shared-contracts section for it; the
explorer branch is now its only caller.

### Three settings changed owner rather than being deleted

`showConnectionQuality`, `showSessionInfo` (its tooltip half) and
`showFullscreenToggle` gated thumb-bar presentation. Deleting the toolbar would
have left them gating nothing. They are now read by the **explorer.exe
branch**, which is the surface that presents all three — the same
one-setting-read-by-the-owning-branch pattern `enableReconnect` already used
(D-28). `showSessionInfo` is read in both branches: the mstsc branch still uses
it for the floating overlay's own status row. `showFullscreenToggle` now
collapses the panel's fullscreen button, exactly as `enableReconnect` collapses
its Reconnect button. `showThumbbar` itself is gone.

### Action-button tooltips

Each of the panel's five buttons gained a short plain-language tooltip saying
what the click actually does to the session — that Minimize leaves the session
connected and only clears the screen, that Disconnect leaves your programs
running on the remote machine, that the fullscreen toggle sends Remote
Desktop's own Ctrl+Alt+Break rather than reimplementing anything — instead of
restating the button's own name. The fullscreen tooltip is rewritten together
with the label on every state change, so it never describes the wrong
direction.

### Verification

`compile-check.ps1` passes end to end: compile, **real link** to a DLL with the
mod's own `@compilerOptions` (D-32), and the settings-block YAML validator
(D-20) — which matters here because the `showThumbbar` entry was removed from
that block and six `$description` values were rewritten.

**No `@compilerOptions` entry became unused**, and this was checked rather than
assumed. Two ways: dropping each `-l…` in turn from the link of the new object
gives exactly the same required/droppable split as dropping it from a link of a
pre-change object built from `HEAD`; and the DLL-name strings in the two linked
binaries are identical (`advapi32`, `bcrypt`, `gdi32`, `ole32`, `oleaut32`,
`shcore`, `user32`, and the WinRT api-sets). Nothing this change removed was
the last user of any library — `ole32` / `oleaut32` are still reached by the
event sink's `CoCreateInstance` hook, `StringFromGUID2`, `LoadRegTypeLib` and
the `VARIANT` helpers; `advapi32` by the shared-secret registry access; `gdi32`
by the overlay and alert painting. `@compilerOptions` is therefore left exactly
as it was. The one header that did become unused, `<shobjidl.h>`
(`ITaskbarList3`, `THUMBBUTTON`), and the local `THBN_CLICKED` fallback
definition were both removed; the build confirms nothing else needed them. The
only compiler warning is the pre-existing unused `GetRdpMonitorRect`, present
at `HEAD` too.

Not live-tested — this mod injects into `mstsc.exe` and `explorer.exe` and
cannot be executed here. See LOOP-007 / LOOP-008.

## D-34: LocalWidget thread gains an unconditional 1 s status tick, closing the D-22 gap that D-33 made serious

**Date:** 2026-09-01. **Client mod v0.9.1 → v0.9.2.**

D-22 (client mod v0.5.0) accepted, in writing, that with both `showOverlay`
and `stuckDetection` off nothing drives `WriteLocalWidgetStatus` and the
taskbar-embedded panel is left showing "no session" — a residual gap kept
deliberately, in service of a no-new-timer instruction, because the two
*existing* 1 s ticks (the overlay's `STATUS_TIMER_ID` and the watchdog's
`WATCHDOG_TIMER_ID`) were judged to cover the common cases well enough.

That trade stopped being acceptable once D-33 removed the mstsc-side taskbar
thumbnail toolbar. Before D-33 the thumbnail toolbar was a second
full-featured client surface — a session with both settings off still had
*some* live client UI on the taskbar hover preview. After D-33 the
taskbar-embedded panel is the **only** full-featured client surface (D-33's
own words: "the panel is now the toolkit's only full-featured client
surface"). A panel that silently goes blank because of an settings
combination that has nothing to do with the panel itself is a materially
bigger problem now than when D-22 first weighed the trade-off — there is no
fallback surface left to catch the user.

**Fix.** `LocalWidgetThread`'s message-only window (`CitadelRdpTaskbarLocalWidget`,
D-13/D-23) already runs for the mod's entire lifetime, independent of every
settings gate — it is started unconditionally in `Wh_ModInit` alongside
`StartRelayThread()`, specifically so toolkit components can always reach it.
That makes it the natural home for the missing driver: a third 1 s
`SetTimer` (`LOCAL_WIDGET_STATUS_TIMER_ID`, alongside the window's existing
`WM_COPYDATA` handling) that calls `WriteLocalWidgetStatus()` on every tick,
unconditionally — it does not read `g_showOverlay` or `g_stuckDetection` at
all. `RelayThread`'s message-only window was the other candidate (it is
equally always-alive), but it is the DVC relay command receiver, a channel
with no existing tie to widget status, whereas the LocalWidget receiver
already exists to serve the panel — adding the tick there keeps "what feeds
the panel's snapshot" in one place.

The two existing drivers are untouched: the overlay's status timer and the
watchdog's poll still call `WriteLocalWidgetStatus()` exactly as before.
`WriteLocalWidgetStatus`'s own throttle (an atomic compare-exchange, ~900 ms
minimum interval — see D-22) already makes any number of concurrent callers
safe; going from two drivers to three needed no change there. The new timer
is started with `SetTimer` right after the window is created in
`LocalWidgetThread`, and torn down with `KillTimer` in the window's own
`WM_DESTROY` handler — the same start/stop placement every other timer in
this file uses (`FADE_TIMER_ID`/`ICONIC_TIMER_ID`/`STATUS_TIMER_ID` on the
overlay window, `WATCHDOG_TIMER_ID` on the alert window).

**Verification.** `compile-check.ps1` passes end to end: compile and the
real link step (D-32) to a fresh DLL with the mod's own `@compilerOptions`.
Not live-tested — see LOOP-008, whose D-22 residual note this closes.
