# Threshold tissue masks

The first tissue workflow shares one native C++ implementation between Python
and ImGui. It thresholds an area ROI at a native pyramid level, then labels connected
components across every decoded tile boundary. Algorithm identity is
`qpx.threshold-components/1` at level zero and `/2` at nonzero levels.
Native pyramid sampling supports large image regions within a bounded raster;
arbitrary resampling, contour creation and saved masks remain open.

## Numerical contract

`TissueRequest` captures the selected object's body or nucleus ROI, native
source identity and normalized stain transform. Nominal sampled pixel centers inside the ROI
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
`width` and `height` count sampled pixels; they are not the level-zero extent.

## Pyramid resolution and coordinates

`TissueParameters.level` defaults to `-1`: choose the available level with the
smallest downsample whose cropped grid fits both the work budget and uint32
indexing. Ties use the first level index. Capture performs no image decoding.
It skips levels with no nominal centers in the ROI bounds. If no level fits,
capture fails before allocating the mask. An explicit nonnegative level is
strict: capture either uses that level or fails, without coarsening it. Sources
with only level zero cannot automatically synthesize a smaller raster.

For level downsample `d`, nominal centers are `((gx+.5)*d, (gy+.5)*d)` in
level-zero coordinates. Crop start/end indices are `ceil(edge/d-.5)`, clipped
to the native level dimensions. These half-open bounds preserve the level-zero
sampling rule and support fractional ROI edges. ROI predicates use those nominal
centers; body/nucleus holes and ellipses work at the selected level. Tiny ROIs
may contain a coarse nominal center even when they contain no level-zero center.
An irregular ROI can still contain no eligible centers after its bounds fit;
the worker then fails rather than returning a fabricated mask.

`request.level` and `request.downsample` expose the resolved resolution while
`request.parameters.level` retains the user's requested value. Both request and
result expose `level_bounds` in sampled grid coordinates and `bounds` in
level-zero coordinates. Component bounds and centroids use the same scale.
Pixel counts and size filters count sampled pixels, not level-zero pixels or
physical area. A nominal edge cell may extend beyond fractional ROI/image edges;
the mask is a sampling grid, not an exact ROI contour.

Each tile starts at the rounded integer level-zero position
`(round(gx*d), round(gy*d))` and reads native-level pixels, without extra
resampling. Fractional scales can give the reader a subpixel phase that depends
on the tile origin. Consequently, masks at different fractional-scale tile
partitions need not agree. The input digest records the exact partition and
origins; reference tests reconstruct those actual reads. This follows the integer
origin interface, including [OpenSlide generic-TIFF's coordinate conversion](https://github.com/openslide/openslide/blob/v4.0.0/src/openslide-vendor-generic-tiff.c#L110).
It does not establish phase-independent resampling or vendor-reader parity.

## Python example

```python
import numpy as np
import qupathxx as q

# doc contains an image and object_id identifies an area ROI.
parameters = q.TissueParameters()
parameters.threshold = 0.3
parameters.connectivity = 8
parameters.min_pixels = 20
parameters.level = -1  # Or an explicit native pyramid level.
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

The inspector exposes feature, threshold, native level and size/connectivity filters.
White/stain vectors and the nucleus toggle are shared with region analysis.
Ctrl+D starts detection; Ctrl+Shift+D cancels. The mask preview uploads at most
four 256-pixel tiles per frame, with one tile of RGBA scratch. It becomes visible
only after preparation completes. A separate 32 MiB logical GPU payload budget
and 4096-position limit bound the preview; background tiles need no texture.
Stale results release their preview. GPU driver overhead is outside this bound.

Level-zero input SHA-256 retains the existing `qpx.rgba-tiles/1` decoded-tile
stream, preserving previous digests. Other levels use `qpx.rgba-level-tiles/1`:
the ASCII format tag followed by little-endian uint64 source width/height,
selected level, level width/height, the IEEE-754 binary64 downsample bits,
grid x0/y0/x1/y1 and tile size. Each tile appends grid x/y, integer level-zero
read x/y, output width/height (all uint64), then row-major RGBA bytes.
`result.input_format` identifies the stream. Parameters are stored separately. This is
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
The installed GUI wheel passes all 558 Python tests (two unavailable Windows
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

Sixteen additional pyramid cases verify native levels at scales 1.5 and 4,
independent formulas/flood filling and little-endian digest reconstruction,
automatic budget boundaries, strict explicit levels, tiny coarse samples,
nucleus holes, stale publication and a framebuffer overlay at the correct
level-zero scale. Native fixtures additionally test partition-independent labels
for an exact-grid fractional-scale reader and pre-decode large-image selection.

The [205520896-pixel pyramid recording](benchmarks/tissue-pyramid-205520896-windows.json)
selects level 3 at 8x downsample, processing 3211264 samples (64 times fewer).
Its two measured workers take 153.79/134.65 ms, including native TIFF decoding;
capture takes 0.0052 ms and maximum sampled polling is 0.0167 ms. Estimated work
is 33204224 bytes and retained buffers use 19245488 bytes. Independent constant
fixture pixels reconstruct the exact input SHA-256, labels, bounds and centroid.
The [100001-selected-object pyramid viewer](benchmarks/tissue-pyramid-view-100001-windows.json)
records 137.43 ms GUI worker time, 17.22 ms sampled frame p95 and 19.00 ms maximum,
with 12845056 bytes of mask GPU payload and 876 visible objects. Generate the
fixture with `tools/generate_large_fixture.py` and run
`tools/benchmark_tissue_pyramid.py`, adding `--gui` for that workload.
These are synthetic first/second runs, not controlled cold-storage or vendor
benchmarks; sample reduction is not a measured speedup against full resolution.
The [level-zero regression recording](benchmarks/tissue-level-zero-regression-windows.json)
retains every analytic label and the original input digest, recording 69.50 ms
worker time for 4194304 pixels. Single recordings do not establish a performance
trend against the initial packet's 58.66 ms run.

[QuPath v0.7.0 SimpleTissueDetection2](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core-processing/src/main/java/qupath/imagej/detect/tissue/SimpleTissueDetection2.java)
uses a different workflow, including grayscale thresholding, downsampling,
smoothing, cleanup and annotation conversion. This implementation has no claim
of numerical QuPath parity. Arbitrary resampling, physical-size filtering,
morphology, hole filling, contours and annotation conversion, persistent mask
provenance, large-mask streaming, cell detection and runtime comparisons remain
required under M5.
