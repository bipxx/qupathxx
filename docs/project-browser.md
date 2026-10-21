# Multi-image viewer workspace

Launch a project with `python -m qupathxx --project study.qpxproj`, or pass
`GuiOptions.project` with its active document to `show`. The native GUI now
switches documents inside the same GLFW window and OpenGL context.

The inspector's **Project images** list shows image names and missing sources.
Click a row or use **Ctrl+PageDown / Ctrl+PageUp** to cycle in stable image-ID
order. The text filter matches image names (case sensitive); cycling uses the
whole project. **Refresh** checks source availability again. File checks and
catalog copies happen on initial load, explicit refresh and catalog changes,
not every frame. The filtered list uses ImGui clipping for visible rows.

Expand **Image source / name** to rename the active entry, add a file (**F8**),
or relink the active file-backed entry (**F9**). Set the source path before
using the shortcuts. `GuiOptions.project_image_path` prefills this field for
embedded callers. Adding a valid file selects its newly allocated image ID;
failed add/relink operations retain the active image and report the error.
Relinking retains the native document, objects, history and view, checks the
existing structural metadata contract, and refreshes the active tile reader.
Embedded RGB entries do not need relinking. Names can also be changed through
`Project.rename_image(id, name)`; this changes project metadata, not the
document's undo history. Ctrl+S saves every project document and the active ID.

## Isolation and retained state

A pending switch commits at the next frame boundary, before draw commands are
created. This prevents a frame from mixing old texture IDs with the new image's
coordinates or geometry. Each visited image retains its pan/zoom, RGB display
settings, native selection and list anchor for the current `show` session.
Each document already owns its independent object IDs, classes, calibration and
bounded undo/redo history. Identical object IDs in different images do not
identify the same object.

The workspace moves each native selection into its saved view instead of
copying or rebuilding all selected IDs. Drawing, marquee, vertex/hole previews,
inspector drafts and geometry-tool state reset on switch. Background mean
measurements are cancelled and their publication identity invalidated, so an
old image's result cannot publish against an equal ID/revision in another
document. An unfinished measurement can keep its worker until cancellation is
observed; starting another measurement waits for that worker to finish.

Only the active image owns a GPU tile cache. Switches destroy old textures and
the old scheduler before uploading new tiles; the GPU payload limit remains
64 MiB. Returning to an image reloads its visible tiles. Switches currently
join outstanding tile workers, so a slow/noninterruptible vendor read can delay
the UI. The dense-image benchmark does not establish a slow-storage or vendor
latency bound. Inactive reader handles and their internal caches need separate
long-running vendor-pressure validation.

View/LUT/selection retention is session-local. Canonical project save still
does not persist these views, editing history or unfinished tools. The browser
does not yet provide project-file replacement, image removal, autosave/recovery,
dirty-close decisions or background saves; those remain M4 work. Missing-source
documents retain annotations and metadata while tile errors are shown. Source
availability is a file-existence check, not proof of reader or byte identity.

## Evidence

Real ImGui replay in `tests/test_gui_project.py` covers list/keyboard switching,
overlapping object IDs with isolated undo, restored selection/view, independent
RGB display settings, cancellation of a partial rectangle, add/save/restart,
missing-file relink and failed-add errors. Pixel checks inspect captured OpenGL
framebuffers. The native project suite and Python storage tests cover persistent
image renaming. `GuiReport` exposes the active image, completed switch count,
maximum synchronous switch time and final project error for embedded callers.

`tools/benchmark_project_browser.py` exercises a 1000-image catalog and 20
switches while preserving 100000 selected objects in a narrow view. It records
CPU frame work, switch time, visible objects and GPU payload. It does not measure
simultaneous display of all objects, complex geometry, filesystem latency or
process-restart view restoration.

The [recorded Windows run](benchmarks/project-browser-1000-windows.json) retained
all 100000 selected IDs, drew 828 visible objects, completed all 20 switches,
and measured 0.63 ms maximum synchronous switch time, 17.0 ms p95 CPU frame work
and 3 MiB peak GPU tile payload. Vendor/slow-reader behavior remains unverified.
