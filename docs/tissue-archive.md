# Persisted tissue masks and provenance

Native tissue archives retain the complete label raster, component statistics,
source ROI/body-or-nucleus choice, normalized stain basis and white reference,
threshold/cleanup/filter parameters, sampling level and scale, captured object
and document revisions, algorithm identity, timings and exact tile input digest.
Each record also names its producing engine (`qupathxx/0.1.0`). Source paths,
reader selection and pyramid metadata remain in the parent project image entry.
Full-file hashes and a complete compiler/dependency/environment manifest remain
unfinished M5 provenance work.

```python
request = q.TissueRequest(document, source_id, transform, parameters)
mask = q.detect_tissue(request)
record_id = document.keep_tissue(request, mask)
project.save("study.qpxproj")

restored_project = q.Project.open("study.qpxproj")
restored = restored_project.document(image_id)
record = restored.tissue_record(record_id)
assert restored.tissue_status(record_id) == "unverified"
replay = q.TissueRequest.replay(restored, record_id)
fresh = q.detect_tissue(replay)
assert restored.verify_tissue(record_id, replay, fresh)
```

Each result has a native capture ticket. Keeping or verifying requires the
matching request and result as well as current document authority, source and
object revision. A newly constructed request cannot certify a result from a
different capture. Tickets contain no reader or Python ownership and are not
serialized. `TissueJob.keep_result` and `verify_result` use their captured
request after polling against the current document.

`tissue_records`, `tissue_record(id)` and `latest_tissue(object_id)` return
immutable shared native records; retrieving a record does not copy labels.
Python's explicit `labels`/`mask` properties still return owned byte snapshots.
`keep_tissue` advances the canonical document revision once; `remove_tissue`
advances it only when a record exists. Archive changes mark projects dirty and
are independent of object Undo/Redo. Deleting an object retains its historical
records. Removing records does not invalidate already returned snapshots.

Verification is session state and does not change revisions or saved files.
Reopened, copied or independently assigned document authorities require a
recheck. Status is `current`, `unverified`, `object_changed`, `source_changed`,
`source_closed` or `verification_failed`. Rechecking compares parameters,
geometry, object revision, sampling, statistics, input digest and every label.
Timings and unrelated document edits do not affect equality. A changed sampled
input fails verification even when threshold labels are identical. Object edits
followed by Undo still change the object revision. Replaying rejects changed
objects and sampling pyramids. Calibration/display changes leave masks valid.

The inspector offers Keep tissue mask (Ctrl+U), Recheck saved mask
(Ctrl+Shift+U) and Remove saved mask. Rechecking runs the native worker and
reconciles verification on completion. Loaded values are historical until
verified; fresh worker output supplies the live preview and contour workflow.
Readiness replay adds `TISSUE_ARCHIVED` and `TISSUE_VERIFIED` conditions.

## Schema, encoding and resource limits

SQLite schema 4 adds `tissue_records(image_id,id,producer,geometry,mask)`.
Schemas 1, 2 and 3 remain readable without rewriting them. Saving migrates old
schemas in the same transaction as all canonical rows; failures roll back
tables, version changes and data together. Background saves and recovery
snapshots use the same record codec. GeoJSON exports annotation geometry and
does not contain the mask archive.

The `QTM` version-1 blob uses little-endian integers and IEEE-754 doubles.
Labels use raw uint32 values or lossless `(run length, label)` uint32 pairs,
whichever payload is smaller. Component bounds, sample counts and centroids
are reconstructed from the labels, preserving native level-zero formulas.
An appended SHA-256 covers the blob and canonical ROI WKT. This detects
accidental corruption and geometry/payload mismatch; it is not an authenticity
signature. The existing input digest independently identifies decoded RGBA
tile bytes and read geometry, including level-aware origins.

Loading checks the checksum, finite numbers, enum/parameter limits, identities,
ROI/image/grid alignment, pyramid scale, exact work estimate, stage counts,
dense label order, run lengths, component sizes and foreground totals. Counts
and encoded lengths are checked before raster allocation. Semantic rechecking
against source pixels remains necessary; structural loading alone does not
prove that labels came from a particular source or algorithm run.

Each document permits 10,000 records and 256 MiB of logical archive payload.
Accounting includes retained raster/component capacity and ROI payload, even
when multiple records share one allocation. Snapshots include that payload in
their existing memory budget. Live indexes stay on the owner thread; save
workers share sealed blocks of 256 immutable records. Removing or replacing a
slot clones one block, preserving a captured snapshot without copying raster
bytes. Codec scans, hashing and large-run expansion check cancellation.
Transient encoded blobs, decoder statistics, SQLite storage and allocator
overhead are additional memory; the logical budget is not a strict RSS cap.

## Verification and performance recordings

The installed wheel passes 727 Python cases with two unavailable Windows
file-symlink cases skipped. Sixty-two new archive/desktop cases cover exact
roundtrips, raw/RLE payloads, custom stain bases, nucleus/body ellipses, native
pyramids, corruption with recomputed checksums, rollback, source changes,
worker teardown and snapshot isolation. All twenty-one native suites pass in
full/core/minimal builds. The native codec also checks all 512 three-by-three
masks under four- and eight-connectivity and the 10,000-record boundary.

The [rectangle recording](benchmarks/tissue-archive-4194304-windows.json)
stores 4194304 labels in 107548 bytes, versus 16777216 raw label bytes. Keeping
the native result records 0.056 ms and save capture 0.017 ms; writing takes
75.28 ms, reload 23.40 ms and exact rechecking 1.82 ms after recomputation.
The [checkerboard recording](benchmarks/tissue-archive-raw-4194304-windows.json)
uses raw labels, taking 16777724 encoded bytes including metadata/checksum.
Its write/load/recheck times are 155.27/75.47/2.56 ms. These files also embed
12 MiB of source RGB pixels.

The [eight-record recording](benchmarks/tissue-archive-shared-8-windows.json)
shares one native mask across all records, checks each persisted result, and
records 0.019 ms capture despite 198014935 logically charged snapshot bytes.
Reload materializes independent rasters and takes 84.13 ms. The
[100001-object viewer](benchmarks/tissue-archive-view-100001-windows.json)
records 0.043 ms keeping, 18.10 ms capture and 271.72 ms writing. Object copying
remains linear. Its selected viewer records 17.17 ms sampled frame p95 and
20.28 ms maximum, with 876 visible objects and 16 MiB each of image/mask GPU
payload. These are synthetic recordings, not general latency or QuPath parity
guarantees. Reproduce with:

```powershell
build/check-install/Scripts/python.exe -I tools/benchmark_tissue_archive.py
# Variants: --checkerboard, --records 8, or --objects 100000 --gui.
```
