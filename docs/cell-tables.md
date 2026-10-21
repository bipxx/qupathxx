# Cell measurement tables

Archived cell features export through the native C++ engine as deterministic
UTF-8 CSV. The file contains one row per cell, keyed by `(record_id, cell_label)`.
It includes source/capture revisions, captured currentness, input digest, sampled
level and parameters, transform/white reference, compartment sample counts,
level-zero centroids and all RGB/channel means. No image pixels or geometry
measurements are recomputed during formatting.

```python
import qupathxx as q

options = q.CellTableOptions()
options.require_current = True
request = q.CellTableRequest(doc, [record_id], options)
job = q.CellTableExportJob()
job.request(request, "cells.csv")
status = job.wait()  # Releases the Python GIL.
if status.state != "succeeded":
    raise RuntimeError(status.error or status.state)
print(status.rows, status.bytes)
```

`cell_measurements_csv(request)` returns a string; `save_cell_measurements_csv`
writes synchronously. Python also accepts `document`, optional `record_ids` and
`options` directly for these two functions. Capture accesses the document on its
owner thread; formatting and file I/O release the GIL. CSV support works with
GeoJSON, GEOS and project storage disabled when the source run itself is supported.

The GUI offers a CSV destination and **Export cell table (Ctrl+Alt+X)** below the
latest saved cell run. It exports that run, shows rows processed and terminal
status, and can require current verified data. **Cancel table** or
**Ctrl+Alt+Shift+X** cancels a pending export. `GuiOptions` supplies
`cell_table_path`, `cell_table_require_current`, `cell_table_byte_limit` and
`cell_table_snapshot_budget`. Replay can wait for `CELL_TABLE_SAVED`; finite runs
finish an active export before returning its durable result in `GuiReport`.

## Schema and units

Every data row identifies schema `qpx.cell-table/1`. Records sort by unsigned ID,
then cells by dense label. Requested ID duplicates are removed, unknown IDs fail
capture, and an empty ID list selects all archived runs. Empty runs contribute
zero rows; an empty selection still writes the schema header. CSV is a measurement
view, not a replacement for complete [run archives](cell-archive.md).

| Columns | Meaning |
| --- | --- |
| `record_id`, `cell_label` | Run-local cell identity within this document |
| `source_object_id`, `source_object_revision` | Source ROI authority at detection |
| `captured_document_revision`, `export_document_revision` | Detection capture and table capture revisions |
| `status_at_capture`, `producer`, `algorithm`, `input_sha256` | Historical status and stored run identity |
| `image_width_px`, `image_height_px`, `level`, `downsample` | Source image extent and actual native sampling |
| `detection_feature`, `channel_space` | Threshold feature and meaning of the three measurement channels |
| Threshold, radius, size, connectivity, level, tile and budget fields | Captured detector/contour parameters; `requested_level=-1` means automatic selection |
| `white_r/g/b`, `stain_1_r/g/b` through `stain_3_r/g/b` | Captured white reference and normalized transform basis |
| `nucleus_samples`, `cell_samples`, `cytoplasm_samples` | Sample counts; cytoplasm excludes nuclei |
| `nucleus_centroid_x/y_px`, `cell_centroid_x/y_px` | Mean sample-center coordinates in level-zero pixels |
| Compartment `mean_r/g/b` | Means of original unsmoothed RGB samples |
| Compartment `mean_channel_1/2/3` | Means of original transform channels |

For OD-sum runs, `channel_space=optical_density_rgb`: channels are OD R/G/B.
For stain-feature runs it is `stain_concentrations`: channels are coefficients
in the recorded basis, including the signed third residual. These are not
calibrated molecular concentrations. Empty cytoplasm has zero samples and six
empty mean fields, preserving the distinction between absent and zero-valued
measurements. Length parameters use sampled pixels; counts are not multiplied by
downsample. Centroids use the sampled grid, not clipped-polygon centroids.

IDs are written as exact decimal integers, including the full unsigned 64-bit
range. Finite doubles use locale-independent round-trip precision. Text fields
are quoted, embedded quotes doubled, and records end with CRLF. Importers should
preserve ID columns as integer/text values rather than floating point.

Rows refer to archived labels, **not published object IDs**. Editing/classifying
published cells does not rewrite historical features. Persistent mapping of
published object features, project-wide multi-image tables and complete source/
environment manifests remain open work. No physical-area column is synthesized
from current calibration, which may differ from capture time.

