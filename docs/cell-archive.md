# Archived cell runs

Cell runs can be retained independently of published Cell objects. An immutable
record preserves both lossless label planes, selected native level/grid, source
ROI and object revisions, transform/white reference, every detection and contour
parameter, input tile digest, marker/remapping information, compartment counts
and RGB/channel means, algorithm identity and elapsed time. C++, nanobind and
ImGui use the same records and project/recovery storage.

```python
request = q.CellRequest(doc, source_id, transform, parameters)
result = q.detect_cells(request)
record_id = doc.keep_cells(request, result)
assert doc.cell_status(record_id) == "current"
project.save("study.qpxproj")

loaded = q.Project.open("study.qpxproj")
restored = loaded.document(image_id)
record = restored.cell_record(record_id)
assert restored.cell_status(record_id) == "unverified"
replay = q.CellRequest.replay(restored, record_id)
fresh = q.detect_cells(replay)
assert restored.verify_cells(record_id, replay, fresh)
```

`CellJob.keep_record` retains a completed current job; `CellJob.verify_record`
checks a replayed job. `keep_result` still publishes Cell objects and remains a
separate operation. `cell_records`, `cell_record_count`, `latest_cells` and
`remove_cells` provide inspection/removal. Returned records/results are immutable
native shared values; modifying Python parameter/feature snapshots cannot change
an archived run. Results remain usable after their document or job is destroyed.

The GUI offers **Keep cell run (Ctrl+Shift+C)** and **Recheck cells (Ctrl+Alt+C)**.
It displays the latest run's ID, cell count and current status. Rechecking uses
the saved parameters and transform, irrespective of current inspector settings.
After a successful recheck, normal previews and **Add cells** use the regenerated
job. Replay supports `KEEP_CELL_RUN_KEY`, `RECHECK_CELL_RUN_KEY`, `CELLS_ARCHIVED`
and `CELLS_VERIFIED`. A retained run does not create another undoable object batch.

## Currentness and publication

Keeping a run requires the exact live request ticket, matching document authority,
source object and immutable native result. A different request with
identical numeric settings cannot claim that result. Record IDs share the
canonical document revision namespace with analysis and tissue records. Retaining
or removing advances the revision and marks the project dirty, without adding an
Undo entry; removing a run leaves published annotations intact.

Reopened/copied documents begin `unverified`. Rechecking requires the unchanged
source object revision/ROI and compatible sampled level. It compares captured
parameters, transform, digest, samples, threshold counts, both complete label
planes, markers/remapping and every compartment feature. A failure becomes
`verification_failed`; it cannot be promoted by matching the digest alone.
Verification is derived state and does not modify canonical revisions or dirty
the project. Timing differences are excluded from the comparison.

Changed/deleted objects remain archived as `object_changed`; Undo does not restore
the old authority revision. Closed/replaced readers produce `source_closed` or
`source_changed` when applicable. Cancelled/stale/failed jobs cannot retain or
verify a partial result. Saved geometry/measurements remain inspectable as history.
Neither a checksum nor a stored input digest alone proves that current pixels
still match a run.

Contours are derived geometry and are **not duplicated in the run blob**. The
native loader rebuilds them from validated historical labels, subject to current
GEOS availability and saved limits, sharing detector normalization. If those
limits prevent reconstruction, masks/features remain readable with contours unset.
A pixel replay still requires the original parameters and computes a fresh result. Published body/nucleus objects already use normal
project geometry storage. Numeric archives work without GEOS; polygon ROIs still
require the existing optional geometry backend. Automatic-level replays reject a
changed level choice rather than silently comparing a different grid.

## Storage and resource policy

Project/recovery schema 5 adds a strict `cell_records` table with image/record ID,
producer, source geometry and run blob. Schemas 1–4 remain readable. Saving an old
project migrates and writes canonical rows in one SQLite transaction, so injected
save failures roll back the table creation and prior data together. Unknown
schemas, orphaned image rows and record IDs colliding across archive types fail
before a loaded workspace is published.

`QCL` encoding version 1 uses little-endian integers and binary64 measurements,
with the fixed numerical identity `qpx.distance-watershed/1`. Each uint32 label
plane independently chooses raw or run-length storage, whichever has fewer
payload bytes. A SHA-256 checksum covers the transport and canonical ROI WKT.
The decoder validates version, finite values, parameters, grid/source bounds,
marker order/mapping, feature counts/means, run lengths, label range, ROI sample
inclusion and nucleus/body relationships. Statistics are rebuilt from labels;
body labels must respect the saved Euclidean nearest-nucleus expansion and tie
policy. Trailing data and checksum-preserving invalid structures are rejected.

