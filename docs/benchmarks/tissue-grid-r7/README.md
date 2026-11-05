# R7-GRID1 evidence (in progress)

## G1 native foundation — 2026-11-05

The working tree based on7985a13e adds explicit ScientificGrid area-RGB tissue
requests and per-ROI count/context factories through the existing sampler,
transformed channels, digest and cleanup/component authorities. This is native
foundation evidence, not installed SDK/UI, persistence or complete GRID1 acceptance.
No GRID1 bindings build, generated stubs, wheel or install has run.

The new tissue_grid suite passes144 combinations of context0/1/2, sigma0/0.8,
median off/on, thresholds0.5/1/1.5 and no/open/close/fill cleanup. The independent
oracle directly intersects world footprints, integrates grayscale RGB, computes
log10 optical density and full2D support-normalized Gaussian, binary rank,
square morphology and dual-connectivity hole fill. It checks mask/core counters
including filled pixels. Additional cases cover supported versus unsupported
small ROIs, per-axis geometry, digest-only identity, exact/one-byte-under grid
admission, wrong source/level, nucleus refusal, pre-cancel, calibrated count
resolution and stale asynchronous publication. This is a synthetic RGB8 engineering
contract; it does not establish specimen, whole-slide or speed acceptance.

Existing project_batch, cell_calibration, tissue, tissue_smoothing and tissue_median
suites pass in [the first run](native-g1-first.log). Their code inputs remain
unchanged by the following observer-only fixes; CMake refreshed provenance.cpp
for those edits, so later archive bytes are not relabeled as the earlier product.
The [complete corrected grid run](native-g1-grid-final.log) passes.

Retained failed attempts: first the grid observer compared const char* addresses;
fixed to compare string contents. The [next attempt](native-g1-grid-corrected.log)
rejected a small ROI whose selected footprints all touched the transparent source
pixel. Retained it as an explicit rejection and widened the separate supported
context case. No numerical assertion was relaxed and no production kernel fix
was needed after those attempts. Static inspection also corrected the initial
oracle's logarithm base and grid digest routing before the first test execution.

Build: existing gridfrontends167-package cache, Release, MSVC x64, parallel2.
One initial six-target native build, one digest/expanded-oracle incremental build,
and two observer-only grid-target repairs. Zero Python compilation/package work.

Commands (executed from build/ci/scientific-grid-159):

```powershell
$cmake = 'C:/Users/HEQ/Projects/qupathxx/.venv/Lib/site-packages/cmake/data/bin/cmake.exe'
$ctest = 'C:/Users/HEQ/Projects/qupathxx/.venv/Lib/site-packages/cmake/data/bin/ctest.exe'
$cache = 'C:/Users/HEQ/Projects/qupathxx/build/ci/gridfrontends167-package'
& $cmake --build $cache --config Release --target qupathxx_tissue_grid_tests qupathxx_tissue_tests qupathxx_tissue_smoothing_tests qupathxx_tissue_median_tests qupathxx_cell_calibration_tests qupathxx_project_batch_tests --parallel 2
& $ctest --test-dir $cache -C Release -R '^(tissue_grid|tissue|tissue_smoothing|tissue_median|cell_calibration|project_batch)$' --output-on-failure
# Observer repairs rebuilt only qupathxx_tissue_grid_tests, then:
& $ctest --test-dir $cache -C Release -R '^tissue_grid$' --output-on-failure
```

## G2 retention and export — 2026-11-05

QTM5 now retains exact area-grid geometry, policy, Gaussian/median controls and
embedded provenance. It reuses the ten-integer cell grid prefix and ProvenanceCodec;
external project provenance must agree. Native record acceptance admits only the
tissue grid algorithm/input identity, leaving cell/nominal masks in their own
archives. Replay preserves the descriptor, and changed effective calibration
refuses replay and reports source_changed. Scalar geometry is never requested for
a grid record. The old natural recipe and QTM1-4 encodings are unchanged.

