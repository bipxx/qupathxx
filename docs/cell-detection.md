# Native cell detection

`qpx.distance-watershed/1` detects nuclei and expands nonoverlapping cell bodies
inside an existing area ROI. C++, nanobind and Dear ImGui share the same kernel,
captured request and publication transaction. This is an initial detector;
numerical or workflow equivalence with QuPath is not established.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
roi = doc.add_rectangle(q.Rect(0, 0, 200, 200))
parameters = q.CellParameters()
parameters.sigma = 1
parameters.cell_expansion = 5
transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
job = q.CellJob()
job.request(q.CellRequest(doc, roi, transform, parameters))
status = job.wait(doc, roi)  # Python releases the GIL while waiting.
if status.state != "ready":
    raise RuntimeError(status.error or status.state)
result = job.result
print(result.cell_count, result.features)
ids = job.keep_result(doc, roi, class_name="Detected")  # Requires GEOS.
assert doc.undo()  # Removes this entire cell batch.
assert doc.redo()
```

The GUI inspector provides feature/threshold, Gaussian sigma, peak radius,
seed spacing, expansion, nucleus size limits, connectivity and native level.
Ctrl+G detects, Ctrl+Shift+G adds cells, and Ctrl+Alt+G cancels. Nucleus and body
previews reuse the bounded tissue overlay uploader. White reference and supplied
stain vectors use the shared stain controls. `GuiOptions.cell_parameters`
captures a value snapshot. Replay can wait for `CELLS_READY` or `CELLS_PUBLISHED`.

## Sampling and numerical policy

The request captures the source ROI, reader, transform and document authority.
An explicit level is strict; automatic selection chooses the finest native level
whose sample grid and reserved work fit. There is no arbitrary resampling.
Coordinates and contours use level-zero image coordinates. Sigma, peak radius,
seed spacing and expansion use **sampled pixels**; size filters use sample counts.
Physical calibration does not change those parameters.

Only opaque pixel centers inside the ROI enter detection or measurements.
Thresholding uses the sum of three optical densities, or one supplied-vector
stain channel. Each native transform channel is float32; sums and subsequent
processing use double precision. Gaussian smoothing is separable, truncated at
`ceil(3*sigma)`, and normalized by the eligible-domain weights. It operates over
the full crop, so decode tile edges introduce no Gaussian seams. Native fractional
level read origins follow the [tissue sampling contract](tissue-detection.md).

The binary threshold is inclusive (`score >= threshold`). An exact squared
Euclidean distance transform treats the exterior crop as background. Equal-height
regional maxima use the selected 4/8 connectivity, with the first raster point
representing each plateau. Candidates meeting minimum peak radius are ordered by
height, then raster position. Greedy Euclidean spacing rejects distances strictly
below the requested spacing. Accepted seeds receive labels in raster order.

The marker watershed floods negative distance with a minimum heap, monotone path
cost and FIFO insertion ties. Pixels are assigned on first insertion; there are
no watershed lines. Foreground components without retained seeds are omitted.
Nucleus size filtering occurs after watershed, followed by dense label remapping.
Markers retain their original label and expose `result_label=0` when filtered out.

Expansion assigns eligible pixels within the inclusive radius to the closest
retained nucleus pixel. Equal distances prefer the source pixel with lower row,
then lower column. Cells do not overlap. Expansion measures straight-line distance
and may cross excluded gaps; excluded pixels remain background and a body may be
multipart. With zero expansion, bodies equal nuclei.

Compartment features report sample counts, RGB means and three transform-channel
means for nuclei, bodies and cytoplasm. Means use original unsmoothed samples;
cytoplasm excludes nuclei and is `None` when empty. Channel accumulation uses
compensated double sums, including a separate cytoplasm sum. OD-sum detection
reports OD R/G/B; stain detection reports the three deconvolved channels.

## Ownership, publication and limits

`detect_cells(request)` is synchronous; `CellJob` runs one native worker with no
backlog. Polling reports read progress and processing phase. Cancellation, changed
source authority/ROI, closed readers and changed selection prevent publication.
Failures leave no partial result or document edits; a completed job can be reused.
The result owns immutable native rasters, features and contours independently of
the document, worker or Python wrappers.

With GEOS, both raster layers are traced and clipped on the worker; every body
must cover its nucleus. Without GEOS, numeric results and previews remain
available, while contour publication is unavailable. `keep_result` creates Cell
objects with body/nucleus geometry, source parent and optional class in one atomic
document transaction. It preserves source selection. Each result can be published
once; Undo/Redo operates on that batch. An empty result adds no history entry.

For `N` samples and maximum grid dimension `D`, the logical reservation is
`80*N + 40*D + 1024*min(N,max_cells) + 16*tile_size**2 + 65536`, plus two contour
work budgets when GEOS is enabled. The default 32 MiB contour budget per layer
therefore reserves 64 MiB even for small crops. Reduce contour budgets explicitly
when requesting a smaller total work limit. `max_cells` caps regional-maximum
candidates **before** spacing and size filtering; contour edge/vertex/component
limits can also reject the whole request. Reservations describe native payload
and scratch plans, not a process RSS guarantee or a bound on GEOS allocator costs.
`retained_bytes` counts native result containers and contours, excluding shared
source ROI payload, allocator overhead and string capacity. Preview GPU payload
is bounded separately at 32 MiB, uploading at most four 256-pixel tiles per frame.

Published body/nucleus objects persist through project/recovery and GeoJSON
storage. [Cell run archives](cell-archive.md) retain masks, parameter bundles,
markers and feature tables with input digests and rechecking.
[Published cell feature links](cell-feature-links.md) retain measured run/label
identity in objects and schema-6 project/recovery storage.
[Project-wide cell tables](project-cell-tables.md) add image context to batch
exports. [Native cell CSV export](cell-tables.md) exposes archived run labels and
compartment features through Python and ImGui. Complete source and
environment manifests remain unfinished. The raster layers reuse the `TissueResult` read interface
with distinct algorithm IDs; they cannot be retained as threshold tissue records.

Background subtraction, physical-unit filters, marker editing, split/merge,
stain estimation, classifiers, joined published-object tables and pinned QuPath comparisons
remain unfinished. Broader slides, hardware and prolonged memory pressure need
validation. The overall M0–M7 plan remains active.

## Validation and reproduction

`tests/cell_reference.py` independently uses SciPy smoothing/distance transforms,
scikit-image regional maxima/watershed and brute-force expansion. Install the
`test-cells` extra for these pinned reference dependencies; neither is required
at runtime. Tests compare complete labels, marker identities and compartment
means across stain features, smoothing, connectivity, decode boundaries,
ROI holes and coarse native levels. Native tests exhaust all 512 binary 3×3
distance/nearest-site cases, including tie rules, and check alpha exclusion and
cancellation. Desktop tests compare native labels, captured orange preview pixels,
publication, source selection and Undo/Redo. Persistence checks compare project
state and topologically equivalent GeoJSON body/nucleus geometry.

The complete regression run passes 786 Python cases with two unavailable Windows
symlink cases skipped. The rebuilt final wheel passes all 51 focused cell/desktop
cases after the last numerical and control-width fixes; all 23 native suites
pass in full/core/minimal configurations. Tiny positive sigma is checked against
the identity limit to prevent kernel underflow from silently erasing detections.

Run `python tools/benchmark_cells.py --output build/cells.json` with the installed
wheel; add `--gui` for the selected 100001-object viewer. The synthetic 1024²
fixture has 256 disk nuclei and verifies complete nucleus/body labels against
independent disk and enumerated-dilation masks before recording timings.

The [headless recording](benchmarks/cells-1024-windows.json) takes 261.52 ms
worker time, with 0.0775 ms submission and 0.0210 ms maximum sampled poll. The
reservation is 162390016 bytes; retained result containers report 9384086 bytes.
The [zoomed viewer recording](benchmarks/cells-view-1024-windows.json) keeps
100001 objects selected, 911 visible, and records 17.05 ms sampled CPU frame p95
and 17.73 ms maximum with 4 MiB each of image and cell preview GPU payload.
The [all-visible overview](benchmarks/cells-overview-1024-windows.json) records
109.39 ms p95 and 158.22 ms maximum. Reproduce that case with `--gui --overview`.
Drawing a dense visible object set remains a limit; the zoomed result does not
establish a general frame-time guarantee. Runs are warm synthetic observations
on Windows 11, an Intel Family 6 Model 151 CPU and NVIDIA RTX 4090.

Algorithm references: the [QuPath cell-detection workflow](https://qupath.readthedocs.io/en/stable/docs/tutorials/cell_detection.html)
motivates ROI-based detection; the [scikit-image watershed example](https://scikit-image.org/docs/stable/auto_examples/segmentation/plot_watershed.html)
illustrates distance/marker separation of touching objects. The native squared
distance implementation independently follows the lower-envelope formulation in
[Felzenszwalb and Huttenlocher (2012)](https://cs.brown.edu/people/pfelzens/papers/dt-final.pdf).
