# Live Python viewer sessions

`ViewerSession` is a thread-safe endpoint for a native-owned ImGui workspace.
The main Python thread calls `q.show(session)`; a Python producer thread submits
typed commands and waits for their notifications. The viewer opens its own
project from a `.qpxproj` file through the background loader. It paints while
loading and performs document edits/publication on its owner thread.
Successful activation publishes the adopted image in `session.stats` before
notifying waiting producers, including while the window is minimized.

```python
import threading
import qupathxx as q

session = q.ViewerSession("study.qpxproj")

def script():
    ticket = session.catalog()
    event = session.wait(ticket, 30)
    if event.state != "succeeded":
        session.close(True)
        return
    event = session.take(ticket)
    image = event.images[0]
    edits = [q.ObjectEdit.create(q.Rect(0, 0, 64, 64), "Python ROI")]
    ticket = session.execute(image.id, edits)
    event = session.wait(ticket, 30)
    if event.state == "succeeded":
        print(session.take(ticket).object_ids)
    session.close(True)

worker = threading.Thread(target=script)
worker.start()
report = q.show(session)
worker.join()
```

The [complete live workflow](../examples/live_workflow.py) queries the image
catalog, creates/selects an ROI, runs OD analysis, exports only the new record,
saves a project and closes the viewer. It handles producer errors and can be
checked with the shipped public [type declarations](python-api.md).

## Ownership and GIL

Managed sessions accept a project **filename**, not a live Python Project or
Document. They expose no mutable model/reader reference. The viewer's model is
the same native authority used by its UI and queued commands; an independently
opened Python Project remains separate. Saved results can be reopened normally.
`GuiOptions.project` and startup overrides are rejected for managed sessions.
Optional parameter instances are copied into native ownership before the GIL is
released. The original options/parameter objects can remain Python-owned.

`show(ViewerSession)` must run on Python's main thread and releases the GIL.
`show(Document)` and `show(Project)` retain their existing GIL behavior and
single-owner contract. A native guard allows only one qupathxx viewer at a time
in a process. No arbitrary Python callback executes on the GUI or analysis
worker. The endpoint exchanges native values, immutable ROIs, numeric results
and owned pixel/label buffers. Only the GUI thread touches its live project,
selection, archive publication and graphics resources.

## Commands and notifications

| Command | Behavior |
| --- | --- |
| `catalog(after_id=0, limit=64)` | Cached image descriptors, ascending IDs; at most 256 per page |
| `inspect(image_id, object_id=0)` | Reader metadata and optionally an immutable annotation snapshot |
| `execute(image_id, edits, ...)` | One native atomic create/replace/remove batch, with returned IDs |
| `select(image_id, object_ids)` | Replace selection in the active image |
| `activate(image_id)` | Request an image switch and notify after it becomes active |
| `read_region(image_id, request)` | Native worker read; result owns RGBA pixels with NumPy access |
| `analyze`, `tissue`, `cells` | Worker computation and owner-side archival after freshness checks |
| `export_measurements`, `export_cells` | Captured native tables and atomic background CSV export |
| `save(path)` | Existing GUI save controller, immutable capture and generation reconciliation |
| `close(discard_changes=False)` | Request normal dirty-close handling; explicit discard closes directly |

Image ID zero means the active image for queries/analysis/region reads. Edit,
selection and activation commands require explicit positive image IDs. For
exports, zero means project scope; record-ID filters require one explicit image.
CSV follows the existing native export contract, including atomic replacement
of the explicitly named destination. The endpoint's `project_path` is its
immutable initial filename; save events return the actual destination.

Each submission returns a ticket. `poll(ticket)` returns its current event.
`wait(ticket, timeout_seconds)` releases the GIL and waits for a terminal event;
a timeout returns the current queued/running state. `take(ticket)` requires a
terminal event and removes its cached slot/payload. Already-registered waiters
retain their immutable event even if another consumer takes the ticket. New
operations on consumed/unknown tickets reject. Consume completed tickets to
reclaim capacity. Arrays/results retained by callers remain valid afterward.

Event states are `queued`, `running`, `succeeded`, `failed`, `cancelled`, `stale`
and `detached`. Events include action/IDs, workspace epoch, errors, current
document revision, archive record ID, elapsed time and applicable payloads.
`committed` distinguishes actual graph/file changes; close/selection/query
success does not imply a file write. A save/export may commit while its workspace
is replaced; detached completion preserves that fact. `bytes` reports cached
result payload charge, not the CSV file size or process RSS.

