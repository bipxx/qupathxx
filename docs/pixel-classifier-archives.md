# Retained pixel classifier artifacts

`PixelClassifierArchive` retains immutable native training cohorts or source
prediction masks, together with their resolution-bound model and original
source/build provenance. `PixelClassifierRecord` stores that artifact in a
Document. C++ uses `qupathxx/pixel_classifier_archive.hpp`; Python exposes the
same values and functions.

```python
request = q.PixelTrainingRequest(document, labelled_ids, transform, options)
table = q.sample_pixel_training(request)
model = q.train_pixel_classifier(table)
archive = q.archive_pixel_training(table, model)
record_id = document.keep_pixel_training(request, archive, "Training cohort")

# The record is included in the project's next explicit or recovery save.
project.save("study.qpxproj")
loaded = q.Project.open("study.qpxproj").document(image_id)
cohort = loaded.pixel_classifier_record(record_id).archive
assert cohort.table is not None
values = cohort.table.numpy()  # Read-only, owned native storage.
q.save_pixel_classifier(cohort.model, "retained-model.qpc")
```

Training artifacts retain the original selected annotation IDs, revisions,
kinds, class names and exact geometry, plus sampled grid coordinates, owner
annotation IDs, class labels, feature values/schema, eligible/used counts,
conflicts, scan/tile statistics, seeds/options, calibration, transform and model.
They preserve captured geometry and labels after subsequent edits or deletion.
`annotations` contains the original selected areas, not only those that supplied
retained samples. The model's training identity must match the sampled values,
labels, class order and schema.

Prediction artifacts are prepared with `archive_pixel_prediction(result)` and
retained with `document.keep_pixel_prediction(request, archive, name)`. They
contain the original target geometry/kind/class/revision, crop and natural grid,
model, threshold/options, labels, validity, winning tree-vote fractions, counts,
timings and source/build provenance. The model remains reusable with the normal
resolution checks; the mask remains an audit of its original source and crop.

`table` is present only for training, and `result` only for prediction. Arrays
remain read-only unless an explicit writable NumPy copy is requested. Results
and views keep their native storage alive independently of the project or source.
Artifacts are prepared from immutable values and require no further source reads.
Python preparation and byte decoding release the GIL and can run on a caller's
worker. Retaining/removing a record belongs to the document owner thread.

## Authority and document revisions

Both keep methods require the exact still-current captured request and original
artifact snapshot. An independently captured equivalent request, a decoded
artifact, another Document, closed/replaced source or intervening document edit
cannot adopt old work. Successful retention increments the document revision,
marks its project dirty, and retires older live worker authority. The record
retains the still-current next-object counter at adoption; loading cannot roll
that counter backward and reuse a deleted unselected object ID. Archival
retention/removal is separate from object Undo/Redo, matching the existing native
analysis/classifier records. `pixel_classifier_records`,
`pixel_classifier_record_count`, `pixel_classifier_record(id)` and
`remove_pixel_classifier_record(id)` inspect and manage retained records.

Loading restores immutable data and provenance. It restores no live request or
publication authority. Loaded prediction masks can be traced for inspection,
but cannot be published using a new request. Predict again against the current
source/model and publish that current result. Retained input/output hashes are
not proof that an external source file is unchanged. Likewise, a checksum seals
an artifact's contents; it does not authenticate its author.

The F5 panel currently predicts, traces, publishes and saves/loads `.qpc` models.
Its retained-artifact browser and keep/load actions remain to be added. Python
can already seed the panel with a retained model through `GuiOptions.pixel_model`.

## Encoding, quotas and persistence

`to_bytes()` and `PixelClassifierArchive.from_bytes(data)` expose the standalone
`QPA` version-1 artifact. Project records use a `QPR` version-1 envelope containing
record identity, captured next-object counter and name. Both use little-endian numeric framing and SHA-256
checksums. The artifact also retains numeric cohort/mask hashes and decoded
source/model manifests. Geometry uses the existing lossless native `QRO` codec,
provenance its native codec, and models the portable `QPC` recipe.

Encoding is limited to 128 MiB, with envelope headroom; charged retained payload
is limited to 256 MiB. Each Document permits 10,000 pixel records within a combined
256 MiB charged budget. Names are bounded to 256 UTF-8 bytes. Decoding checks
sizes before arrays allocate, validates class/model/schema and training identity,
coordinates, geometry, grid/resolution, mask labels/votes/counts and numeric hashes.
Cohort and mask geometry verification has a 100-million estimated-operation preflight
limit. Charges are conservative estimates, including shared buffers; they are
not process RSS or allocator peak measurements. Native preparation supports
cancellation. Hashing, encoding and decoding are worker work rather than GUI
capture work.

SQLite schema 9 adds `pixel_classifier_records(image_id,id,payload)`. Schemas
1–8 open without migration; saving upgrades inside the same transaction as the
canonical rows. A failed save rolls back the migration and data together.
Explicit save snapshots and recovery/autosave snapshots retain immutable record
pointers and cached encoded artifacts, avoiding pixel-sized capture copies.
Batch materialization budgets include retained pixel payloads, and schema-9
checkpoint data digests include the entire pixel record table. Unmodified
schema-7/8 checkpoints retain their earlier digest contract.


## Measured save capture and portable fixtures

The [512-square](benchmarks/pixel-archives-512-windows.json) and
[1,024-square](benchmarks/pixel-archives-1024-windows.json) samples retain a
4,000-row, six-feature cohort and a full-area mask. Every label and winning vote
fraction is checked, as are cohort arrays, model/archive hashes and immutable
save capture after live record removal.

| Area | Mask encode | Mask decode | Owner save capture | Worker save write | Project reopen |
| --- | ---: | ---: | ---: | ---: | ---: |
| 512 × 512 | 26.79 ms | 25.40 ms | 0.024 ms | 36.02 ms | 31.36 ms |
| 1,024 × 1,024 | 117.14 ms | 113.85 ms | 0.025 ms | 102.56 ms | 144.47 ms |

Cohort encode/decode is about 2 ms in both. Charged mask payload is 4.99 MB and
19.15 MB respectively; complete charged save snapshots are 6.50 MB and 23.02 MB.
These are single shared-workstation CPU observations, excluding allocator/RSS
and hardware/biological acceptance. The 512 case also runs in hosted validation.
Reproduce with `python tools/benchmark_pixel_archives.py --size 1024 --output report.json`.

Synthetic original Windows `QPA1` fixtures under `tests/data/pixel-archives` preserve
fixed encoded and numeric hashes. The installed Windows tests read and re-encode
training, rectangle and polygon-with-hole values byte for byte. A backend-free
Linux native probe independently reads and re-encodes the Windows training and
rectangle artifacts; polygon topology validation requires GEOS. Full-backend
hosted candidates run the same fixed fixtures. Retained original source/build
metadata is intentionally unchanged by a cross-platform read.


The [local validation record](benchmarks/pixel-archives-local-windows-linux.json)
verifies 1,497 full-suite passes with six expected skips, 80 targeted cases, all
native configurations, strict typing, exact source/wheel/native identities and
installed software-GL launch. It retains the diagnosed shared-pointer/GIL cleanup
deadlock and its bounded regression, plus fixture/report and desktop replay
corrections. Retained artifacts, ID-counter rollback and namespace rejection,
transaction rollback, asynchronous/recovery snapshots and batch digest/budget
binding have explicit runtime coverage.
