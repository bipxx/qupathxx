# Project-wide density reports

[project_density.py](../examples/project_density.py) runs the existing native
density workflow across explicitly selected project images. It uses the installed
[Windows candidate4](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.4)
API; no new wheel, binding build, segmentation or batch UI is required. The
[coverage checklist](project-density-coverage.md) and
[evidence](benchmarks/project-density-runner/receipt.json) describe the selected scope.

Use the [class-based manifest generator](density-manifest.md) to select cohorts
by existing class names and preview counts/calibration before running.

## Run

Install candidate4 in the environment described by the [density guide](benchmarks/density-workflow/README.md).
Save/close the input project, prepare a manifest, and choose a new output directory:

```powershell
python -I examples/project_density.py selections.json results/run-001
python -I examples/project_density.py selections.json results/run-002 --resume-from results/run-001
```

Each run gets a new directory. Existing outputs are never replaced. The runner
opens the source project once, hashes it at setup/final publication and checks
file state between images. Existing SQLite journal/WAL/SHM files require closing
or recovering the project first; this runner does not repair or modify inputs.

## Manifest

The [example manifest](../examples/project_density_manifest.json) uses sample IDs;
replace them with IDs from your own project. Project paths are relative to the
manifest, or absolute. Images run in ID order; each cohort is canonicalized.

```json
{
  "schema": "qpx.project-density/1",
  "project": "study.qpxproj",
  "images": [
    {"image_id": 1, "windows": [10], "tissue": [11], "excluded": [], "cells": [20, 21]},
    {"image_id": 2, "windows": [7], "tissue": [8], "excluded": [9], "cells": [15, 16]}
  ],
  "options": {"prefer_nucleus": true, "polygonize_curves": false}
}
```

All four cohort fields are explicit. Windows/tissue require at least one ID;
excluded/cells may be empty. Duplicate images/IDs, unknown fields, wrong types
and nonfinite JSON values are rejected. Limits are1000 selected images,16MiB
manifest,1..10000 IDs per area cohort and0..100000 Cells per image, subject to
native capture/work limits. Native validation owns calibration, geometry, planes,
Cell types and scientific semantics. Images may have different calibrations.

Options are the existing `SpatialDensityOptions` fields: `prefer_nucleus`,
`polygonize_curves`, `snapshot_bytes`, `work_bytes`, `max_membership_tests`,
`max_geometry_operations`, `max_geometry_vertices`. The native defaults and
[area/boundary/curve contract](benchmarks/density-workflow/README.md#scientific-contract)
apply. Empty area remains undefined; positive area without Cells has density0.
Each row remains an independent ROI measurement.

An optional per-image `record_id` is a saved-result reuse hint. The runner checks
native replay, captured IDs/revisions/geometry hashes/centroids, recipe, calibration,
extent and native engine identity. An absent/stale/mismatched hint computes anew.
Results from another native source tree, compiler/configuration or GEOS version
are recomputed. Smaller resource limits also request fresh native admission.
Reused results are labelled; their numerical kernel has not been rerun and they
are not silently marked numerically verified in the source project.

## Outputs and resume

- `density.csv`: complete combined table. It prefixes the unchanged native CSV
  values with source project/image/name, record ID, reuse status, current request
  identity and checkpoint path. Quoting is UTF-8; numeric text comes from native
  CSV rather than Python recomputation.
- `density.partial.csv`: explicit incomplete table when an image fails, the run
  is cancelled, an input changes or the combined byte limit is exhausted. An
  image's rows publish together; export failure rolls back all of its rows.
- `report.json`: per-image outcomes/errors, computed/reused/failed/not-run counts,
  input hashes, native engine and output paths. Invalid images do not become
  fabricated zero-density rows. Default failure handling continues; use
  `--stop-on-error` to stop after the first image failure.
- `image-ID.qpxproj` and `image-ID.json`: completed single-image native checkpoint
  and its bounded identity/checksum receipt. Existing metadata/classes/view are
  retained. Source image IDs map to `checkpoint_image_id` in the receipt; native
  record IDs remain available for opening/inspection/replay.

`--resume-from` reads ready per-image receipts even if the earlier final report
was interrupted. Valid unchanged checkpoints skip native jobs. Relevant changes
compute anew; unrelated document edits can reuse unchanged captured geometry.
Corrupt or inconsistent checkpoints fail explicitly and remain untouched.
`--recompute` bypasses both checkpoint and saved-record reuse.

Resumed runs reference the existing QPX checkpoints; they do not copy projects
or embedded pixels. Keep referenced checkpoints when cleaning older run folders.
The combined CSV contains its measurements directly. Linked-image checkpoints
preserve original source links. A completed numerical checkpoint can be resumed
even when the prior run's CSV export failed its byte limit.

## Cancellation and resource limits

Ctrl+C cancels the active native job and prevents partial image publication. Setup
hashing/project load, atomic project save and a single GEOS operation are
synchronous and must return before cancellation can finish. Completed checkpoints
remain reusable; the next invocation uses a new directory with `--resume-from`.

CLI limits: `--csv-bytes` defaults64MiB, `--project-bytes` and
`--checkpoint-bytes` default1GiB. Each accepts1 byte..1GiB; a CSV budget smaller
than its header is rejected before creating output. Combined CSV exhaustion stops
further images. Single-image saves use the existing512MiB native snapshot limit;
native per-image CSV conversion is bounded to64MiB. Checkpoint receipts are at
most1MiB and final reports16MiB. These are admitted payload/file limits, not a
whole-process RSS guarantee. No dense image/ROI/Cell matrix is allocated.

Exit codes:0 completed,1 partial/image failures,2 setup/publication error,
130 cancellation. Stdout is a JSON summary; detailed outcomes are in report.json.

## Python callers

With `project_density.py` on your Python path:

```python
from pathlib import Path
from threading import Event
from project_density import run

cancelled = Event()
report = run(Path("selections.json"), Path("results/run-003"),
             resume_from=Path("results/run-002"), cancelled=cancelled,
             on_image=lambda outcome: print(outcome["image_id"], outcome["state"]))
```

Another thread may set the Event. The callback receives a copy after each image
attempt. The caller runs native owner operations sequentially; this phase adds
neither parallel scheduling nor dedicated batch UI controls. Existing native
density kernels/serialization remain frozen at candidate4.
