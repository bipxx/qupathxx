# Pinned QuPath cell-workflow characterization

The independent corpus calls QuPath v0.7.0 `WatershedCellDetection` through its
public scripting API. It captures actual plugin outputs rather than comparing
the native detector with itself. The reference runtime is revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`; its archive, all 496 runtime files,
and license identity are pinned by the existing [runtime contract](qupath-comparisons.md).
The QuPath runtime is downloaded into ignored build storage and is not distributed
with qupathxx. The Groovy generator calls APIs; it does not copy the Java kernel.

## Inputs, parameters and identities

The [contract](../tests/fixtures/qupath-cells-0.7.0/contract.json) specifies nine
256 by 192 RGB phantoms, fixed stain vectors, white values, pixel calibration,
detector parameters, tile sizes, assignment thresholds and numerical tolerances.
The phantoms cover blank input, isolated nuclei, touching and separated pairs,
size filters, an ROI clipped through nuclei with a hole, gradient/noise,
anisotropic calibration and a requested resampling grid.

For calibrated QuPath input, pixel-space sigma, expansion and size limits are
converted with the averaged pixel size and its square. That mapping follows
the pinned plugin's parameter units; it does not make the algorithms equivalent.
The original legacy comparison uses sampled-pixel parameters on natural image
grids. Its resampling case requests 0.75 micrometers on a 0.5 micrometer source
and retains that recipe's grid mismatch. The original-source nominal recipe has
a separate comparison below; these historical results retain their identities.

Each capture retains the PNG, cell/nucleus GeoJSON, all plugin measurements,
independent pixel-center RGB/OD/stain means and original/decoded label maps.
Labels are uint32 little-endian, row-major, gzip compressed. Deterministic cell
names and UUIDs follow centroid order. Original and QuPath-decoded geometry are
sampled separately so interchange rounding can be distinguished from sampling.
Overlapping raster memberships are counted, not silently accepted. All captured
overlap counts are zero. Two live captures produced identical bytes for all 64
manifested files (236,941 bytes); their timestamps and console logs are excluded
from that equality assertion. The [repeat receipt](benchmarks/qupath-cells-repeat.json)
records both manifest identities.

The capture manifest binds the input contract, phantom generator, Groovy
generator and every payload to SHA-256 identities. Native comparison reports
also bind the comparison tool, source fingerprint and installed native artifact.
Fixture verification precedes comparison, and changed payloads are rejected.
Regeneration always requires a new directory; fixtures are never updated implicitly.

## Measurement and segmentation evidence

The [Windows report](benchmarks/qupath-cells-windows.json) and
[cross-platform receipt](benchmarks/qupath-cells-validation-windows-linux.json)
retain the actual results. Nucleus foreground IoU below compares unions of labels;
individual object matches and cell-body metrics remain in the full report.

The [hosted artifact audit](benchmarks/qupath-cells-ci-windows-linux.json) accepts
the characterization at commit `7a4bc4ea7552582007e569cf5e89cac7d4ac3111`, run
39989634700. All five jobs pass. Windows desktop passes 1,995 Python/41 native,
Windows headless 1,617 Python/40 native, Linux headless 1,615 Python/40 native,
and Linux minimal/ASan/UBSan each pass 40 native tests. Each Python environment
passes 19 characterization cases with exactly four strict scientific expected
failures. The inherited reader/runtime/typing/standalone artifact checks pass,
and the completed desktop overview is visually inspected. These checks accept
the defined comparison evidence, while segmentation and biological gates remain open.

| Case | QuPath cells | Native cells | Nucleus foreground IoU |
| --- | ---: | ---: | ---: |
| Blank | 0 | 0 | 1.00000 |
| Isolated three | 3 | 2 | 0.42041 |
| Touching pair | 2 | 2 | 0.83929 |
| Gap pair | 2 | 2 | 0.80612 |
| Size filters | 1 | 1 | 0.65000 |
| ROI cut and hole | 3 | 3 | 0.78296 |
| Gradient/noise | 4 | 4 | 0.78218 |
| Anisotropic level zero | 3 | 2 | 0.42041 |
| Resampled reference | 3 | 2 | 0.41200 |

Object matching maximizes total IoU with a one-to-one assignment, then retains
matches with IoU at least 0.1. Reports retain unmatched objects, matched area and
centroid differences, RGB/stain feature differences and foreground IoU/Dice.
Split/merge relations use at least two intersecting pixels and an overlap of at
least 10% of the smaller object's area. Metric tests include empty, permuted,
disjoint, split and merged labels. These thresholds characterize relations;
they do not constitute scientific accuracy acceptance.

Across all nine cases, native label maps and compartment features are identical
at read tile sizes 16, 64 and 256. Shared-geometry sampling and OD/stain means pass
the unchanged count and 1e-6 absolute/relative tolerances for all 36 compartments
in the eight natural-grid cases. This checks the measurement kernel independently
of detector geometry. Plugin-specific statistics are retained as reference data;
they are not all implemented or accepted as matching native features.

The isolated-cell count, minimum matched nucleus IoU (0.85) and centroid (one pixel)
gates fail. Native normalized smoothing and threshold segmentation produce a larger
largest nucleus. Disabling only the maximum-size filter in a separate diagnostic
produces three nuclei with 324, 208 and 540 pixels; the declared maximum is 512.
The captured acceptance contract was not changed to retain that nucleus.

The six resampled compartments also fail exact shared-ROI membership and mean
gates. Native includes 12, 10 and 14 additional nucleus centers and 16, 16 and 30
additional cell-body centers. Every difference lies on a polygon boundary; there
are no reference-only differences. Original and decoded QuPath maps are identical
in this corpus, ruling out export quantization as the cause. The native closed
polygon boundary predicate and QuPath membership disagree on these edge centers.
Changing the global boundary policy would affect existing geometry and measurement
contracts and is not silently performed by this comparison tool.

Three isolated-cell segmentation gates remain strict expected failures in pytest.
The resampled shared-ROI gate now explicitly selects
[`POLYGON_WINDING`](roi-sampling.md). Across all nine cases, its 42 compartment
masks and counts match exactly, and OD/stain means pass the unchanged 1e-6
absolute/relative tolerances at all three tile sizes. The comparison retains
`shared_roi_regions` for legacy coverage and adds `shared_roi_regions_winding`;
the six legacy resampled discrepancies above remain unchanged. Global geometry
membership, picking and the default analysis policy retain their existing rules.

The report exposes `explicit_winding_sampling_accepted` and the compartment
count separately. This accepts the explicit sampling mode on the pinned corpus;
cell detection itself is unchanged. The tool continues to report
`characterized-partial` and `segmentation_parity_accepted: false`. Exit zero means
the characterization ran, not that segmentation, biological accuracy or M5
acceptance passed. The corrected [Windows full validation](benchmarks/roi-sampling-windows-local.json)
and [five-job hosted audit](benchmarks/roi-sampling-ci-windows-linux.json) accept
the explicit sampling engineering scope. The [Windows comparison](benchmarks/qupath-cells-winding-windows.json)
retains every case and both sampling policies.

## Reproduction and limits

### Original-source nominal production recipe

`tools/compare_qupath_nominal_cells.py` runs the existing nominal cell request on
each original image in the immutable nine-case capture. The default processing
rectangle is the shared ROI factory's integer enclosure. `--grid-scope full-image`
uses an explicit full-image nominal descriptor while retaining the target ROI.
Both routes use the same native detector. Threshold, sigma, expansion and area
limits come from the capture; the typed LoG recipe keeps its existing zero
peak-radius/spacing defaults. The legacy distance recipe and its strict expected
failures remain unchanged.

The [two-host receipt](benchmarks/nominal-cell-comparison-296/receipt.json) records
35 new installed checks on Windows and Linux. Eight cases have exact level-zero
nucleus/body raster masks, counts, one-to-one matches, pixel centroids and
split/merge relations. They include the original-source downsample1.5 case.
Every body mask matches across all nine cases. Cross-host mask hashes, metrics,
processing arithmetic decisions and boundary diagnostics agree exactly.

| Scope | Nucleus foreground IoU | Body foreground IoU |
| --- | ---: | ---: |
| Seven natural-grid cases without a clipped ROI | 1 | 1 |
| Resampled reference at nominal step1.5 | 1 | 1 |
| Clipped ROI, context0 | 0.99481865 | 1 |
| Clipped ROI, context32 | 0.96984925 | 1 |

The ROI differences retain the earlier four reference-only pixels at context0
and24 native-only pixels at context32, with exact coordinates in the reports.
Matching the processing extent does not change the normalized-Gaussian recipe
into the plugin's arithmetic. The report remains `characterized-partial` with
`segmentation_parity_accepted: false`; no epsilon, cropping rule or tolerance
change is introduced to force agreement.

Nominal features use processing-grid pixels. At step1, their RGB/OD means also
match the independent level-zero region means under the original1e-6 tolerances.
At step1.5, the six level-zero region-mean comparisons differ. Independent
captured processing RGB, label counts, RGB/OD means and calibrated sample areas
pass the same tolerances. The reports retain both domains and every difference.
Descriptor/count/shape/context checks precede exact rational projection; hidden
context labels and uncovered level-zero centers reject.

Pixel-unit comparisons include the anisotropic source. Isotropic micrometre
requests reproduce the pixel recipes. The physical comparison explicitly omits
the anisotropic conversion because the plugin's averaged calibration and the
native per-axis metric define different requests; this does not close M5's
anisotropic physical requirements. Request-recipe sidecars record requested and
resolved parameters, ROI/hole, source calibration and sampling per case.

The Windows runtime remains native bb190eb7; Linux uses an authenticated CI295
wheel at a7eb78de. All424 compiler inputs and the retained Windows product/package
bytes are unchanged. No native rebuild follows this harness-only change.
Comparison observer0c874848 has its own source proof and does not inherit
frozen295's complete qualification. The corpus is synthetic; broader biological,
vendor, scale, nondyadic and resource/hardware requirements remain open.

```text
python -I tools/compare_qupath_nominal_cells.py --context 0 --report build/nominal-roi-context0.json
python -I tools/compare_qupath_nominal_cells.py --context 32 --units micrometres --report build/nominal-physical.json
python -I -m pytest tests/test_qupath_nominal_cells_reference.py -q
```

### Explicit-grid production recipe

`tools/compare_qupath_grid_cells.py` compares the existing explicit-grid RGB
detector with the same immutable QuPath capture. It uses a full-image unit
grid, captured threshold/sigma/expansion/area limits, and the grid recipe's
existing zero peak-radius/spacing defaults. It never substitutes the legacy
distance-watershed recipe or changes its three strict expected failures.
Those failures describe that older recipe; existing LoG tests already pass the
isolated-cell thresholds. They are not evidence that every detector mode fails.

The new installed regression requires exact nucleus/body masks, counts,
centroids and split/merge relations on the seven natural-grid cases without a
clipped ROI. Matched RGB/OD features use the original 1e-6 absolute/relative
tolerances. Independent source-pixel arithmetic checks the actual native
feature counts and means on all nine cases, including differing masks. Context
0 and 32 must yield identical core masks and features, while their captured
input identities remain distinct. Invalid grid projections and labels outside
the core reject, so cropping cannot hide output differences.

Isotropic micrometre parameters are derived from the captured calibration and
must reproduce the pixel recipe. The anisotropic case is explicitly excluded
from that conversion comparison: QuPath's averaged calibration and the native
per-axis physical metric describe different recipes. Its pixel-unit comparison
remains included. This does not exclude anisotropic physical work from M5.

The clipped ROI retains a nucleus-mask difference; no fitted tolerance is added
to call it equivalent. The requested 0.75-micrometre case retains its existing
grid mismatch and both compartment comparisons. The report states
`characterized-partial`, `segmentation_parity_accepted: false` and
`biological_validation: false`. Matching seven synthetic cases does not establish
general segmentation or biological acceptance. Cross-platform qualification of
this new comparison harness is still required; source169 qualification predates it.
The [local source-bound receipt](benchmarks/grid-cell-qupath-comparison-local-windows.json)
retains both complete reports, installed tests, source proof and ROI diagnostic.
The latter identifies 24 native-only nucleus pixels at x=32/33, y=69..80,
immediately inside the ROI's left edge, and no reference-only pixels. A plugin
stage/processing-boundary comparison is needed to determine the cause. These
coordinates are a diagnosis, not a clipping rule or an adjusted acceptance limit.

```powershell
python -I tools/compare_qupath_grid_cells.py --units pixels --report build/grid-cells-pixels.json
python -I tools/compare_qupath_grid_cells.py --units micrometres --report build/grid-cells-um.json
python -I -m pytest tests/test_qupath_grid_cells_reference.py -q
```

### Clipped processing domain and public debug stages

The [ROI debug fixture](../tests/fixtures/qupath-cells-roi-debug-0.7.0/manifest.json)
captures the pinned plugin's public five-slice debug stack for `roi_cut_hole`.
Its 192×128 extent equals the ROI bounding box, rather than the 256×192 full
image used by the initial native comparison. Debug mode reproduces all ordinary
nucleus/body and decoded label arrays, reference measurements and source input.
Its final debug labels independently reproduce all 772 ordinary nucleus pixels.
Two live captures have identical bytes for all twelve non-log payloads; the
timestamped runtime logs remain separate. The manifest binds the original
capture, pinned runtime, selected recipe and both public-API scripts. The only
ordinary-generator change enables its debug flag. No upstream kernel is copied.

The grid comparison now accepts `--grid-scope reference-roi --context 0` to use
that processing rectangle explicitly. Projection verifies the declared origin,
extent, unit spacing, dimensions and empty context labels before constructing
the full comparison canvas. This is a comparison choice, not a change to the
production detector or its default context. On this crop, the nucleus difference
changes from 24 native-only pixels to four reference-only pixels; bodies remain
exact. Padding this ROI by 32 samples reproduces the original full-image masks.

The four missing pixels are (32,68), (33,68), (32,81), (33,81). The captured
QuPath LoG responses there are approximately 2.74e-6 and 1.17e-5, both positive.
A standalone GCC probe of the unchanged native C++ kernel, fed float64 OD values
exported by the installed native sampler, gives zero or -2.22e-16 and a false
positive-mask bit at those points. This probe is scoped numerical evidence,
not a new platform qualification. The grid recipe's validity-normalized
float64 Gaussian and zero-flux Laplacian are distinct from the plugin's captured
processing. No epsilon, column trimming or new acceptance tolerance is introduced
to force agreement. Broader arithmetic equivalence remains unproved.
The [Windows/Linux stage and domain receipt](benchmarks/grid-cell-roi-debug-local-windows-linux.json)
retains 275 passing checks per platform, two declared desktop skips per platform,
both domain reports, the repeated debug payload, native numerical probe and
source/artifact identities. The new 32 comparison and 14 capture checks all run
without skips. The Linux native bytes match the qualified source169 wheel;
the application compiler inputs are unchanged. This local extension of the
comparison evidence does not transfer source169's full hosted qualification to
the newer harness or establish biological accuracy.

```powershell
python -I tools/compare_qupath_grid_cells.py --grid-scope reference-roi --context 0 --report build/grid-cells-cropped.json
python -I tools/qupath_roi_cell_debug.py --report build/roi-debug-verification.json
# Windows: new directory required; verifies the existing pinned runtime cache
python -I tools/qupath_roi_cell_debug.py --capture build/new-roi-debug --runtime-cache build/upstream/qupath --report build/new-roi-debug-verification.json
```

### Gaussian stage characterization

The [37-case ImageJ capture](../tests/fixtures/qupath-gaussian-1.54p/manifest.json)
isolates the remaining smoothing difference without changing a native recipe.
It uses the pinned QuPath runtime's public ImageJ1.54p FloatProcessor operations.
Inputs include the original clipped-ROI float32 OD stage, constant/ramp/impulse
images, single rows/columns and sigma values on both sides of8.5. Two original
executions reproduce150 payloads exactly; the final generator's numeric/input
payloads match those executions and additionally record runtime versions.

An independent neighbor-difference Laplacian reconstructs every captured LoG
array byte-for-byte from its captured blur. The ROI result also exactly matches
the earlier plugin debug stack. This localizes the observed difference to the
blur/input stage rather than requiring a new Laplacian threshold.

At sigma1 the captured requested-sigma kernel has nonzero weights through five
pixels, while the native physical Gaussian stops at three. The native Gaussian
also normalizes clipped valid support, whereas ImageJ documents nearest-edge
extension. Its larger-sigma route can downsample before convolution and upsample
afterward; the separately exported requested-sigma kernel is diagnostic and
does not describe that entire route. See the pinned
[ImageJ implementation](https://github.com/imagej/ImageJ/blob/v1.54p/ij/plugin/filter/GaussianBlur.java).

Float64 diagnostic models with support extended from three to five make all
four disputed ROI responses positive. They still differ elsewhere and do not
reproduce ImageJ's complete arithmetic. An unchanged native-header GCC probe
fed the captured float32 OD promoted to float64 keeps the four nonpositive
responses. That input is distinct from production float64 OD, and the probe is
numerical characterization rather than platform qualification.

The next compatibility step is an explicitly versioned, opt-in Gaussian/input
policy with its arithmetic, support, edges and large-sigma behavior specified
together. Validate the stage arrays before detector masks, then carry the policy
through the existing request/archive/replay and consumers. Preserve existing
quantitative/default recipes, strict xfails and scientific tolerances. These
captures establish neither full cell equivalence nor biological accuracy.

```text
python -I tools/qupath_gaussian.py --report build/gaussian-verification.json
python -I -m pytest tests/test_qupath_gaussian.py -q
```

A new Windows capture uses `--capture build/new-gaussian-capture` and the existing
`--runtime-cache build/upstream/qupath`; the output directory must not exist.

### Requested resampling: pixels and nominal geometry

The [resampled processing fixture](../tests/fixtures/qupath-cells-resampled-debug-0.7.0/manifest.json)
adds a public `ImageServer.readRegion` request to the existing public debug
capture. At requested downsample 1.5, QuPath returns a 171x128 RGB image for the
256x192 source. Its optical densities reproduce the debug input at the unchanged
1e-6 tolerance. The ordinary reference values, source and decoded label arrays,
and exported geometry remain identical to the original capture. A second live
capture reproduces all fourteen non-log payloads byte-for-byte.

The output geometry uses the nominal step **1.5**, not the realized width ratio
256/171. Inverting that nominal transform puts every exported vertex on the
integer processing lattice. Independent rectilinear polygon-center rasterization
reconstructs 379 nucleus and 825 body processing pixels. Exact rational inverse
mapping to level-zero centers reproduces both ordinary label canvases. The wrong
realized ratio can happen to reproduce the same coarse nucleus raster here;
the geometry-lattice and input-pixel controls reject it independently.

A diagnostic center-bilinear model using nominal spacing and half-up byte
rounding reproduces this captured RGB array exactly. Realized-width spacing
changes 654 channel components; ties-to-even rounding changes 32. This is a
model characterization on one synthetic input, not a general Java resize or
native compatibility implementation. No production sampler changes are made.
The native float64 area sampler at the same 171x128 shape differs in 951 channel
components, by up to 48.1328125. Shape agreement does not imply input equivalence.

Running the existing explicit-grid detector on the captured processing RGB,
with the original distances/areas converted to processing pixels and 0.75-um
calibration, reproduces all three nucleus/body masks, counts, centroids and
split/merge relations exactly. Independent captured-geometry RGB/OD means pass
the original tolerances; sample counts and calibrated sample areas agree.
This document and result belong to the captured processed image. They do not
acquire request/replay authority for the original source image. The tool retains
`sampler_equivalence_accepted: false`, `segmentation_parity_accepted: false` and
`biological_validation: false` even when this detector comparison is exact.
The [Windows/Linux resampled processing receipt](benchmarks/grid-cell-resampled-processing-local-windows-linux.json)
records 305 passing checks and two declared desktop skips on each platform,
including all 26 new resampled checks. Both report the same exact masks/features
and distinct original-source sampler inputs. Its 462-file proof and retained
native hashes bind this local scope; no production native rebuild or new full
platform campaign was required.

```powershell
python -I tools/qupath_resampled_cell_debug.py --compare-native --report build/resampled-processing.json
python -I -m pytest tests/test_qupath_resampled_cell_debug.py -q
# Windows: verified runtime, new capture directory
python -I tools/qupath_resampled_cell_debug.py --capture build/new-resampled-debug --runtime-cache build/upstream/qupath --compare-native --report build/new-resampled-processing.json
```

The [expanded RGB resampling corpus](qupath-rgb-resampling.md) adds36 public
reads across four synthetic inputs. It preserves the original1.5 match while
showing nearest-center enlargement, black odd-sized reduction edges and remaining
nondyadic one-byte discrepancies. A single matching cell image cannot establish
a general compatibility sampler. Native quantitative sampling is unchanged.

These comparisons now identify the processing-domain, pixel recipe and geometry
differences explicitly. Preserve them and the legacy recipe while extending
typed quantitative and vendor/pyramid/tiled scientific workflows. Broader
scientific and biological acceptance remain separate requirements.

From an installed package environment with the CI scientific dependencies:

```powershell
python -I tools/compare_qupath_cells.py --report build/cell-comparison.json
python -I -m pytest tests/test_qupath_cells_reference.py -q
# Windows only: hash-verified official runtime, new output directory required
python -I tools/compare_qupath_cells.py --capture build/new-qupath-cells --report build/new-cell-comparison.json
```

This is a small synthetic comparison. It does not establish biological performance,
whole-slide cell throughput, equal algorithm semantics, unsupported-grid behavior,
complete feature parity or hardware performance acceptance. Reported native timings
include the Python call and detection only, exclude runtime startup/reference
execution, and are observations rather than cross-language speed claims. Broader
slides, physical filters, grid matching and scientific tolerances remain M5 work.
