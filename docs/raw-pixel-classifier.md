# Quantitative pixel classifier numeric contract

Status: native features/models and bounded owner-captured sampling/prediction
jobs have [local Windows/Linux evidence](benchmarks/raw-pixel-workflows-local.json)
at `7beb80b`. Archive/retention source `440f610` now has a separate
[local receipt](benchmarks/raw-pixel-archives-local.json): the affected suite
passes on both Windows/Linux hosts; ten unchanged regression suites retain
their source220304d results. Failed development rounds remain in the receipt.
Backend-enabled forest training, live replay verification, apply/undo,
rational-window streaming, Python and UI integration remain open. RGB `pixel-v1` and QPC1/QPC2 stay unchanged.

The existing path is `PixelTrainingRequest` → `extract_pixel_features` →
float32 training rows → `ForestModel` → `PixelPredictionRequest` /
`PixelPredictionStream` → classifier archive/stream files and checkpoints.
Both the feature extractor and `classify_pixels` currently take display
`PixelBuffer`. The new private `raw_pixel_features.hpp` authority instead
consumes the immutable `ScientificRawGrid`; request capture, model codecs,
source guards and frontends must subsequently compose this authority.

## Numeric recipe

- Use the existing typed, level-zero scientific raw sampler. It rounds exact
  finite weighted means once to binary64 and preserves source dtype/bits,
  selection, calibration and missing-data identity in its input commitment.
  All ten sample types are eligible. Do not convert a 64-bit integer before
  its weighted mean. Classification does not promise to distinguish integers
  that round to the same sampled binary64 or final float32 value.
- Each selected channel requires an explicit finite binary64 offset and
  positive finite scale. Evaluate binary64 `shifted = mean - offset`, then
  `normalized = shifted * scale` as separate operations; no adaptive range,
  clipping, hidden FMA, imputation or image-wide statistics. Reject a nonfinite
  intermediate or `abs(normalized) > FLT_MAX/32`. This declared bound keeps
  centered derivatives within the forest's existing `FLT_MAX/4` domain.
  Choose normalization that fits the scientific task; it is part of the model,
  not a presentation setting. Negative zero recipe parameters are rejected.
- Each scale is a pair `(sigma_x, sigma_y)` in sampled-grid pixels, in `[0,16]`.
  Pairs increase componentwise and cannot repeat; at most eight pairs and 128
  feature columns. A zero component is an identity kernel on that axis.
  Physical sigma must be converted using each axis's validated sampled pixel
  spacing and captured in the later request recipe. Unknown physical
  calibration must reject a physical request. The private kernel infers none.
- For each channel, convolve the valid-value numerator and support using
  Gaussian weights `exp(-d*d/(2*sigma*sigma))`, radius `ceil(3*sigma)`, along
  each axis, then divide by support once. Unsupported source samples have
  zero numerator and support. Missing one channel never erases another.
- Feature eligibility requires valid source support at the center. Smoothing
  never fills an invalid center. Derivatives use smoothed left/right/top/bottom
  values only where that neighbor's original channel support is valid;
  otherwise substitute the center. Gradient is `hypot((R-L)/2,(B-T)/2)`;
  Laplacian is `L+R+T+B-4*C`, in sampled-grid units.
- Order columns by selected channel, scale pair, then Gaussian, optional
  gradient and optional Laplacian. Filter in binary64; round only the final
  features to IEEE binary32 nearest/even. Subnormal/underflow rounding is
  explicit, not a source-zero claim. Invalid features carry NaN plus a
  per-feature mask. A forest row requires every feature to be valid; no
  implicit missing-value training or prediction.

The caller supplies per-axis context of at least `ceil(3*sigma_axis)`, plus
one sample when either derivative is enabled, including unsupported context
outside the image. A streamed block must retain the full-image grid phase;
block edges are not image edges. Native preflight charges raw sampling,
metadata, captured HWC support, feature output, temporary planes and scratch
against the explicit feature work budget before IO. This is allocation
admission, not a process RSS claim. Cancellation publishes no partial result.

## Identity and remaining integration

Feature schema names include dtype, absolute selected channel, offset, scale,
both sigmas and operation. Input identity also includes the sampler's captured
input commitment. Result identity includes feature identity, geometry, support
and canonical float32 bits; invalid NaNs have one hash encoding. The later
model recipe must additionally capture channel labels, series/Z/T selection,
grid policy, calibration/physical units and normalization policy. Prediction
must validate all of those semantics rather than accepting equal column counts.
Do not put a particular training image hash into the reusable model schema.

