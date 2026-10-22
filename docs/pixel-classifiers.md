# Pixel features and portable classifiers

The C++ and Python APIs extract multiscale pixel features, sample labelled
ROIs on native workers, train through the optional forest backend, and predict
source-aware ROI masks using portable saved recipes. See the [runnable example](../examples/pixel_classifier.py)
and [native contracts](../cpp/include/qupathxx/pixel_classifier.hpp).
The annotation-driven ImGui pixel panel and project pixel records remain open.
See the [source-aware example](../examples/pixel_workflow.py).

## Numeric contract

`PixelFeatureOptions` selects RGB bytes, optical densities and/or three stain
channels, with 1–8 strictly increasing sigmas in 0–16 **sampled-grid pixels**.
The default is RGB at 0, 1, 2 and 4. Each channel/scale always includes a Gaussian
value; gradient magnitude and Laplacian are optional. Up to 128 columns are
allowed. Ordering is group (RGB, OD, stains), channel, scale, then Gaussian,
gradient, Laplacian. Schema names include scales and derivative kinds; OD names
bind the white reference and stain names bind the full stain model. RGB feature schema
and feature/input identities ignore unused stain/white settings.

Gaussian support is `ceil(3*sigma)` on each axis, normalized over opaque pixels
inside actual image edges. Sigma zero returns channel values. Derivatives use
central differences on the smoothed double field, then round to float32:
`hypot((right-left)/2,(bottom-top)/2)` and
`left+right+top+bottom-4*center`. Missing, outside-image or nonopaque stencil
neighbors use the center value (zero normal flux). RGB units are device bytes;
OD/stains use the existing stain conversion. Derivative units refer to grid
pixels, not physical micrometers. These definitions are versioned as
`qpx.pixel-features/1`; QuPath algorithm equivalence is not established.

Only alpha 255 is valid. Other alpha values produce validity zero and NaN
features and never reach the forest. No alpha blending or NaN imputation occurs.
The returned H×W×F float32 matrix and H×W uint8 validity mask own native storage.
Their NumPy views are read-only and retain that storage; `copy=True` returns
independent writable arrays. Metadata access returns copies.

## Training, prediction and persistence

Extract features, select labelled valid rows, and pass their matrix/schema to
`train_forest`. Labels index an explicit class list; the forest's existing
100,000-row, 128-feature and per-class minimum limits apply. Selection,
balancing and biological labels are the caller's responsibility.

`PixelClassifier(forest, options, transform)` validates the exact schema and
keeps an immutable copy of the extraction recipe. RGB-only callers can omit the
transform. `to_bytes`/`from_bytes` and `save_pixel_classifier`/
`load_pixel_classifier` preserve flags, scales, work budget, normalized stains,
white reference and portable forest. Encoding `QPC\1` (unbound buffer recipe) or `QPC\2` (resolution-bound recipe)
is bounded to 64 MiB plus 4 KiB, checksummed and canonically validated. Existing
unbound recipe bytes remain readable and re-encode identically. New-file writes refuse existing
destinations. Explicit replacement first validates an existing pixel recipe;
arbitrary image, project or bare-forest files are preserved. Publication is
atomic and native file reads validate stable file observations.

`classify_pixels(pixels, recipe, min_vote_fraction=0, window=None, work_bytes=0)`
extracts features and gathers at most 512 valid rows per forest call. No
whole-image per-class vote table is retained. Labels are uint32: zero means
unclassified, otherwise class-list index plus one. Validity remains one for
valid pixels below the threshold. Their top-tree vote fraction is retained;
invalid pixels have NaN fractions. Threshold decisions use the native vote
ratio before float32 output rounding. Ties choose the first class, following the
portable forest. Fractions are tree votes, not calibrated probabilities.
`class_counts` excludes zero labels. Recipe, input, feature and output hashes
identify the calculation; they grant no live document or source authority.

