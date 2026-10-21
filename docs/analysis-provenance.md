# Retained analysis records and ROI input provenance

Body/nucleus optical-density and supplied-stain summaries can now be retained
in the native Document and saved with a multi-image project. C++, Python and
ImGui use the same records. This extends [analysis jobs](analysis-jobs.md);
completion of a calculation alone still does not mutate a document.

## Keep, save and recheck

```python
import qupathxx as q

project = q.Project()
doc = q.Document(q.Image.demo())
image = project.add_document(doc)
obj = doc.add_rectangle(q.Rect(100, 100, 128, 128))
request = q.AnalysisRequest(doc, obj, q.StainTransform([[.65, .70, .29], [.07, .99, .11]]))
record_id = doc.keep_analysis(request, q.analyze(request))
project.save("study.qpxproj")

restored = q.Project.open("study.qpxproj").document(image)
assert restored.analysis_status(record_id) == "unverified"
job = q.AnalysisJob()
job.request(q.AnalysisRequest.replay(restored, record_id))
if job.wait(restored, obj).state == "ready":
    matched = job.verify_result(restored, record_id)
    print(matched, restored.analysis_status(record_id))
```

`keep_analysis(request, result)` checks the live request key and result metadata,
then adds a record and advances the document revision once. `AnalysisJob.keep_result`
uses its own current immutable request/result. Record IDs are the new document
revisions, so removing/reopening cannot reuse an older ID. Retention participates
in project dirtiness, ordinary/background saves and recovery snapshots. A stale
request cannot append or advance the revision. Failed capacity/insertion checks
roll back that prospective revision.

`AnalysisRecord` exposes `id`, `result`, immutable `roi` and `tile_size`. The result
includes algorithm version, original object/document revisions, operation,
body/nucleus choice, normalized stain directions, white values, three means,
sample/tile counts, worker duration and `input_sha256`. Values returned to Python
are read-only snapshots. `analysis_record(id)` retrieves one; `analysis_records`
copies all records; `analysis_record_count` is constant time.

`latest_analysis(object_id)` uses an ordered secondary index and returns only the
latest record for that object in logarithmic time. It does not scan/copy the
archive every frame. Records preserve earlier ROI/parameters/values after later
object edits or deletion. They are historical measurements, not silently
recomputed feature columns. `remove_analysis(id)` explicitly removes one and
advances the revision; absent IDs return false. These archive operations are
separate from object Undo/Redo. Table browsing, filtering, GUI removal and an
integrated analysis-history undo policy remain open.

Each document retains at most 100000 records and an estimated 256 MiB archive
payload, including record/map overhead estimates and each retained ROI payload.
The limits apply during append and validated loading. Shared geometry can make
the estimate conservative. This is not a total process/source/reader-memory
budget. Save snapshot accounting includes the archive estimate.

## Exact ROI input digest

The worker computes SHA-256 incrementally over the RGBA tiles already being
read for the summary. No extra image read or full-image allocation is required.
The `qpx.roi-channel-means/1` input stream is:

1. ASCII `qpx.rgba-tiles/1`, without a NUL terminator.
2. Seven unsigned 64-bit little-endian values: image width, image height,
   candidate-center bounds `x0, y0, x1, y1`, then tile side.
3. For each tile in the job's existing row-of-tiles order: little-endian uint64
   `x, y, width, height`, then its tightly packed straight RGBA bytes in row order.

The digest is 64 lowercase hex characters. It covers all bounding-box tile
pixels, including pixels excluded by the ROI mask or alpha policy. A changed
excluded pixel can therefore conservatively fail a recheck even when the means
stay equal. Geometry, operation, nucleus choice, tile side and all transform
coefficients are checked separately against the saved request. Different tile
partitions have different input streams; replay preserves the original side.
The stream/version must not be changed silently under this algorithm version.

This fingerprints the actual **decoded ROI inputs**, not the whole source file.
Changes outside these tiles are not detected. An already-open reader may retain
old source pixels; reopen/relink when validating a changed file. A multi-tile
read is not an atomic filesystem snapshot. Records do not retain original pixel
data, and cannot reconstruct a lost/modified source. Full-source fingerprints,
immutable input artifact management, reader identities/versions, job/environment
manifests and complete reproducibility after source replacement remain open.

Windows uses the system
[CNG SHA-256 provider](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptcreatehash).
A standard-library-only fallback implements SHA-256 from
[FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final) for other platforms.
Both implementations run the same known-vector/chunk-boundary tests on Windows;
Python independently reconstructs the tile stream with `hashlib`. No new
third-party runtime or Python hashing dependency is introduced. This is a content
consistency check, not a signature or proof of who performed an experiment.

## Session verification

