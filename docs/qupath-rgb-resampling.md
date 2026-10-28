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

The [local receipt](benchmarks/qupath-rgb-resampling-286/receipt.json) binds
source83cd41df, retained native7231d95, complete scientific admission and
unchanged compiler/product evidence. New-source platform acceptance remains open.

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

## Private native kernel and held-out context

[Kernel287](benchmarks/rgb-nominal-kernel-287/receipt.json) adds63 independent
public reads on new dimensions, nondyadic factors, offset crops and thin/single-
pixel images. Two captures reproduce72 holdout payloads exactly. Native golden
RGBA hashes derive directly from those PNGs and the original36 outputs; their
generator performs no interpolation. The C++ kernel matches all99 under MSVC
and GCC13.3 with ASan/UBSan.

The observed fixed-point recipe uses32.32 center coordinates, truncated eight-
bit interpolation fractions and one final half-up division by65536. Enlargement
uses nearest centers. Centers outside the full source image become opaque black;
bilinear edges inside that image clamp to its border. Integer origins stay
separate from fractional offsets, including the tested3e12-pixel coordinates.

Eight held-out crop cases fail if the source is cut to the requested rectangle
before interpolation. Including neighboring source pixels resolves them exactly.
The private planner therefore returns the needed level-zero source window,
including interpolation context beyond the crop. It admits nominal extents,
output and source-window buffers against the existing256MiB limit, including
the halo. Window plus output can coexist; decoder costs remain additional.
It rejects nonopaque/malformed windows and checks native cancellation throughout.

Kernel287 was private. [SDK289](benchmarks/rgb-nominal-sdk-289/receipt.json)
now supplies the public immutable request, ImageServer adapter, nanobind interface,
region CLI and an independent ImGui preview example. Original-document analysis,
capture/archive/replay opt-in and its application control remain open. Opaque RGB
fixture agreement does not establish general Java rendering, transparency,
vendor/pyramid behavior or biological acceptance.

## Public native, Python and CLI route

The additive contract is `qpx.rgb-nominal/1`; see the
[API review](rgb-nominal-api-review.md) and
[native header](../cpp/include/qupathxx/resampling.hpp). Coordinates and extents
are integer level-zero pixels; downsample is a positive dimensionless nominal
step. The reader must expose a unit level zero. Returned dimensions are rounded
from extent/step; they do not redefine that step.

```python
import qupathxx as q

source = q.dense_image_server(q.Image.read_ppm("input.ppm"))
try:
    request = q.RgbNominalRequest(0, 0, 17, 13, 1.5)
    pixels = q.read_rgb_nominal_region(source, request)
finally:
    source.close()
rgba = pixels.numpy()  # owned 9-by-11-by-4 output remains valid
assert request.algorithm == "qpx.rgb-nominal/1"
```

```text
qupathxx_region --rgb-nominal input.ppm 0 0 17 13 1.5 output.pam
python examples/rgb_nominal_preview.py input.ppm --width-px 17 --height-px 13 --downsample 1.5
```

The constructor validates dimensions, finite positive step, overflow and budgets
before pixel IO. The native read plans and validates one bounded level-zero window
including its interpolation halo; a wholly outside request produces opaque black
without decoder IO. Closed readers, malformed/nonopaque windows and decoder errors
produce no partial result. Native cancellation is checked around synchronous IO
and during sampling; it cannot interrupt the decoder itself. Python releases the
GIL for the read and returns owned storage. The CLI validates requests before
opening the source, protects the source path and writes PAM after a full read.

The example imports native RGB pixels into a new document and uses the existing
ImGui viewer. Known X/Y calibration is multiplied by nominal downsample separately;
unknown calibration stays unknown. This avoids substituting the realized output
dimension ratio. The original reader is closed before showing the preview. The
new source has no original-source capture/replay authority.

## Remaining production integration

Keep existing area and typed scientific sampling defaults. If detector or batch
workflows opt into nominal RGB, retain the algorithm version, nominal transform,
original source identity and halo policy in their capture/archive/replay recipes
before exposing that option in existing application controls. Reuse this native
adapter and current job/publication consumers; do not filter pixels in Python or
start a second viewer/controller implementation.

Accept the integrated workflow through one source-bound platform qualification
after local consumer and persistence evidence. The reference runtime remains an
optional generator; the C++ application does not invoke Java to obtain pixels.
No general speed, sampler-equivalence or research-accuracy claim follows here.

## Reproduction

```text
python -I tools/qupath_rgb_resampling.py --native --report build/rgb-resampling.json
python -I -m pytest tests/test_qupath_rgb_resampling.py -q
```

For a new Windows capture, pass `--capture build/new-rgb-capture` and
`--runtime-cache build/upstream/qupath`; the output directory must not exist.
The generator authenticates the pinned runtime before invoking its public API.
