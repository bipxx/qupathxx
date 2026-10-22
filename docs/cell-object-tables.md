# Joined cell object tables

Cell measurement exports can join live cell identity, classification and geometry
to the immutable measurements referenced by each cell. This supports analysis of
current classifications and edited cells while keeping detector values historical.
The same native capture/writer serves a document or selected project images.

```python
import qupathxx as q

options = q.CellTableOptions()
options.published_objects = True
request = q.CellTableRequest(project, options=options)
job = q.CellTableExportJob()
job.request(request, "cell-objects.csv")
status = job.wait()
if status.state != "succeeded":
    raise RuntimeError(status.error or status.state)
```

The string/file convenience overloads also accept these options. The default
`published_objects=False` retains the existing archived-label table schemas and
bytes. Joined rows identify `qpx.cell-object-table/1` (document, 92 columns) or
`qpx.project-cell-object-table/1` (project, 97 columns). No project schema migration
is needed: this is an export view over existing native cell references.

## Row selection and identity

An empty document run selection includes every live `Cell` object, including
manual cells and cells whose archives were removed. Annotation and Detection
objects are excluded. Several publications of the same detector label produce
separate rows identified by `object_id`; deleted/undone cells contribute no row.
Run/label identity remains in `record_id` and `cell_label` when a link exists.

An explicit document `record_ids` selection sorts/deduplicates and validates all
IDs, then includes only objects linked to those present runs. A selected run with
no published objects contributes no row. Project `image_ids` retain the existing
sorted, deduplicated, at-most-10000-image selection contract; each image includes
all its cell objects. Ordering is unsigned image ID, then unsigned object ID.
The project key is `(project_identity, image_id, object_id)`; document-local run,
label and object IDs must not be joined across images without this context.

`row_count` counts live cell objects. `record_count` counts unique present archives
actually referenced by those rows, once per image. Missing archives and manual
cells do not fabricate retained runs. `image_count` includes selected empty images.
Empty documents/projects produce the appropriate header without synthetic rows.

## Historical and current columns

The first 71 columns preserve the [archived cell table](cell-tables.md) meanings;
project tables prepend the five [project context](project-cell-tables.md) fields.
For resolved rows every historical value remains unchanged, apart from the new
`table_schema`. In particular, `status_at_capture` is the run status, and all
sample counts, compartment means and centroids come from the stored detector.

The joined schema appends these 21 fields:

| Fields | Meaning |
| --- | --- |
| `object_id`, `object_revision`, `object_name`, `object_parent_id` | Live object identity and metadata at table capture; parent zero means root |
| `object_class`, `object_class_color` | Captured class name and decimal RGB color; color is blank for no class |
| `object_status_at_capture` | Exact native object-feature status, independently of run status |
| `object_min_x_px`, `object_min_y_px`, `object_width_px`, `object_height_px` | Current body bounding box in level-zero pixels |
| `object_area_px2`, `object_length_px` | Current body shape area and boundary length |
| `object_nucleus_area_px2`, `object_nucleus_length_px` | Current nucleus geometry; blank when absent |
| `export_pixel_width_um`, `export_pixel_height_um` | Effective document calibration at table capture; unknown axes are blank |
| `object_area_um2`, `object_length_um` | Current body geometry under that captured effective calibration |
| `object_nucleus_area_um2`, `object_nucleus_length_um` | Current nucleus geometry under the same calibration |

Physical measurements use anisotropic native geometry, including analytic ellipse
lengths. They are blank unless both calibration axes are known. These are current
shape measurements, not physical reinterpretations of historical raster sample
counts. Detector features still refer to [capture-time provenance](analysis-capture-provenance.md).
A later calibration change cannot change a captured table's values.

`geometry_changed` preserves stored features while describing that the current
body/nucleus differs. `geometry_unavailable` also retains readable numeric
features when archived contours cannot be reconstructed. `archive_missing` keeps
the object's run/label and current geometry but leaves unavailable detector
fields blank. Manual cells use `no_features`, blank run/label and blank detector
fields. Extent, export revision and object metadata remain available in both
cases. Missing numeric values are empty CSV fields, never invented zeroes.

`require_current=True` rejects the entire joined capture if any included object
has a status other than `current`, including manual, missing-archive, unverified
and edited cells. Source-current run status alone is insufficient when published
geometry changed. This is a capture-time check, not a lock on subsequent edits.

## Ownership, budgets and worker behavior

Capture runs on the document/project owner thread. Storage-enabled builds scan
contiguous immutable object blocks and retain only blocks containing included
cells; they copy no per-cell names or geometry. Source status and archive payload
are resolved once per referenced run per image. A shared native geometry-status
helper checks each object against the captured contours, including strict mode.
The snapshot retains class colors, statuses and effective calibration. Builds
without storage create a private bounded block of selected cell values. Neither
path performs image reads or source-file existence checks. Formatting computes
current shape metrics on the native export worker from those captured handles;
it never accesses a live project/document, reader or Python wrapper.

