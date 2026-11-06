# Radius-based cell neighborhoods

The new native operation counts selected target centroids within a chosen
radius of each selected source centroid. C++, nanobind Python, the batch CLI
and Windows ImGui share the same immutable capture, indexed kernel and native
retention/export path. Candidate2 remains available separately; neighborhood
delivery is pending the final frozen candidate/install and publication.
See the [live coverage checklist](../../neighborhood-coverage.md).

## Contract and limits

- Sources:1..100000 IDs; targets:0..100000 IDs. IDs are sorted canonically;
  duplicate/missing IDs are rejected. Select classes into explicit ID cohorts
  before capture. Later objects do not silently join a saved cohort.
- Radius must be positive and finite. Pixel units use unit scales. Micrometre
  units require both positive finite calibration axes; anisotropic scales are
  applied independently. All selected objects must share one Z/time plane.
- Count when calibrated binary64 Euclidean distance is <=radius. Prefer an
  available cell nucleus centroid by default, otherwise use the body/object
  centroid. Exclude the identical ID by default; distinct coincident objects
  count. Empty targets give zero. These are centroid counts, with no density,
  cell-boundary distance or edge-area normalization.
- Defaults:64MiB snapshot allowance,128MiB worker allowance,100000000 actual
  distance comparisons. Supported ranges:1..256MiB snapshot,1..1024MiB work,
  1..2^40 comparisons. Admission is conservative native payload accounting,
  not a total process-memory cap. The worker performs zero pixel reads and
  allocates no dense source-by-target matrix or per-source neighbor list.
- Broad radii/dense coincident cohorts can still require O(sources*targets)
  comparisons. Work-limit exhaustion fails the whole operation. Cancellation
  checks run during traversal. Jobs reject stale document publication and join
  on shutdown; cancelled/failed jobs expose no partial publishable table.
- Retention is limited to10000 neighborhood records/256MiB conservatively
  accounted payload per document. Copies/reopened results begin unverified.
  Historical results remain inspectable/exportable; fresh replay checks saved
  object revisions/geometry/calibration and radius/cohorts. Live jobs conservatively
  reject document edits while running. No numeric or stored-identity relaxation
  was added.

## Python and CLI

The final Windows package requires CPython3.12 x64 for Python, the Microsoft
Visual C++ x64 runtime, and an OpenGL3.2 desktop/driver for ImGui. Reader/codecs
remain openly licensed; the project license is AGPL-3.0-only. Install the new
wheel build tag3 when delivered; candidate2 does not expose this operation.

Use existing objects in a native project:

```powershell
python -I examples/neighborhood_workflow.py C:/data/cells.qpxproj C:/data/neighborhoods --image 1 --source 10 --source 11 --target 12 --radius 25 --units um
```

Repeat source/target flags for explicit cohorts. No targets means a valid empty
cohort/zero counts. The example preserves existing output directories and writes
`neighborhoods.csv`, `study.qpxproj` and `workflow.json`, then verifies saved replay.
It does not rerun segmentation. `--body` and `--include-self` change the declared
recipe explicitly.

```python
options = q.SpatialNeighborhoodOptions()
options.units = q.SpatialDistanceUnits.MICROMETRES
options.radius = 25
request = q.SpatialNeighborhoodRequest(document, source_ids, target_ids, options)
result = q.measure_spatial_neighborhoods(request)
record = document.keep_neighborhood(request, result)
q.save_spatial_neighborhood_csv(result, "counts.csv")
```

`SpatialNeighborhoodJob` provides request/poll/wait/cancel/result. Waiting releases
the GIL. Result rows are immutable, ordered by source ID and paged at up to4096
rows per call. `SpatialNeighborhoodStats` aliases the existing common spatial
stats type; generated stubs retain the alias and validate its canonical members.

```powershell
qupathxx_batch --measure-spatial cells.qpxproj measured.qpxproj --image 1 --source-ids sources.txt --target-ids targets.txt --operation radius-count --radius 25 --units um
qupathxx_batch --export-spatial measured.qpxproj counts.csv --image 1 --record 42 --operation radius-count
qupathxx_batch --verify-spatial measured.qpxproj --image 1 --record 42 --operation radius-count
```

Use the actual returned record ID. ID files contain whitespace-separated positive
integers and are bounded to4MiB. Measurement requires an explicit radius. Existing
nearest-distance commands/defaults are unchanged. Guarded input paths and
no-overwrite atomic output publication apply to both operations.

## ImGui and saved projects

Open the project, use **Spatial...**, choose **Radius count**, select sources and
targets using the existing selection buttons, choose units/radius and run. The
table displays source ID, target count, radius and units. Keep, inspect, verify,
export and save use the shared native result; cancellation/retry cannot retain
partial counts. Python can configure the same controls with
`GuiSpatialOptions.operation=SpatialOperation.RADIUS_COUNT`, `radius`, `analysis`
and explicit `sources`/`targets`.

New neighborhood records use a separate QSN1 archive and project table in
schema28. Old QSD1 nearest-distance bytes/APIs remain unchanged. Older projects
open without writes; saving upgrades transactionally. Projects saved by this
candidate require this candidate or later, including when no new records were
added. Keep older originals when an older application must still open them.

## Evidence and reproduction

- [T1 native source/scope](t1-native.json), [native tests](t1-tests.log): independent
  exhaustive count oracle, radius boundaries, units, self/coincident semantics,
  capture, quotas and jobs. [T2](t2-native.json) adds retention, QSN1, atomic CSV,
  rollback, exact old snapshot budgets and all27 historical schema upgrades.
- [T3 native consumers](t3-consumer-native-tests.log): old/new mode transitions,
  lifecycle, history/export guards and CLI operation selection.
- [Python consumer run](t3-python-tests.log):86 passed and one diagnosed checker
  failure; [alias correction](t3-stub-alias-repair.log) passes without weakening
  canonical member checks. [Positive typing](t3-typing-valid.log) and
  [three negative typing cases](t3-typing-invalid.log) pass their intended gates.
- [Actual ImGui cancellation/retry](t3-gui-cancel.log),
  [506-cell workflow](staged-workflow.json), [inspected UI](desktop.png),
  [authenticated imported scene](scene.json). Cell geometry and the256x256
  captured crop are translated together from the pinned NDPI ROI, preserving
  original anisotropic calibration. No segmentation or specimen accuracy claim.
  All506 count rows match an independent exhaustive count over the reused
  centroid geometry authority; Python/CLI/ImGui identities/CSV/save/replay agree.
- [Dense observation](resources.json):20000 coincident cells each have19999
  neighbors,399980000 comparisons,12.49MB estimated native work and68.60MB
  process peak. Python advanced191 observer ticks during native wait. This is
  one local, source-scoped worst-case observation, not a speed/RSS guarantee.

Reproduce with the matching build, NumPy/Pillow, and empty output paths:

```powershell
python -I docs/benchmarks/neighborhood-workflow/workflow.py C:/data/neighborhood-check --gui
python -I docs/benchmarks/neighborhood-workflow/resources.py C:/data/neighborhood-resources
```

The workflow reuses the repository's captured cells and crop. Its optional
`--input-project`/`--expected` arguments reuse that prepared scene and passing
count oracle for a final build whose native source tree is unchanged. Native
source identities remain attached to the actual observed builds; final delivery
will record its own clean revision and installed member/runtime checks.
