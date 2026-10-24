# Captured TMA cohorts (candidate 144)

This candidate pools raw measurement rows across explicitly captured TMA grids.
It extends the [single-grid workflow](tissue-microarrays.md); existing single-grid
CSV schemas and output bytes are retained. Native core/file regressions and
13 standalone CLI probes pass. The [first installed Windows build](benchmarks/tma-cohort-initial-windows.json)
passes 47 native suites and 111 TMA API/CLI cases, including 46 new cohort cases.
Initial desktop replay passed 16 of 17 cases and exposed an owner-thread
preflight error that left the export status idle. The rebuilt fix passes all
17 desktop cases, packaged stubs and typing. The [full installed validation](benchmarks/tma-cohort-validation-windows.json)
passes 2,470 Python tests, 13 optional skips and three unchanged strict science
xfails, plus 47 native suites. The saved-state cohort capture is visually
reviewed and verifies weighted mean 5 across two grids with zero UI overflow.
[Both Mac native architectures](benchmarks/tma-cohort-macos.json) pass all 46
suites, including cohort pooling and atomic file markers. Complete Windows/Linux
and portable builder/consumer acceptance remain pending.

## Captures and identity

A native `TmaCohort` maps positive image IDs to immutable `TmaGrid` captures.
Python supplies a dictionary of those native snapshots. A core is identified by
`(image_id, object_id)`: different images may have the same local core IDs.
Grid names do not identify images. Equal nonempty case IDs pool across captures;
empty case IDs remain separate by image and core. IDs and case strings are not
renumbered, trimmed, case-folded or normalized during export.

All captures are retained through the synchronous operation. The ImGui worker
receives native immutable snapshots and strings captured on the document owner
thread. It does not access project/document state or invoke Python on a worker.
Subsequent document edits do not alter an earlier capture.

```python
snapshots = {image_id: project.document(image_id).tma_grid()
             for image_id in selected_image_ids}
manifest = q.tma_cohort_manifest_csv(snapshots)
summary = q.tma_cohort_case_summary_csv(
    snapshots, measurement_csv, "value", "weight")
q.save_tma_cohort_case_summary_csv(
    snapshots, "new-cohort.csv", "measurements.csv", "value", "weight")
```

Each selected capture must exist. The API admits 1 to 4096 grids. Total captured
payload, measurement input, result CSV and source-manifest CSV each have their
own `byte_limit` admission, defaulting to 64 MiB with a maximum of 256 MiB. Each
grid metadata CSV used to construct the source manifest must also fit that
limit. These are component quotas; `byte_limit` is not a process-memory ceiling.

## Measurement and statistics contract

A table requires `image_id` or the preferred `source_image_id`, plus
`source_object_id`/`source_object_revision` (with the existing
`object_id`/`object_revision` fallback) and the requested numeric columns.
Image and object selectors are canonical unsigned decimal integers. A row must
resolve to a captured core in that image and match its recorded revision.
Existing revision columns are always checked. Tables without revisions require
explicit `allow_unversioned=True`.

Present cores contribute raw rows to case sums, weighted sums, means, extrema
and measured-core counts. This does not average previously aggregated core or
grid means. Missing cores contribute inventory and ignored-row counts; their
selectors and numeric fields are still validated. Weights are nonnegative.
A zero total weight leaves weighted mean blank. Empty measurement input exports
inventory with blank numeric statistics. Signed sums use compensated arithmetic;
nonfinite values and numeric overflow or nonzero underflow are rejected.

The result schema is `qpx.tma-cohort-case-summary/1`. Alongside the inherited
statistics it records:

- `cohort_grids`, `cohort_slots`, `cohort_vacant_slots`: repeated context for the
  complete capture set; do not sum these columns across result rows.
- `source_manifest_sha256`: SHA-256 of the exact canonical source-manifest CSV.
- `unassigned_image_id`, `unassigned_core_id`: identities for an unassigned core;
  both are blank for an assigned case.
- `grids`: distinct captured images represented by the case inventory, including
  missing cores; this is not the count of measured images.
