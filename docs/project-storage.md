# Native multi-image project storage

The native `Project` owns shared documents, stable image IDs, image names and
source descriptors. SQLite schema version 9 stores canonical object state,
class definitions, per-axis calibration, source metadata, the active image and
per-image camera/display views, retained analysis records, tissue masks, cell runs,
object-classifier artifacts and pixel training/prediction artifacts. Schemas 1–8
remain readable; see [workspace migration](project-workspace.md) and
[analysis provenance](analysis-provenance.md), [tissue archives](tissue-archive.md)
[cell archives](cell-archive.md) and [pixel classifier archives](pixel-classifier-archives.md).
The public Python API uses the same native container; it does not serialize a
separate Python model. All project/document mutation is single-threaded and
retains the Python GIL. Shared documents remain alive after removal from a project.

```python
import qupathxx as q

project = q.Project()
image_id = project.add_image("slide.svs")
doc = project.document(image_id)
doc.add_rectangle(q.Rect(100, 100, 200, 200), "Region")
project.save("study.qpxproj")

restored = q.Project.open("study.qpxproj")
assert restored.document(image_id).object_count == 1
```

`add_image` selects PPM or OpenSlide from the path, with an optional explicit
reader. `add_document` attaches an existing shared document. Pass `source_path`
for a file-backed document; otherwise a dense RGB image is embedded. Embedding
is limited to 256 MiB per image. Whole-slide pixels stay in their source file.
When a provided file exists, its dimensions and pyramid are checked against
the attached document. A missing link may retain already available document state.

## Identity and exact state

The project has a random 128-bit hexadecimal identity. Image and object IDs are
monotonic unsigned 64-bit values. The schema stores IDs, next-ID counters and
document/object revisions as canonical decimal text, preserving the unsigned
range beyond SQLite's signed integer limit. Image dimensions remain signed
64-bit integers. Deleted-ID gaps survive restart and are not reused.

Each geometry BLOB starts with `QRO`, encoding version 1 and a shape kind. Bounds
for rectangles and ellipses are four little-endian IEEE-754 doubles. Polygon
parts store shells and holes separately; polylines/points store coordinate
sequences. Counts are checked before allocation, the one-million-vertex ROI
limit applies, and trailing bytes or unknown encodings are rejected. This
preserves analytic ellipses exactly rather than round-tripping their polygon WKT.
Nucleus geometry is a separate nullable BLOB.

Opening a project constructs new native documents and validates topology,
in-image extents, classes, hierarchy, cycles, depth, revisions and next IDs
before returning them. Child/class-count indexes are derived again; stale
spatial caches are not serialized. Source calibration and manual document
calibration are separate, so explicitly clearing calibration remains unknown
after restart even if reader metadata contains a known scale.

Canonical state and stored camera/display views survive restart; editing history,
history-retention settings and selection remain session state. Area and RGB
measurements are recomputed from the restored shape/calibration/source. Stored
OD/stain analysis records now persist original ROIs, parameters, summaries and
input digests. They load as unverified historical values until a background
recheck; see [analysis provenance](analysis-provenance.md). Full-source and
environment provenance remain M5 work.

## Transactions and save conflicts

Saves use one `BEGIN IMMEDIATE` transaction with `synchronous=FULL`. Prepared
statements replace canonical rows for all images; source pixels are not read
for linked images. Existing schema/application identity is checked before rows
are removed. Commit updates the in-memory saved path, generation and saved
document revisions only after SQLite reports success. A failure rolls back the
transaction and leaves unsaved in-memory edits available.

The application ID is `0x51505858` and new `user_version` is 5. Schemas 1–4 migrate
transactionally on save. Unknown schemas and
unrelated databases are rejected. A different project identity at the destination
is rejected rather than overwritten. Save to a new filename for a separate copy.
Each successful save increments a generation. A writer loaded from an older
generation cannot overwrite a newer one; reopen or save its changes to a new path.
This is optimistic conflict detection, not collaborative editing or a merge tool.

