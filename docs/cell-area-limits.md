# Physical nucleus area limits

`CellRequest.with_nucleus_area_limits` accepts inclusive minimum/maximum sampled
nucleus areas in square micrometers. It captures the same image/ROI and selects
the same natural sampling grid as the ordinary request, then compiles the area
limits into ordinary integer nucleus-sample cutoffs before detection.

**Validation update:** the calibration-staleness gap found by the managed-viewer
test is fixed. Three regressions failed before the fix and now pass. Corrected
Windows/Linux full validation and hosted run 40024473049 pass. The
[artifact audit](benchmarks/cell-area-limits-ci-windows-linux.json) accepts the
defined engineering scope; scientific, hardware and release gates remain open.

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
parameter hash. Both-disabled manifests retain the existing field layout; their build/source
identities still describe the executing build. Unknown calibration
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
suites, 99 focused passes and the corrected full regression: 1,683 passes,
394 optional skips and four strict scientific expected failures. Installed stubs,
strict positive/negative typing and relocated runtime checks pass. The generated
large pyramid is supplied to this full run.

The [Windows receipt](benchmarks/cell-area-limits-windows.json) records 43 native
suites, 120 focused Python/desktop passes, installed workflows and matching
stubs/typing. The corrected full Windows regression passes 2,067 cases, with
10 optional skips and four strict scientific expected failures. Its prior
2,064-pass result predates the calibration-staleness fix and remains historical
evidence. The visually inspected [completed desktop frame](images/cell-physical-area-limits.png)
uses the corrected build and shows both limits, one saved cell, the frozen
81–90-sample recipe and successful one-row calibrated CSV export, with no UI
overflow. Hosted run 40024473049 at source
`f8cb02a139d5c4b4651978425cb064472b3f1b81` passes all five jobs and the full
artifact audit. Windows desktop passes 2,066 Python/43 native tests, Windows
headless passes 1,683/42, Linux headless passes 1,681/42, and minimal/sanitizer
configurations each pass 42 native suites. All installed Python cohorts pass
22 physical-area cases, including the three stale-calibration regressions; desktop
passes three physical-area GUI cases. Four scientific expected failures remain
in every Python cohort. Preceding run 40022136685 was cancelled after the gap was
reproduced. The accepted scope does not establish segmentation parity, hardware
performance, portable packaging or a supported release.


Published receipt-file hashes bind Git-committed UTF-8 LF bytes. Separate
`captured_file_sha256`/`captured_receipt_sha256` values preserve the original local
capture bytes; canonical JSON hashes preserve content identity across line-ending
normalization. Native, wheel, fixture and downloaded artifact hashes continue to
bind their original binary/file bytes.
