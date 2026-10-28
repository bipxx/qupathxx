# Pinned QuPath RGB resampling characterization

The public `WrappedBufferedImageServer.readRegion` capture extends the earlier
[single processed-cell image](qupath-cell-comparisons.md#requested-resampling-pixels-and-nominal-geometry).
It covers four opaque RGB inputs and nine in-bounds requests per input: identity,
enlargement, dyadic and nondyadic reduction, and offset crops. The three small
inputs are a 17×13 gradient, checkerboard and seeded noise; the fourth reuses the
existing 256×192 synthetic cell input. No biological specimen is introduced.

The [contract](../tests/fixtures/qupath-rgb-resampling-0.7.0/contract.json) and
[manifest](../tests/fixtures/qupath-rgb-resampling-0.7.0/manifest.json) bind all42
payloads, the public-API Groovy generator and the existing hash-verified QuPath
0.7.0/Java25.0.2 runtime. Two complete captures reproduce all43 payload/manifest
files byte-for-byte. Runtime logs remain separate. The optional GPL reference
runtime is neither embedded in the application nor required to verify fixtures.
No upstream interpolation implementation is copied.

## What the captures establish

The comparison tests six mathematical candidates: nominal or realized spacing,
each with nearest-center sampling, float64 bilinear half-up rounding, or float64
bilinear ties-to-even rounding. These candidates characterize the capture;
they do not implement a general Java renderer.

| Captured requests | Observation |
| --- | --- |
| Identity | All four inputs match exactly, including the native RGBA8 area sampler |
| Enlargement at requested downsample0.75 | Nominal nearest-center sampling matches all four inputs; nominal bilinear and realized nearest do not |
| Reductions1.25,1.5,3.75 and offset crop1.5 | Nominal bilinear with half-up rounding matches these16 captures exactly |
| Odd-sized17×13 input reduced by2 | Returned9×7 image has a black bottom row and right column; a clamped bilinear model and native area sampling disagree |
| Reduction2.3, full input and offset crop | Nominal float64 bilinear half-up differs by one byte in some components of all eight captures |

All36 returned shapes match rounding each requested extent/downsample half-up.
That shape rule does not replace nominal spacing with extent/returned-size.
The original cell image at1.5 remains an exact match for its original model;
the additional cases show why that one example cannot justify general equivalence.

Native `resample_area` is compared separately using the same opaque crop and
returned dimensions. It integrates the complete source extent. Its differing
pixels are retained as a different documented sampling recipe, rather than
treated as permission to change quantitative measurements.

The82 new installed checks cover every captured request, exact identity and
matching subsets, enlargement/edge/nondyadic counterexamples, payload mutation,
mixed runtime/generator identities, and false reference metadata. Their exact
pytest identities are registered in the scientific qualification contract;
independent collection checks that the registration includes every real case.
They add no scientific tolerance and do not remove the three legacy detector
expected failures. Broader server types, transparency, pyramid selection,
out-of-bounds requests and biological acceptance remain untested here.

## Easiest production route

Keep the existing area and typed scientific sampling policies. First implement
an explicitly versioned RGB compatibility kernel against this immutable corpus,
including the currently failing boundary and nondyadic cases. Kernel choice,
nominal steps, source bounds and returned dimensions must be separate parts of
its descriptor. Avoid selecting an interpolation mode from output shape alone.

Then expose that native descriptor through thin nanobind bindings and the
existing region-reading CLI. Reuse the current owned pixel buffers, admission,
cancellation and image-server lifetime contracts. If detector or batch workflows
opt into this policy later, retain its version, nominal transform and source
identity in their capture/archive/replay recipes before exposing the option in
the existing UI. Matching captured RGB must not grant replay authority for a
different original source. Default scientific policies stay explicit.

Accept that complete workflow through one source-bound platform qualification
after local kernel and consumer evidence. The reference runtime remains an
optional generator; the C++ application must not invoke Java to obtain pixels.
No general speed, sampler-equivalence or research-accuracy claim follows here.

## Reproduction

```text
python -I tools/qupath_rgb_resampling.py --native --report build/rgb-resampling.json
python -I -m pytest tests/test_qupath_rgb_resampling.py -q
```

For a new Windows capture, pass `--capture build/new-rgb-capture` and
`--runtime-cache build/upstream/qupath`; the output directory must not exist.
The generator authenticates the pinned runtime before invoking its public API.
