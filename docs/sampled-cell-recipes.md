# Sampled LoG cell recipes

The explicit `SampledLogWatershedBodies` C++ recipe combines the native
[sampled shape stage](sampled-shape-watershed.md) with eight-connected background
hole filling and strict-radius watershed cell expansion. Existing recipes and
the no-argument constructor keep their previous behavior.

```python
parameters = q.CellParameters(q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
request = q.CellRequest(document, object_id, transform, parameters)
result = q.detect_cells(request)
```

The constructor sets minimum peak radius and seed spacing to zero. This recipe
requires both values to remain zero and connectivity to remain eight. Shape
prominence, threshold, smoothing, area limits, context, sampling level and cell
expansion remain explicit captured parameters. Transform measurements retain
the existing contract: OD-sum detection reports OD R/G/B, while stain detection
reports deconvolved channels.

The result identity is `qpx.sampled-log-watershed-bodies/1`. Input digest formats
are `qpx.sampled-log-rgba-tiles/1` and
`qpx.sampled-log-rgba-level-tiles/1`. The CLI uses
`--cell-algorithm log-sampled`, CSV records `detector_mode=log-sampled`, and the
direct and batch ImGui controls expose **LoG sampled shape**. Batch resume
identity distinguishes all four algorithms.

New results use QCL4, including a packed historical opaque ROI domain. Loading
checks its length, unused padding, sample count and ROI membership, then
recomputes watershed bodies from the stored nuclei and domain without rereading
the source image. Classifier origins containing this recipe use QCA4; mixed
origins retain their individual algorithms. Earlier QCL1/2/3 and QCA versions
keep their existing encodings and validation rules.

Clipping may retain a nucleus whose maximum lies outside the ROI. Such nuclei
still receive measurements, expansion and replay validation; marker coverage
does not determine the retained feature array size. The textured-crop native
regression covers these components and saved label reconstruction.

The current local workflow checks cover 53 sampled cases plus 92 existing LoG
and body cases, including two pinned NDPI context settings, historical alpha
domains, classifier origins, CLI/batch resume and desktop capture. These are
engineering checks. The pinned real-slide characterization remains 503 nuclei
at zero context and 478 at 32-pixel context against 506 upstream nuclei; neither
biological validation nor general segmentation parity is established.

Full performance qualification, additional scientific comparisons, hosted
production-recipe validation and the overall M0–M7 plan remain open.

## Full workflow profiling

`tools/profile_sampled_cells.py --python PATH_TO_INSTALLED_PYTHON
--output-directory NEW_DIRECTORY --report REPORT.json` alternates the existing
LoG body and sampled recipes in fresh processes at 256, 512 and 1,024 pixels per
side. It retains all observations and excludes one recorded warmup per
size/recipe from summaries. The default records three measured trials per pair.

Each child checks dense labels, one nucleus per isolated disk, independent
strict-radius body expansion, compartment RGB/count arithmetic, exact saved
labels and verified pixel replay. The parent requires identical masks across
both recipes and every trial. Detection, capture, keep/save, reopen validation,
replay and process wall time are separate fields. Windows peak working set
includes fixture construction, verification and persistence. The profile uses
an uncontrolled host and does not accept biological accuracy, general nucleus
boundary parity, GUI performance or a hardware target. Use a new output
directory so previous raw observations and projects remain intact.

## Hosted evidence gates

`tools/ci/accept_unified_engine.py RUN COMMIT --artifacts DIRECTORY --output REPORT
--sampled-cells --ndpi-readers --rendered-loop --gui-stack` requires the inherited
kernel/body gates, all 53 source-bound production workflow names, native QCL4 and
clipped-component regressions, and the sampled comparison report. GUI jobs must
pass all 53 cases; headless jobs may skip only the three named desktop cases.

`tools/ci/accept_manylinux.py --artifacts DIRECTORY --run-json RUN.json --commit
COMMIT --output REPORT --sampled-cells` also requires the earlier engine/body/NDPI
gates. Both builders must pass 50 sampled cases and skip three desktop cases.
Each fresh consumer must pass 48 and explicitly skip the same desktop cases plus
the two NDPI contexts, since its fixture scope excludes the full slide.

`tools/ci/accept_macos_native.py RUN COMMIT --artifacts DIRECTORY --output REPORT
--sampled-cells` requires both native architectures, all 26 frozen shape cases,
QCL4 and clipped-component regressions. It does not accept Python, OpenSlide,
GUI or relocated macOS packaging. All three gates reject earlier kernel-only
or body-only sources when sampled production coverage is requested.