Closing success means the close intent was accepted. Normal close may still
wait on the existing dirty-project dialog. Explicit discard bypasses that dialog
and can bypass paused commands; remaining commands are cancelled at shutdown.
It does not erase previously committed primary files or recovery checkpoints.
`cancel(ticket)` is best-effort: queued work can be removed immediately, workers
observe cancellation checkpoints, and already-committed atomic operations win.
Native decoder calls and a spatial-index build must finish before cancellation
can complete. A session can run once; after close/failure it rejects new work.

Workspace replacement advances an epoch and rejects older queued commands.
Analysis publication checks the captured request against the live document;
removed/relinked/changed sources discard stale results. Region reads require the
same source reader at publication. Their pixels are a decoded region snapshot,
not proof of complete file/dependency continuity. Optional optimistic edit
revision checks protect geometry edits computed from an inspected model state.
`expected_revision=None` disables the check; zero is a valid checked revision.
Pixel-based adapters should retain the relevant inspection epoch/revision and
use explicit freshness checks; full versioned model adapters remain unfinished.

## Resource and performance boundaries

Default limits are 128 outstanding tickets, 16 MiB queued payload, 4 MiB per
command, 1024 edits, 16 MiB per region, 64 MiB retained result payload and 64 MiB
table/save snapshot charge. `ViewerLimits` validates explicit overrides.
Transforms, ROI payloads and ID vectors are included in queue charges. Terminal
ticket metadata is separately bounded by ticket/string limits. Results are
shared native buffers; taking a result refunds the cache charge but does not
limit memory deliberately retained by Python callers or native archives.

One script job runs at a time. Starting another asynchronous job while it is
busy fails explicitly; metadata queries and edit commands can still run. Native
kernel/table/work budgets apply in addition to endpoint limits. The owner drains
one queued command per frame. Atomic edits finish once begun; this is not a
preemptive time budget. Ordinary mutation commands pause during startup and
modal workspace transitions, while cancellation/close remain available.

The source project, one active work item, queues, archived results, GUI jobs,
SQLite, decoded tiles and caller-held buffers can coexist. These limits are not
a global RSS cap. `ViewerStats.max_command_ms` measures dispatch/capture work;
job wall time includes frame-based reconciliation, and GUI CPU frame metrics
include rendering/publication. They exclude presentation wait. The viewer's
script status occupies a fixed layout row to avoid invalidating the canvas when
jobs start/finish. Archival-only changes preserve the geometry render cache;
object/class/calibration commands still invalidate it. Initial spatial-index
preparation runs on the loader's unpublished native model before handoff.

Broader live command coverage, versioned external model adapters, release API
compatibility, real-slide/long-running memory campaigns and remaining M0-M7
requirements stay active.


## Validation and measured scope

The focused installed GUI wheel passes 20 endpoint cases: queued lifecycle and
limits, atomic errors, checked zero/nonzero revisions, producer/GIL progress,
queries and all three analysis jobs, retained arrays, record-filtered exports,
save/reopen, initial failure/cancellation, modal close, finite-run shutdown,
main-thread/single-window guards and native parameter ownership. Native tests
also exercise simultaneous wait/take, source removal, workspace epochs, stale
jobs, repeated ticket reclamation and render-cache revision preservation.
Strict mypy accepts the live example/public declarations; negative fixtures
reject invalid endpoint filenames/edit types along with existing API mistakes.

The mixed synthetic workload loads 100001 objects, selects all of them, performs
100 queued inspections, creates one object, archives OD analysis, reads pixels
and saves, then closes. The final run reports 100002 visible objects, 151 frames,
0.0068 ms p95 submission, 17.2125 ms p95 inspection round trip and 0.1032 ms maximum
dispatch/capture. Analysis notification wall time is 6.89 ms. GUI p95 CPU frame
work is 17.0061 ms with a 313.73 ms maximum; the maximum includes cold scene work.
Initial native preparation is 189.94 ms with two loading frames. Overlay payload
is 3523400 bytes and tile payload 1048576 bytes.

