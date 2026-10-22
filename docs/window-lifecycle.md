# Window resize and minimization

The native viewer detects actual GLFW iconification. While minimized, it skips
ImGui frame preparation, spatial queries, overlay construction, tile upload and
OpenGL drawing/presentation. It continues polling workers, processing managed
commands, reconciling project saves and scheduling recovery autosaves. An idle
iteration waits for events for up to 20 ms; input can wake it earlier. A close
request restores a minimized window when a visible confirmation is needed.
Force-close commands can finish directly without rendering.

Resizing keeps the image point at the canvas center and the zoom unchanged.
Layout-only changes do not rewrite an already synchronized project view. The
new clip/size/viewport invalidates the existing canvas cache through its normal
key; restored rendering retains pixel alignment. Uncommitted drawing/vertex
gestures are cancelled when minimization or a resized canvas invalidates them.

## Replay contract

Window replay operations call GLFW on the native owner thread:

- `RESIZE_WINDOW`: `x`/`y` are integer logical window width/height, within
  640x480 through 8192x8192. Target, wheel and modifier fields are ignored.
- `MINIMIZE_WINDOW` and `RESTORE_WINDOW`: position/target fields are ignored.
- Window controls may start at logical frame zero; ordinary ImGui inputs still
  start at frame two. All replay frames must be within a finite invocation.
- Restore or close before issuing further UI inputs. Resize while minimized is
  allowed. A malformed minimized/UI sequence is rejected before a window opens.
- `WINDOW_READY` waits for a rendered layout matching the current logical window
  dimensions. Use it for image/canvas input following resize or restore.
  It is not a widget visibility or tile-settlement condition; named buttons use
  `TARGET_READY`. Window controls cannot themselves wait for `WINDOW_READY`.

```python
import qupathxx as q

resize = q.GuiInput()
resize.frame, resize.kind = 3, q.GuiInputKind.RESIZE_WINDOW
resize.x, resize.y = 1040, 660
point = q.GuiInput()
point.frame, point.kind = 4, q.GuiInputKind.MOVE
point.target = q.GuiInputTarget.IMAGE
point.x, point.y = 400, 300
point.wait_for = q.GuiInputWait.WINDOW_READY
options = q.GuiOptions()
options.inputs = [resize, point]
report = q.show(q.Document(q.Image.demo()), frame_limit=20, options=options)
```

`GuiReport.frames` counts rendered frames. `suspended_iterations` counts loops
that service the owner without drawing. Logical replay time advances while
minimized, and readiness waits still have their existing deadlines. Managed
`ViewerStats.frame` remains the rendered frame count. Activation failure detection
uses owner-pump cycles so it also progresses without drawing.

`suspended_work_ms` is wall time spent before the event wait on suspended loops,
including model/command/save servicing and window operations. It is not process
CPU time. `suspended_wait_ms` records the event wait separately. Rendered-frame
CPU statistics exclude suspended loops and retain their existing presentation
exclusions. `window_iconified` records the final native window state.

A finite call can end minimized without rendering another frame. Capture and
tile settlement require restoring the window before the logical end; otherwise
the call fails explicitly. A finite startup that cannot paint because it remains
minimized is bounded by the replay deadline and cancels pending session commands.
Framebuffer dimensions while minimized are platform dependent. Viewport/tile
summaries describe the available layout state; a newly activated image may not
have been fitted or drawn yet.

## Validation and measurement

Native tests cover window input validation and activation success/failure with a
frozen rendered-frame counter. Installed GUI tests use actual GLFW resize,
minimize and restore calls. They check exact overlay-pixel translation, unchanged
saved center/zoom, image-coordinate selection after layout changes, rendering
suspension, autosave before the first rendered frame, managed edit/inspect/image
activation/save/force-close and bounded minimized startup/capture failures.

Run the paired synthetic benchmark with the installed GUI wheel:

```powershell
python -I tools/benchmark_window_lifecycle.py --objects 100000 --frames 180 --repetitions 3 --output build/window-lifecycle.json
```

The [Windows software-rendered record](benchmarks/window-lifecycle-windows.json)
retains all six alternating runs and tool/source/native hashes. Each active run
draws 180 frames; each minimized run draws 24 and services 156 suspended loops.
All six final framebuffers match exactly. Median whole-invocation process CPU
per wall second is 2.2645 cores active and 1.2632 minimized (about 44% lower).
Process CPU sums all threads, including Mesa/LLVM work, and the invocation includes
context setup, initial drawing, capture and teardown. Scene construction is
excluded. Suspended-loop work totals 14.58–28.18 ms over 156 loops per run.

This measures reduced activity while the application is actually minimized. It
does not improve or accept the separate cold-first-render, dense interactive
frame-time, hardware driver or per-monitor DPI budgets. Small-screen layout,
docking and broader OS interaction campaigns remain M2 work.
