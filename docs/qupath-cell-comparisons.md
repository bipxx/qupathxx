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
Native detection currently uses sampled-pixel parameters on natural image grids.
The resampling case requests 0.75 micrometers on a 0.5 micrometer source. It is
explicitly marked as a grid mismatch; native arbitrary resampling remains open.

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
