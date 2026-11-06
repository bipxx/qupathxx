# Tissue-aware ROI cell density

Measure selected Cell centroids per mm² of valid tissue inside each selected ROI.
The C++ kernel owns geometry, calibration, counting, retention and CSV; Python,
CLI and ImGui use that authority. Local native evidence is in [native.json](native.json)
and [native-tests.log](native-tests.log). Consumers and the shipping candidate are
still being checked; see the [current task](../../current-task.md) and
[coverage checklist](../../density-coverage.md).

## Scientific contract

For each window, valid area is its intersection with the image bounds and the
union of explicit tissue ROIs, minus the union of explicit exclusions. Tissue
overlap contributes once. A Cell ID contributes once per window when its chosen
centroid is covered by the valid footprint and is outside every exclusion,
including exclusion edges. Overlapping windows produce independent rows; adding
their rows does not establish a pooled density.

Prefer an available nucleus centroid by default; otherwise use the cell body
centroid. `prefer_nucleus=False` uses bodies. Targets must be Cells. All selected
objects must share one Z/time plane. Closed polygon coverage includes outer and
hole boundaries; hole interiors are outside. Explicit exclusion boundaries are
removed. No pixels or cell segmentation are read/run in the worker.

Both X/Y pixel sizes must be positive finite micrometres. Area in mm² equals
pixel area × X size × Y size ÷ 1,000,000; anisotropy is preserved. Empty valid
area gives count0, area0 and an undefined density (`None` in Python, an empty CSV
field with `empty_area` status). Positive area with no cells gives density0.
Unrepresentable calibrated area or density fails the operation.

Rectangles, polygons, holes and multipart inputs use linear topology. Ellipses
require explicit `polygonize_curves=True` / `--polygonize-curves`. This accepts
the existing512-segment representation for both area and membership. The policy
`qpx.polygon512-closed-exclusion/1` and curve acceptance are included in identity
and exports. This is a declared geometric approximation, not analytic ellipse area.

## Python

The public API is `SpatialDensityOptions/Request/Result/Row/Job/Record` plus
`measure_spatial_density`, `spatial_density_csv` and `save_spatial_density_csv`.

```python
import qupathxx as q

project = q.Project.open("cells.qpxproj")
doc = project.document(1)
request = q.SpatialDensityRequest(
    doc, windows=[10], tissue=[11, 12], excluded=[13], cells=[20, 21, 22])
result = q.measure_spatial_density(request)
row = result.row(0)
print(row.count, row.area_mm2, row.density_per_mm2)
record = doc.keep_density(request, result)
q.save_spatial_density_csv(result, "density.csv")  # refuses an existing target
project.save("density-study.qpxproj")
```

IDs are examples: supply IDs from your own document. Cell selection is explicit;
an empty cell cohort is permitted, but windows and tissue are required. The
[working example](../../../examples/density_workflow.py) accepts repeated
`--window`, `--tissue`, `--exclude` and `--cell` flags, writes into a new/empty
output directory, and verifies saved replay. It does not automatically select
all cells or run detection.

Use `SpatialDensityJob.request(request)` and `job.wait(doc)` for a native worker;
Python releases the GIL while waiting. `poll(doc)`, `cancel()`, `busy` and `result`
follow existing owner-controlled job semantics. Failed, cancelled or stale jobs
expose no partial table. Results expose typed membership/geometry counters rather
than labelling them distance comparisons. Table pages are limited to4096 rows.

## CLI and desktop

```powershell
qupathxx_batch.exe --measure-spatial cells.qpxproj density.qpxproj --image 1 --operation tissue-density --source 10 --tissue 11 --exclude 13 --target 20 --target 21
qupathxx_batch.exe --export-spatial density.qpxproj density.csv --image 1 --operation tissue-density --record 25
qupathxx_batch.exe --verify-spatial density.qpxproj --image 1 --operation tissue-density --record 25
```

Use the returned record ID, rather than assuming25. `--source` means ROI window;
`--target` means Cell. Each cohort can instead use a whitespace-separated ID
file: `--source-ids`, `--tissue-ids`, `--exclude-ids`, `--target-ids`. Do not mix a
file and repeated IDs for the same cohort. File inputs are bounded to4MiB and
protected against output replacement. Measure writes a new project, export a new
CSV, and verify leaves the project file unchanged. `--body` and
`--polygonize-curves` explicitly change the recipe. Density rejects distance
units, radius, include-self and distance-comparison options.

Open **Spatial...**, select **Tissue cell density**, and use the selection buttons
for ROI windows, Cells, tissue and exclusions. Run, inspect count/valid tissue
area/cells per mm²/status, then Keep, Export CSV and save the project. Empty area
is shown as Undefined. Saved records can be inspected, verified or removed.
Curve acceptance and resource limits are available in the same panel. Small
styling differences from QuPath are outside the parity target.

## Budgets, persistence and support

- Each area cohort allows up to10000 IDs; windows/tissue require at least1,
  exclusions may be empty. Cells allow0..100000. IDs sort canonically; duplicates,
  missing IDs, wrong target types and mixed planes fail capture.
- Defaults:64MiB snapshot,128MiB work,100000000 membership predicates,10000 Boolean
  operations and10000 geometry vertices. Options accept1..256MiB snapshot,
  1..1024MiB work,1..2^40 predicates,1..1000000 Boolean operations/vertices. Aggregate
  input vertices and aggregate retained-footprint vertices are limited separately.
  Cohort maxima remain subject to these resource limits.
- CLI density limits are `--max-membership-tests`, `--max-geometry-operations`,
  `--max-geometry-vertices`, `--snapshot-bytes` and `--work-bytes`. GEOS temporary
  scratch is opaque: planned native payload allowances are not process RSS caps.
  No dense ROI-by-cell matrix is allocated. Broad windows can still require
  O(windows × cells) membership work. A single GEOS call is synchronous, with
  cancellation checked before/after it and throughout surrounding loops.
- Native retention allows10000 density records/256MiB accounted payload per
  document. QDN1 records store captured roles, body geometries, centroids, policy,
  calibrated values and footprints. Reopened/copied records begin unverified.
  Replay refuses changed saved geometry, object revisions, calibration or policy;
  newly added objects do not join the original cohorts. Historical results remain
  inspectable/exportable after changes or removal from the document.
- New project saves use schema29. Old candidates cannot open schema29 projects.
  This implementation opens earlier supported schemas without rewriting them;
  the next save upgrades transactionally. Existing QSD1/QSN1 payloads are preserved.
- Selected local support remains Windows x64, CPython3.12, MSVC runtime and an
  OpenGL3.2 desktop for ImGui. The project uses AGPL-3.0-only and openly licensed
  readers/codecs. Delivery-specific install/assets/source identity will be linked
  here after the candidate is verified. This phase makes no speed-superiority,
  full QuPath parity, additional-platform or biological validation claim.