The existing classifier archive and stream/checkpoint versions must migrate
together; old RGB bytes retain their semantics. Additive native request types
or recipe variants are to be chosen at that consumer integration, not exposed
prematurely. Python/NumPy, CLI/batch, managed viewer and ImGui all call the same
native authority. Save/reopen/resume, stale publication, undo, source/model
mutation and physical-calibration rejection remain required integration gates.

## Independent reference and acceptance

`tools/reference_raw_pixel_features.py` uses stdlib Decimal at 80 digits for
direct two-dimensional normalized convolution, independent of native separable
filter traversal. Integer source values and binary64 affine rounding are
explicit; reference features round to binary32 only at the final boundary.
Its committed generated header is consumed by the native feature suite.
Missing support must match exactly. Finite features require
`abs(actual-reference) <= 4 * ulp32(reference) + 1e-12 * max(1,max_abs_normalized)`.
Sigma-zero intensity cases additionally require exact float32 bits. These
tolerances are declared before native verification and are engineering numeric
criteria, not biological/model-equivalence tolerances. Include all ten dtypes,
64-bit extremes, subnormals, both-axis and one-axis smoothing, missing channels,
outside support, derivative seams, cancellation and exact work admission.

A1 numeric/reference preparation and A2 native checks do not complete SCI1-A:
A3-A6 still require training/prediction, codecs/resume, installed frontends and
the full frozen qualification. Biological labels, model comparisons and real
hardware acceptance retain their separate prerequisites.


## Native raw model and captured-grid prediction (QRM1; legacy raw QPR1)

`RawPixelRecipe` captures dtype, series, selected channel indices/names and Z/T,
explicit affine normalization, both rational sampled-grid spacings and feature
operations. `physical_sigmas=true` interprets the requested sigma pair in
micrometres, requires known positive calibration on both axes and resolves it
to per-axis grid units. The kernel limit applies after that conversion; a
100-micrometre request on 100-micrometre sampled pixels is a one-pixel kernel.
A physical model matches exactly equal binary64 sampled physical spacing on
each axis. A grid-unit model matches exactly equal reduced rational spacing.
No biological tolerance or automatic approximate resolution match is implied.

The reusable model has no training image dimensions, ROI/origin, filename or
content hash. Changed dtype, channel labels/order/selection, series or Z/T
reject; inference can use a new image or subwindow with matching sampling
semantics. `RawPixelClassifier` validates the recipe against the existing
`ForestModel` schema and reuses its bounded votes. `classify_raw_pixels` takes
an immutable captured `ScientificRawGrid`; it retains both grid axes, owning
labels, validity, vote fractions, class counts and model/feature/input hashes.
An invalid row remains unclassified. A low-confidence valid row has label zero
and retains its vote fraction. This is not calibrated probability.

New raw models use QRM1, distinct from RGB QPC1/QPC2 and the RGB QPR1 record
wrapper. The original raw QPR1 framing is read only through explicit model
parsing, preserving its bytes/hash. The new model binary header is `Q R M 0x01`; integers and binary64 bits are little-endian uint64. Fields in
order are dtype, series, first channel, channel count, Z, T, sampler work budget;
X numerator/denominator then Y numerator/denominator; physical flag and optional
sampled X/Y micrometre spacing; one uint64-length UTF-8 name per channel; one
(offset, scale) binary64 pair per channel; scale count followed by X/Y sigma
pairs; gradient/laplacian bits 0/1 and feature work budget; forest byte count
and the existing forest bytes. The final 64 ASCII bytes are SHA-256 of the
preceding bytes. Reject unsupported versions, noncanonical/repeated scales,
negative zero/nonfinite parameters, invalid selections, oversized text/arrays,
truncated/trailing data, checksum/schema mismatch and noncanonical re-encoding.
The model is bounded by 64 MiB plus 128 KiB; names total at most 64 KiB.

Loading uses the established regular-file guard and verifies stable source
bytes. Saving uses the existing atomic exporter. New-file mode refuses an
existing destination; replacement first validates an existing QPR model and
never overwrites an unrelated file. Pre-cancellation preserves destinations.
The native feature suite contains a separately authored QPR framing oracle,
exact forest vote cases, physical calibration mismatch/unknown controls,
streamed/full-grid checks and exact allocation admission tests.

These model and captured-grid APIs now compose through the owner-captured
controllers below. The candidate protocol below adds retention and archives;
its local acceptance is recorded separately. Live replay, apply/undo,
checkpoints, Python bindings, batch and desktop actions remain open.
Those remaining A3-A6 integrations must preserve the same raw authority rather
than routing quantitative values through the scalar-downsample RGB result.


## Owner-captured native workflow contract

