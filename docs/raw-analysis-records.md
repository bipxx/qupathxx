# Raw analysis records

This work adds persistent quantitative measurements for the native TIFF reader.
The declared scope passes [local verification](benchmarks/raw-records-local-windows-linux.json);
the [hosted verification receipt](benchmarks/raw-records-ci-windows-linux.json)
also passes all five jobs for implementation commit `bd422ccc21e397549aacf80deddd1c34067db488`.
This is an M5/M6 increment, not a completed
milestone or a claim of QuPath compatibility.

`RawAnalysisRequest` captures a rectangle or an object's body/nucleus ROI, selected
channels, natural pyramid level, Z, time, calibration and original object context.
Native workers read typed samples in bounded tiles. They calculate finite sample
counts, nonfinite counts, means, minima and maxima per channel. Means use float64;
the input hash preserves exact sample bits, including 64-bit integers and NaNs.
Display channel projections and intensity windows do not affect these values.

```python
request = q.RawAnalysisRequest(document, raw_region, object_id=object_id)
result = q.analyze_raw(request)
record_id = document.keep_raw_analysis(request, result)
table = q.RawTableRequest(document, [record_id], require_current=True)
q.save_raw_analysis_csv(table, "measurements.csv")
```

Requests and results use native ownership. Results contain metadata and observed
file versions, without a retained decoder. Requests intentionally retain their
source until released. `RawAnalysisJob` releases its request's decoder facade
after completion and supports cancellation and stale-result rejection.

The result preserves its native shared ownership when passed through Python,
using `std::enable_shared_from_this`. Save workers can therefore retire immutable
snapshots without acquiring the GIL. A timeout-bounded subprocess regression
saves a record while removing it from the live catalog.

The input digest `qpx.raw-roi-input/1` includes typed sample bytes in canonical
little-endian order, valid-pixel masks, selected axes, raw image metadata,
calibration, ROI geometry and effective processing tile size. It excludes the
display projection, object IDs, labels, source path and observed file identity.
The decoded bounding rectangle is hashed even outside a nonrectangular ROI;
statistics include only valid pixels inside the ROI. Changing that rectangle's
input may therefore invalidate a record without changing its statistics.

Records retain original object names, classes and nearest ancestor TMA core
context. ROI, calibration and TMA context changes affect eligibility separately
from the numeric input hash. Eligibility resolves the current nearest TMA ancestor;
moving a parent into, between or out of cores invalidates affected child records
even when their own revisions stay unchanged. A changed patient/case label cannot silently become
the label for an older measurement.

Project schema 13 stores bounded, checksummed native record payloads. After a
project is reopened, records start as unverified. Explicit replay recomputes both
the input digest and the statistics; verification changes derived state without
changing the canonical document revision. A file observation is not a hash of the
whole file. External image bytes are not embedded or frozen by saving a record.

```python
replay = q.RawAnalysisRequest.replay(document, record_id)
verified = document.verify_raw_analysis(record_id, replay, q.analyze_raw(replay))
```

CSV snapshots retain immutable records and status at capture. They can export
historical values after source deletion. CSV uses UTF-8, RFC-style quoting, CRLF
record separators and one row per selected channel. Native file exports default
to new files and protect recorded source paths. Project-aware GUI and viewer
exports also protect the project path. No decoder is opened by CSV serialization.

The document catalog supports at most 10,000 records and 64 MiB of charged record
payload, with pages limited to 512 records. Save snapshots share immutable blocks
of 256 records. Individual encoded records are limited to 16 MiB; raw tile buffers
remain within 256 MiB. CSV snapshot limits account for retained logical payload,
not total process RSS. Dataset decode caches have separate reader limits.

The managed viewer accepts `analyze_raw`, `verify_raw_analysis` and
`export_raw_measurements` commands. Owner-thread publication checks source and ROI
eligibility before retaining a worker result. Verification is reported separately
from a canonical commit. ImGui exposes Data, Results and Records tabs for measuring,
keeping, verifying, removing and exporting results.

`ProjectBatchKind.RAW` writes independent per-image checkpoints. It captures each
image's displayed Z/time by default; `raw_follow_display=False` selects explicit
axes from `options.raw`. Each ROI supplies its own bounding rectangle. Resume
checks canonical checkpoint integrity and freshly hashes the input without
recomputing statistics. `raw_tiles` and `raw_hashed_tiles` report these scans
separately. Reuse does not mark an opened document's records as verified.

The native batch CLI accepts `--kind raw`, `--raw-level`, `--raw-channel`,
`--raw-channels`, `--raw-z`, `--raw-time`, `--raw-explicit-axes` and
`--raw-max-pixels`. Axes are zero-based. Wider reader support, raw cell feature
pipelines, full runtime/build manifests and biological equivalence to QuPath
remain outside this packet.


```text
qupathxx_batch study.qpxproj raw-output --kind raw --raw-channel 0 --raw-channels 2 --raw-z 2 --raw-time 1
```

The [native Records panel](images/raw-analysis-records.png) shows captured axes,
original TMA case, verification state and per-channel results for a synthetic
image. The [10,000-record benchmark](benchmarks/raw-records-windows.json) keeps
distinct results for one child ROI nested under a parent and TMA core; it checks all 40,000 CSV
rows and reopens the saved records. [Cross-platform evidence](benchmarks/raw-records-cross-platform.json)
compares native Windows/Linux input hashes, statistics, original case context and
archive interchange. Timing observations apply to the measured machine.


## Explicit-grid native records

The additive `RawAnalysisRequest(document, ScientificGrid, ScientificRawOptions,
object_id, nucleus, max_pixels)` path uses `qpx.raw-grid-statistics/1`. It captures
original dtype/axes/calibration separately from effective document calibration
and derived binary64 samples. Grid results expose `grid()` and reject natural
`region()` access. Core-center eligibility is separate from full footprint
support; missing source support is neither a finite zero nor a nonfinite sample.

QRA2 stores the explicit grid and original source metadata; QRA1 stays unchanged
for natural results. Both have independent struct/hashlib byte references.
Retained/reopened records are historical until a fresh explicit replay verifies
the source, object/context, calibration, hashes and statistic bits. Existing
project blob storage supports both versions. Natural-only CSV remains schema 1;
grid/mixed CSV uses schema 2 with explicit core geometry, contexts, sampling kind,
source dtype and derived sample type, and blanks for natural scalar coordinates.
See the [typed contract](scientific-grid-contract.md#qra2-grid-records-and-typed-raw-csv)
and [local native evidence](benchmarks/raw-grid-retention-local-windows-linux.json).

The grid path is exposed through Python/stubs, CLI/batch and ImGui/managed
viewer workflows; see the [workflow guide](grid-cell-workflows.md) and
[current task](current-task.md) for exact source and qualification scope.
Large hash-only source verification now uses bounded decoded chunks plus the
canonical validity suffix, retaining the same digest and archive bytes. Full
materialized sampling still has its own admission and source-window limits.
Biological, hardware and supported-release acceptance remain separate.
