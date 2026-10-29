# Held-out nominal cell processing gaps

The [303 receipt](benchmarks/cell-policy-edges-303/receipt.json) characterizes 13
new public QuPath 0.7.0 workflows on the retained Windows policy302 package.
Two upstream executions repeat all 146 shared input, numeric, geometry and recipe
payloads exactly. Five workflows match both processing masks exactly; eight
remain unresolved. The 32 new engineering checks include those explicit limits,
alpha refusal, admission and negative-source support. Passing those checks does
not accept the eight segmentation differences.

| Cases | Captured RGB versus current native sampling | Full nucleus/body masks |
| --- | --- | --- |
| Slide edges at steps 1, 1.25, 1.5 | Exact | Exact, five cells |
| Odd crop at steps 1, 1.25 | Exact | Exact |
| Slide edges at step 2.3 | Exact | Reference five cells; native six |
| Odd crop at step 2.3 | Exact | Reference four cells; native five |
| Odd crop / hole at step 1.5 | Each has 82 differing components, one byte each | Reference five/four cells; native four/three |
| Sigma zero | Exact | Reference zero cells; native two |
| Sigmas 8.5, 8.500001, 16 | Exact | Two cells each; boundaries differ |

The retained private Gaussian object produces every captured LoG array exactly
for all 13 inputs, including sigma zero and the large-sigma route. A small driver
links that authenticated object without rebuilding the application. Reusing
captured processing RGB in a separate diagnostic document, with synthetic
calibration preserving the original distance/area metric, retains all eight
mask gaps. That diagnostic document carries no original-slide replay authority.
Fixing RGB sampling alone therefore does not resolve these detector differences.

The public server applies scale and translation before drawing its source tile.
See the pinned [RGB read implementation](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/images/servers/AbstractTileableImageServer.java).
For the odd crop, the public Java inverse yields first X center
`7.749999999999999`. The current sampler's integer-origin decomposition instead
keeps `7.75`. This changes an eight-bit interpolation weight. A diagnostic using
captured inverse centers, 32.32 starts/strides and one final rounding matches all
13 new captures and all 99 earlier reads. It is a candidate model, not a shipped
sampler change or a generalized compatibility claim.

The upstream detector also performs boundary refinement when its processing
sigma exceeds 1.5. This accounts for the additional named debug stage in the
large-sigma captures; causation of every final boundary difference remains to
be isolated. See the pinned [detector source](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core-processing/src/main/java/qupath/imagej/detect/cells/WatershedCellDetection.java).

The shortest implementation order is:

1. Compare captured watershed, binary, refined and final label stages with the
   existing shared native post-LoG path. Start with sigma zero and the two
   exact-input step-2.3 split cases. Keep counts, masks and feature tolerances
   fixed; do not write another Gaussian or transform implementation.
2. Specify and deliver an additive, versioned affine-sampling option in the
   existing nominal request. Preserve the stable version-1 numerical recipe and
   old history. Carry the option through the shared descriptor, archive/replay,
   resume keys and existing Python/CLI/ImGui/managed consumers. Use the 112 exact
   diagnostic comparisons as independent pixel gates, including crop origins.
3. Deliver any required QuPath post-LoG/refinement behavior with an explicit
   algorithm identity. Keep the default quantitative and shipped ImageJ
   numerical-policy contracts intact. Close the eight named mask gaps through
   the same native detector and all consumers before freezing one combined
   own-source qualification.

Broader specimens, stain/features, whole-slide seams/resources, hardware/speed,
supported packages, licensing and release requirements remain open. The existing
three strict scientific expected failures remain unchanged.
