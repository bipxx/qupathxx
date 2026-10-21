# Threshold tissue masks

The first tissue workflow shares one native C++ implementation between Python
and ImGui. It thresholds an area ROI at level zero, then labels connected
components across every decoded tile boundary. Algorithm identity is
`qpx.threshold-components/1`. This is a bounded ROI workflow; whole-slide
downsampling, contour creation and saved masks remain open.

## Numerical contract

`TissueRequest` captures the selected object's body or nucleus ROI, native
source identity and normalized stain transform. Pixel centers inside the ROI
are eligible; holes and multipart geometry use the existing region-analysis
predicate. Only fully opaque samples contribute. A source with no eligible
opaque samples fails; an eligible image with no foreground returns an empty mask.

Features are the sum of three float32 optical densities, or one float32
supplied-vector stain concentration. OD summation uses double precision.
Threshold comparison is inclusive (`value >= threshold`). Negative stain
concentrations remain intact. White RGB and normalized stain vectors are captured
in the result. See [stain transforms](stain-transforms.md) for their formulas.

Global row-major flood filling uses four or eight neighbors. Decode tile size
does not divide the component graph. Minimum and optional maximum component
sizes count pixels, independently of calibration. The raw component limit
applies before size filtering; exceeding it fails the entire operation. Retained
components receive compact labels in first-seed row-major order.

Result labels are row-major native-endian uint32 values, with zero as background.
Python `labels` and the binary `mask` (0/255) return independent byte copies.
Component statistics contain pixel count, level-zero pixel-edge bounding box
and pixel-center centroid. Bounding boxes are statistics, not annotations.
The result's bounds locate the cropped label buffer in image coordinates.

## Python example

```python
import numpy as np
import qupathxx as q

# doc contains an image and object_id identifies an area ROI.
parameters = q.TissueParameters()
parameters.threshold = 0.3
parameters.connectivity = 8
parameters.min_pixels = 20
transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
request = q.TissueRequest(doc, object_id, transform, parameters)
job = q.TissueJob()
assert job.request(request)
status = job.wait(doc, object_id)
if status.state != "ready":
    raise RuntimeError(status.error or status.state)
result = job.result
labels = np.frombuffer(result.labels, dtype=np.uint32).reshape(
    result.height, result.width
)
```

`detect_tissue(request)` provides the synchronous path. Both synchronous work
and `wait` release the Python GIL. Parameter drafts and exposed result metadata
are snapshots; changing them cannot mutate a captured job or result.

## Ownership, cancellation and limits

The job follows the [analysis job](analysis-jobs.md) owner-thread contract:
one worker, no backlog, nonblocking polling, cancellation and applicability
checks against the live document and selected object. Source closure, relinking,
graph replacement or relevant edits reject stale publication. Calibration and
unrelated edits do not invalidate dimensionless masks. The owner retains source
ownership until the worker joins; results own native buffers without document,
source or Python references. Cancellation checks occur during thresholding,
component scanning and labeling. An uninterruptible reader call can still delay
worker completion. Failed, cancelled and stale jobs publish no partial result.

Defaults are 256-pixel decode tiles, 100000 raw components and a 64 MiB work
budget. Tile size accepts 16 through 1024; work budgets accept 1 byte through 512 MiB.
Before decoding, capture rejects a crop whose estimated work exceeds the budget:

`8 * crop_pixels + min(crop_pixels, max_components) * sizeof(TissueComponent)
+ 16 * tile_size**2 + 65536`

This covers label/queue capacity, component capacity and tile scratch with fixed
headroom. It is a logical allocation bound, not an RSS guarantee. Source images,
captured ROI geometry, reader caches, Python result copies and GPU payload are
outside it. `retained_bytes` reports actual label/component capacities after
the worker's queue and scratch have been released.

The inspector exposes feature, threshold, connectivity and size filters.
White/stain vectors and the nucleus toggle are shared with region analysis.
Ctrl+D starts detection; Ctrl+Shift+D cancels. The mask preview uploads at most
four 256-pixel tiles per frame, with one tile of RGBA scratch. It becomes visible
only after preparation completes. A separate 32 MiB logical GPU payload budget
and 4096-position limit bound the preview; background tiles need no texture.
Stale results release their preview. GPU driver overhead is outside this bound.

The input SHA-256 uses the existing `qpx.rgba-tiles/1` decoded-tile stream,
including crop and tile partition. Parameters are stored separately. This is
input evidence, not a whole-source/environment fingerprint. Tissue results are
not persisted in schema-3 analysis records; Keep/Recheck currently retains
OD/stain means only.

## Evidence and remaining work

Forty-five Python cases compare independent NumPy formulas and flood filling,
including diagonal connectivity, tile-crossing components, size filters, holes,
fractional crops, nucleus masks, budgets, stale results and source teardown.
Three GUI cases check native result agreement, actual framebuffer mask pixels
and deletion invalidation. Native tests add a controlled blocked reader,
cancellation/reuse, transparent input and resource failures.
The installed GUI wheel passes all 542 Python tests (two unavailable Windows
file-symlink cases skipped); eighteen native suites pass in three configurations.

The Windows [headless recording](benchmarks/tissue-4194304-windows.json) labels
4194304 pixels and 285 components in 58.66 ms, with 0.105 ms submission and
0.0224 ms maximum sampled polling. Estimated work is 41068544 bytes; retained
buffers use 23177600 bytes. The independent analytic mask matches every label.
The [100001-selected-object viewer](benchmarks/tissue-view-100001-windows.json)
records 39.72 ms GUI worker time, 17.34 ms sampled frame p95 and 18.88 ms maximum,
with 16 MiB mask GPU payload, on an RTX 4090. These single synthetic recordings
do not establish vendor-reader or hardware-wide latency guarantees. Reproduce
with `tools/benchmark_tissue.py`, adding `--gui` for the selected-object case.

[QuPath v0.7.0 SimpleTissueDetection2](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core-processing/src/main/java/qupath/imagej/detect/tissue/SimpleTissueDetection2.java)
uses a different workflow, including grayscale thresholding, downsampling,
smoothing, cleanup and annotation conversion. This implementation has no claim
of numerical QuPath parity. Pyramid-level selection, physical-size filtering,
morphology, hole filling, contours and annotation conversion, persistent mask
provenance, large-mask streaming, cell detection and runtime comparisons remain
required under M5.
