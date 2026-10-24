# Native LoG cell detector integration

The native nucleus kernel is implemented and validated as a standalone engine
component. It is not yet exposed as a production `CellParameters` mode. The
installed distance-watershed detector and its three strict scientific expected
failures remain unchanged.

## Implemented kernel

`cpp/log_watershed.hpp` independently implements float separable Gaussian
smoothing with nearest-edge extension, a discrete negative Laplacian, positive
four-connected regions, an intensity check before per-region hole filling,
bounded morphology, Euclidean distance, prominence reconstruction, peak
components and a watershed with separation lines. Radius/spacing and area/mean
filters precede native ROI clipping and dense output labels. Peak connectivity
is explicit; the region and watershed-line stages use four-connectivity.

The reconstruction uses an indexed maximum heap with one entry per foreground
pixel, without duplicate stale entries. Peak suppression uses spatial bins.
Raster memory has an explicit admission guard and per-region filling has a
bounded traversal guard. Native cancellation checkpoints cover processing.
These guards are logical bounds, not process RSS or throughput acceptance.

The standalone executable reports its compiled source fingerprint, Git revision
and compiler. Its input format is an internal test protocol, not a released
analysis CLI. The Python comparison tool supplies feature rasters and reads
native labels; it does not implement or replace native mask processing.

## Verified observations

At source `11bd25943f826c75b3fe6f16ad7b92681c83ee31`, the native unit suite
passes twelve independent simultaneous fixed-point reconstruction oracle cases,
prominence/plateau cases, blank/disk/domain checks, cancellation and work guards.
The [receipt](benchmarks/native-log-watershed-windows.json) binds native bytes,
compiled and committed source identities, executed tooling and the pinned
capture manifest. All 244 build inputs match the commit after CRLF normalization.

All nine captured cell counts match under both NumPy OD-sum and actual native
channel-OD inputs. Matched nucleus masks and centroids are exact in seven cases:
blank, isolated cells, touching pair, gap pair, size filtering, gradient/noise and
anisotropic level zero. Isolated count is three with IoU 1 and centroid error 0
for each nucleus, exceeding the unchanged 0.85/1-pixel kernel checks.

ROI-edge/hole foreground IoU remains about 0.970; the first partial nucleus has
IoU about 0.893 and centroid error 0.488 pixels. The deliberately different
resampled reference remains about 0.939 foreground IoU. All cases are retained.
Process timings include startup and file I/O and do not accept performance.

## Pending production integration contract

The next implementation must complete these surfaces together:

| Surface | Required behavior |
| --- | --- |
| Native parameters | Explicit distance/LoG algorithm choice; preserve legacy defaults and reject invalid or inactive nondefault options. Capture prominence and bounded context padding. |
| Working grid | Expand the natural ROI grid by captured padding, clipped to the selected image level. Include every read pixel in the input digest. Retain feature values outside the ROI for segmentation context, while published labels remain clipped to valid ROI samples. |
| Resource admission | Freeze the expanded grid and its complete native work estimate before launching a worker or choosing an automatic natural level. Keep cancellation and stale-result rejection. |
| Result authority | Native nucleus labels, body expansion, features, contours, markers and provenance form one immutable result. Preserve original-grid coordinates and explicit detector identity. Resolve clipped-area and marker invariants consistently with archives. |
| Input identity | Separate the LoG pixel-digest domain from the legacy detector; hash the same captured context grid during replay and verification. |
| Cell archive | Use a versioned LoG encoding with detector-specific parameters and context-grid validation. Keep QCL1 byte/algorithm identity and rejection rules. Unknown versions and policies reject even with a valid checksum. |
| Saved classifier sources | Preserve the detector and every captured parameter in immutable cell feature sources. Version the affected classifier encoding; old records remain readable. |
| Python and typing | Thin nanobind parameters and typed exports; no Python callbacks or Python-owned worker allocations. Existing analysis/job/record APIs execute the new native mode. |
| Batch and CLI | Named detector/options, distinct manifests and resume identities, cancellation, save/reopen and exported checkpoints. No silently ignored inactive settings. |
| ImGui and viewer | Detector and applicable controls, copied request parameters, immutable worker capture, presets/reset, managed viewer commands and stale publication rejection. |
| Tables | A versioned cell-table format retains detector-specific parameters and optional captured units. Preserve existing distance-detector schemas and explicit format compatibility. |
| Acceptance | Exercise all nine pinned cases and three tile sizes through the installed production path. Preserve counts, IoU/centroid tolerances and measurement checks. Cover ROI boundaries/holes, transparency, natural pyramids, archive/replay, batches, UI, typing, cancellation and resource failures. |

Arbitrary resampling, exact ROI-edge refinement, broad biology, RSS/frame budgets
and supported release qualification remain separate unfinished requirements.
Kernel observations cannot accept the public detector before those production
paths exist and have been exercised.