The [earlier run](benchmarks/live-viewer-before-windows.json) recorded 49.838 ms
p95 / 349.54 ms maximum GUI work and 21.741 ms p95 round trips. Fixed-height job
status, render revisions independent of archives, and unpublished-loader spatial
preparation remove avoidable work. These are observations on one Windows PC,
not a controlled universal speedup claim; startup/cache conditions vary. The
[final raw run](benchmarks/live-viewer-windows.json) records exact native/source
hashes, workload, GPU and scope. No extra idle/cached frames were added to the
mixed workload to reduce its p95. Separate finite capture runs are visual checks
and are explicitly tagged with a different measurement scope.

The sidebar and scripted ROI are visually checked. The 16.7 ms frame target is
not established for this mixed/cold large scene, and the cold maximum remains a
performance gap. Broader M2 rendering/startup work and M7 long-running/platform
validation remain required; this packet does not close those gates.


Finite embedded runs also drain an already-authorized save-and-replace/discard
transition with a bounded deadline before returning its workspace authority.
A delayed SQLite-writer fixture exercises the frame-limit boundary. The complete
live CLI example is run separately and its output project reopens with the new
analysis record. The final benchmark uses the exact native binary of the final
regression; the first post-render-cache run is additionally retained in
[the intermediate raw record](benchmarks/live-viewer-render-cache-windows.json).


The final installed GUI wheel passes 1143 Python cases with two existing Windows
file-symlink permission skips in 279.33 s. The combined live-session/workspace
focused set passes 34 cases, including delayed save-and-replace at a finite frame
boundary. Full/core/minimal native suites pass 33/32/32; storage-disabled probes
pass. Stub regeneration matches the installed module, both examples and positive
fixtures pass strict mypy, and seven intended negative cases reject. The standalone
live CLI example saves/reopens its output, and visual inspection checks the fixed
status row and scripted ROI. The raw final performance record uses this exact
native artifact. The full project goal remains active.

Developer frame profiling is opt-in through `QUPATHXX_GUI_PROFILE`, an output
filename read when the viewer starts. For example, in PowerShell:

```powershell
$env:QUPATHXX_GUI_PROFILE = "$PWD/build/gui-frames.json"
python tools/benchmark_viewer_session.py --output build/live-profile.json
Remove-Item Env:QUPATHXX_GUI_PROFILE
```

The private `qpx.gui-profile/1` trace retains the first 4096 rendered frames,
then writes atomically at normal shutdown. It performs no file I/O in the frame
loop. Each frame includes its sequence, startup-at-entry state, canvas cache
hit, visible-object count and CPU phase timings. The source fingerprint is stored
once at trace level. Model, widgets, tiles, cache setup,
query, geometry, remaining UI and OpenGL submission phases partition timed CPU
work. Query also includes image/preview draw-command preparation; widget time
includes GLFW/ImGui frame setup. This is CPU wall timing, including any driver
waits during GL submission, rather than GPU execution timing. Swap/presentation
and work before the frame loop are outside these phase totals. Profiling an
empty/loading canvas attributes its UI preparation to `other_ms`; phases not
visited remain zero. An invalid/unwritable profile destination reports an error.

The [initial phase trace](benchmarks/gui-profile-before-windows.json) isolates
272.74 ms in the first OpenGL render, with no visible objects. The following
100001-object rebuild spends 30.57 ms in geometry and 4.80 ms in spatial querying.
Dense selections now avoid repeated per-object selection-tree searches: complete
selections use their synchronized count; substantial partial selections walk a
sorted snapshot. Small views of large selections retain bounded per-object tree
lookups rather than copying the entire selection. Render order, geometry, labels
and class/selection colors are preserved. A regression compares canvas pixels
between dense and sparse membership paths with the same visible selection.

The [new trace](benchmarks/gui-profile-windows.json) and its
[matching workload record](benchmarks/live-viewer-profile-windows.json) measure
17.176 ms p95 GUI CPU work and 276.13 ms maximum. The
[initial workload record](benchmarks/live-viewer-profile-before-windows.json)
measures 17.066 ms p95 and 272.91 ms maximum. Geometry phases vary across these
runs; they do not establish a performance improvement. Both retain the same
mixed close-after-jobs scope without appended idle frames. The initial trace
predates the additive `other_ms` field, so its phase sums omit remaining UI work.
Each matching workload record identifies the exact native binary and source.
The first-render delay and cold large-scene rebuild remain open performance work.

The profiled wheel passes 1145 Python cases with two existing Windows symlink
permission skips in 309.94 s, all 33 native tests and the public stub check.
The dense/sparse selection fixture also receives a visual check. These checks
validate this packet; broader compatibility and performance gates remain open.
