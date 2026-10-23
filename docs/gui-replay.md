# GUI replay readiness

Native replay drives the same ImGui input queue as the GLFW backend. It never
invokes document commands or modal actions on behalf of a click. Fixed-frame
events remain the default. Readiness waits are opt-in so asynchronous workflows
can be verified without guessing when a modal or loaded workspace will appear.

## Event conditions and clocks

`GuiInput.wait_for` accepts `GuiInputWait.IMMEDIATE` (default), `TARGET_READY`,
`STARTUP_FINISHED`, `TISSUE_READY`, `TISSUE_CONTOURS_READY`, or
`TISSUE_CONTOURS_PUBLISHED`, `TISSUE_ARCHIVED`, or `TISSUE_VERIFIED`.
Result/publication conditions require an idle job and the corresponding current
result or publication flag. Archive readiness requires a latest record for the
selected source; verified readiness also requires completed rechecking and a
current session proof. See [tissue contours](tissue-contours.md).
Events are ordered by their scheduled frame, preserving
input order for ties. Each accepted event is emitted once. If a due event's
condition is false, that event and all later events wait while physical UI
frames continue rendering, polling jobs and handling real window input.

`TARGET_READY` is valid only for `LEFT_DOWN` on a named button target, such as
`DISCARD_CLOSE`, `CANCEL_CLOSE` or `DISCARD_RECOVERY_REPLACEMENT`. The button must
have been drawn on the previous physical frame, be visible and enabled, and
belong to a window past its appearing frame. Current modal state also guards
close/replacement actions. Buttons behind another modal are excluded. Old coordinates from a hidden or disabled button
are not readiness evidence. Pointer motion, key events and ordinary canvas
clicks retain their explicit frame semantics.

`STARTUP_FINISHED` waits until initial source/project/recovery opening has been
adopted, failed or cancelled. It does not require success: an Open-project key
can then exercise recovery from a failed initial opening. Check the returned
startup and project fields to verify the actual workflow outcome.

With waits, `GuiInput.frame` and finite `frame_limit` use a logical replay clock.
The clock pauses at a blocked event. Subsequent events retain their relative
spacing, including the release on the frame after a delayed mouse press.
`GuiReport.frames` still counts physical rendered frames. `replay_frame` counts
logical frames, `replay_wait_frames` counts physical frames spent waiting, and
`replay_complete` reports whether every scripted event was dispatched. User close
can interrupt a replay; dispatch completion alone is not proof of an action's
application, so tests also inspect the resulting project/document and report.

`GuiOptions.replay_timeout_seconds` defaults to 5 and must be finite, positive
and at most 60. Each blocked event has its own steady-clock deadline. Expiry
raises an error naming its logical frame rather than injecting an unavailable
action or silently skipping it. Resource teardown still joins native workers;
an uninterruptible reader can delay return beyond the replay deadline.

```python
press = q.GuiInput()
press.frame = 11
press.kind = q.GuiInputKind.LEFT_DOWN
press.target = q.GuiInputTarget.DISCARD_CLOSE
press.wait_for = q.GuiInputWait.TARGET_READY
release = q.GuiInput()
release.frame = 12
release.kind = q.GuiInputKind.LEFT_UP
release.target = press.target
# Add these after the save/edit/close events in options.inputs.
```

The clock is native and independent of OpenGL. It sorts one captured event
vector, then consumes a cursor rather than rescanning the entire script on every
frame. Readiness uses constant-sized frame stamps and the actual ImGui item
state. No replay worker thread or Python callback runs inside the viewer.

## Complete tile captures

`GuiOptions.settle_tiles=True` requires a positive `frame_limit`. After the
logical frame budget (including the existing post-startup grace period), the
viewer continues rendering until the current image's planned tiles have been
decoded and uploaded. It uses the same bounded scheduler/cache and per-frame
upload budget. `settle_frames` counts those additional physical frames and
`tiles_settled` reports completion. A pending load/adoption does not count as a
settled current workspace; unrelated metadata scans, autosave and analyses are
outside this option's scope.