CSV4 is selected only when a chosen tissue record has a grid. It includes filter
fields, sampling kind, source dimensions, signed origin, extent, counts, context
and per-axis steps. Grid scalar downsample is blank. Mixed native rows keep blank
grid-descriptor fields and their natural step. Component area uses the product
of both steps and captured calibration. Old-only selections keep CSV1/2/3.

[Eight affected native suites pass](native-g2-first.log): project_batch,
measurement_tables, cell_records, tissue_records, tissue_contours, tissue_grid,
tissue_median and tissue_smoothing. This includes the existing cell codecs through
the extracted descriptor helper. New archive cases cover both ROI policies,
sigma0/0.8 and median off/on, exact re-encoding, retained statistics/centroids,
request-ticket refusal, mismatched/removed provenance, changed-grid verification,
calibration status, cancelled decode, re-signed malformed recipe/grid prefixes,
project restart/replay, mixed CSV4 and exact/one-byte-under table admission.

The [final targeted observer](native-g2-final.log) also passes signed-origin
archive/CSV, empty-mask archive/CSV and a re-signed context-label witness. That
witness moves a foreground label out of the core while preserving its total, so
count agreement alone cannot accept the archive. No production code changed
between the eight-suite run and this observer extension; CMake refreshed provenance
for the added test source, so those binaries keep their distinct metadata identity.

All three [pre-grid MSVC/x64 witnesses](../../../tests/fixtures/tissue-grid-v1/manifest.json)
pass their exact retained/payload/table/batch/JSON/work boundaries in the new
native target. They cover unfiltered, Gaussian and Gaussian+median recipes. These
platform-specific allocation values are guarded on other toolchains. Installed
consumer checks remain G3/G4 work; these results are not a wheel/package claim.

One initial native build [failed](native-g2-build.log): the new grid flag collided
with an existing local bounds variable. Corrected the local name, removed a new
shadowed size name and charged sizeof the actual new decoded component scratch.
The corrected eight-target cached build passed; a final grid-only observer build
added the three remaining integrity assertions. No numerical test failed, no
assertion was weakened, and no bindings/stubs/wheel/install ran.

G2 commands used the same cmake/ctest/cache as G1:

```powershell
& $cmake --build $cache --config Release --target qupathxx_tissue_grid_tests qupathxx_tissue_record_tests qupathxx_measurement_table_tests qupathxx_cell_record_tests qupathxx_tissue_contour_tests qupathxx_project_batch_tests qupathxx_tissue_median_tests qupathxx_tissue_smoothing_tests --parallel 2
& $ctest --test-dir $cache -C Release -R '^(tissue_grid|tissue_records|measurement_tables|cell_records|tissue_contours|project_batch|tissue_median|tissue_smoothing)$' --output-on-failure
# Observer extension rebuilt only qupathxx_tissue_grid_tests, then:
& $ctest --test-dir $cache -C Release -R '^tissue_grid$' --output-on-failure
```

## Remaining and waste review

G3 must expose the settled native route through nanobind, managed viewer, ImGui,
batch and CLI. G4 must verify one installed workflow, including the retained legacy
witnesses. No GRID1 bindings build, stubs, wheel or install has run. G1/G2 synthetic
and native retention checks do not close GRID1, broader R6/R7/R12 or M0-M7.

G1 observed waste: guessed paths/oversized inspection output; a build started before
checking the digest entry point; two observer setup mistakes caused extra grid
compiles. Apply: discover declarations; inspect identity callers/return types;
establish fixture eligibility with the independent oracle before selecting an ROI.
G2 observed waste: local-name collision required a cached compiler retry. Inspect
existing locals before inserting a branch; keep the failure. Reuse passing native
code results for observer-only extensions rather than repeating all suites. Reuse
sampler/codec/provenance/table authorities and old fixtures; no full campaign or
new result model is required. QTM5 keeps the producer's historical work estimate
but separately admits the current decoder layout, avoiding an ABI-dependent
capacity-equality requirement. Legacy exact estimate semantics remain unchanged.
