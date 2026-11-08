# R7-FILTER1 — pre-threshold tissue Gaussian smoothing

Locally verified at code source `7873a3a52380f4f3b5039fceedb766ffaad7a0c8`.
All M0-M7 remain Partial. [Numerical/API contract](../../tissue-smoothing.md),
[native oracle](../../../cpp/tests/tissue_smoothing_tests.cpp) and
[SDK/consumer tests](../../../tests/test_tissue_smoothing.py) define the scope.

Tissue smoothing reuses the existing masked Gaussian, followed by the existing
threshold/morphology/components and optional physical limits. Sigma is in native
sampled pixels, 0..16; support is normalized over eligible opaque ROI-center
samples in the captured crop. No outside-ROI context reads or separate tile
filters are introduced. A direct 2D oracle independently covers OD/stain values,
alpha/ROI/crop edges, noninteger pyramid levels and decode seams. This is native
engineering acceptance, not universal QuPath/biological or speed acceptance.

Positive sigma uses algorithm /4 or winding-4 and QTM3. Old QTM1/2 remain canonical
and replay without smoothing. Mixed smoothed tissue CSV2 retains sigma; an
unfiltered-only selection keeps CSV1. SDK/managed viewer/ImGui/batch/CLI share
TissueParameters. Batch recipe identity distinguishes positive sigma values.
The request/decoder share one Gaussian work estimator; legacy inactive-parameter
metadata remains within the existing bookkeeping allowance, with new smoothed
storage fully charged.

## Evidence

| Scope | Result |
| --- | --- |
| [Independent Gaussian native role](native-smoothing-tests.log) | Pass: direct 2D versus separable masks, crop/ROI/alpha, native1.5x level, short/long kernels, tile16/31/256, QTM3 canonical replay/resigned invalid recipes and work admission |
| [Five affected native regressions](native-regressions.log) | Tissue, tissue_records, measurement_tables, cell_calibration and project_batch pass |
| [Initial staged SDK run](stage-tests.log) | 130 pass,9 explicit GUI skips, one real old-batch-budget regression; old tissue/table/grid/classifier limits already pass |
| [Repaired budget node](stage-budget-repaired-tests.log), [native batch role](native-budget-repaired-tests.log) | Both pass at the original103012-byte unfiltered batch limit; no quota increase |
| [Actual desktop](stage-gui-tests.log), [control capture](smoothing-controls.png) | Two flows pass: managed owner/save/reopen and inspector detection; visible Sigma(px)=1.7 and expected913 kept pixels |
| [New worker phase](stage-lifecycle-tests.log) | Cancel and object-stale publication during smoothing both discard output |
| [Valid typing](typing-positive.log), [invalid typing](typing-negative.log) | Strict valid fixture passes; exactly three intended assignment errors remain |
| [Installed wheel](installed-tests.log) | Eight shipping cases pass: CLI/batch/resume, old batch/tissue boundaries and packaged public-stub surface |
| [Pre-smoothing QTM1/2](installed-legacy-codec-tests.log) | Two old archives replay with zero sigma and save byte-identical mask blobs |
| [Shipping integrity](shipping-integrity.json) | All 481 recorded source/build inputs match; all 65 hashed wheel members match RECORD and installed bytes; source stubs/facades agree |

The [reuse proof](evidence-reuse.json) binds first-stage source 2eb092ee+dirty to
its raw inputs. Only cpp/project_batch.cpp changed for the demonstrated inactive
metadata repair; 480 other inputs are unchanged. Repaired stage source matches
final shipped source exactly despite its earlier 3fbd15a2+dirty revision stamp.
No prior result is relabeled as a different product. Two new GUI nodes were run
explicitly; seven other skipped checks remain unclaimed.

The old [ordinary fixture](../../../tests/fixtures/tissue-smoothing-v1/manifest.json)
was captured at 40ece8a9. Reopened table/result and batch boundaries are 68865,
2542 and103012 bytes; work admission is 1118720 bytes. Exact metadata tests are
MSVC/x64 scoped. Original capture sizes remain separate because archive decode
compacts component capacity. The old [winding witness](../../../tests/fixtures/tissue-smoothing-v1/winding-manifest.json)
uses the retained pre-smoothing 77e2bc9f+dirty runtime and the same 8x8 pixels, with
its own source/input hashes. It proves the QTM2 route without a new specimen.

One cached wheel and one install were used, after staged checks and visual
inspection. The existing project-metadata-r5-probe-env loaded the two retained
stages; project-metadata-r5-env loaded the wheel. Root .venv supplied build/static
tools only. Raw build/install logs, failed first-stage runtime and repaired stage
remain under build/tissue-smoothing-r7. No full platform campaign was run.

Reproduce native scope with CTest selection
`^(tissue_smoothing|tissue|tissue_records|measurement_tables|cell_calibration|project_batch)$`.
The initial SDK selection was tests/test_tissue_smoothing.py,
test_tissue_area_limits.py, test_tissue_records.py, test_measurement_tables.py,
test_public_stubs.py, plus these explicit nodes in test_cell_intensity_statistics.py:
`test_legacy_exact_grid_budget_still_reopens_and_replays` and
`test_old_classifier_exact_snapshot_budget_reopens`. Later added lifecycle and
QTM1/2 cases have their separate logs; do not infer their results from the first run.
Enable QUPATHXX_TEST_GUI=1 for `test_managed_viewer_retains_smoothing` and
`test_desktop_smoothing_options_and_controls`. Typing uses
tests/typing/tissue_smoothing_valid.py and tissue_smoothing_invalid.py with
config-only silent stub-import behavior; do not suppress misuse errors or use
the root editable runtime.

## Waste review and next task

One full public-header bindings compilation and one stub generation were needed.
The existing build session stayed live and was polled rather than restarted.
Shared filters, raster, cleanup, codecs and consumer parameter routes were reused.
A guessed roi.hpp read failed; declaration/file discovery replaced that guess.

The valid old snapshot witness caught a real 8-byte inactive batch-options growth
before packaging. JSON remained 1340 bytes while capture grew 103012->103020.
Preserving legacy bookkeeping required only a core-body rebuild/relinks. The
single failed node and affected native role were rerun; 130 earlier passes were
reused. Inline parameter owners now join the pre-package layout trace. This
avoided the extra wheel/install seen in the previous task. Scrolled controls were
inspected before packaging, and existing capture RGB was converted unchanged to
PNG. Failures, both stages and original fixtures remain retained.

The final compatibility audit added direct old QTM2 canonical evidence after
packaging. It changes tests/evidence only and needs no new wheel. Next
R7-MEDIAN1 traces the median workflow, its stage/order and support contract
against existing/pinned references before choosing a missing implementation.
Median, arbitrary-grid/context, stain/model/specimen, resources, hosts/release and
all other R2-R16 gaps remain in the single queue. No new reader, model,
environment, generic report framework or broad campaign was added.
