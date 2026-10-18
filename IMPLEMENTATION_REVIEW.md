# Diagnostics and session reliability implementation review

The review covered the new client/host implementation, shared protocol and
target discovery, relay COM lifetime and transport, local diagnostics, candidate
packaging and their prerequisite registration code. Corrections are included in
client **0.11.1**, host **0.5.1** and relay **0.3.1**, retaining protocol v3.
The installed runtime was left untouched.

## Findings and corrections

| Area | Finding | Correction and evidence |
|---|---|---|
| Client lifetime — high | A failed COM `Unadvise` cleared ownership and the unload safety signal even though the connection point might still own the sink. | Retain references and the advised flag on failure. Coordinate new advice with shutdown; keep code mapped if advice or teardown cannot finish. Extracted production teardown tests cover failure, wrong apartment, success and repetition. |
| Targeting — high | The older frame fallback searched all processes by window class, beneath the new process guard. | Fallback enumeration is restricted to the current process and requires exactly one frame; cached handles require matching PID and class. Disconnect uses that resolver too. Real window/process fixtures cover foreign and multiple frames. |
| Relay lifetime — high | `OnDataReceived` held its state mutex across COM `Write`; a reentrant `OnClose` could deadlock. | Retain a temporary channel reference and release state/request locks before `Write`. A fake channel synchronously closes during the actual callback implementation. |
| Transport compatibility — medium | The host read buffer was smaller than Microsoft's documented DVC minimum, and parsing skipped eight bytes without validating the PDU header. | Use `CHANNEL_PDU_LENGTH`; validate declared payload length and FIRST/LAST flags. A successful empty read is a timeout, rather than an incompatible reply. Parser checks cover malformed headers and unrelated flags. |
| Feedback — medium | Partial `SendInput` could leave modifiers down and be reported as rejection. Zero WM_COPYDATA results also lost uncertainty. | Refuse injection while user modifiers are held; release the keys inserted by a partial shortcut; report an unknown outcome without retry. Preserve zero as unknown, reject oversized results. Tests execute the extracted production input function with inert input fixtures. |
| Frame ambiguity — medium | A unique mstsc process could still contain multiple top-level RDP frames. | Both the native guard and diagnostics detect this case. Independent process/window fixtures cover one and multiple frames. |
| Clipboard recovery — medium | Failed termination or an exit timeout could be treated as successful recovery when the original helper remained present. An inaccessible candidate could be mistaken for absence. | Abort unconfirmed recovery, avoid duplicate launch, retain uncertainty on failed inspection, and release scan memory through ownership. Tests execute the production restart function with inert process fixtures. |
| Startup and shutdown — medium | Client taskbar hook initialization failure left its status thread alive. Failed readiness setup and subclass restoration also lacked safe handling. | Join/close startup resources on failure; do not start a worker without its readiness event; preserve code and original procedure state after restoration failure. A retained module refuses reinitialization until its process restarts. Compile/link checks cover both branches; real lifecycle acceptance remains required. |
| Taskbar backlog — medium | A stalled dispatcher could accumulate unlimited delegates, with a separate unload wait for every item. Cleanup scheduling exceptions could leave live handlers without retained code. | Bound normal pending delegates, make delayed updates inert during shutdown, use one drain deadline and retain code if cleanup cannot be scheduled. Existing scan exception handling resets its admission latch. Explorer behavior still requires live acceptance. |
| Snapshot handling — medium | Structurally sized snapshots could contain invalid BOOLs or missing identity. Predictable temporary files were opened with truncation. | Validate identity and canonical state fields in all readers. Use per-process/thread sequence names and exclusive new-file creation so an occupied temporary path is preserved. Native and PowerShell regressions cover invalid records and temporary-file collisions. |
| Diagnostics — medium | Suppressed process enumeration failures, malformed storage paths or unreadable source files could hide uncertainty or abort independent collection. | Report incomplete process/receiver/source inspection; contain malformed storage and source errors; retain other observations. Tests cover real collection with an isolated malformed registry-value provider. |
| PowerShell compatibility — medium | Windows PowerShell 5.1 evaluated the toolkit-root parameter default before `PSScriptRoot` was available; standalone diagnostics failed even though dot-sourced tests passed. | Resolve the default after parameter binding. A fresh Windows PowerShell 5.1 process runs the real command and validates JSON/privacy. |
| Observation ordering — low | A local sample arriving during a command send could predate the feedback timestamp; host comparison used an older UI sample. | Timestamp local feedback before sending and use the relay's pre-action sample as the host baseline. Disconnect/reconnect report acceptance only when posting the close request succeeds. |

## Validation and remaining acceptance

The exact runs, counts and replacement candidate artifact are recorded in
[WORK_LOG.md](.raiden/state/WORK_LOG.md). The expanded
[runtime regressions](probes/runtime/README.md) compile the actual shared code
and extracted production functions; they do not simulate success by checking
source strings. Existing registration, path-reader and prerequisite regressions
remain part of the check set. Both standalone mods are compiled and linked,
including their settings checks; the relay is built separately.

These checks do not load Windhawk mods, exercise Explorer dispatchers or open
an RDP connection. Before home use, follow
[TOP_THREE_ACCEPTANCE.md](TOP_THREE_ACCEPTANCE.md), especially matched-version
controls, a second connection dialog, foreground/input behavior, clipboard
recovery and disable/re-enable during work. A log that says code was retained
requires restarting the affected mstsc or Explorer process before re-enabling.
OS channel-open completion can still delay host unload; a timeout does not
make it safe to unload an executing worker. LOOP-011 retains these live gates.

The toolkit remains within its existing personal-use, same-user trust boundary.
WM_COPYDATA's sender HWND is supplied by the caller; its registered-image check
does not authenticate a hostile same-user process. The local shared secret and
user-owned snapshots also do not establish such isolation. The review corrected
misleading source commentary rather than claiming a new IPC security boundary.
The manual candidate remains unsigned and is not an installer.

Native client window subclasses are tracked separately from Windhawk function
hooks. Unload fences their owning UI threads and waits for active subclass calls;
failure retains the module and its lock. Their wait policy is regression-tested;
the actual Explorer/mstsc lifecycle remains a live acceptance gate. Windhawk's
[documented mod lifetime](https://github.com/ramensoftware/windhawk/wiki/Mod-lifetime)
removes its function hooks before `Wh_ModUninit`.

Repository-relative evidence links refer to the source checkout; the standalone
bundle includes this review and the acceptance guide for its test procedure.

The transport correction follows Microsoft's
[WTSVirtualChannelRead buffer requirements](https://learn.microsoft.com/en-us/windows/win32/api/wtsapi32/nf-wtsapi32-wtsvirtualchannelread)
and [CHANNEL_PDU_HEADER semantics](https://learn.microsoft.com/en-us/windows/win32/api/pchannel/ns-pchannel-channel_pdu_header).
The retained buffer lifetime follows
[IWTSVirtualChannel::Write](https://learn.microsoft.com/en-us/windows/win32/api/tsvirtualchannels/nf-tsvirtualchannels-iwtsvirtualchannel-write).