Each document permits at most 10000 runs and 256 MiB of conservatively charged
logical retained payload. Shared results are charged once per record even when
their physical buffers alias. Failed retention leaves revisions/indexes intact;
removal reclaims that reservation. Blob and decoded-work caps apply separately.
Decode scratch reserves both rasters, distance/nearest-site buffers, line scratch,
features, component statistics, markers and ROI payload before large allocations.
These logical limits exclude allocator/GEOS/SQLite overhead and do not guarantee
process RSS. Save budgets include the live record payload and captured handles.

Save/recovery capture copies native shared record handles in ID order and shares
the immutable buffers; it does not scan labels or copy feature tables. Capture
is linear in the bounded record count, independent of raster size. Owner-thread
removal/reuse after capture cannot alter the worker snapshot. Native shared
ownership avoids Python-wrapper destruction or GIL acquisition on save workers.
Encoding/decoding have cancellation checkpoints. The input digest follows the
[cell sampling policy](cell-detection.md) and includes decoded native tile bytes.

Coarse curved-ROI testing also exposed a contour boundary issue: independently
clipped nuclei/bodies could fail strict GEOS coverage despite an empty area
difference. Before a new result escapes its worker, only such zero-area remnants
can be normalized by union. The replacement must cover both original geometries,
remain in the source ROI and fit contour budgets. Area-bearing protrusions still
fail. Generic ROI containment predicates are unchanged.

[Published cell feature links](cell-feature-links.md) preserve run/label identity
through object edits, history and schema-6 storage.
[Project-wide CSV](project-cell-tables.md) combines archived rows with image
identity. [Native cell tables](cell-tables.md) export archived compartment features through
Python and ImGui with immutable capture and background atomic publication.
Full source/environment manifests, physical filters, marker/split/merge editing,
joined current-object/ROI/tissue tables and pinned QuPath comparisons
remain unfinished. The complete
M0–M7 project scope stays active.

## Reproduction

`tests/test_cell_records.py` compares every numeric field through save/reopen and
recheck, including stain channels, empty/filtered results and fractional pyramids.
It exercises modified source pixels, finite feature tampering, invalid transport
with recomputed checksums, migrations, quotas, snapshots and Python-wrapper-free
worker cleanup. Desktop cases retain, reopen, verify and publish through ImGui.
The native suite exhausts all 512 binary 3×3 masks at both connectivities and checks
authority, cancellation, save isolation and record limits.

Run with the installed wheel:

```powershell
python tools/benchmark_cell_archive.py --records 8 --output build/cell-archive.json
python tools/benchmark_cell_archive.py --objects 100000 --gui --output build/cell-archive-view.json
```

The synthetic 1024² disk fixture verifies all nucleus labels independently,
compares restored body labels/features, and rechecks the first run before timing
capture, storage, loading and verification. Viewer runs preserve all 100001 selected
objects and report canvas/image/mask GPU payloads separately.

The [eight-run recording](benchmarks/cell-archive-8-windows.json) encodes each
1024², 256-cell run to 259620 bytes. Native containers retain 9384134 bytes per
original result; that recording predates loader contour reconstruction, so its
decoded results omit derived contours and retain 8480168 bytes.
Eight shared runs capture in 0.016 ms while conservatively charging 78227751
snapshot bytes; write/load record 53.21/245.06 ms and result verification 0.91 ms.

The [100001-object viewer](benchmarks/cell-archive-view-100000-windows.json)
keeps every object visible and selected, retains a run in 0.0332 ms and captures
a save in 0.0258 ms. Frame p95 is 16.90 ms and maximum 114.68 ms. Image and mask
payloads are 4 MiB each; the canvas cache adds 3675040 bytes. Invalidated canvases
still render the full dense scene. These are warm synthetic Windows observations
on one machine; broader geometry, readers, hardware and storage remain unproven.

The installed wheel's full regression passes 837 tests with two Windows symlink
cases skipped. All 46 focused archive/desktop cases pass, including eleven added
after that run for identity, transport, source and budget checks. All 25 native
suites pass with GUI enabled and 24 in each core/minimal build.