Feature scales remain grid-pixel values. `PixelResolution` optionally binds a
recipe to a declared training grid: its downsample and, when available, both
sampled physical pixel spacings. Native C++ calls accept cancellation tokens.
Python extraction, inference, training and recipe-file operations release the
GIL. The managed training/prediction controllers below reconcile live authority
on their owning thread.

## Context, tiling and budgets

`PixelFeatureWindow` identifies the logical image size, requested output bounds,
and supplied RGBA buffer origin. Supply the full halo:
`ceil(3*max_sigma) + (gradient or Laplacian ? 1 : 0)`, clipped only at actual
image edges. Missing context is rejected. Only the required context is copied
and processed even if the supplied buffer is much larger. A tile and the same
window of a full input have identical feature/input/output hashes. Tile outputs
and full-image slices are bit-identical; hashes of differently sized output
windows are deliberately different.

Budgets are 1–1024 MiB conservative allocation estimates checked before feature
or prediction allocations. They cover context, features, output masks and
bounded vote scratch. Prediction can override the recipe budget without
changing the recipe. Estimates exclude caller RGBA/model storage, Python copies
and runtime overhead; they are not process RSS limits. A whole large slide
must be tiled rather than materialized by this API.

The synthetic samples use 18 features and 16 trees/depth 4. Full timings
are medians of three calls; tiled timing is one sequential traversal, including
Python tile copies and stitching. Both verify every synthetic class label and
bit-identical full/tiled labels and vote fractions.

| Grid | Full prediction | Full work estimate | Tiled traversal | Maximum tile work |
| --- | --- | --- | --- | --- |
| [512×512](benchmarks/pixel-classifier-512-windows.json) | 195 ms | 52.8 MB | 190 ms, 16 tiles | 3.8 MB |
| [1024×1024](benchmarks/pixel-classifier-1024-windows.json) | 762 ms | 210.8 MB | 918 ms, 64 tiles | 3.8 MB |

Both use 128×128 output tiles. Tiling keeps per-call work bounded and adds
context/copy overhead; the larger case takes longer when tiled. These
shared-workstation observations establish no paired implementation speedup,
biological accuracy or hardware/frame-budget acceptance. Reproduce with
`python tools/benchmark_pixel_classifier.py --size 1024 --output report.json`.

