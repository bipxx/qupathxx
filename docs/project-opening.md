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
loader is claimed. Old graphs whose document control blocks come entirely from
native factories
now use one [retirement worker](project-retirement.md). Externally supplied
`add_document` graphs remain on the owner thread because their deleters may require
the GIL. Those graphs, GPU cleanup and vendor shutdown can still delay replacement.
[Explicit primary saves](project-save.md)
now run on a worker. [In-window recovery loading](recovery-maintenance.md) also
uses this loader slot. Both launchers now defer initial file opening as described below.

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


## Initial file opening

The native executable and `python -m qupathxx` now create an empty workspace and
paint its first frame before opening a supplied project, recovery checkpoint or
image. The same single native load slot performs file access and constructs the
candidate. The loading header names the file, reports metadata/data/reference
validation stages and shows image and object-row counts for project/recovery
inputs. Images are counted after native document validation; objects are counted
as rows are decoded, before final hierarchy/index validation. Counts are not a
percentage-complete estimate. Editing remains disabled while initial loading is
active; **Cancel opening** and window close remain available.

A complete validated result is adopted at a frame boundary, with its stored views
and requested `--image-id`. A bad image ID fails the request before adoption.
Failure or cancellation keeps the caller's empty `Project` and its initial dirty
state. A newly created project is unsaved/dirty even when it has no images; this
placeholder does not trigger a replacement decision or a dirty-close prompt while
the initial request is active. Afterwards the usual project controls and save/
close behavior apply. Recovery retains captured generation/conflict rules and
moved-primary verification through `--recovery-base`.

```python
project = q.Project()
options = q.GuiOptions()
options.startup_kind = "project"  # "recovery" or "image" also accepted.
options.startup_path = "study.qpxproj"
options.startup_image_id = 2     # Optional stored image selection; 0 uses saved active image.
q.show(project, options=options)
```

Deferred startup requires a fresh, unmodified empty project (no stored path or
images); startup kind/path/authority validation runs before creating a window. `project_open_path`
continues to prefill the ordinary in-window opening field. `Project.open` and
`Project.recover` remain synchronous standalone APIs.

The startup worker uses native factories, immutable path values and shared
atomics. It never borrows a Python document graph. SQLite decoding checks
cancellation between rows/images and during SQL execution. Initial header lock
waits use a cancellation-aware busy handler with 5 ms retry sleeps and a bounded
retry count. Image hierarchy validation checks cancellation before and after the
native document restore. Vendor metadata calls, image reads and operating-system
I/O finish at their safe boundaries; no forced thread termination is used.
Shutdown joins outstanding work. Ordinary in-window loading retains its existing
finish-and-discard cancellation behavior.

For a deferred startup, a positive `frame_limit` gives that many frames after the
request finishes as well as the frames spent loading. Replay frame numbers and
framebuffer capture retain their absolute frame positions, so a capture at the
requested frame can intentionally record an in-progress load. CLI `--smoke`
checks startup success and tile upload after the additional settled frames;
failed inputs produce a nonzero exit status. Normal interactive failures stay in
the empty viewer with an error so another file can be opened.

`GuiReport` exposes `startup_succeeded`, `startup_cancelled`, `startup_frames`,
`startup_images`, `startup_objects` and `startup_ms`. The duration covers a
successful worker load, not window creation or the first painted frame. Failed/
cancelled loads can still report decoded row counts, but are never adopted.

`tests/test_gui_startup.py` covers project/recovery/image loading, selected-image
validation, both launcher families, actual Aperio pixels, locked SQLite startup,
cancellation, close and unchanged primary bytes. A captured loading screen was
visually checked with a blocked source: the path/status and enabled Cancel button
are visible above the disabled empty workspace. The benchmark tool
`tools/benchmark_project_startup.py` measures 100000-object native initial project
and recovery loading with a selected narrow view after adoption. Cold/network
storage and prolonged vendor metadata calls remain outside that timing scope.


The [initial project run](benchmarks/project-startup-100000-windows.json) records
237.7 ms native loading, six loading frames, 16.91 ms CPU frame p95 and 202.69 ms
maximum frame work. The [initial recovery run](benchmarks/recovery-startup-100000-windows.json)
records 240.7 ms native loading, two loading frames, 16.87 ms p95 and 274.77 ms
maximum. Both verify 100000 selected objects, 792 visible overlays, 3 MiB GPU
payload and unchanged primary bytes. All startup runs now include the first ten
frames in CPU statistics; ordinary non-startup runs retain their existing warm-up
sampling. The [earlier warm-frame baseline](benchmarks/project-startup-warm-frame-baseline-windows.json)
excluded those frames and therefore did not reveal the startup spikes.

A [temporary stage trace](benchmarks/project-startup-stage-profile-windows.json)
locates a 224.65 ms rendering-stage stall on frame 1, before candidate adoption.
That stage includes ImGui rendering and OpenGL submission and may wait inside GL;
it is not GPU-profiler attribution. Candidate adoption on frame 6 takes 0.56 ms
in boundary work and 28.99 ms building that frame's UI. Instrumentation was
removed afterward. These measurements verify visible background loading rather
than uniformly smooth startup; early rendering/adoption latency remains an
optimization target. Counts/loading frames do not measure first-paint latency or
input-to-cancel completion latency. Cold/network/vendor paths and other hardware
remain unverified by this timing packet.


Final verification: 375 installed-wheel Python tests pass, with two unavailable
Windows file-symlink tests skipped. Eighteen startup tests include reopening a
valid project after initial failure in the same window. All fourteen native
suites pass in full, core and minimal feature configurations.
