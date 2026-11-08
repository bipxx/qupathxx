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

## G3 staged consumers — 2026-11-05

One combined cached build completed _native, batch/app and grid/project-batch/
viewer native tests. [Three native suites pass](g3-native.log), including captured
C++ mutable-option alias isolation, identity changes and checkpoint reuse. No
binding compiler retry, wheel or install was needed. The live compiler was
verified against its response file/CPU and awaited rather than restarted.

[95 non-GUI cases pass across retained runs](g3-python-first.log), with only
[the corrected CLI node rerun](g3-python-cli-corrected.log). SDK coverage includes
owning descriptor/option copies, NumPy owner lifetime, exact/ROI factories, physical
limits, QTM5 project replay, CSV4, batch/CLI checkpoint identity and all three old
stored boundaries. Other selected record/area/viewer lifecycle checks pass;14
optional cases were skipped in that non-GUI run and are not claimed as passed.

[Four initial GUI flows pass](g3-gui-first.log), then the [corrected grid preview/
retention flow passes](g3-gui-corrected.log) and the [grid desktop batch passes](g3-gui-batch.log):
six distinct passing GUI cases. They cover managed exact/count owner capture,
retention/save, legacy managed median, both legacy tissue overlays, grid preview/
per-axis reporting/retention and a batch using the same captured recipe. GUI
controls were inspected at their actual scroll position in the [capture](g3-controls.png):
counts5x3/context1x2, source-level-zero mode, readable filter controls and grid
steps1.4x1.6667. The temporary control PPM was retired only after decoded RGB
[equality](g3-capture-proof.json); PNG and failed-run captures/logs remain.

One stub generation produced both public stub files. [Strict valid typing passes](g3-typing-valid.log);
[three intended invalid assignments fail](g3-typing-invalid.log). Both staged stub
files match source. The [candidate identity](g3-candidate-identity.json) matches485
raw compiler inputs to the module's source-tree hash, three staged products and
two stub files at a8956989+dirty. This is staged evidence, not installed shipping.

Two runtime failures were observer identity mistakes: CLI default H&E vectors
were compared with an SDK identity basis, then the GUI OD expectation incorrectly
used displayed stain vectors. Masks matched; the existing GUI OD branch captures
the identity basis. Passed matching CLI vectors and corrected the GUI expectation;
no production code changed and only the failed nodes reran. Next derive identity
from the actual consumer request branch before comparing digests. Equal masks do
not establish equal captured transforms.

G3 used the same cmake/ctest/cache above and the isolated probe interpreter:

```powershell
$probe = 'C:/Users/HEQ/Projects/qupathxx/build/ci/project-metadata-r5-probe-env/Scripts/python.exe'
& $cmake --build $cache --config Release --target _native qupathxx_batch qupathxx_app qupathxx_tissue_grid_tests qupathxx_project_batch_tests qupathxx_viewer_session_tests --parallel 2
& $ctest --test-dir $cache -C Release -R '^(tissue_grid|project_batch|viewer_session)$' --output-on-failure
& $probe tools/generate_stubs.py
& $probe -m pytest tests/test_tissue_grid.py tests/test_tissue_records.py tests/test_tissue_area_limits.py tests/test_viewer_session.py -q --tb=short
```

After the first Python run, only test_grid_batch_cli_checkpoint_and_identity
reran. With QUPATHXX_TEST_GUI=1, the selected GUI nodes were
`test_managed_grid_capture_retention_and_exact_descriptor`,
`test_gui_grid_report_preview_and_retention`,
`test_managed_owner_retains_median_recipe`, and both parameter cases of
`test_gui_tissue_matches_native_mask_and_draws_overlay`. Only the grid preview
node reran after its observer correction. The additional
`test_gui_grid_batch_matches_captured_recipe` then passed separately. Strict mypy
used no-incremental/strict with follow_imports=silent and
follow_imports_for_stubs=True; its positive/negative logs are linked above.

## G4 installed workflow closeout — 2026-11-05