- `input_table_sha256`: SHA-256 of the exact supplied measurement CSV bytes.

The source manifest uses `qpx.tma-cohort-sources/1`, with one row per image in
ascending numeric ID order. Each row records document revision, grid name,
slots, vacancies, core count and `metadata_csv_sha256`, which hashes the exact
`tma_metadata_csv(snapshot)` output including geometry, core selectors and case
metadata. This lets a caller retain and independently inspect the captured
source inventory. Source order does not change either CSV.

## Files, CLI and desktop

The shared native loader checks observed input size before data reads, reserves
admitted storage, checks per-chunk limits and verifies the input before atomic
new-file publication. Existing destinations, input aliases and protected project
or image sources are preserved. Failures leave no published result or abandoned
export temporary. Native cancellation is checked during capture, row parsing,
metadata construction, hashing and publication; Python bindings release the GIL.

```powershell
qupathxx_batch --export-tma-cohort project.qpxproj cases.csv --image 1 --image 3 --measurements measurements.csv --value-column value --weight-column weight
qupathxx_batch --export-tma-cohort project.qpxproj inventory.csv --all-tma-images
```

Selection is explicit: repeated unique `--image` IDs and `--all-tma-images` are
mutually exclusive. An explicitly selected image without a grid is an error.
`--all-tma-images` includes every project image with a TMA grid, skips images
without one, and rejects an empty cohort. Success emits
`qpx.tma-cohort-export/1` with saved state, grid count, byte count and output path.
The inherited exit codes are 0 success, 1 data/I/O failure, 2 argument failure
and 130 cancellation. The CLI retains its project observation through publication.

In an open project with an active TMA grid, the TMA Export tab offers **Pool TMA
grids across project** under the case-summary option. It explicitly captures all
project images with a grid and requires image selectors in any measurement CSV.
The value/weight, revision opt-in, new output and cancellation controls retain
the single-grid workflow. Project pooling requires an open project.

This candidate does not implement automatic dearraying, QuPath TMA interchange,
survival analysis, biological validation or supported packaging. M0-M7 remain
partial. Its scale probe and full validation must establish their stated scopes
before this candidate is promoted.

## Initial scale observations

The [cohort scale probe](benchmarks/tma-cohort-profile-initial-windows.json)
retains 24 trials, including four declared warmups, through one million rows
across four grids and 3,996 captured cores. All inventory, raw-row statistics,
image/core identities, source metadata hashes and output hashes pass independent
checks. The one-million-row median is about 236 ms on this shared workstation.
The [inherited single-grid paired pilot](benchmarks/tma-cohort-single-paired-pilot-windows.json)
retains all six fresh processes and 108 samples, including 18 warmups; every
output hash agrees with the prior installed binary. The aggregate 100,000-row
medians are 32.41 ms before and 33.16 ms in the candidate, while smaller sizes
and individual processes show mixed differences. These observations do not
establish a general speedup, hardware qualification or peak-memory acceptance.
They use the first installed source, before the unrelated GUI error-state fix.

The [rebuilt Windows candidate](benchmarks/tma-cohort-fixed-initial-windows.json)
now passes all 47 native suites and 128 focused TMA cases, including all five
cohort desktop cases. The no-project failure reports failed state and retains
its input. Packaged stubs, positive/negative typing and all 258 raw compiler
inputs match their stated evidence. Twenty [altered-evidence controls](benchmarks/tma-cohort-audit-controls.json)
reject old, missing, duplicate, failed, skipped and stale evidence. Full
installed regression and capture review now pass as recorded above; complete
Windows/Linux and portable platform acceptance remain pending.

The [first portable workflow](benchmarks/tma-cohort-portable-initial-rejection.json)
finished successfully but omitted the required extended-reader dispatch input.
The unchanged audit rejected missing native NDPI project replay in both builders.
Its cohort helper cases pass, which does not satisfy the complete qualification.
Original artifacts and the failed audit are retained. One corrected dispatch
with extended readers enabled is running at the same source; acceptance remains
pending and no gate is relaxed.