The implementation relies on SQLite's
[transaction guarantees](https://www.sqlite.org/transactional.html), under the
filesystem and operating system assumptions of SQLite. Current failure tests
install a trigger that aborts object insertion after earlier tables have been
rewritten inside the transaction. They verify rollback, the previous project,
generation/dirty state and SQLite `quick_check`. This is actual mid-save failure
coverage; a process-kill/power-loss fault campaign remains a delivery gate.
The format is a local SQLite database. External journal-mode/schema manipulation
and network-filesystem durability are not part of the validated support matrix.

## Links, missing files and relinking

File links are stored relative to the project directory when their filesystem
roots permit it. Saving to a new directory recalculates these paths. Moving a
project with its source tree preserves relative links. `images` returns copied
descriptors with an `available` filesystem check, without opening all slides.

Linked servers retain stored metadata and open the reader lazily at the first
pixel request. Missing slides therefore do not prevent restoring annotations,
classes or image dimensions. Pixel operations report a reader error until the
link is restored. `relink(id, path)` opens a candidate and checks dimensions,
pixel layout and every pyramid level before changing the descriptor/server.
Manual calibration and object state remain intact. Existing references to an
older server retain that server; the document uses the new link.

Relink checks structural metadata compatibility, not byte identity. Same-sized
images with different pixels are not detected by a full-file hash. Source
fingerprints, explicit mismatch review and long-running job invalidation remain
work for the complete project workflow.

## Viewer and command line

```powershell
.venv/Scripts/python -m qupathxx --project study.qpxproj --image-id 2
build/project-wheel/Release/qupathxx_app.exe --project study.qpxproj --image-id 2
```

The normal launchers attach their image/demo to a project. The inspector shows
the destination path, dirty state and Save project action; Ctrl+S saves canonical
state. For programmatic viewing, set `GuiOptions.project` to a project whose
active document is passed to `show`. The GUI checks this association before
opening. Both launchers pass project-opening smoke checks with `--smoke`.
Image selection at launch uses `--image-id`; the in-window
[project browser](project-browser.md) also switches images and adds, renames or
relinks entries. [Background autosave/recovery](project-recovery.md) and dirty-close
decisions are implemented. [Explicit primary saves](project-save.md) now use a
worker with owner-thread baseline acknowledgment. [In-window recovery loading](recovery-maintenance.md)
also uses a worker; initial CLI loading remains synchronous. [Ordinary project-file opening](project-opening.md) uses a worker
and staged replacement. [Empty workspaces and confirmed image removal](project-lifecycle.md)
are implemented, as is reviewed checkpoint deletion. [Exact-destination abandoned
work cleanup](recovery-work-cleanup.md) and opt-in [retention](recovery-retention.md)
are implemented; cross-session work discovery remains M4 work.
[Moved-primary recovery association](recovery-association.md)
validates identity without advancing the checkpoint's baseline generation.
A hot rollback journal from a killed writer is recovered by a temporary read/write
connection only after SQLite reports `SQLITE_READONLY_ROLLBACK`; normal loads stay
read-only. See the explicit-save contract for permissions and process-death evidence.

## Build and evidence

`QUPATHXX_BUILD_PROJECT` defaults ON. CMake fetches and statically compiles pinned
SQLite 3.54.1, with SHA256 and upstream SHA3 identity recorded in the
[fixture manifest](../tests/fixtures/manifest.json). No SQLite DLL is needed.
The public-domain notice is packaged with the wheel. Disable the option to omit
storage; storage methods then report unavailable capability. A dependency-free
native build also disables `QUPATHXX_BUILD_INTERCHANGE`, GUI, OpenSlide and GEOS.
`has_project` and `sqlite_version` expose the build capability.
Projects containing polygons require a GEOS-enabled build. Linked slides need
OpenSlide when their pixels are requested; missing/unavailable readers do not
prevent restoring their stored dimensions and annotation state.

The installed GUI wheel passes 208 Python tests. Storage tests include a 100352-pixel-wide linked pyramid without embedded pixels and
exercise multi-image
restart, every ROI kind, cell/nucleus/class state, exact IDs/revisions, relative
moves, missing sources, relinking, Unicode paths, unknown schemas, malformed
geometry, stale writers and actual transaction rollback. GUI replay verifies
Ctrl+S, in-window switching, state isolation, addition, relinking and recovery. Thirteen native
suites pass with optional readers/geometry enabled, with storage/interchange
enabled and readers/geometry disabled, and with all optional dependencies disabled.

The [100000-object benchmark](benchmarks/project-100000-windows.json) records
initial save, full resave and validated load on local storage. The recorded run
measured about 225 ms initial save, 380 ms full resave and 201 ms validated load
for a 13.1 MB database including 2.36 MB of RGB pixels. Fixture and
descriptor construction are excluded. It does not establish autosave latency,
cold/network storage performance or broader vendor behavior. Schema migration,
GeoJSON/CSV interchange, QuPath-assisted migration, source fingerprinting and the
remaining M0-M7 gates remain open; M4 is not yet accepted.


## Validated bulk restoration

Native project/recovery loading now builds a private document's canonical object
and class maps directly. It no longer allocates before/after edit patches, applies
a synthetic edit and walks the map again to reset persisted object revisions.
Publication still follows complete validation; failed candidates are destroyed
without changing the caller's project or source file. Object identity/revision
and class definitions are checked before validation. The same object/ROI/class/
parent containment validator used by live edits checks every reconstructed object.

The loader builds child and class-count indexes and memoizes hierarchy depths.
A bounded 256-ID path detects cycles and enforces the existing 256-level limit;
validated ancestor depths are reused. Flat documents allocate no ancestor-depth
nodes. Stored per-object revisions, document revision, next-ID gaps, calibration,
class colors and empty undo/redo history are preserved. This path changes neither
the file schema nor live edit transaction behavior.

Nineteen additional storage tests cover both parent row orders, accepted depth
256 and rejected depth 257, disconnected cycles, missing parents, registry/text/
revision constraints, containment, non-area cells and nuclei on non-cell objects.
They also verify class counts, spatial queries, subtree deletion/undo, next-ID
continuation, resaving and unchanged invalid source bytes.

The [project startup run](benchmarks/project-startup-bulk-restore-100000-windows.json)
records 105.9 ms native loading versus the preserved 237.7 ms startup baseline.
The [recovery run](benchmarks/recovery-startup-bulk-restore-100000-windows.json)
records 113.6 ms versus 240.7 ms. Both verify all 100000 objects, preserved primary
bytes, 792 visible overlays and 3 MiB GPU payload. These separate warm local
single-host runs show roughly halved worker loading; they do not establish a
universal speedup or a memory-byte reduction. Temporary profiling measured
142.6 ms in the former document reconstruction stage of a 228.3 ms load.

The rendering limit remains separate: project/recovery runs still show 238.2/
207.1 ms maximum early frame work, despite about 16.8 ms p95. A
[1000-object run](benchmarks/project-startup-bulk-restore-1000-windows.json) loads
in 3.3 ms but still has a 276.3 ms early rendering maximum. The benchmark now
centers shorter object rows in view, so its supported 1000-object minimum can
exercise visible overlays as well as model loading. Initial rendering, other
hardware, cold/network storage and larger complex geometries remain open.


The fixture generator also now grows its embedded image for the advertised
one-million-object upper range. The [one-million-object run](benchmarks/project-startup-bulk-restore-1000000-windows.json)
records 891.3 ms native loading, 16.94 ms CPU frame p95 and 295.17 ms maximum
frame work, with all one million objects selected, 792 visible and 4096000 bytes
of GPU tile payload. This extends the measured flat-rectangle loading workload;
it does not establish complex-shape, memory-pressure or cold/vendor performance.
