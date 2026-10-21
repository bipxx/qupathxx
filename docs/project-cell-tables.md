# Project cell measurement tables

A native project capture exports archived cell measurements across selected images
into one deterministic UTF-8 CSV. The project/image context disambiguates run IDs
and labels that are local to each document. Detection, image reads and geometry
measurements are not rerun during export.

```python
import qupathxx as q

options = q.CellTableOptions()
options.require_current = False  # Historical/unverified rows retain their status.
request = q.CellTableRequest(project, image_ids=[], options=options)
job = q.CellTableExportJob()
job.request(request, "project-cells.csv")
status = job.wait()  # Native worker, releases the Python GIL.
if status.state != "succeeded":
    raise RuntimeError(status.error or status.state)
print(request.image_count, request.record_count, status.rows)
```

An empty image selection means all catalog images. Explicit image IDs sort by
unsigned value and deduplicate; unknown IDs reject capture. Both explicit lists
and complete-catalog capture are limited to 10000 images. Every archived run in
each selected image is included, sorted by unsigned run ID, then dense cell label.
Images without runs and runs without cells contribute no synthetic data rows;
request counts still include them. An empty project writes the project header.

Python also accepts a project directly in `cell_measurements_csv(project,
image_ids=[], options=...)` and `save_cell_measurements_csv(project, path,
image_ids=[], options=...)`. The existing request/string/file/job APIs serve both
scopes. Document overloads and their 71-column `qpx.cell-table/1` bytes retain
their existing behavior.

## Context and schema

Project rows identify `table_schema=qpx.project-cell-table/1`. Five context fields
precede all 71 [document cell-table columns](cell-tables.md), for 76 columns total:

| Field | Meaning |
| --- | --- |
| `project_identity` | Persistent native project identity; unsaved projects already have one |
| `project_generation` | Captured persisted generation; zero before the first save |
| `project_revision` | Captured in-memory catalog/view revision, not a global measurement revision |
| `image_id` | Unsigned project-local image ID |
| `image_name` | Captured catalog name, including correctly quoted Unicode/commas/newlines |

`project_revision` is a workspace counter and can restart when a project opens.
It does not advance for every independent document edit. Each row's
`export_document_revision` captures its own image document authority; the
`captured_document_revision` remains the detector's original capture revision.
All currentness/status fields describe capture time. Neither generation nor a
stored pixel digest alone proves that live pixels remain unchanged.

The historical row key is `(project_identity, image_id, record_id, cell_label)`.
[Published cell references](cell-feature-links.md) use `(record_id, label)` within
their image document. Several publication sets may refer to the same archived
row. Project CSV does not duplicate a historical row for every published object,
nor replace captured values with current names, classes or edited geometry.
Tables that join current published-object metadata, project-wide ROI/tissue
measurement tables, complete file/binary/runtime manifests and QuPath numerical comparisons
remain open work. [Capture-time source/build facts](analysis-capture-provenance.md) are available
through the associated archived results. This CSV is a measurement view, not a
complete project manifest.

`CellTableRequest.project_scope`, `image_count`, `project_identity`,
`project_generation` and `project_revision` expose captured scope. For a document
request, project identity is empty and project generation/revision are zero;
`image_count` is one. A project request and its job stats report
`document_revision=0` because there is no single document revision. Per-image
revisions are in the rows.

## Snapshot and file publication

Capture reads the native catalog directly and retains immutable native run handles,
image names/extents, revisions and status strings. It does not call catalog file
availability checks or open linked readers. Missing linked source files therefore
do not prevent historical/unverified export. Capture runs on the project/document
owner thread, then formatting and writing release the GIL. Workers retain no
project, document, reader or Python wrapper ownership. Catalog renames, image/run
removal, source closure and wrapper destruction cannot change captured output.
Copying a request shares its native immutable data in constant time.

The existing default 256 MiB logical snapshot budget applies to the entire
selection, configurable to 512 MiB. It charges retained run payloads (including
aliases separately), metadata/handle capacities and one 64 KiB scratch allowance.
Default output limit is 256 MiB. Exact endpoint limits are checked; exceeding a
quota rejects the complete capture/export. Limits describe retained logical
payload and exclude temporary allocation/allocator/OS overhead, not process RSS.

