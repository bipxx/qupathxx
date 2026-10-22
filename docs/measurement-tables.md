# Project ROI and tissue measurement tables

`MeasurementTableRequest` captures saved ROI channel means or saved tissue
components from a document or a selected set of project images. C++ owns the
snapshot and CSV writer; Python and ImGui call the same implementation.
No image pixels are decoded during capture or export.

```python
import qupathxx as q

request = q.MeasurementTableRequest(project, q.MeasurementTableKind.ROI)
print(request.record_count, request.row_count, request.capture_ms)
job = q.MeasurementTableExportJob()
job.request(request, "roi-measurements.csv")
status = job.wait()  # releases the Python GIL
assert status.state == "succeeded", status.error

# Tissue components, with explicit selected image IDs:
request = q.MeasurementTableRequest(
    project, q.MeasurementTableKind.TISSUE, image_ids=[first_image, second_image])
text = q.measurements_csv(request)
q.save_measurements_csv(request, "tissue-measurements.csv")
```

Both constructors take `options=MeasurementTableOptions()`. The document form
accepts `record_ids`; the project form accepts `image_ids`. Empty selections mean
all saved runs or all images. IDs are sorted and deduplicated, and every selected
ID must exist. Project selection accepts at most 10000 input IDs and the document
selection at most 100000. Records are ordered by image ID then record ID; tissue
components retain ascending labels. Images without runs contribute no rows.
An empty project or archive produces a header.

Direct `measurements_csv(document, kind, record_ids=..., options=...)` and
`measurements_csv(project, kind, image_ids=..., options=...)` overloads capture
first, then release the GIL while formatting. The corresponding save overloads
insert the destination path after `kind`.

## Data and units

Schema identifiers are `qpx.roi-table/1`, `qpx.tissue-table/1`, and their project
variants `qpx.project-roi-table/1` and `qpx.project-tissue-table/1`.
Project tables prepend identity, generation, project revision, image ID and image
name. Names are quoted and embedded quotes are doubled; CSV uses CRLF and
locale-independent round-trip numbers. ROI tables have 43 columns, tissue tables
70; project variants add five columns.

Common columns identify the archived run and source object/revisions, capture
and export document revisions, status at table capture, algorithm, input pixel
stream digest, selected body/nucleus, original ROI kind/bounds/area/length,
original effective calibration, physical ROI area/length, provenance timestamp,
source tree SHA-256, white reference and all three normalized stain vectors.
ROI kinds use the native enum values: rectangle 0, ellipse 1, polygon 2,
multipolygon 3, polyline 4, points 5. Analysis archives contain area shapes.
The input digest identifies the analysis tile stream, not the complete slide file.

ROI rows contain channel space, samples, tiles, tile size, three channel means
and elapsed milliseconds. Tissue rows include producer, actual level/downsample,
threshold/cleanup parameters and resource limits, sample and cleanup counts,
raw/kept components, component label/count/bounds/centroid/area and timings.
Bounds and centroids are in level-zero pixels. Component sample counts remain
in the sampled grid; component area is `samples * downsample**2` in level-zero
pixel area. This is sampled raster area, without contour clipping to an ROI.

Each tissue component has one row. A run with zero kept components has one
summary row with blank component fields; its run counts and parameters remain
available. This distinguishes an empty detection from an image with no run.

Physical values use the original effective X/Y calibration captured with the
analysis. Later calibration changes never rewrite historical measurements.
Missing legacy provenance leaves calibration, physical measurements, timestamp
and source hash blank. Pixel measurements remain available. Anisotropic ROI
length scales geometry independently along each axis. Overflow/nonfinite
computed values fail export and preserve the previous destination.

## Snapshot, bounds and worker lifetime

The request retains immutable native records and a compact status code per run,
plus per-image identity. It retains no live document, image reader, Python object
or mask copy. Later edits, verification, removals, relinking, project replacement
and wrapper collection cannot alter the table. Status is the existing archive
status at capture: current, object_changed, unverified, source_changed,
source_closed or verification_failed. `require_current=True` rejects a selection
containing any other status before starting a write. A loaded project remains
unverified until the existing replay/verification workflow succeeds.

Control calls and capture belong on the document owner thread. Capture walks
selected runs and validates their current source geometry. This cost depends on
run count and geometry complexity; it is reported as `capture_ms`. Work after
capture formats native values and writes CSV on one worker without a backlog.
The job exposes state, error, rows/total_rows, bytes, snapshot_bytes, capture_ms,
write_ms, document_revision and successful completed count. `poll()` is
nonblocking; `wait()` joins. Destruction waits for the accepted export.

