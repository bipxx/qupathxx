# Explicit-grid tissue sampling

GRID1 is locally verified for the bounded area-grid workflow below, including
installed SDK, managed/desktop, batch/CLI, replay/export and legacy limits. Broader
scientific, whole-slide, host and release claims remain open.
See the [current task](current-task.md) and [evidence](benchmarks/tissue-grid-r7/README.md).

## Choose the shortest workflow

| Researcher action | Existing authority / entry point |
| --- | --- |
| Sample a selected ROI at chosen counts/context | `TissueRequest.for_grid_roi`; no manual bounds calculation |
| Supply exact origin, extent and per-axis counts | `ScientificGrid` and the grid overload of `TissueRequest` |
| Apply physical component limits | `minimum_um2`/`maximum_um2` on the ROI factory, or `with_grid_component_area_limits` for an exact descriptor |
| Run asynchronously | `TissueJob` with the same request; cancellation/stale publication use the existing job path |
| Save and reproduce a mask | `Document.keep_tissue`, project save/reopen, `TissueRequest.replay`, then `verify_tissue` |
| Export measurements | `MeasurementTableRequest` and the existing CSV writer; a selected grid record enables CSV4 |
| Submit from a managed viewer producer | `ViewerSession.grid_tissue` for ROI counts, `tissue_on_grid` for an exact descriptor; capture happens on the owner thread |
| Use the desktop | Enable **Explicit tissue grid**, set counts/context, detect and retain through the existing Tissue controls |
| Process project ROIs | Set `ProjectBatchOptions.tissue_grid`, then use the existing batch request/run/resume APIs |
| Run headlessly | `qupathxx_batch input.qpxproj output --kind tissue --tissue-grid NX NY CX CY` |

```python
import qupathxx as q

sampling = q.TissueGridSampling()
sampling.samples_x, sampling.samples_y = 512, 384
sampling.context_x, sampling.context_y = 8, 8
parameters = q.TissueParameters()
parameters.smoothing_sigma = 1.0  # sampled-array pixels
request = q.TissueRequest.for_grid_roi(
    document, object_id, transform, sampling, parameters,
    minimum_um2=100.0,
)
result = q.detect_tissue(request)
record_id = document.keep_tissue(request, result)
labels = result.labels_numpy(copy=False)  # read-only, retains its native owner
```

The ROI factory uses the integer enclosing level-zero ROI bounds. Counts divide
that extent independently on X and Y; they need not preserve aspect ratio.
For exact placement, construct `ScientificGrid(image_width, image_height,
ScientificGridAxis(origin_x, extent_x, samples_x, context_x),
ScientificGridAxis(origin_y, extent_y, samples_y, context_y))` and pass it as the
`grid` argument. Signed origins are retained; unsupported source footprints do
not contribute output. Use owning `scientific_grid` and per-axis downsample
properties to inspect geometry. Scalar downsample and natural-level bounds raise
for this recipe. Returned labels include context storage, with outside-core labels0.

## Sampling and filter contract

This recipe requires openly licensed reader/codec implementations exposing an
open, matching, unit-scale RGBA8 level zero. It computes exact-footprint area RGB
means in float64, then transforms those means to log10 OD/stain channels. It does
not round RGB means to uint8 or average already-transformed source OD values.
Every contributing source pixel must have alpha255 and the entire footprint must
be inside the source. Invalid support remains excluded background.

Gaussian, threshold, binary3x3 median, opening, closing and hole fill use the
captured valid source support, including context outside the ROI. After cleanup,
output is clipped to eligible core ROI sample centers before components are
labeled/filtered. Sigma and morphology radii use sampled-array pixels, so the same
radius can span different physical distances on the two axes. Context can change
filter/edge results; choose it as part of the recipe. This differs intentionally
from native-level tissue filtering's ROI-clipped support.

Samples, threshold/cleaned/filled counts and retained components refer to core
output. Component pixel area is sample count times both level-zero axis steps;
physical area also uses both captured calibration axes. Physical limits resolve
to inclusive count bounds on that lattice. An effective calibration change makes
an outstanding request stale and refuses replay of a retained grid recipe.
`level` must be0 or auto(-1), and tile size must remain256: the source route is one
bounded level-zero area-sampling window. Admission checks precede allocation/IO.
This is bounded-window analysis; whole-slide streaming/resource claims remain R12.

## Consumers, retention and compatibility

`GuiOptions.tissue_grid` and `ProjectBatchOptions.tissue_grid` accept a sampling
value or None and return owning copies. Reassign an edited copy to change options.
A batch request copies its options again when capturing the snapshot; later edits
do not alter checkpoint identity or execution. Enabled grid settings enter batch
JSON; absent settings preserve the old default representation. Grid options are
valid only for tissue batches. Each selected ROI resolves its own descriptor. Exact input identity includes the
white/stain transform, so pass matching values across consumers when comparing
digests; equal masks alone do not establish an identical recipe.

QTM5 stores the exact descriptor, parameters and embedded captured provenance;
project provenance must agree. Decode checks core eligibility, counts, geometry,
resource admission and canonical fields. Historical producer work estimates are
retained while current decoder storage is admitted independently. QTM1-4 remain
canonical for their older natural recipes. Cell and nominal masks retain their
own versioned archive routes.

A mixed grid/natural CSV selection uses CSV4, including filter fields, sampling
kind, image dimensions, signed origin, extent, counts, context and per-axis steps.
Grid scalar downsample is blank; natural rows have blank grid descriptor fields.
Old-only selections continue to use CSV1/2/3. GUI reports expose an owning
`tissue_grid` descriptor and `tissue_downsample_x`/`tissue_downsample_y`; their old
scalar field is0 for grid results. Native numerical and archive evidence does not
establish biological equivalence, all vendor support or speed superiority.