`require_current=True` rejects the whole capture if any included run is not
current and verified; it does not silently omit stale images or records. This
check is not a promise that live sources stay unchanged after capture. The
single worker has no backlog and reuses the bounded row buffering, byte quotas,
progress, cancellation and atomic sibling-file replacement from document export.
Only `succeeded` proves that a complete file committed. Failure/cancellation
before replacement preserves the previous destination. Late cancellation after
replacement reports success.

## Desktop workflow

The shared **Cell table export** section contains a destination and current-run
requirement. **Export project cells (Ctrl+Shift+X)** exports all archived runs in
all project images, without requiring an annotation or active image. Empty
projects can export a header. **Export cell table (Ctrl+Alt+X)** continues to
export the selected annotation's latest saved run. **Ctrl+Alt+Shift+X** cancels
an active export. The row/error display and durable finite-run `GuiReport` serve
both scopes.

Replay uses `EXPORT_CELL_TABLE_KEY` with `shift=True` for project export; its
default remains document export. `CELL_TABLE_SAVED` waits for either export to
commit. Worker snapshots survive later workspace changes; captured data is
published even if the active image changes during writing.

## Validation and reproduction

Project tests compare every measurement and context field against independent
per-document CSV parsing. They cover colliding/local IDs, image/run ordering,
empty runs/catalogs, Unicode and quoted names, full unsigned image IDs, global
quotas, strict currentness, source absence, snapshot lifetime, worker cleanup and
atomic file reuse. Native tests cover selection, capture survival and exact
snapshot limits. Desktop tests cover no-selection/all-image export, empty
workspaces, strict-current rejection and byte-limit preservation.

```powershell
python tools/benchmark_project_cell_tables.py --output build/project-cell-tables.json
python tools/benchmark_project_cell_tables.py --images 1000 --objects 100000 --gui --output build/project-cell-tables-view.json
```

The synthetic fixture has eight independent images with 4096 five-sample cells
each (32768 rows); remaining catalog images are empty. It verifies image keys,
counts, values and streamed/in-memory byte equality. The viewer exports after
frame timing begins while keeping 100001 objects selected in the active image.


The [eight-image recording](benchmarks/project-cell-tables-8-windows.json)
exports 32768 rows (17402402 bytes) in 45.63 ms, about 718000 rows/s. First
capture is 0.0392 ms; 20 subsequent captures record 0.0023 ms p95. Native snapshot
charge is 49723543 bytes; submission is 0.0805 ms and maximum poll is 0.0341 ms.

The [1000-image catalog](benchmarks/project-cell-tables-view-1000-windows.json)
keeps the same eight measured images plus 992 empty images. First capture is
0.2595 ms, with 0.1417 ms p95 over the next 20 captures, charging 49806071 logical
snapshot bytes. Separate headless export is 46.26 ms. The active viewer keeps
100001 objects selected and visible across 180 frames: CPU frame work is
17.18 ms p95 and 18.63 ms maximum, with 0.7876 ms GUI capture and 107.25 ms
background export. The unchanged canvas uses 3675040 GPU payload bytes on an
RTX 4090. Export starts after frame timing begins. These are single synthetic
Windows observations; initial rendering, invalidated views, other hardware,
large complex ROIs and prolonged memory pressure remain separate workloads.

The [document compatibility recording](benchmarks/project-cell-tables-legacy-document-windows.json)
still produces 15469017 bytes with SHA-256
`9d6024e61d340ce357fdd91fc46308f58c9d74e74c7c71aa8c9528283fbf4c10`,
identical to the earlier document schema. Project output hashes additionally
depend on the captured project identity and catalog context.

The final installed-wheel regression passes 918 Python cases with two Windows
symlink cases skipped. All 26 native full-GUI suites and 25 suites in each
core/minimal build pass. The 44 focused export cases include 16 new project/
desktop cases. A native desktop capture verifies destination/currentness controls,
project export shortcut, successful state and the complete row count.
