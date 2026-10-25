# Explicit-grid cell workflows

Integrated source `3fc3bcb` passes the complete Windows/Linux engine and portable
builder/fresh-consumer workflow gates, plus both-architecture Mac native checks.
See the [qualification receipt](benchmarks/scientific-grid-169-qualification.json).
Mac Python/GUI/OpenSlide, scientific/hardware acceptance and supported release
remain separate requirements.

Use `CellRequest.for_grid_roi` for an ordinary area annotation. It encloses the
ROI bounds in an integer level-zero rectangle, uses the requested sample counts
on each axis, and preserves the original ROI for eligibility and clipping.
`GridCellSampling` expresses counts and context, not a target physical spacing.
For exact control over the grid origin and extent, pass a `ScientificGrid` to
the `CellRequest` constructor instead. Both paths use the same detector.

```python
sampling = q.GridCellSampling()
sampling.samples_x, sampling.samples_y = 256, 128
sampling.context_x, sampling.context_y = 32, 32
parameters = q.GridCellParameters()
parameters.units = q.GridCellUnits.MICROMETRES
parameters.sigma = 1.0
parameters.min_nucleus_area = 5.0
parameters.cell_expansion = 5.0
request = q.CellRequest.for_grid_roi(doc, roi_id, transform, sampling, parameters)
result = q.detect_cells(request)
record_id = doc.keep_cells(request, result)
```

Distances use the selected units; areas use those units squared. Micrometres
require both calibration axes. The resolved descriptor reports the actual
per-axis steps, Gaussian sigmas and sampled-area cutoffs. Grid results expose
`scientific_grid`, `grid_parameters`, and `resolved_grid_parameters`; these
properties return independent metadata values in Python. Scalar legacy
`parameters`/`downsample` accessors reject grid results. Use the raster's
`downsample_x`, `downsample_y` and `sample_edge_x/y` for geometry.

Capture validates budgets before pixel IO. The current scientific sampler reads
a bounded level-zero source window, so reducing output counts alone does not
make an arbitrarily large ROI admissible. Retained QCL version 5 records carry
the original grid and calibration. `CellRequest.replay(doc, record_id)` checks
the current source and capture dependencies; reopening a project does not grant
old results fresh request authority.

For project processing, set `ProjectBatchOptions.kind` to `CELLS`, assign
`cell_grid` and `grid_cells`, then construct a `ProjectBatchRequest`. That capture
freezes the recipe; each image resolves its own calibration. The existing
`run_project_batch`/`ProjectBatchJob` checkpoint and resume workflow applies.
Use grid-unit area limits instead of `cell_min/max_nucleus_area_um2`. The open
source project is unchanged; results live in independent checkpoints.

The native CLI offers the same count-based route:

```text
qupathxx_batch INPUT.qpxproj OUTPUT --kind cells --cell-algorithm grid \
  --cell-grid-samples 256,128 --cell-grid-context 32,32 \
  --grid-units um --grid-sigma 1 --grid-min-area 5 --grid-expansion 5
```

Keep each command on one line in PowerShell. Grid flags reject legacy detector,
level, tile and physical-area flags before opening a project. Existing CSV export
commands preserve the typed descriptor; grid rows leave scalar-only legacy
columns blank. A repeated matching batch reuses complete checkpoints.

For managed viewers, submit `session.grid_cells(image_id, roi_id, transform,
sampling, parameters, retain_result=True)` while the owner is pumping. Parameters
are copied at submission; calibration is resolved when the owner captures the
request. Changes after capture produce stale delivery. Consume the ticket using
the ordinary poll/wait/take API. Ticket cancellation and retention budgets apply.

In the desktop image tools, enable **Explicit cell grid**, set counts/context,
units and detector controls, and use **Detect cells**. Preview, publication,
retention, replay and export share the existing cell job. Set
`GuiOptions.cell_grid` and `grid_cell_parameters` for initial values. In the batch
dialog, **Use image settings** copies this recipe; disabling it exposes the
batch's own grid controls. Switching to a non-cell batch clears the grid recipe.

Native callers use the corresponding `qpx::GridCellSampling`,
`GridCellParameters`, `CellRequest::for_grid_roi` and `ViewerSession::grid_cells`
APIs. Legacy natural-level cell requests remain separate and retain their units
and serialization formats.
