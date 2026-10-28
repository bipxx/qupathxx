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
region CLI and an independent ImGui preview example. [Native cells291](benchmarks/nominal-cells-291/receipt.json)
now adds original-document nominal cell capture, detection, historical storage and
fresh replay. Its thin Python/CLI/batch/ImGui/managed consumers remain open. Opaque RGB
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

Keep existing area and typed scientific sampling defaults. The native cell recipe
below retains the algorithm version, nominal transform, original source identity
and halo policy in QCL6/QCA6. Carry that recipe through the remaining CLI, batch
and application consumers. Reuse the native adapter and current job/publication
consumers; do not filter pixels in Python or start a second viewer/controller.

Accept the integrated workflow through one source-bound platform qualification
after local consumer and persistence evidence. The reference runtime remains an
optional generator; the C++ application does not invoke Java to obtain pixels.
No general speed, sampler-equivalence or research-accuracy claim follows here.

### Native original-document cell recipe

The C++ contract now accepts `NominalCellSampling` without creating a derived
document or virtual pyramid level:

```cpp
qpx::GridCellParameters parameters;
parameters.threshold = .4;
parameters.sigma = 1; // level-zero pixels unless units selects micrometres
qpx::NominalCellSampling sampling(qpx::RgbNominalRequest(0, 0, 256, 192, 1.5), 0, 0);
qpx::CellRequest request(document, object_id, transform, sampling, parameters);
auto result = qpx::detect_cells(request);
auto record_id = document.keep_cells(request, result);
auto replay = qpx::CellRequest::replay(document, record_id);
bool verified = document.verify_cells(record_id, replay, *qpx::detect_cells(replay));
```

This is native C++ delivery. Python bindings for the new cell descriptor and its
constructor, CLI/batch options and application controls are the next consumers.
The earlier Python RGB read/preview API remains available independently.

The cell recipe identity is `qpx.nominal-rgb-log-watershed-bodies/1`; its input
identity is `qpx.nominal-cell-rgb/1`. It captures the original document's owner,
ROI, transform, source and calibration. The descriptor retains integer source
origin/extents, nominal binary64 step and nonnegative per-axis output context.
Contexts default to32 samples per axis and are capped at4096. Read admission
includes the interpolation halo and simultaneous detector buffers before IO.
Every world center must remain strictly between its representable edges.

Context feeds the existing float64 physical LoG/watershed and expansion kernels;
published labels belong only to supported nominal core centers inside the ROI.
Nominal support means a world center inside the original image, distinct from
the full-area support rule of scientific area sampling. Unknown calibration
stays unknown; physical parameters require both axes. Sample areas and centroids
use the nominal step and original per-axis calibration.

QCL6 stores nominal geometry, original provenance, masks, features and historical
domain. Source-free decoding validates the stored recipe and regenerates derived
geometry. Project reopen restores an unverified historical record; fresh replay
reads the original source and verification compares the retained recipe/result.
QCA6 preserves nominal cell classifier origins alongside legacy and area-grid
origin types. Existing QCL/QCA1-5 formats remain supported. Nominal sampling does
not change source level zero: provenance attachment checks unit source scale and
requires exact agreement with embedded provenance.

### Original-document cell recipe implementation order

Native291 locally delivers steps2-4 below on the defined corpus, following the
context290 primitive in step1. Step5 is the next delivery work; step6 remains a
broader validation boundary. The receipt retains both failed development runs
and the source-specific repaired evidence.

The current scientific grid stores integer extent and sample count; its step is
their exact rational ratio. A nominal256-pixel request at1.5 has171 output samples
and a256.5-pixel lattice span. Replacing1.5 with256/171 would change geometry and
calibration. Keep nominal RGB as a distinct captured recipe; do not add a hidden
sampling mode to existing scientific area grids or reinterpret QCL5 bytes.

1. **Lattice-preserving input windows.** [Private context290](benchmarks/rgb-nominal-context-290/receipt.json) adds
   `plan_rgb_nominal_window`: output indices refer to the original request,
   including negative context. Each tile retains its original fixed-point phase
   and reads only its required source halo. The99 captured outputs stitch exactly
   at1/7/32-pixel tile sizes; core pixels are unchanged with added context. Six
   exact-factor source translations independently check negative context. These
   checks prepare input delivery; they do not implement a cell recipe.
2. **Capture on the original document.** Add an explicit nominal request overload
   to `CellRequest`, carrying the immutable RGB request, per-axis output context
   and existing `GridCellParameters`. Reuse `AnalysisRequest`'s owner/source/ROI
   snapshot and calibration revision. Validate representable world centers/edges,
   whole live detector/input/halo memory, units and ROI domain before IO. Nominal
   output support and ROI membership must be defined separately from area-sample
   support. The interpolation halo and detector context are separate quantities.
3. **Share detection and result construction.** Reuse `grid_log_watershed`,
   `PhysicalGridMetric`, expansion, compensated feature accumulators, contours and
   the existing `CellJob`. Extract their common orchestration from
   `ScientificCellKernel` where required. A private nominal geometry/input adapter
   supplies original-coordinate edges/centers and RGB bytes converted exactly to
   float64. Preserve current scientific arithmetic and legacy recipes. No derived
   document, virtual pyramid level or second job/controller grants original-source
   authority. Store explicit nominal geometry on results and expose its descriptor.
4. **Retain and replay the complete recipe.** Introduce an additive QCL version
   for nominal results; keep QCL1-5 encodings unchanged. Bind algorithm identity,
   original source/ROI/calibration, integer origin/extents, binary64 nominal step,
   context, resolved units and sampled-input digest. Decode historical labels and
   verify their domain/geometry without opening the source. Fresh replay uses the
   same descriptor and current original source. Update request equality, cell
   status, `input_sha256`, verifier and classifier-origin encoding together so a
   dropped sampling field cannot make distinct recipes equivalent. Existing
   length-delimited project/recovery blobs should be reused unless inspection
   proves an outer schema change necessary.
5. **Finish all consumers.** Bind the same native request and readonly result
   descriptor; update generated stubs and strict typing. Adapt direct/batch CLI,
   batch checkpoint identity, existing direct/batch ImGui controls and managed
   viewer commands. Save/reopen/replay/export must retain the descriptor. A
   per-control resampler or a Python-owned preview is insufficient.
6. **Decisive checks.** On the pinned resampled-cell fixture, read processing pixels
   from the original native reader and compare RGB, OD, masks, geometry and
   features at the original tolerances. Require original-document keep/save/reopen,
   historical inspection and fresh verification. Reject changed source halo,
   geometry/calibration, descriptor bits, malformed/checksum-valid archives,
   stale publication and late cancellation. Check offset/context/nondyadic and
   anisotropic/unknown-calibration cases. Preserve existing three strict legacy
   xfails. Only after complete local consumer evidence freeze one full own-source
   platform campaign; biological/general-speed/release gates remain separate.

## Reproduction

```text
python -I tools/qupath_rgb_resampling.py --native --report build/rgb-resampling.json
python -I -m pytest tests/test_qupath_rgb_resampling.py -q
```

For a new Windows capture, pass `--capture build/new-rgb-capture` and
`--runtime-cache build/upstream/qupath`; the output directory must not exist.
The generator authenticates the pinned runtime before invoking its public API.