## Capture, file publication and limits

`CellTableRequest` captures immutable native record handles, status strings, image
extent and document revision. Copies of the request share that native snapshot.
Later record removal, source edits/closure or destruction of Python wrappers
cannot change its bytes. The default permits historical/unverified records with
their explicit status. `require_current=True` rejects the whole request if any
selected record is not current; it checks at capture time. It does not promise
that the live document remains unchanged while an export runs.

The request retains no document or reader reference. Capture is linear in the
bounded run count and charges retained run payload plus handles/statuses and
64 KiB scratch. The default logical snapshot budget is 256 MiB, configurable
from one byte to 512 MiB. Aliased runs are charged separately. The output limit
defaults to 256 MiB and may be reduced to one byte. Invalid selections, budgets
and currentness requirements do not mutate the document.

File export formats rows on one native worker with no backlog. It buffers up to
32 KiB plus one bounded row and writes exclusively created temporary files beside
the destination. Only a completed, flushed, closed file replaces the destination.
Byte limits, cancellation, write or replacement failures remove the temporary
file and preserve the previous destination. GeoJSON and general measurement CSV
exports use the same native atomic-file helper.

Cancellation is checked between rows and file chunks, before flush and before
replacement. A cancellation arriving after replacement reports success, since
the complete file has already committed. Progress `rows` and `bytes` count
formatted output, which can be buffered before it reaches disk; only `succeeded`
proves publication. `completed` counts successful files across reuse. Errors or
cancellation permit another request. Controller methods remain owner-thread
operations; the worker owns only native snapshot handles and atomic progress.

Synchronous string export holds the full CSV string and Python may copy it.
Use the file worker for large tables. Logical limits exclude string allocator,
driver and OS overhead; no process RSS bound is claimed. Record retention still
uses its separate archive quota, and no new measurements are added to history.

## Validation and reproduction

Python tests independently parse CSV and compare every compartment value and
transform channel across features, expansion and coarse levels. They check full
unsigned IDs, quoted producer text, exact byte/snapshot limits, immutable capture,
source closure, failed replacement, cancellation and worker cleanup without Python
owners. Desktop cases compare complete native/GUI CSV bytes, retain/export a new
run, gate unverified export, and preserve capture identity after source deletion.
Native tests additionally replace the numeric locale and check pre-cancellation,
atomic file preservation, resource endpoints and controller reuse.

```powershell
python tools/benchmark_cell_tables.py --output build/cell-tables.json
python tools/benchmark_cell_tables.py --objects 100000 --gui --output build/cell-tables-view.json
```

The fixture has isolated five-sample nuclei with zero expansion. It checks the
expected cell count, parsed row counts/values, selected run IDs and equality of
streamed and in-memory CSV before recording throughput and capture/poll latency.


The [before](benchmarks/cell-tables-before-windows.json) and
[optimized](benchmarks/cell-tables-32768-windows.json) Windows recordings export
32768 rows (15469017 bytes) from eight shared 4096-cell runs. Hoisting invariant
metadata formatting out of the cell loop reduces worker time from 78.32 to
38.17 ms on this machine, with identical output SHA-256. The optimized run
records 0.0104 ms capture, 0.115 ms submission and 0.0222 ms maximum poll, charging
49722624 logical snapshot bytes. It processes about 858000 rows/s. These are
single warm synthetic observations, including local file flush/replacement;
they do not establish general storage or hardware throughput guarantees.


The [all-visible viewer](benchmarks/cell-tables-view-100000-windows.json) keeps
100001 objects selected and visible during a native latest-run export. It records
16.93 ms p95 and 17.25 ms maximum CPU frame work, 0.0024 ms GUI capture and
11.21 ms worker export for that 4096-row run. Export is requested after timing
starts; the unchanged canvas uses 3675040 GPU payload bytes. The separate
32768-row export in that recording takes 43.34 ms. The viewer runs 180 frames on
an RTX 4090; invalidated canvases, initial rendering, other devices and prolonged
memory pressure remain separate workloads.

The final installed-wheel regression passes 876 Python cases with two Windows
symlink cases skipped. All 26 full-GUI native suites and 25 suites in each
core/minimal configuration pass. A native desktop capture additionally verifies
that the destination, export button, successful status and row count are readable.