Defaults are a 256 MiB CSV limit and a 256 MiB logical snapshot budget. Valid
ranges are 1 byte through 256 MiB for CSV and through 512 MiB for snapshots.
Accounting includes retained record payloads, image strings, entry storage,
vector capacity and 64 KiB writer allowance; repeated shared provenance may be
charged more than once. This is conservative logical accounting, not an RSS cap.
Row formatting and disk output use bounded groups rather than a complete CSV
copy. The returned-string API necessarily allocates the complete output.

Exports use a temporary sibling and atomic publication. Cancellation or failure
preserves the old destination and removes the temporary file. Cancellation
checks occur between rows and before publication; cancellation arriving after
commit reports success. A busy job rejects additional requests. Failed or
cancelled jobs can be reused.

## Desktop

Open **ROI and tissue tables**, choose ROI channel means or tissue components,
enter a CSV destination, and export the active image or whole project. The panel
shows progress/errors and offers cancellation. Shortcuts: Ctrl+Alt+Q exports the
image; Ctrl+Shift+Q exports the project; Ctrl+Alt+Shift+Q cancels. GUI options
`measurement_table_path`, `measurement_table_kind` (0 ROI, 1 tissue),
`measurement_table_require_current`, `measurement_table_byte_limit` and
`measurement_table_snapshot_budget` configure the same workflow. Native replay
adds EXPORT_MEASUREMENT_TABLE_KEY, CANCEL_MEASUREMENT_TABLE_KEY and the
MEASUREMENT_TABLE_SAVED readiness gate. Finite embedded runs join pending exports;
interactive close requests cancellation before joining.

## Remaining scope

These tables export the current ROI-channel and tissue-component archives.
Live annotation class/name joins, arbitrary user-defined feature registries,
fluorescence channels, classifiers and complete portable execution manifests
remain separate project-plan work. File observations are not yet associated
with reader/request generations; a later checksum cannot establish which bytes
an old run read.


## Verification

The focused installed-wheel suite passes 27 cases, including independent CSV
parsing, historical/legacy calibration, empty runs, nucleus/stain measurements,
pyramid sample scaling, exact limits, snapshot isolation, atomic failure/reuse,
cancellation, native lifetime, GIL release and image/project desktop replay.
The final installed GUI wheel passes 1011 Python cases, with the two existing
Windows file-symlink permission skips, in 238.56 seconds. Visual review verifies
the full selector, export controls and status in the narrow sidebar.
All 29 native full-build suites and all 28 suites in each core/minimal
configuration pass, including locale, pre-cancellation and immutable snapshots.


## Recorded Windows workload

[Raw measurements](benchmarks/measurement-tables-view-windows.json) come from
`tools/benchmark_measurement_tables.py --runs 1000 --images 1000 --objects 100000 --gui`.
Eight synthetic 256x256 images contain saved runs; the other 992 are empty.
Each populated image has 1000 ROI runs and one 4096-component tissue run.
The active image has 100001 selected/all-visible objects. Captures are repeated
20 times; native CSV is parsed independently and compared with the synchronous
writer, including project identities, physical areas and every row count.

| Measurement | ROI table | Tissue table |
| --- | ---: | ---: |
| Records / rows | 8000 / 8000 | 8 / 32768 |
| Capture p95 | 1.483 ms | 0.204 ms |
| Logical retained snapshot | 15312327 bytes | 35801775 bytes |
| CSV bytes | 3764842 | 18274755 |
| Background write | 20.397 ms | 38.031 ms |
| Maximum sampled poll | 0.021 ms | 0.069 ms |
| Desktop capture | 1.287 ms | 0.226 ms |
| Desktop background write | 21.183 ms | 69.287 ms |
| Desktop CPU frame p95 | 16.997 ms | 16.852 ms |
| Maximum desktop CPU frame | 17.325 ms | 17.089 ms |

The desktop uses an RTX 4090, 3675040 bytes of cached overlay and 262144 bytes
of GPU tile payload. Native binary SHA-256 is
`8d80d7fdd0a7dca2a08c4495a788376b6ae79531e16c9f714034d7d9a852bf91`;
source tree SHA-256 is
`214c36ffaf3a96ac0fbb1ba58680823fa2389581f970842ad94bed8d1a2bf2a1`.
These are synthetic measurements on one Windows PC. Broader complex geometry,
larger archives, real vendor slide catalogs and other operating systems remain
unverified; the selected object count is not the archived ROI run count.
