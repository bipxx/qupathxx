# Regional expression summaries and pseudobulk

A02 reduces an existing native expression matrix over explicit captured cohorts.
Raw sums remain `uint64`, including values above 2^53. Means use binary64; empty
cohorts have sum zero and an absent mean (a blank CSV field). Sparse absence and
explicit stored zero are measured zeros and remain in the observation denominator.
No original counts, observation coordinates or ROI geometry are changed.

The current consumer is the native batch CLI. Desktop controls and optional
Python/installed SDK exposure are separate work. No Python process or runtime
is needed for these commands.

## Create, reopen and export

Use a matrix association ID already retained on the selected project image. The
native API lists these with `expression_matrix_ids(project, image)`. A new output
project is required; the input is preserved.

```powershell
qupathxx_batch --aggregate-expression input.qpx output.qpx --image 1 --matrix MATRIX_SHA256 --spec regions.json
qupathxx_batch --verify-expression-aggregation output.qpx --image 1 --record RECORD_SHA256
qupathxx_batch --export-expression-aggregation output.qpx sums.csv --image 1 --record RECORD_SHA256
qupathxx_batch --export-expression-aggregation output.qpx members.csv --image 1 --record RECORD_SHA256 --members
```

Creation returns a JSON report containing `record` and `result_sha256`. Export
and verification replay the retained recipe through the same native reducer.
CSV and project publication are atomic and new-only. Existing destinations,
changed parents, invalid specifications and overflowing sums fail visibly.
`--work-bytes` defaults to 256 MiB; `--project-bytes` bounds input/output project
files. Creation also accepts `--save-snapshot-bytes`; exports accept `--csv-bytes`.

## ROI membership

```json
{
  "schema": "qpx.expression-aggregation-spec/1",
  "kind": "regional-summary",
  "overlap": "independent",
  "frame_id": "image-level-zero-px",
  "coordinates": "source",
  "regions": [
    {"id": "tumour", "roi": 12},
    {"id": "margin", "roi": 19}
  ]
}
```

`roi` is an existing area annotation ID on the selected channel/Z/time plane.
`frame_id` must match the retained expression point frame; use
`expression_matrix_spatial_scopes(document)` to discover explicit scopes.
`channel` (default all channels), `z` and `time` (default zero) may select a plane.
Membership tests the observation centre with the existing `Roi::contains`
geometry/boundary policy, rather than spot or cell area intersection.

`independent` counts an observation in every matching region; these overlapping
regional sums must not be pooled as independent observations. `reject` fails
when a captured observation belongs to multiple requested regions. Empty regions
remain in the result. Unassigned observations are excluded explicitly.

Source mode requires identical source/ROI frames. To use a retained registration,
set `"coordinates": "registered"` and supply `"registration": "REGISTRATION_ID"`.
The native service verifies its exact captured source and target frames before
mapping centres. A source-mode request never applies a registration implicitly.
Positions outside the image are retained and tested without clamping.

Omit `features` to reduce every source feature. Regional summaries may instead
name an explicit nonempty `features` array of stable feature IDs; the result uses
source-axis order. An optional `normalization` field selects an existing A01
normalization record ID on that image. Its exact raw parent and full-source QC
are verified before reducing derived values. The CSV records the raw parent,
value-source identity and explicit raw/normalized/log domain.

## Explicit observation cohorts and pseudobulk

```json
{
  "schema": "qpx.expression-aggregation-spec/1",
  "kind": "pseudobulk",
  "overlap": "reject",
  "cohorts": [
    {
      "id": "sample-a-region-1",
      "sample": "sample-a",
      "replicate": "animal-a",
      "group": "control",
      "observations": ["BARCODE_A", "BARCODE_B"]
    },
    {
      "id": "sample-b-region-1",
      "sample": "sample-b",
      "replicate": "animal-b",
      "group": "treated",
      "observations": ["BARCODE_C"]
    }
  ]
}
```

These labels are illustrative; supply actual sample/biological-replicate
metadata. The native service joins observation IDs exactly, refuses missing or
duplicate IDs and retains source-axis memberships. Pseudobulk requires the full
raw integer feature universe, disjoint cohorts and nonempty sample, replicate
and group labels. A replicate ID cannot change sample/group between columns.
Multiple regions of one replicate retain that same replicate identity.
Normalized/continuous input, selected-feature pseudobulk and implicit replicate
inference are refused. A single-mouse spatial demo cannot establish independent
biological replicates or differential-expression acceptance.

Regional summaries can also use explicit observation cohorts. ROI cohorts may
carry `sample`, `replicate` and `group` metadata and use `kind: pseudobulk` when
the same raw/disjoint/full-universe requirements hold. Empty pseudobulk columns
remain explicit; downstream methods must handle their insufficient information.

## Native service and persistence

`expression_aggregation.hpp` exposes `expression_observation_cohort`,
`expression_region_cohorts` and `aggregate_expression`. The immutable result
contains feature metadata, feature-major cohort sums, exact memberships, labels,
value domain and identities. `mean(feature, cohort)` returns an optional value.
The pseudobulk count buffer is a feature-by-cohort matrix with explicit
replicate associations, ready for a later native molecular method.

`expression_aggregation_workflow.hpp` owns specification interpretation and the
project create/replay operations. The CLI only parses arguments and invokes
these services. Call expensive matrix IO/reduction/replay from a native worker
when integrating a GUI. APIs accept cancellation pointers and recheck source
currentness before returning or publishing exports.

`keep_expression_aggregation` stores one undoable, chunked
`qpx.expression-aggregation/1` recipe against the existing matrix association.
It records exact captured source indices, selection provenance, labels,
preprocessing and result identity; it does not copy a normalized matrix or embed
gene sums in document metadata. Capture
`expression_aggregation_open_request(project, image, id)` on the owner, then
call `open()` on a worker. Replay recomputes and verifies exact saved identity,
including after an owned sidecar Save As. Work/batch choices do not enter that
identity. Later ROI/registration edits do not change a historical captured cohort;
create a new aggregation to select against edited geometry.

The initial bounded contract is at most 64 cohorts, 65,536 total memberships
(including overlaps), 768 KiB canonical recipes and 16–1024 MiB accounted work.
Some workflows need more than the minimum to retain results and read buffers.
Only the feature-by-cohort result is dense; observations-by-features remain
sparse and disk-backed. Accounted work is a conservative reservation, not an
allocator/RSS or throughput claim. Existing metadata/project quotas still apply.

The results CSV contains feature IDs/indices, cohort/sample/replicate/group IDs,
observation counts, explicit value domains, exact sums, means and recipe/result
identities. The membership CSV links every cohort to exact observation IDs and
source-axis indices using the same record ID. Keep both when exporting a cohort
analysis. All scientific interpretation remains native C++.
