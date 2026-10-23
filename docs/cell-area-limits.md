# Physical nucleus area limits

`CellRequest.with_nucleus_area_limits` accepts inclusive minimum/maximum sampled
nucleus areas in square micrometers. It captures the same image/ROI and selects
the same natural sampling grid as the ordinary request, then compiles the area
limits into ordinary integer nucleus-sample cutoffs before detection.

**Validation update:** a subsequent managed-viewer test exposed a calibration
staleness gap in the initial candidate. Active physical requests now capture the
calibration revision and reject publication after it changes. The three new
regressions fail against the preceding build; rebuilt validation is pending.
The local results below describe the preceding candidate and do not accept this
fix. Hosted run 40022136685 also predates it. Main promotion remains withheld.

```python
parameters = qupathxx.CellParameters()
parameters.min_nucleus_pixels = 1
request = qupathxx.CellRequest.with_nucleus_area_limits(
    document, annotation_id, transform,
    minimum_um2=10.1, maximum_um2=100.0, parameters=parameters,
)
result = qupathxx.detect_cells(request)
print(request.parameters.min_nucleus_pixels, request.parameters.max_nucleus_pixels)
record_id = document.keep_cells(request, result)
```

Both sampled-pixel and physical limits apply. `None` disables an individual
physical limit. Both axes of effective image calibration must be known whenever
either limit is enabled; zero is an enabled bound. A maximum of zero retains no
positive-area nucleus. Negative, nonfinite or reversed physical ranges reject
the request. Native work budgets and candidate limits still apply.

For sample count `n`, native downsample `d` and captured pixel sizes `sx, sy`,
the area predicate uses `n*d*d*sx*sy`, with the same stable product as the
[calibrated cell features](calibrated-cell-features.md). Anisotropic axes and
fractional natural downsamples are supported. Inclusive comparisons follow the
represented double value without an added epsilon. Binary searches over possible
sample counts avoid rounding a divided bound in the wrong direction. Normalized
products preserve representable results after intermediate overflow/underflow;
positive areas outside double range are compared at their corresponding range
end. Maximum zero remains empty even when positive area would underflow to zero.

The intersection may exclude the actual nuclei despite having a nonempty count
interval. If no sample count in the grid can satisfy the intersection, the frozen
recipe uses a minimum greater than the grid's sample count. The existing detector
then rejects all nuclei through its ordinary size filter. Its watershed, expansion,
algorithm identity and default behavior are unchanged. Resolution takes logarithmic
time in grid size and performs no image reads; the detector adds no per-cell unit
conversion.

The returned request retains the **resolved pixel recipe**, not the invocation's
raw physical thresholds. Existing QCL version-1 archives preserve those integer
cutoffs and capture provenance. Replaying a saved run executes its frozen cutoffs
and does not reinterpret them using later document calibration. Call the factory
again for a new image/ROI or when requesting physical limits under new calibration.
An unarchived request with an enabled physical bound becomes stale if the
document calibration revision changes after capture. Both-disabled requests
retain ordinary matching behavior. Replaying an already archived recipe does not
reenable a physical calibration dependency.
Copying `request.parameters` explicitly copies the sampled recipe. Calibrated
historical measurements continue to use their original captured units.

Project batches accept `ProjectBatchOptions.cell_min_nucleus_area_um2` and
`cell_max_nucleus_area_um2`. They resolve each image/ROI independently. The
`cell_area_limits` entry in the batch parameter manifest retains both original
physical arguments, including null disabled bounds, and participates in the
parameter hash. Default manifests retain their existing bytes. Unknown calibration
follows the selected per-image failure policy; changed calibration cannot reuse an
incompatible checkpoint. These options reject non-cell batch kinds.

```text
qupathxx_batch input.qpxproj output-directory --kind cells --cell-min-area-um2 10.1 --cell-max-area-um2 100
```

The image inspector's **Physical nucleus area** controls use the same resolver.
Embedded callers can initialize `GuiOptions.cell_min_nucleus_area_um2` and
`cell_max_nucleus_area_um2`. Project batch presets expose the same options; using
image analysis settings captures the current inspector's physical limits.

This adds physical nucleus area filtering. Blur, peak/seed distance and cell
expansion still use sampled pixels. Arbitrary resampling, physical radii, QuPath
segmentation agreement and biological validation remain separate requirements.

The [Linux receipt](benchmarks/cell-area-limits-linux.json) records 42 native
suites, 96 focused passes and the full regression: 1,680 passes, 394 optional
skips and four strict scientific expected failures. Installed stubs, strict
positive/negative typing and relocated runtime checks pass. The generated large
pyramid is supplied to this full run.

The [Windows receipt](benchmarks/cell-area-limits-windows.json) records 43 native
suites, 117 focused Python/desktop passes, installed workflows and matching
stubs/typing. The full Windows regression passes 2,064 cases, with 10 optional
skips and four strict scientific expected failures. The visually inspected
[completed desktop frame](images/cell-physical-area-limits.png) shows both limits,
one saved cell, the frozen 81–90-sample recipe and successful one-row calibrated
CSV export, with no UI overflow. Hosted run 40022136685 is pending; this candidate
remains provisional. These checks do not accept segmentation parity or hardware
performance.
