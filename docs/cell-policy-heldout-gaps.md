# Held-out nominal cell processing gaps

The [private native candidate304](benchmarks/postlog-trace-304/receipt.json) now
reproduces all 13 captured nucleus/body masks, their basin labels, binary masks
and applicable refinement stages. Its 78 stage payloads are byte-identical on
Windows and GCC ASan/UBSan. A separate public oracle supplies 33 small response
cases, with 102 shared payloads repeating exactly. These results use captured
processing input. Public requests still select the existing recipes, so the
original-slide sampling, provenance and consumer integration remains to deliver.

The candidate reuses the existing Gaussian/LoG, region filling, sampled shape
watershed, nucleus labeling and cell expansion. The new response flood preserves
the captured plateau/border seed rules and initial boundary frontier. Filled
basin means determine region admission, and the sampled shape/refinement stages
resolve the former split, sigma-zero and large-sigma differences. Shared helpers
are factored without changing their callers' counters or recipes. The rebuilt
application passes 57 native suites, 896 scientific checks/18 declared skips,
1,099 auditor controls and 19 desktop/managed checks. Public stubs are unchanged.

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

1. Reuse candidate304's verified post-LoG path in one additive original-source
   recipe. Capture the actual request, metric, ROI, source and transform; use the
   existing feature/contour/result publication path. Ordinary requests should
   retain the necessary scratch and results; full intermediate stage copies
   belong to explicitly budgeted diagnostics.
2. Specify and deliver an additive, versioned affine-sampling option in the
   existing nominal request. Preserve the stable version-1 numerical recipe and
   old history. Carry the option through the shared descriptor, archive/replay,
   resume keys and existing Python/CLI/ImGui/managed consumers. Use the 112 exact
   diagnostic comparisons as independent pixel gates, including crop origins.
3. Carry the new sampling and detector identities through the existing
   archive/replay/classifier provenance, CSV, Python/stubs, CLI, batch checkpoints,
   direct/batch ImGui and managed commands. Keep the default quantitative and
   shipped ImageJ numerical-policy contracts intact. Verify the eight named
   gaps using original-slide requests and the same fixed comparisons, including
   feature accuracy, owner/stale/cancel behavior and source-edge support. Then
   freeze one complete combined source for platform qualification.

Broader specimens, stain/features, whole-slide seams/resources, hardware/speed,
supported packages, licensing and release requirements remain open. The existing
three strict scientific expected failures remain unchanged.