The owned-buffer increment's [local validation record](benchmarks/pixel-classifier-local-windows-linux.json)
checks wheel/native/source identities, 1,380 full-suite passes with six expected
skips, and 139 focused passes. Deduplication verifies 1,381 distinct passing
cases, including the additional saved-stain recipe regression. Native checks
pass 38 Windows GUI and 37 each Windows headless/Linux training/minimal/sanitizer.
Exact stubs, strict positive/seven-error negative typing and the installed
software-GL GUI workflow pass. Hosted [run 39854833786](https://github.com/bipxx/qupathxx/actions/runs/39854833786)
targets `875006d` and passed all five jobs. The [hosted artifact record](benchmarks/pixel-classifier-ci-windows-linux.json) verifies exact installed
artifacts and fixture hashes, including the seeded pixel model/input/output
identities on Windows and Linux. It precedes the annotation/source-worker work
below.

## Annotation-driven worker training

`PixelTrainingRequest(document, annotation_ids, transform=identity, options=...)`
captures immutable labelled area ROIs and class names. Explicit IDs can refer to
annotations, detections or cells; there are no implicit labels inferred from
unselected objects. Capture accepts 1–4096 unique IDs, 2–256 classes and at most
64 MiB of charged geometry/provenance, within the overall work budget. Empty
labels, non-area geometry and unsupported source metadata reject before reads.
All selected ROIs must lie inside the image; holes use the actual geometry.

`PixelTrainingOptions.level` chooses an existing natural pyramid level (default
zero). `calibrated=True` records both effective document calibrations times that
level's downsample when both are available; otherwise the recipe records raw
level downsample. Set `calibrated=False` to explicitly train a raw-grid model.
Sigmas and derivative units still refer to that sampled grid.

The worker builds a bounded map of tiles intersecting ROI bounding boxes,
merging duplicate tile visits. At each opaque pixel center, same-class overlaps
count once and the lowest matching object ID is retained. Conflicting classes
exclude the pixel and increment `ambiguous_pixels`; partially transparent pixels
are excluded before membership counts. The algorithm is
`qpx.annotation-pixel-sampling/1`.

A seeded, deterministic bottom-k priority reservoir runs separately per class.
Coordinates determine priorities, with y/x breaking ties. Output rows are ordered
by sorted class name, priority and coordinate. Sampling is independent of input
ID order and tile size for readers delivering the same logical grid. By default
at most 2,000 rows/class and 20,000 total are allowed; the total cap is divided
equally between classes before sampling. Balanced mode then trims every class
to the smallest available reservoir. `balanced=False` keeps each class's
individual bounded reservoir. Every class needs at least two unambiguous opaque
centers. `eligible_class_pixels` and `sampled_class_counts` expose the distinction;
there is no oversampling or class weighting.

The default scan cap is ten million pixels across planned tile rectangles,
including outside-ROI pixels within those tiles, with a configurable maximum of
100 million. The plan is checked before decoding. Use a coarser level or smaller
ROIs when the cap is exceeded. Tile masks are computed before features, and a
tile whose candidates cannot enter any reservoir skips feature extraction.
Halo context remains unmasked by the training labels, so features include the
surrounding image rather than creating artificial ROI edges.

`sample_pixel_training` returns an immutable `PixelTrainingTable`: float32 N×F
values, uint32 N labels, int64 N×2 sampled-grid x/y coordinates, uint64 N source
object IDs, schema/classes, source provenance, resolution
and input/numeric identities. The native table also retains captured ROI snapshots
internally. Arrays retain native ownership, are read-only by
default and support independent `copy=True` results. The table's numeric hash
identifies its ordered cohort; the input hash additionally seals decoded tile
context and per-pixel class membership. Retained-byte fields are conservative
charged estimates, rather than a process RSS measurement.

`train_pixel_classifier(table, parameters)` returns a resolution-bound portable
recipe. Omitted forest settings use `pixel_forest_parameters()`: 32 trees and
depth 8. This high-level pixel preset fits the default sampled cohorts within
the work budget; the general `ForestParameters()` defaults remain 64 trees and
depth 12. Explicit parameters are preserved and checked against the remaining
budget, rather than silently reduced. `PixelTrainingJob.train(request, parameters)` performs sampling and
training in one native worker. `poll(document)`/`wait(document)` report planning,
sampling and training phases, completed/total tiles and failure details, then
expose `table`/`model` when ready. There is one worker and no backlog. Cancellation,
source closure/relink, using a different/copied document or any document revision change prevents
publication and clears controller results. Kernels never inspect a live document;
source ownership remains on the controller through worker completion. Forest
training itself has cancellation checks before/after the backend call, rather
than interruption inside OpenCV.

## Source-aware prediction

`pixel_classifier_level(document, recipe)` resolves a natural level matching the
recipe. Physical models match **both** calibrated axes with relative tolerance
1e-9, even when training and prediction have different level-zero calibration
and therefore use different downsample factors. Raw models match downsample.
Missing calibration or no matching natural level is an error. No automatic
resampling or reinterpretation of scales occurs. Unbound `QPC\1` models can still
use the owned-buffer API; source prediction requires an explicitly declared
`PixelResolution` or a model produced by annotation training.

`PixelPredictionRequest(document, object_id, recipe, options=...)` captures an
area ROI, source/grid, provenance and strict live authority. Owner capture checks
raster output and worst-case tile memory before any image read. The worker reads
bounded halo tiles, calls the portable predictor, and assembles the cropped ROI
bounding-grid result. Alpha and the actual ROI jointly determine validity;
outside-ROI/hole pixels have label zero, validity zero and NaN vote fraction.
Low-vote opaque pixels inside the ROI retain validity and their vote fraction.
`PixelPredictionResult` exposes classes through its immutable model, counts,
threshold/options, level/grid/world bounds, original ROI and provenance, owned
mask arrays and decoded-input/output identities. Its algorithm is
`qpx.source-pixel-classification/1`.

`PixelPredictionJob.request(request)` runs this on a native worker with bounded
tiles and no backlog. Poll/wait reject the same stale-document/source conditions
as training. A result already held by Python stays an immutable historical value
after controller retirement, without restoring any live publication authority.
Both source workers map a grid index to a level-zero decoder origin by rounding
`index * downsample`, matching the existing tissue/cell pyramid sampling contract.
Exact fractional-scale tile equivalence is verified with a reader implementing
that grid contract; decoder-specific fractional-phase behavior still needs
reader/reference research coverage.

The global work estimate covers snapshots, planning/heap capacity and retained
training rows plus tile decode/features, or the assembled prediction masks plus
per-tile scratch. The retained training table is charged before setting the
forest backend's remaining work allowance. Caller image/model storage, Python
copies and runtime overhead remain excluded. Large output ROIs can still exceed
the budget even with tiled features; choose a smaller ROI when that happens.
There is no full-slide streaming output file implementation yet.

## Worker measurements and validation

The [runnable source example](../examples/pixel_workflow.py) trains from two
labelled ROIs, saves/reopens a calibrated recipe, predicts an ellipse, and verifies
that its mask remains owned after a document edit retires the controller result.
The [benchmark driver](../tools/benchmark_pixel_workflow.py) verifies every
sampled class/RGB value, a balanced 4,000-row cohort, recipe roundtrip, all output
labels, exact source-tiled/direct-buffer votes and stale-result rejection.

| Synthetic grid | Worker sampling + training | Source prediction | Prediction work estimate | Direct buffer work estimate |
| --- | --- | --- | --- | --- |
| [512×512](benchmarks/pixel-workflow-512-windows.json) | 191 ms | 206 ms | 6.8 MB | 52.8 MB |
| [1024×1024](benchmarks/pixel-workflow-1024-windows.json) | 797 ms | 982 ms | 13.9 MB | 210.8 MB |

Owner training capture takes 0.08–0.13 ms in these samples and prediction capture
about 0.02 ms. Sampling work is about 6.1 MB and retained charged table size about
0.69 MB in both. These are single observations per phase on a shared workstation,
with 18 features, 128-pixel output tiles and 16 trees/depth 4. Direct-buffer timing
excludes source decode/capture and uses a fully owned RGBA frame. They do not
establish a paired implementation speedup, process RSS, UI publication/frame
budget or biological accuracy acceptance.

Focused installed tests include a real Aperio fixture where source-tiled labels
and votes match a direct-context calculation. Native tests independently cover
opaque/partial alpha, overlap conflicts/deduplication, tile/selection order,
fractional/integer natural grids, budgets before reads, calibration mismatch,
backend-free portable inference and gated stale-worker rejection. The supplied
ROI classes are synthetic labels, not annotated biological ground truth.

The [source-worker local validation record](benchmarks/pixel-workflow-local-windows-linux.json)
verifies exact working-source, installed native and wheel identities, 1,399
full-suite passes with six expected skips and 157 focused passes. Windows GUI
native passes 39; Windows headless/Linux training/minimal/sanitizer pass 38 each.
Exact public stubs, strict positive/seven-error negative typing, the source
example and installed software-GL GUI launch pass. The record distinguishes an
outside-ROI test-fixture correction and a benchmark property-call correction.
A three-class default-training regression first reproduced the inherited
forest preset exceeding the work budget; the 32-tree/depth-8 pixel preset now
trains its 6,000-row cohort successfully. An optional read-only Claude review
timed out without a result; no independent
review acceptance is claimed. Hosted validation for this increment is pending.

## Remaining workflow

The ImGui pixel training/prediction panel, bounded overlay upload, contour
publication, project/recovery pixel records, full-slide streaming outputs,
expanded texture features and resampling between unmatched resolutions remain
open. Research cohorts and direct QuPath comparisons also remain required.
Object classifier records do not persist pixel recipes or these training tables.
