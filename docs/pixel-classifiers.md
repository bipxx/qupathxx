# Pixel features and portable classifiers

The owned-buffer C++ and Python APIs extract multiscale pixel features, train
through the existing optional forest backend, and produce label/vote masks from
a portable saved recipe. See the [runnable example](../examples/pixel_classifier.py)
and [native contracts](../cpp/include/qupathxx/pixel_classifier.hpp).
This increment does not provide the annotation-driven ImGui pixel workflow.

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
white reference and portable forest. Encoding `QPC\1` is bounded to 64 MiB plus
4 KiB, checksummed and canonically validated. New-file writes refuse existing
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

The recipe records grid-pixel scales, not an automatic physical resolution or
pyramid policy. Callers must supply a consistently sampled grid for training
and inference. Changing resolution without resampling changes the meaning of
features. Native C++ calls accept cancellation tokens; Python feature extraction, prediction and recipe-file
operations release the GIL but currently have no managed pixel job controller.

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

The [local validation record](benchmarks/pixel-classifier-local-windows-linux.json)
checks wheel/native/source identities, 1,380 full-suite passes with six expected
skips, and 139 focused passes. Deduplication verifies 1,381 distinct passing
cases, including the additional saved-stain recipe regression. Native checks
pass 38 Windows GUI and 37 each Windows headless/Linux training/minimal/sanitizer.
Exact stubs, strict positive/seven-error negative typing and the installed
software-GL GUI workflow pass. Hosted [run 39854833786](https://github.com/bipxx/qupathxx/actions/runs/39854833786)
targets `875006d`; its result is pending.

## Remaining workflow

Annotation sampling/overlap policies, source-aware ImageServer workers,
calibrated resolution selection, stale-result tickets, ImGui training/overlay
controls, contour publication, project/recovery pixel records and expanded
texture features remain open. Research cohorts and direct QuPath comparisons
also remain required. Object classifier records do not persist pixel recipes.
