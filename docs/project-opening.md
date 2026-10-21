# Opening projects inside the viewer

The project inspector provides a separate **Project file to open** field and
**Open project (Ctrl+O)** action. File decoding, SQLite validation and native
object construction run on one worker while the existing image, tools and undo
remain usable. Only one load can be outstanding; repeated requests do not queue
more project graphs. A failed load leaves the current workspace intact and
shows the native validation or filesystem error.

When loading succeeds, a clean workspace switches at the next frame boundary.
Dirty work presents **Save current and open**, **Discard current and open** and
**Cancel**. The dialog names the selected source and provides the current save
destination. Save failure keeps both the original authority and the dialog;
Cancel retains its selection, history and edits. Saving over the file selected
for opening triggers a worker reload of that successful save, so the adopted
workspace contains the latest saved changes and generation.

Replacement preserves the caller's `Project` object but adopts the opened
project's identity, path, generation, documents and stored views. All subsequent
saves use that authority. Existing external `Document` references remain valid
and still address the previous graph; query the project's current document to
continue working in the new workspace. New catalog rows, active image, camera,
RGB display and overlays are rebound together. Session selection, transient tools,
old tile textures and pending mean-measurement publication are reset. The
current session's old autosave is finished/cancelled and cleared separately;
checkpoints from other controllers are retained.

```python
import qupathxx as q

project = q.Project.open("study.qpxproj")
options = q.GuiOptions()
options.project = project
options.project_open_path = "another-study.qpxproj"  # Prefill, not automatic open.
q.show(project.document(project.active_image), options=options)
# Ctrl+O in the viewer opens this file. The same project now owns that workspace.
```

**Cancel opening** prevents adoption without blocking the active viewer. It
allows an already running SQLite/validation operation to finish; it does not
interrupt operating-system I/O. Closing the viewer cancels adoption as well,
and shutdown joins its outstanding load. The loader retains a second canonical
graph until the replacement decision. No new memory limit or streaming project
loader is claimed. Old graph references are released on the owner thread:
Python-origin shared-pointer deleters can require the GIL, so moving their last
release to a worker would deadlock an embedded viewer. Large inactive graphs
can therefore still cause a replacement-frame pause. [Explicit primary saves](project-save.md)
now run on a worker. [In-window recovery loading](recovery-maintenance.md) also
uses this loader slot; initial CLI opening remains synchronous.

The viewer now accepts empty projects and projects without an active image.
See [project lifecycle](project-lifecycle.md) for `show(project)`, `--empty`, New
project, adding the first image and confirmed removal of the last image.

## Evidence

`tests/test_gui_workspace.py` exercises real ImGui event replay and captures the
OpenGL framebuffer to verify new pixels. It covers Save/Discard/Cancel, failed
pre-replacement save, missing/corrupt/recovery-as-primary input rejection and accepted empty projects, same-file
save/reload generations, changed active image/catalog/views, autosave cleanup,
and a locked source loaded while the old document remains undoable. A separate
process with a timeout verifies retirement of Python-owned inactive documents
without a GIL deadlock. Existing recovery and browser replay tests cover the
shared replacement path and image switching.

`GuiReport.project_replacements` counts successful authority swaps, including
recovery. `project_load_ms` records the most recent ordinary worker load, from
open through validated graph construction. It excludes user decision time and
frame-boundary binding. `tools/benchmark_project_open.py` loads 100000 objects
while a separate live 100000-object workspace is selected, replaces the graph,
and selects the loaded objects. It verifies primary bytes, identity, selection
and a narrow visible overlay count. CPU frame measurements include concurrent
loading and replacement; they exclude buffer-swap wait. Cold/network storage,
vendor reader latency, full-slide overlays and prolonged memory pressure remain
unverified.


The [recorded Windows run](benchmarks/project-open-100000-windows.json) measured
171.7 ms worker load, 17.0 ms p95 and 27.0 ms maximum CPU frame work after warmup.
The final graph retained all 100000 selected objects with 792 visible overlays
and 3 MiB peak GPU tile payload. This single warm local-storage run does not
establish a bound on replacement latency or on owner-thread graph destruction.
