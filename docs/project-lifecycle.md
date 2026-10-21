# Empty workspaces and image removal

The desktop supports projects with no images, including opening an empty saved
project or recovery. Start one with `qupathxx_app --empty` or
`python -m qupathxx --empty`, or choose **New project (Ctrl+N)** in the inspector.
New project uses the same Save/Discard/Cancel decision as opening another
project. Saving empty work is supported; closing dirty empty work offers the
same save/recovery/discard choices as an image workspace.

```python
import qupathxx as q

project = q.Project()
q.show(project)  # Owns the live project binding, including its empty state.
```

The existing `show(document, options=...)` API remains available. The project
form rejects conflicting `GuiOptions.project` ownership. C++ uses
`run_project_gui(shared_ptr<Project>, ...)`. Both launchers accept empty
`--project` and `--recover` inputs, and `--smoke` verifies the empty viewer
without requiring an image upload. `--empty` is mutually exclusive with image,
project and recovery inputs and does not accept an image ID.

An empty canvas gives an add/open instruction. Project controls remain active;
image tools, annotation keyboard gestures, measurements and interchange cannot
mutate a removed or placeholder document. The implementation keeps a private
UI backing document, inserts no artificial image into the project, requests no
pixels and uploads no textures for the empty state. It publishes no view for
image ID zero. An existing project with images and `active_image=0` displays its
catalog and allows selecting an existing entry. Empty reports use zero camera,
selection, visible-object and pending-tile values; upload/peak counters remain
cumulative if real images were displayed earlier in the session.

## Removing an entry

**Remove image (F12)** acts on the active image. A modal names it, gives its
object count and explains that its objects, view and editing history leave the
workspace. Removal has no undo command. Cancel preserves the entry, selection
and edits. The modal blocks underlying editing shortcuts. Confirmation only
changes the in-memory project; explicit save or recovery determines what reaches
disk. The linked source image is never deleted, overwritten or closed through a
shared external `Document` reference.

At the next frame boundary the viewer prepares the surviving image binding,
then removes the canonical entry and its stored view. It selects the first
remaining project ID, restoring that image's retained session selection/view
when present. Old GPU textures and tile jobs are retired, and draft tools,
geometry handles, measurement publication and removed-image session caches are
cleared. Removing the final image enters the empty canvas. Adding an image from
there creates a fresh stable ID; IDs are never reused. A close request arriving
with confirmation first observes the completed removal, then applies the normal
dirty-close decision.

External document references remain valid and preserve their own annotations
and history. The project-only viewer does not retain an invisible first-image
owner for the whole session. Its active binding owns the document until the
switch, and releases it on the owner thread: Python-origin shared-pointer
deleters can require the GIL. Native model and selected-ID destruction can still
cost time proportional to the removed graph. Vendor reader shutdown and long
memory-pressure campaigns remain unverified.

## Evidence

`tests/test_gui_lifecycle.py` covers empty display without uploads or mutations,
remove/save/reopen, Cancel and blocked undo, restoration of a surviving image's
session state, add after emptiness, all new-project decisions, linked-file byte
preservation, empty recovery, immediate-close ordering, project ownership and
both CLI launchers. A timeout-guarded subprocess with a weak reference verifies
that a removed Python-owned document is released without a GIL deadlock. The
existing workspace tests now treat empty projects as accepted inputs and retain
malformed/missing/recovery-as-primary rejection coverage.

`tools/benchmark_project_remove.py` removes an image with 100000 selected
rectangles from a 1000-image catalog. It verifies the remaining count/active
image, unchanged primary bytes, cleared selection and release of the Python
object after binding retirement. `GuiReport.images_removed` counts confirmed
canonical removals. `max_image_switch_ms` includes removal and the surviving
binding in this workload. Frame work includes owner-thread destruction and
excludes buffer-swap wait. The benchmark uses dense sources and local storage;
it does not establish cold/network/vendor or long-pressure guarantees.


The [recorded Windows run](benchmarks/project-remove-100000-windows.json) measured
14.1 ms removal/binding, 17.0 ms p95 and 17.5 ms maximum CPU frame work after warmup,
with 3 MiB peak GPU tile payload. The removed Python document was released and
999 catalog entries remained. These are one local dense workload's measurements,
not general bounds for complex geometry, retained undo journals or vendor readers.
