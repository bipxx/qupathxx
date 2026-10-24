# Real-tissue cell comparison

The immutable QuPath 0.7.0 capture in
`tests/fixtures/qupath-ndpi-cells-0.7.0/capture` extends the synthetic corpus
with actual tissue from the CC0 CMU-1.ndpi slide. It is a software comparison,
with no pathologist ground truth or accepted biological accuracy claim.

The full slide is 198,030,965 bytes, SHA-256
`edf4a1ccf395c7000ae93ad3b44c07d97043810e00be0c1d167dd09bbe436e46`.
The level-zero rectangle is `(11264, 17664, 256, 256)`. Its native anisotropic
pixel sizes are 0.45641259698767683 and 0.4550625711035267 micrometers.
The contract records the source URL, CC0 provenance, stain vectors, white,
native parameters and physical conversions for the upstream plugin. The
capture contains source RGB, global GeoJSON, independently rasterized nucleus
and body labels, plugin measurements, runtime log and a byte manifest.
The large slide and QuPath runtime remain external downloads.

`tools/upstream/qupath_ndpi_cells.groovy` invokes public QuPath APIs using the
pinned official runtime and its OpenSlide server. It does not copy the detector
implementation. `tools/compare_qupath_ndpi_cells.py` verifies the full slide,
capture bytes, generator, upstream identity, source calibration and exact RGB
agreement before running native detection. Label decoding is limited to the
contracted canvas; count ledgers must agree. Cropping allocates only the ROI,
despite its large whole-slide coordinates.

## Current observations

At native source `2afdcb99555bf142211cdfa836a4671a6f54bbf1`, input RGB matches
QuPath exactly. Both native LoG recipes detect 299 cells versus QuPath's 506.
Results are invariant over tile sizes 16, 64 and 256, and every native cell and
nucleus count/RGB mean agrees with independent source-pixel arithmetic.
The [complete comparison](benchmarks/ndpi-cells-windows.json) preserves all
label assignments, unmatched objects and split/merge relations.

| Native recipe | Nucleus foreground IoU | Body foreground IoU | Unmatched reference nuclei | Unmatched native nuclei |
| --- | ---: | ---: | ---: | ---: |
| `log` | 0.506401 | 0.633233 | 239 | 32 |
| `log-bodies` | 0.506401 | 0.630488 | 239 | 32 |

These are substantial unresolved differences. Natural-grid synthetic equality
does not establish real-tissue detector parity.

The [prespecified diagnostic](benchmarks/ndpi-cell-peak-diagnostics-windows.json)
changes one parameter at a time, keeping every observation:

| Change from baseline | Cells | Raw peaks | Selected markers | Nucleus foreground IoU |
| --- | ---: | ---: | ---: | ---: |
| None | 299 | 826 | 761 | 0.506401 |
| Seed spacing 0 | 302 | 826 | 826 | 0.499250 |
| Seed spacing 2 | 302 | 826 | 826 | 0.499250 |
| Minimum peak radius 0 | 299 | 826 | 761 | 0.506401 |
| Minimum nucleus area 1 | 444 | 826 | 761 | 0.503656 |
| Unlimited maximum nucleus area | 315 | 826 | 761 | 0.594464 |
| Prominence 0.25 | 329 | 1106 | 984 | 0.514111 |
| Prominence 1 | 226 | 649 | 600 | 0.478439 |

Raw peaks and selected markers include the contextual raster; final counts
include the ROI and area/intensity filtering. Reducing minimum area adds many
unmatched detections. No diagnostic selects new defaults or accepts parity.

Inspection of the [pinned upstream detector](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core-processing/src/main/java/qupath/imagej/detect/cells/WatershedCellDetection.java)
shows that it separates LoG-response basins before testing their intensity,
then merges admitted regions and performs a shape split. The current native
kernel instead tests connected positive LoG regions. That structural difference
is a candidate explanation requiring isolated stage comparisons. Spacing
adjustments alone did not close the observed gap. A corrected stage must retain
archive replay semantics and pass both real and synthetic comparisons before
production integration.

## Reproduction and validation

With the installed native package and test dependencies:

```powershell
python -I tools/compare_qupath_ndpi_cells.py --image build/fixtures/CMU-1.ndpi --reference tests/fixtures/qupath-ndpi-cells-0.7.0/capture --report build/ndpi-cells.json
python -I tools/diagnose_qupath_ndpi_peaks.py --image build/fixtures/CMU-1.ndpi --reference tests/fixtures/qupath-ndpi-cells-0.7.0/capture --report build/ndpi-cell-peaks.json
$env:QUPATHXX_HAMAMATSU_FIXTURE = 'build/fixtures/CMU-1.ndpi'
python -I -m pytest tests/test_qupath_ndpi_cells_reference.py tests/test_label_comparison.py
```

An explicit `--capture NEW_DIRECTORY` can create a fresh runtime capture;
ordinary tests reuse the committed one. Capture checks run without the full
slide or Java runtime. The optional real-slide test requires the pinned slide.
It checks input identity, tile invariance and independent features, preserving
the observed disagreement without treating it as scientific acceptance.

The bounded contingency matcher equals the original pixel-pair oracle on all
twelve real-slide comparisons and twelve independent small-map cases. It
rejects oversized dense pair scratch. The [local validation receipt](benchmarks/ndpi-cells-validation-windows.json)
records 35 passing tests, including corruption, identity, decoded-length,
count/ledger/collision and large-coordinate cropping checks. This packet changes
comparison tooling and evidence; the native detector remains at source2afd.
