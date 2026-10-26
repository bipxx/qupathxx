# Quantitative pixel classifier numeric contract

Status: native feature implementation in progress; no public Python/UI or
archive support is claimed. This completes the initial SCI1-A1 consumer trace
and defines the oracle before native execution. RGB `pixel-v1` stays unchanged.

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