The default 256 MiB snapshot budget (configurable to 512 MiB) now also charges
retained object blocks (including any neighboring unselected values), row/status
storage and handle/vector capacities. Storage-disabled captures charge their
private selected-value block. Shared geometry aliases are conservatively charged
inside each block. Existing archive
payloads, project/image metadata and the scratch allowance remain charged.
Capture/output limit failures reject the complete result; process RSS and
transient allocator overhead are outside these logical budgets. Output defaults
to 256 MiB. Copying the request shares the native snapshot in constant time.

The existing single native worker provides progress, cancellation between rows,
bounded row buffering and atomic sibling-file replacement. Failure/cancellation
before replacement preserves the previous destination; committed exports report
success. Live rename/reclassification, edits, calibration changes, deletion,
archive removal, image/catalog changes and wrapper destruction cannot change
already captured rows. Historical-label export remains available independently.

## Desktop workflow

Enable **Join live cell objects** in **Cell table export**. The UI explains that
names/classes/geometry are current and feature values archived. **Export image
cells (Ctrl+Alt+X)** captures all cells in the active image without requiring
a selected source annotation. **Export project cells (Ctrl+Shift+X)** captures all
images in joined mode. The existing currentness option, progress, errors and
cancel command apply. The saved-run export button is disabled in joined mode;
turn the option off to export archived labels from that run.

`GuiOptions.cell_table_published_objects` initializes the checkbox. Desktop replay
uses the existing export event and `CELL_TABLE_SAVED` readiness gate. Finite GUI
runs finish the export and return its durable result.

## Validation and reproduction

Python tests compare all archived columns, live shape metrics, Unicode/quoted
metadata, duplicate publications, run filters, edited/missing/manual cells,
calibration, snapshot lifetime, project identity, save/recovery, exact quotas,
atomic worker output and desktop exports without a selected source. Native suites
cover manual cells with optional dependencies disabled and resolved/missing
archive joins when GEOS is enabled.

```powershell
python tools/benchmark_cell_object_tables.py --output build/cell-object-tables.json
python tools/benchmark_cell_object_tables.py --images 1000 --objects 100000 --gui --output build/cell-object-tables-view.json
python tools/benchmark_cell_object_tables.py --copies 4 --output build/cell-object-tables-131072.json
```

The synthetic fixture contains eight independent images with 4096 five-sample
cells each, explicitly published into live cell objects. It verifies every joined
object key, archived count, class, physical body area and streamed/string byte
equality before recording timings. Repeated publication increases object rows
without duplicating a run; it is not additional detection work.

The final Windows recording captures 32768 rows at 3.83 ms p95 and writes their
21.1 MB CSV in 90.45 ms. A 1000-image catalog with 100000 unrelated annotations
captures at 5.42 ms p95. During export its active 104097-object selected/all-visible
viewer measures 16.87 ms p95 and 17.11 ms maximum CPU frame work, including a
5.68 ms GUI capture; worker writing is 151.95 ms alongside rendering. The captured
logical payload is about 92 MB, independent of most unrelated annotation blocks.

Publishing four sets per run produces 131072 rows, 217745007 logical snapshot
bytes and an 84.6 MB CSV written in 333.39 ms. Capture measures 18.68 ms p95;
this larger strict-capture workload exceeds one 60 Hz frame budget and remains
a responsiveness target. No whole-slide/vendor throughput or process RSS claim
is implied by these warm synthetic observations on one PC.

The archived-label regression writes 32768 rows in 41.64 ms and retains the
original CSV SHA-256, `9d6024e61d340ce357fdd91fc46308f58c9d74e74c7c71aa8c9528283fbf4c10`.
Raw evidence, including the native binary fingerprint:

- [32768 joined rows](benchmarks/cell-object-tables-32768-windows.json)
- [131072 joined rows](benchmarks/cell-object-tables-131072-windows.json)
- [1000-image selected viewer](benchmarks/cell-object-tables-view-1000-windows.json)
- [Historical document output](benchmarks/cell-object-tables-legacy-document-windows.json)

Nineteen new Python/desktop cases cover the joins, full-width object IDs,
numeric-only archives, worker geometry overflow and file preservation. Native
tests also retain the original three-field C++ aggregate initializer. The new
option is appended to the option structure; previous positional fields keep
their meaning. A visual capture verifies readable controls, image/project export
buttons and the successful row count without a selected source annotation.


The final installed GUI wheel passes 958 Python cases, with two file-symlink
cases skipped on this Windows account (the junction case passes). All 27 native
full-GUI suites and 26 suites in each core/minimal configuration pass. The final
full run includes the corrected narrow-sidebar button label and the source-byte
fingerprint check; the focused archive/table/provenance/desktop selection passes
103 cases.
