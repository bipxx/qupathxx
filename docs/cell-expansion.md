# Native watershed cell-body expansion candidate

The production LoG detector still uses nearest-label Euclidean expansion with
an inclusive radius. The separate `cpp/cell_expansion.hpp` kernel instead uses
a strict radius on float distances, stable row-major/FIFO frontier admission,
four-connected propagation and unassigned collision lines. It preserves every
nucleus seed, clips propagation to the supplied domain, admits bounded scratch
before allocations and checks native cancellation. Each nonseed pixel enters
the frontier at most once, with O(n) storage and O(n log n) frontier work.

The [source-bound Windows comparison](benchmarks/cell-expansion-native-windows.json)
starts from the immutable QuPath nucleus maps and exactly reproduces all seven
equivalent natural-grid body label maps: blank, isolated, touching, gap, size
filters, gradient/noise and anisotropic level zero. Every label pixel matches.
The tool verifies all captured file hashes, upstream/contract/generator identities
before comparing; the receipt binds the native binary, compiled source, test
evidence and executed tool. Unit checks cover strict cutoff, collision lines,
domain barriers, seed preservation, bounded frontier admission, invalid inputs,
cancellation and memory rejection.

This accepts only the expansion kernel on the named maps. It does not accept
production detector output, biological performance, context clipping, arbitrary
resampling, general upstream parity or performance budgets. The ROI-hole and
resampled cases are explicitly excluded from this expansion-only comparison.

Reproduce with the built native test executable and an installed scientific
Python environment:

```powershell
ctest --test-dir build/native -C Release -R '^cell_expansion$' --output-on-failure
python -I tools/compare_cell_expansion.py --executable build/native/Release/qupathxx_cell_expansion_tests.exe --output build/cell-expansion.json
```

The binary file protocol is an internal test probe, not a public analysis API.
Production integration requires an explicit captured expansion mode, distinct
algorithm/input identities, versioned archives/classifier sources, replay and
batch-manifest updates, Python types and ImGui controls. Existing saved recipes
must retain their current inclusive nearest-label behavior. The new mode must
then pass full detector/feature, archive, native/Python/CLI/GUI and platform gates
without weakening the pinned tolerances.

## Production integration under validation

`CellAlgorithm::LogWatershedBodies` / Python `LOG_WATERSHED_BODIES` is the
explicit production recipe for the kernel. It shares the existing LoG nucleus
detector and contextual input grid, with strict-radius watershed body expansion.
The default `DistanceWatershed` and existing `LogWatershed` keep their inclusive
nearest-label expansion. Selecting the new mode does not change the nucleus
radius, spacing, area-filter or ROI policies.

The result identity is `qpx.log-watershed-bodies/1`. Input digest domains are
`qpx.log-bodies-rgba-tiles/1` and `qpx.log-bodies-rgba-level-tiles/1` so requests
cannot verify against older detector records just because the source pixels
match. New cell archives use QCL3; QCL1 and QCL2 remain the encodings of the
existing modes. Classifier archives use QCA3 if any source row references the
new mode and preserve mixed distance/LoG/body recipes. Existing QCA1/QCA2
archives remain readable and keep their canonical encodings.

The native batch CLI accepts `--cell-algorithm log-bodies`; its manifest records
algorithm value 2 separately from LoG value 1, preventing checkpoint reuse across
those modes. CSV uses `detector_mode=log-bodies` within the existing parameter-
aware table schemas. Shared ImGui cell and batch controls expose “LoG with
watershed bodies”; managed viewer commands capture the same native parameters.
LoG prominence/context options apply to both LoG variants.

The [local typed production wheel](benchmarks/watershed-bodies-windows-local.json)
passes 47 native suites, 92 focused Python cases, installed workflow, generated
stub consistency, three positive typing inputs and seven intended negative
typing rejections. All 251 committed CMake inputs match the compiled source
fingerprint at `2afdcb99555bf142211cdfa836a4671a6f54bbf1`, including the
private TIFF-zlib symbol-isolation fix. New workflow checks cover the pinned
corpus, independent compartment RGB means, tile invariance, all three saved
recipe modes and classifier sources, CLI/batch resume, pyramid work admission,
stale-result/cancellation behavior and direct/batch/managed desktop capture.
The [nine-case production comparison](benchmarks/qupath-cells-watershed-bodies-windows.json)
now records exact nucleus labels in seven natural-grid cases and exact body
labels in eight, including the ROI-hole case. The ROI-hole nucleus foreground
IoU remains 0.969849; the resampled nucleus/body IoU is 0.938983/0.938699.
All 42 explicit-winding shared-geometry measurements retain their original
tolerances and pass, and all detector labels/features are tile-invariant.
The comparison retains segmentation parity and biological validation as false.

The full local GUI regression passes 2,248 cases, with 13 optional skips and
the same three strict legacy scientific xfails. The inspected
[native workspace capture](images/watershed-bodies-windows.png) shows three
published cells at 1600 × 1000, exact native/GUI labels and no reported overflow.
Source-specific Windows/Linux/Mac runs remain pending.
Use the source auditor with `--watershed-bodies --ndpi-readers
--rendered-loop --gui-stack` to require the new native suite, all 42 production
body cases (three desktop-only) and source/native/tool-bound comparison output.
It accepts the old engine under the old gates and rejects it under the new mode.
Portable consumers, broad science, hardware and supported release remain open.

## Runtime and memory observations

The [paired production profiles](benchmarks/watershed-bodies-scaling-windows.json)
retain all 48 fresh-process records, including 12 excluded warmups, at 256,
512 and 1024 square pixels and tile sizes 64/256. Every count, compartment RGB
mean and per-mode label/feature invariant check passes. Nucleus maps match
between the two modes; their body differences are retained.

At 1024 square pixels and 256 cells, the new mode records detection medians
of 307–318 ms and about 107 MiB process peak after detection. Existing LoG
records 308–327 ms on this sequential comparison. Process peaks after the
independent output checks are also retained. Timers exclude input generation
and those checks; memory includes Python, NumPy, imports, generated inputs and
native allocations. This shared workstation is uncontrolled, so these data
accept neither a speedup nor a hardware/performance budget. The named decoder,
GUI navigation, biology and release gates remain separate requirements.
