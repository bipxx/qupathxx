# R7-MEDIAN1 — binary post-threshold tissue median

Locally verified at code source `34704c24ac7fc0f2087fe36140a285976986a24d`.
All M0-M7 remain Partial. [Contract](../../tissue-median.md),
[native tests](../../../cpp/tests/tissue_median_tests.cpp) and
[SDK/consumer tests](../../../tests/test_tissue_median.py) define the scope.

An optional median_cleanup flag runs fixed 3x3 binary majority after threshold,
before opening/closing/hole filling and component filters. Crop borders replicate
closest samples; excluded ROI/alpha samples are background and output stays in
domain. Two vote passes reuse existing queue scratch, with cancellation checks.
Native requests, owning SDK, managed viewer, ImGui and batch/CLI share the flag.

Pinned QuPath 0.7.0 uses ImageJ radius 1 cleanup at this binary stage. The
[reference identity](reference-identity.json) records exact source and runtime
hashes. Its 524 [captured cases](../../../tests/fixtures/tissue-median-v1/median-reference.json)
include all 512 three-by-three binary patterns and degenerate/border rasters.
They agree with independent rank/edge calculations and the native primitive.
This comparison covers full opaque rectangles; native ROI-clear/context and
upstream preprocessing/annotation policies remain explicit separate contracts.
No whole-plugin, specimen, host or speed acceptance is implied.

Median uses QTM4/algorithm5 or winding-5. Mixed median tables use CSV3 with sigma
and median state; old unfiltered/Gaussian selections preserve QTM1/2/3 and CSV1/2.
The offset-based inactive metadata helper replaces five scattered size deductions
and charges active aligned storage. It preserves both older parameter groups'
exact record, table, batch and work limits without another repair.

## Evidence

| Scope | Recorded result |
| --- | --- |
| [Native roles](native-tests.log) | All seven pass: median, Gaussian, tissue, tissue_records, measurement_tables, project_batch and cell_calibration; exhaustive rank/domain/border, level/tile and QTM4/CSV3 guards/replay |
| [Reference capture](reference-capture.log) | Pinned runtime captured 524 cases once; runtime jar/source and semantic JSON hashes are retained |
| [Staged SDK/CLI/persistence](stage-tests.log) | 148 pass, 11 explicit skips; no product/observer failure. Includes old unfiltered/Gaussian limits, prior archives and tight grid/classifier witnesses |
| [Actual desktop](stage-gui-tests.log), [visible controls](median-controls.png) | Two flows pass: managed owner/save/reopen and inspector detection; readable checked 3x3 control, 415 kept pixels and 92 components |
| [Strict valid typing](typing-positive.log), [invalid typing](typing-negative.log) | Valid fixture passes; exactly three intended invalid boolean assignments remain |
| [Installed shipping checks](installed-tests.log) | Eight pass: CLI/batch/resume/CSV3, exact old boundaries, canonical old QTM1/3 blobs and packaged stub surface |
| [Shipping identity](shipping-integrity.json) | All 482 recorded source/build inputs and 65 hashed wheel members match source, RECORD and installed bytes |

[Stage-to-final proof](evidence-reuse.json) verifies identical source and configuration,
with only the revision stamp differing (stage 336f688b+dirty, final 34704c24).
Numerical and GUI evidence is reused at that same source. Two GUI skips were
run explicitly; the other nine skipped checks remain outside this task's claims.

The [old pre-median fixture](../../../tests/fixtures/tissue-median-v1/manifest.json)
was saved by 7873a3a5, then reopened before measuring limits. It pins unfiltered
and active Gaussian result/table/batch/work boundaries. Metadata limits are
MSVC/x64 scoped. Older QTM2 and exact grid/classifier witnesses are reused from
the existing corpus; no extra decoder or image source was added.

## Reproduction and retained artifacts

Use CTest selection
`^(tissue_median|tissue_smoothing|tissue|tissue_records|measurement_tables|project_batch|cell_calibration)$`.
The staged Python selection included test_tissue_median.py, test_tissue_smoothing.py,
test_tissue_area_limits.py, test_tissue_records.py, test_measurement_tables.py,
test_public_stubs.py and two explicit nodes from test_cell_intensity_statistics.py:
`test_legacy_exact_grid_budget_still_reopens_and_replays` and
`test_old_classifier_exact_snapshot_budget_reopens`.
Enable QUPATHXX_TEST_GUI=1 for `test_managed_owner_retains_median_recipe` and
`test_visible_median_control_and_mask`. Typing uses tissue_median_valid.py and
tissue_median_invalid.py with the existing config-only silent stub-import setup.
The pinned capture producer reads median-contract.json and writes median-reference.json
through the cached QuPath script CLI; do not recapture it for a status update.

The existing project-metadata-r5-probe-env loaded build/tissue-median-r7/stage;
project-metadata-r5-env loaded the wheel. Root .venv was build/static tooling only.
Build/install logs, stage, source reference cache and lossless capture remain under
build/tissue-median-r7 and build/upstream. No full platform campaign was launched.

## Waste review and next step

One full binding compilation, one public-stub generation, one cached wheel and
one install were used. The live compiler was polled rather than restarted.
All native/staged/installed checks passed on their first attempts. Capturing both
older parameter groups before editing and consolidating metadata accounting
avoided the 8-byte compatibility repair needed in the previous task.

The web source reader had a cache miss; direct fetching only the pinned file
resolved it without another checkout. The existing ImageJ runtime and tiny masks
supplied the reference; no new specimen corpus, reader, model or environment.
Scrolled controls were inspected before packaging. Existing capture RGB bytes
were converted unchanged to PNG; the renderer was not repeated for image format.
Reuse passed checks and one evidence summary instead of an audit for each substep.

Next R7-GRID1 traces explicit tissue sampling/context against existing scientific
grid, nominal support and archive authorities. Broader arbitrary radius, context,
measurement/stain/model/specimen, whole-slide resources and R2-R16 remain open.

The redundant build-directory PPM copy was retired after [RGB equality](capture-preservation.json)
was verified against the retained PNG. Original pixel bytes can be reconstructed;
no source, fixture, failed run or required build cache was removed.

The cleanup guard's Split-Path parameter combination was unsupported on this
host. The fixed resolved nonrecursive target and its absence were verified with
System.IO.Path afterward; no deletion was repeated. Use a supported path API
for future guards. The PNG/equality manifest, source, fixtures and caches remain.
