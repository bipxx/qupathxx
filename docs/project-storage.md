# Native multi-image project storage

The native `Project` owns shared documents, stable image IDs, image names and
source descriptors. SQLite schema version 20 stores canonical object state,
class definitions, per-axis calibration, source metadata, the active image and
per-image camera/display views, retained analysis records, tissue masks, cell runs,
object-classifier artifacts, pixel training/prediction artifacts, external pixel
stream links, tissue microarray metadata, TIFF view descriptors, raw analysis
[spatial distance tables](spatial-analysis.md) optional [object properties](object-properties.md) and
[source verification](reader-source-association.md#project-verification-and-reviewed-replacement), historical image facts and saved analysis defaults. Schemas 1–19
remain readable; see [workspace migration](project-workspace.md) and
[analysis provenance](analysis-provenance.md), [tissue archives](tissue-archive.md)
[cell archives](cell-archive.md), [pixel classifier archives](pixel-classifier-archives.md),
[stream links](pixel-stream-records.md) and [tissue microarrays](tissue-microarrays.md).
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

## Historical image facts

Schema19 adds `image_properties(image_id, payload)`, a sparse row keyed by the
owning image. It reuses the bounded checksummed ObjectProperties codec for
metadata and measurements; ROI lock/color/probability state is rejected at image
level. Empty facts need no row. Schemas1-18 load with no image facts, and save
creates the table inside the existing canonical transaction. Bad payload types,
checksums, duplicate/orphan rows or invalid image-level editing state refuse.

`Document.image_properties` owns immutable historical maps. Changes use one
ordinary document history entry and leave object IDs, ROI facts, calibration,
selection and ROI/render revisions unchanged. Save snapshots share the immutable
value and charge its payload before starting IO. An in-flight save retains its
captured value while later edits remain dirty. The loader restores facts directly
into the private candidate without synthetic commands or persisted history.
See [image facts](image-properties.md) for the API and import/inspector contract.

## Saved image analysis defaults

Schema20 adds sparse `image_analysis_settings(image_id, payload)` rows. Immutable
owning values retain image type, profile/channel names, all three normalized
stain vectors, residual identity and positive finite white. The existing
checksummed facts codec carries a distinct typed marker; malformed, incompatible,
duplicate or orphan settings refuse before publication. Schemas1-19 default
empty. Table creation and canonical writes share the existing save transaction.

Set/clear uses ordinary staged document history. Snapshots copy the native value
and charge its payload; an in-flight save preserves its captured defaults while
later edits remain dirty. Settings do not rewrite explicit analysis requests or
retained recipes. See [saved defaults](image-analysis-settings.md) for Python,
import, GUI adoption and RGB8 application limits.

## Transactions and save conflicts

Saves use one `BEGIN IMMEDIATE` transaction with `synchronous=FULL`. Prepared
statements replace canonical rows for all images; source pixels are not read
for linked images. Existing schema/application identity is checked before rows
are removed. Commit updates the in-memory saved path, generation and saved
document revisions only after SQLite reports success. A failure rolls back the
transaction and leaves unsaved in-memory edits available.

The application ID is `0x51505858` and new `user_version` is 20. Schemas 1–19 migrate
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
coverage. Existing `test_project_save.py` also kills a real native writer after
its hot journal appears, verifies writable native recovery, and checks that a
read-only refusal preserves the journal for a later writable recovery.
`test_project_recovery.py` and `test_recovery_work_cleanup.py` cover abnormal exit,
released locks and abandoned native-autosave work. These establish their tested
process-death boundaries, not hardware power-loss durability. Reconcile these
results before selecting another fault; there is no automatic repeat campaign.
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
It retains that exact verified reader in the new link, publishing its source
observation immediately under a fresh wrapper generation. First use does not
reopen the path. PPM retains its verified owned pixels if the path is later
removed/replaced; TIFF/OpenSlide retain their existing reader/version behavior.
Manual calibration, TIFF selection/budgets and object state remain intact.
Existing references to an older server retain that server; closing the new link
does not close those old owners. Failed verification leaves the original link
and dirty state intact. Save snapshots and ordinary project reopening remain lazy.
See the [scoped handoff checks](benchmarks/project-relink-r5/README.md).

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


Opened native image documents
-----------------------------

`Project.add_document(document)` now infers the file link for an opened native
OpenSlide or quantitative TIFF source. It preserves that source's reader and,
for TIFF, its current axes/display selection. The project stores metadata and
analysis records rather than embedding slide pixels. An explicit conflicting
reader is rejected before changing the project. In-memory dense RGB images
remain embedded; caller-supplied source paths retain their existing validation.

The Hamamatsu NDPI validation packet exercises this path with real nine-level
pixels, anisotropic calibration and a saved/replayed LoG run. The new default attachment behavior passes local native and Python/full
Windows headless checks in its original packet. Retain source-specific receipts;
[current scheduling and validation](api-delivery-plan.md#validation-and-build-policy)
determine any new campaign. No database schema or existing archive encoding changes
were needed for that attachment workflow.

## Saved source verification (schema15)

`source_verifications(image_id,payload)` stores optional associations for external
images. The bounded2MiB QSV1 payload has a versioned binary header, source graph,
file fingerprint/status and SHA-256 trailer; canonical decoding rejects malformed,
orphan or embedded-image rows. Ordinary open restores immutable facts without
source I/O. Missing source files remain recoverable project entries.

Schemas1–14 remain readable and are upgraded on save within the same transaction
as project rows. Failure rolls back the schema/data and preserves the prior
committed project. Async snapshots retain the association. Clearing verification
is an explicit project mutation. See the [review API](reader-source-association.md#project-verification-and-reviewed-replacement)
and [scoped evidence](benchmarks/source-verification-r5/README.md).

### Recovery-gap reconciliation (2026-11-04)

Existing [installed results](benchmarks/annotation-migration-recovery-local/installed.xml)
record successful writable/read-only hot-journal cases, abnormal-exit lock release,
whole-snapshot publication during termination, locked cleanup refusal, cleanup
failure/retry and both explicit/automatic killed-native-autosave collection cases.
These assertions were inspected in the corresponding current tests; the older
results retain their original source scope and are not retesting the SOURCE1
product. SOURCE1 separately verifies its changed association persistence and
rollback. [Cross-session work collection](recovery-work-cleanup.md#evidence-and-remaining-scope)
already covers prior destination names and committed snapshot rescue.

No new generic process-kill or cleanup campaign follows from this reconciliation.
Unknown/unmarked debris and cooperating lock inodes are deliberately preserved.
Full power-loss/network-filesystem durability and broader storage-pressure bounds
remain unproved; they need a declared support/workload contract. The next concrete
R5 implementation gap is imported object properties, which the current GeoJSON
and saved-data routes explicitly report as dropped.

## Imported object properties (schema16)

[ObjectProperties](object-properties.md) retains historical string metadata and
named binary64 measurements in immutable owners shared by objects/history/save
snapshots. `object_properties(image_id,object_id,payload)` references the canonical
object identity. Its bounded checksummed QOP1 codec preserves signed zero and
special values; orphan/malformed/noncanonical data rejects before publication.
Schemas1–15 migrate within the existing save transaction. No input-source reads
or second Python object model are introduced. See the [local workflow evidence](benchmarks/object-properties-r5/README.md);
remaining editing-state/plane and release work stays in the queue.

## Object editing state (schema17)

Schema17 uses the existing object_properties table. QOP1 remains readable and
canonical for ordinary metadata/measurements; QOP2 adds lock/color/reported-probability
fields. Opening schema16 permits only QOP1. Saving advances the version in the same
transaction as object rows; a failure rolls back both. No new image read or separate
frontend store is introduced. See the [field contract](object-properties.md#object-editing-state-editstate1-locally-verified)
and [native/Python/UI evidence](benchmarks/object-edit-state-r5/README.md).


## Object planes (schema18; locally verified)

[Local workflow evidence](benchmarks/object-planes-r5/README.md) records the
declared scope and retained failures. The sparse `object_planes(image_id, object_id, c, z, t)` table stores non-default
object planes. Absent rows retain the legacy channel -1/Z0/time0 interpretation;
schemas 1–17 remain readable and migrate transactionally on save. Constraints,
loader checks and object foreign keys reject malformed/orphaned rows. Object
planes travel with immutable save blocks and undo/redo snapshots.

A document identifies one source series. Series changes still require another
document. `ObjectPlane(c=-1,z=0,t=0)` uses zero-based indices; channel -1 applies
to all channels, but Z/time are concrete indices. New interactive/native create
commands use an explicit `ObjectEdit.create/create_geometry(..., plane=...)`
when supplied; otherwise they inherit a parent plane or use the active displayed
Z/time. Geometry/name/class replacement keeps
the object's plane; `set_object_plane` and `ObjectEdit.plane` explicitly change
it. Parent Z/time must match the child and a channel-specific parent can contain
only that channel. Boolean/merge operands require identical planes.

GeoJSON and pinned saved-data decoding retain plane components, including matching
cell/nucleus planes. Live import/plane commands check actual source axis bounds;
non-default planes require the quantitative TIFF reader. Project reopen remains
lazy when a linked file is missing: it checks stored syntax/hierarchy without
opening every source; actual reader/view/request validation checks source bounds
when accessed. Out-of-range imports remain errors even with loss permission.

Spatial queries/picking and interactive selection expose only objects matching
the active Z/time and an enabled display channel. Document object enumeration
still includes all planes for archival/scripted access. Display intensity/stain
analysis refuses an object on another plane, or a channel-specific mask over a
mixed-channel composite. Raw analysis/training/prediction requires matching
Z/time and, for channel-specific masks, exactly the specified single channel.
Source/object replacement uses existing request identity checks to reject stale
publication. Explicit raw requests remain valid across display-only changes and
may sample an object's matching plane even when another plane is displayed.
Created tissue/pixel/streamed contour objects retain the source result plane,
including when publication is unclipped and a different plane is displayed.
Interactive imports retain all objects and select only currently visible ones. Shape-only/historical facts remain usable
without changing the display.

GeoJSON emits non-default planes for body and nucleus; measurement CSV adds
`plane_c,plane_z,plane_t` only when the selection contains a non-default plane.
No serialization of arbitrary Java plugins or execution of Groovy is introduced.

Axisless QPTMA imports retain their legacy default plane, independent of the
current display. QPTMA export refuses a non-default core plane rather than
silently dropping it; native project storage preserves it. The existing 2D
spatial-distance request refuses selections mixing Z/time planes; same-plane
geometry comparisons remain independent of the displayed plane.


## Retained planes on source replacement

[Local SOURCEAXES1 evidence](benchmarks/source-plane-replacement-r5/README.md)
verifies direct relink and reviewed adoption against retained object planes.
Display width/height/pyramid compatibility is followed by current Z/time/channel
bounds when non-default planes require it. A different candidate is checked
immediately before publication, including edits/undo after review. Refusal keeps
the old source/path/baseline and does not consume the review; repairing object
planes can make that same byte review applicable. Candidate metadata/reader
ownership is reused, with no pixel decoding or second reader open. A matching
live-source no-op is unchanged. Schema18 and public signatures are unchanged.