`RawPixelTrainingRequest` captures sorted labelled area IDs, immutable ROI/class
snapshots, one explicit scientific grid, raw selection, normalization and the
model recipe on the owner. Existing `ObjectFeatureRequest` and
`RawAnalysisRequest` remain the document/source authorities. Row coordinates
are sampled-grid indices; label membership uses each rational axis center
rounded once to binary64 and the native ROI containment rule. Same-class
overlap counts once with the lowest selected annotation ID; different-class
overlap is ambiguous and excluded. All feature columns must be supported.
Deterministic bounded per-class reservoirs reuse the RGB priority/comparison
implementation, then optionally balance classes. Return owning immutable rows,
labels, coordinates, annotation IDs, counts and input/result commitments.

`RawPixelPredictionRequest` captures the model, full explicit grid and optional
area ROI through the raw-analysis authority. Native workers read raw data only,
apply the shared classifier and clear validity/labels outside the captured ROI.
The result retains both axes, model identity and an ROI/calibration-bound input
commitment. A model/result is never published to Document by a worker.

The captured Document calibration governs physical sigma resolution, including
explicit overrides of file metadata. Unknown physical calibration rejects.
The actual source metadata epoch is separately captured and compared after
sampling; overrides cannot conceal source metadata changes. Physical model
matching still requires exactly equal sampled spacing on each axis.

Preflight charges the capture, source metadata/observations, sampler/feature
workspace, reservoir/final rows, prediction copy and vote scratch before pixel
IO. The current implementation admits one bounded explicit grid. It does not
claim a phase-preserving streamed-window implementation for arbitrarily large
sample arrays; that remains required for the large-slide/checkpoint workflow.
Do not approximate fractional grid origins by natural scalar-downsample tiles.

`RawPixelTrainingJob.sample` delivers a table without requiring a forest backend.
`train` checks backend capability before starting IO, then calls the existing
native forest trainer. `RawPixelPredictionJob` uses the same native prediction
kernel. Controllers enforce owner-thread operations; cancellation alone is
thread-safe. Busy reflects a running worker, while `await_completion` joins
without delivering a result. Poll/wait validates owner/source state before
making completed results available; cancellation and edits before delivery
produce no table/model/result. File-backed publication checks fresh primary and
companion versions with the existing file guards, including changes after the
worker has finished. Worker exceptions deliver failure, allow retry and never
modify the document or an earlier owned result. The archive/retention candidate
below now supplies a locally verified owner-stage keep path. Live replay,
undo/apply, streamed resume and all frontend integrations remain
required.


## Retained archives and project record protocol

Implementation source `440f610` has [local acceptance](benchmarks/raw-pixel-archives-local.json)
for both hosts' affected workflow suite, including real-TIFF project/snapshot/
recovery readback. Earlier failures and unchanged regression identities remain
explicit. The API queue defines the remaining integration and platform gates.
Source-only provenance has explicit QPV4 encoding/decoding opt-in; existing
object consumers continue rejecting absent object identity by default.

New raw models use `QRM1`. The pre-release raw `QPR1` reader remains explicit,
strict and byte preserving; loading it retains the old model hash. Constructing
a new model from its forest/recipe emits QRM1. The existing RGB pixel-record
wrapper also uses QPR1 and remains unchanged; its magic alone must never be
used to identify an old raw model. `QXS1` is the bounded canonical recipe frame.

`QXT1` archives capture sampled training tables with an optional actually
matching trained model. Sampling without the optional native training backend
is an explicit archive purpose. Attaching a model requires exact recipe,
classes, row count and native forest training-matrix hash agreement. `QXP1`
archives capture a model, exact scientific grid, ROI, document calibration,
raw source metadata/provenance and prediction arrays. Owner prediction hashes
use `qpx.raw-pixel-region/2`, which can be recomputed from retained fields.
Training hashes retain their existing numeric domain. All frames have bounded
lengths, little-endian scalar/array fields and SHA-256 checksums; invalid float,
count, geometry, class/schema, vote/threshold and semantic-hash data rejects.
Archive bytes are limited to 128 MiB and retained payload to 256 MiB.

Decoded tables/results are historical immutable data. They contain no live
capture token. Document keep methods require the original matching capture
and a fresh owner/source check; a decoded archive cannot manufacture that
authority. Standalone archive writes use existing regular-file guards and
atomic export, refuse accidental overwrite and validate an existing archive
before explicit replacement.

`RawPixelRecord` stores source-bound archives in the native Document catalog.
The existing SQL `pixel_classifier_records` table dispatches RGB QPR1 versus raw
QXR1 wrappers explicitly; SQL schema 13 and older-project migration behavior
remain unchanged. Snapshot saves and recovery snapshots carry the immutable
raw records. New raw payloads fail explicitly in older readers that do not
understand QXR1. Historical reads do not mark results current or verified and
do not open a decoder. Source/model replay verification, owner-stage contour
apply/undo, phase-preserving streams/checkpoints and frontends remain required.