GRID1 is locally verified for its declared bounded level-zero RGBA8 area-grid
workflow. [Seven installed cases pass](g4-installed-first.log): exact/ROI SDK,
owning options/NumPy lifetime, retained QTM5 replay/CSV4, batch/CLI checkpoint
identity/resume, the three old exact quota groups, managed owner capture, GUI
preview/report/retention and desktop batch. Installed [region extraction and
empty-app smoke](g4-entrypoints.json) also pass, including exact RGBA bytes.

The final wheel is built from f72c1992+dirty (the pre-package card was a docs-only
change). Its [integrity receipt](shipping-integrity.json) matches485 raw native
inputs and all65 hashed wheel/RECORD/installed members, including both stubs and
shipping executables. Its SHA256 is
e11f92ca3a0aadb9f66453325b949fe8757af891a0c573284087f058582c5499.
The [reuse proof](g4-reuse-proof.json) compares all485 native inputs and relevant
compiler/configuration fields with G3;58 dependency/public files also match
byte-for-byte. Final provenance/linked-product identities are distinct from the
staged a8956989+dirty products. No staged result is relabeled as the final binary.

One [wheel build](g4-wheel-build.log) refreshed provenance.cpp and linked shipping
products; it did not compile a binding translation unit. One [isolated install](g4-install.log)
replaced MEDIAN1 in project-metadata-r5-env, with no dependency reinstall. One full
binding compile and one stub generation were used across G3/G4. No G4 failure,
repair/retry, second wheel/install, new environment or repeated staged95/six-GUI
matrix occurred. Native53/65-style full qualifications were not rerun.

Commands used the retained backend and shipping helpers:

```powershell
& 'C:/Users/HEQ/Projects/qupathxx/.venv/Scripts/python.exe' -X utf8 'C:/Users/HEQ/Projects/qupathxx/build/ci/tissue_grid_r7_backend.py'
& 'C:/Users/HEQ/.local/bin/uv.exe' pip install --python 'C:/Users/HEQ/Projects/qupathxx/build/ci/project-metadata-r5-env/Scripts/python.exe' --no-deps --force-reinstall 'C:/Users/HEQ/Projects/qupathxx/build/tissue-grid-r7/wheel/qupathxx-0.1.0-cp312-cp312-win_amd64.whl'
$installed = 'C:/Users/HEQ/Projects/qupathxx/build/ci/project-metadata-r5-env/Scripts/python.exe'
& $installed -X utf8 'C:/Users/HEQ/Projects/qupathxx/build/tissue-grid-r7/verify_shipping.py'
$env:QUPATHXX_TEST_GUI = '1'
& $installed -m pytest tests/test_tissue_grid.py -q --tb=short
```

The backend reuses gridfrontends167-package and the same Python/GUI/GEOS/OpenSlide/
TIFF/project/training/tests/interchange settings as G3. It builds only _native,
qupathxx_region, qupathxx_batch and qupathxx_app. Stubs come from the reviewed
source; no regeneration. The entrypoint receipt retains exact command outputs;
the region input/output are synthetic files under build/tissue-grid-r7.

## Scope and waste review

This closes only GRID1's selected area-grid route. Nominal/other sampling contracts,
additional vendor/context/feature/stain/specimen comparisons, large-slide tiled
resources/seams, supported host/release scope and speed superiority remain open.
All M0-M7 remain Partial. Next R7-REGION1 traces retained region intensity features
against existing analysis/raw/statistics/table authorities before adding any API.

G1/G2 failures above remain evidence. G3 reused native authorities/artifacts,
settled consumers before one binding build and generated stubs once. Wrong guessed
paths/Windows filename wildcards and oversized enum output were observed: use
directory searches with -g filters, discover names first and bound long declarations.
Short repetitive polls were unhelpful; changed to20-second polls and verified
compiler ownership. Use actual transform branches for identity assertions and
reuse passing checks after observer corrections.

G4 reused the tested source/configuration/dependencies and the existing packaging/
member-check helpers. No material new waste or failure occurred. Improvement for
the next workflow: settle known option/report/result metadata layouts during the
contract stage. G1 and G3 changed umbrella public headers separately; deferring
known report geometry fields forced broad native dependency recompilation. Keep
necessary focused numerical iterations, but group known header changes rather
than discover settled frontend fields only at the bindings stage. Do not start
a broad header refactor as part of this completed scientific workflow.