Settlement uses a separate steady-clock deadline with the same configured
duration. Tile failures raise an explicit settlement error. The framebuffer is
captured on the final rendered frame, after uploads complete; startup captures
also follow the actual post-load frame budget. Without settlement, a finite run
can still return with pending tiles, as before. This option establishes completed
rendering within a timeout, not a fixed-frame or hardware-wide latency guarantee.
An oversized desired tile set or a decoder that does not complete can exhaust
the deadline; settlement does not increase cache budgets or preempt native I/O.

```python
options = q.GuiOptions()
options.settle_tiles = True
report = q.show(doc, frame_limit=1, capture_path="ready.ppm", options=options)
assert report.tiles_settled and report.pending_tiles == 0
```

## Verification scope

Pure C++ tests use an injected clock and readiness predicate to verify ordering,
exactly-once dispatch, relative mouse release, deadline endpoints, reset and
legacy frame behavior. Desktop tests exercise a disabled save-and-close modal,
failed startup followed by replacement, missing and closed targets, context
retirement after timeout, and exact framebuffer pixels after tile settlement.
Existing data-preservation, saved-baseline, dirty-state, image identity and GPU
budget assertions remain part of those workflows. Reader/runtime latency and
broader OS/DPI delivery remain separate M1/M2 acceptance work.

The [Windows readiness recording](benchmarks/gui-replay-readiness-windows.json)
repeats three actual save-under-lock, failed-startup replacement and settled
capture workflows. Save scripts complete at logical frame 13 while physical
rendering takes 24/51/52 frames, including 11/38/39 readiness-wait frames.
Stored snapshots retain the captured edit, live documents retain their newer
Undo, and close occurs only after the enabled discard control is clicked.
Fallback replacement waits two frames in each round and verifies image/project
identity. Four-tile captures take 2/2/6 physical frames for a one-frame logical
budget and match independently specified RGB bytes, retaining 1 MiB GPU payload.
Short settlement runs have no warmed-frame timing samples; their zero-valued
frame timing fields do not mean zero rendering cost.

The [100001-selected-object recording](benchmarks/gui-replay-selected-100001-windows.json)
uses ordinary frame replay, 876 visible objects and the existing analytic tissue
cleanup workload. It records 17.25 ms sampled frame p95 and 19.51 ms maximum,
with 16 MiB each of image and mask GPU payload on an RTX 4090. These synthetic
recordings do not establish broad hardware or vendor-reader latency. Reproduce
with `tools/benchmark_gui_replay.py` and `tools/benchmark_tissue_cleanup.py --gui`.

The final installed wheel passes all 620 Python cases, with two unavailable
Windows file-symlink cases skipped. All nineteen native suites pass in full,
core and minimal configurations. The three previously intermittent checks now
use explicit readiness/completion contracts; source data and workflow assertions
remain verified. This does not accept every asynchronous GUI or reader workload.

## Scripted wheel ownership

A nonempty `GuiOptions.inputs` list selects scripted wheel ownership for the
entire `show` call. Replay-generated wheel events are tagged by ImGui event ID;
other wheel events are filtered before `NewFrame`. Deferred scripted wheel
events retain their tags while queued. This prevents desktop scroll messages
from zooming the canvas or scrolling a panel during deterministic replay.
An unscripted session uses normal GLFW wheel routing.

`GuiReport.replay_physical_wheel_events_ignored` reports how many untagged wheel
events were removed. A Win32 regression posts three real `WM_MOUSEWHEEL`
messages to the test owner's resized GLFW window. It requires at least three
ignored events while a scripted positive wheel step still changes zoom from
2 to 2.4. The same fixture on the prior binary produced 1.3888888888888888,
the exact net result of those three negative steps plus the positive scripted
step. No global cursor movement or messages to other applications are used.