Trust is derived state and is never saved. Restoring, copying, moving or assigning
a document gives a fresh authority token. Loaded records begin `unverified` when
their original object revision/geometry still matches. Missing/edited objects
remain `object_changed`, with their historical ROI/values retained.

`AnalysisRequest.replay(document, record_id)` recreates the saved operation,
body/nucleus choice, tile side, whites and all three normalized stain vectors.
It rejects changed object revisions/geometries. A validated canonical-basis
constructor preserves saved coefficient bits, avoiding another normalization
round on reopening. Verification runs the normal native kernel on a worker.
`verify_analysis(id, request, result)` or `AnalysisJob.verify_result` requires a
current matching request and exactly matches input digest, means and sample/tile
counts. It neither overwrites the historical record nor advances the document
revision. Verification is not a new saved measurement.

Status values are:

| Status | Meaning |
| --- | --- |
| `current` | This live document/source/object passed retention or exact recheck |
| `unverified` | No verification belongs to this live authority yet |
| `object_changed` | Original object revision/geometry is absent or differs |
| `source_changed` | The verified source instance was replaced/relinked |
| `source_closed` | The verified source is closed |
| `verification_failed` | Inputs or numerical summary/counts failed exact comparison |

Changing unrelated objects or physical calibration preserves unitless results.
Object Undo does not revive an older object revision. Verification does not poll
for external file edits continuously. An exact comparison may also fail after
a numerical implementation/platform change; saved values remain historical and
the freshly computed result remains available. Cross-platform numerical
acceptance/tolerances and upstream QuPath parity are separate unfinished gates.

## Schema 3 and ImGui

Schema 3 adds a typed `analysis_records` table referencing images, with unsigned
IDs/counts encoded as canonical decimal text, finite REAL numeric fields and the
existing native ROI geometry encoding. It intentionally has no foreign key to
current objects, so a deleted object's historical measurement survives.
Validation checks versions, kinds, flags, digest syntax, stain normalization/
invertibility, ranges, ROI bounds, sample/tile counts, ID/revision bounds, archive
capacity and orphaned image references. Unknown/malformed records reject the
candidate project rather than entering live authority. Finite altered means
can load only as unverified; rechecking detects them.

Schemas 1 and 2 open without rewriting. The next explicit save migrates to 3 in
the same transaction as canonical data; injected failures roll back both DDL and
rows. Save As leaves the legacy source unchanged. Schema-2 recovery snapshots
remain readable; newly published snapshots use 3. Older application versions
that only support schemas 1/2 cannot open schema 3. Background saves and recovery
capture records at the same baseline as objects, without exporting runtime trust
or Python/source ownership into their worker snapshots.

In the inspector, **Keep result / Ctrl+K** retains a ready current result once
per job generation. **Saved analysis** shows the latest selected-object record,
its verification label and historical channel values. **Recheck saved /
Ctrl+Shift+K** executes its original request in the native worker and updates the
session verification state. A failed recheck preserves the original record;
the user can keep the fresh result separately. **Saved provenance** expands the
record ID, algorithm and input digest. Opening a project does not automatically
read/hash every saved ROI or display historical values as verified.

## Evidence and remaining performance work

`test_analysis_records.py` covers an independent hashlib stream, exact replay,
status/history rules, stale append rollback, finite-summary tampering,
changed pixels with restored file size/timestamp, deleted-object history,
background save/recovery captures, malformed/orphan records and transactional
schema-2 migration/recovery compatibility and archive snapshot budgets.
Existing schema-1 migration/rollback tests now expect schema
3 on success. Native tests cover archive copies and portable/system SHA-256
agreement. GUI replay keeps, saves, reopens and rechecks without changing the
loaded document revision; a framebuffer capture verifies the saved-result rows.

The [100000-record benchmark](benchmarks/analysis-records-100000-windows.json)
also contains 100001 objects. Appending retained values takes 76.3 ms; 10000
latest-record lookups take 3.24 ms. It verifies rejection of record 100001 without
a revision change. Background saving records 50.1 ms owner-thread snapshot
capture, 484 ms worker write and 0.031 ms maximum poll; validated reopening takes
244 ms. Rechecking a 16384-pixel record takes 0.157 ms, with a 0.0073 ms verify
step. The selected viewer shows 910 visible objects, 3 MiB GPU tile payload,
16.84 ms sampled frame p95 and 17.27 ms maximum, with matching retained values.
This stresses many records sharing one captured ROI/result; it is not a benchmark
of 100000 independent analysis computations or varied large geometry archives.
Viewer timings exclude its first ten frames and swap wait. The 50 ms archive/
object snapshot capture remains a responsiveness target; no frame-budget claim
is made for that capture or large archive deletion/retirement. Immutable snapshot
sharing and broader source/hardware workloads remain optimization work.
