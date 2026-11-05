# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. [AREA1](benchmarks/tissue-area-r7/README.md) is locally
verified. FILTER1 is in progress; broader R2-R16 and R6/R7 science remain open.

Current: **R7-FILTER1 — pre-threshold tissue Gaussian smoothing**
(native implementation verified; installed verification pending).

- Outcome: optional tissue smoothing_sigma in sampled pixels, shared by native,
  owning nanobind, managed viewer, ImGui and SDK batch/CLI, with save/replay/export.
- Contract: [tissue-smoothing.md](tissue-smoothing.md). Reuse masked_gaussian;
  sigma0..16, radius ceil(3*sigma), scalar OD sum/stain feature, normalized finite
  opaque ROI-center support clipped at crop/image/holes/alpha exclusions, no
  outside-ROI reads. Whole-raster filtering crosses decode tile boundaries.
  Keep morphology/physical filtering order. Zero preserves old recipes.
- Implemented: Gaussian phase/kernel path and shared admission estimator; QTM3
  policy/sigma with algorithm4/winding-4; old QTM1/2 canonical behavior; mixed
  tissue CSV2 sigma column with unfiltered-only CSV1; all consumers wired.
  Unfiltered metadata and three scientific-grid estimates retain legacy costs.
- Evidence so far: new direct2D Gaussian native test and affected tissue,
  tissue_records, measurement_tables, cell_calibration and project_batch pass
  (six roles). Raw logs: build/tissue-smoothing-r7/native-smoothing-tests.log and
  native-regressions.log. App/batch native compilation completed successfully.
- First next action: resume the existing bindings/region build in live session
  91866 (cl CPU advancing, no errors reported). Do not start a replacement build.
  It uses the existing gridfrontends167-package cache. Stage once, copy facade, reuse isolated probe
  environment, assert actual module/source identity and generate stubs once.
  Run selected SDK/CLI/persistence/typing cases and explicitly enabled GUI nodes.
  Inspect the scrolled Sigma control before packaging. Root .venv is backend/
  static tooling only; do not use its editable hook for runtime tests.
- Compatibility gate before packaging: the old40ece8a9 fixture in
  tests/fixtures/tissue-smoothing-v1 pins reopened68865-byte table snapshot,
  2542-byte result metadata/storage and1118720-byte work boundary, with original
  capture sizes preserved separately. Verify it and existing tight grid/classifier
  witnesses against the staged runtime. No raised quotas or weakened assertions.
- Stop: full selected Gaussian workflow passes, then close only FILTER1. Median,
  arbitrary-grid/context, stain/specimen and whole-slide resource claims remain
  in R7/R12. Do not declare all R7/M5 or the project complete.
- Cost: one cached native build is complete. No FILTER1 wheel/install/stub
  generation has run yet. Group public interfaces before the one full binding
  compilation; fix observer issues before making one wheel/install. Reuse passed
  numerical cases within their source/configuration, and preserve failures.

## Latest task review — FILTER1 native foundation (2026-11-05)

Outcome: a missing smoothing step was demonstrated; the shared masked Gaussian
now supplies it without a reader, segmenter or new object model. An independent
2D oracle covers OD/stain, ROI/alpha/crop boundaries, noninteger levels, short/
long kernels and tile seams. QTM3 canonical replay, resigned invalid headers and
work admission pass. Five affected regression roles also pass. SDK/desktop,
old saved quota and typing checks remain pending; the existing installed runtime
is still40ece8a9, and must not be mistaken for this candidate.

Waste/action: **reuse** existing filters, raster, cleanup, archives, tables,
consumer parameters, environments and build cache. **Merge** request/codec/
consumer fields before binding compilation. One guessed roi.hpp read failed;
use directory declaration searches before explicit filenames. The old table
boundary was refined using its existing reopened file because decoding compacts
component capacity; no new pixels, detection or recapture were needed. Preserve
both capture and reopened sizes. No native failures/retries occurred.

Plan adjustment/next: FILTER1 is now native-verified and awaiting installed
verification in the existing R7 row; no coverage acceptance changes yet. Next
build bindings, then stage/verify old budgets and actual scrolled controls before
packaging. After each bounded task or failed/stopped attempt, record outcome,
actual waste/action, plan adjustment and changed next task/retry before proceeding.

Pending-test refinement: negative stain features, maximum/subnormal sigma and
both QTM3 sampling policies now have explicit SDK cases. These are not yet run;
C++ compiler inputs are unchanged. The active binding process was re-polled and
remains live, so the next turn must resume session91866 and use its terminal
status before staging. No FILTER1 wheel/install/stub generation has occurred.

Pre-package quota trace: ProjectBatchRequest::Data stores TissueParameters inside
its options and charges sizeof(Data), so the new double may affect an unfiltered
batch snapshot too. Pin a valid old batch boundary from the existing reopened
fixture in the unchanged40ece8a9 runtime, then test it in the staged product.
Do not speculate a repair or change files while the bindings build is live.
This is an additional affected-layout check, not another general campaign.

Staged attempt review: 130 checks pass,9 GUI skips and the newly pinned old
unfiltered batch boundary fails with project snapshot exceeds memory budget.
Other old tissue/table/grid/classifier limits pass. Preserve stage and first log.
Measure the batch delta and JSON length, then repair only legacy inactive-parameter
metadata accounting in ProjectBatchRequest::Data. Fully charge smoothed requests.
This is a demonstrated product compatibility failure, not an observer correction.
Reuse all130 passed results; no wheel or second stub generation has run.

Diagnosis/repair: inactive batch capture grew from103012 to103020 bytes while
recipe JSON stayed1340 bytes. Preserve that8-byte legacy metadata charge inside
the existing bookkeeping allowance; new smoothed requests still charge sizeof(Data)
fully. The first481-input stage hash is frozen before editing. Rebuild only the
changed core body and affected links, then retest the failed node and affected
native batch role. Exact saved metadata witnesses are explicitly MSVC/x64 scoped;
portable numerical/recipe tests retain their checks. No quotas were increased.

Repaired-stage review: the old batch boundary and native project_batch role pass.
Both actual GUI flows and positive/three exact negative typing checks pass.
Reuse the130 earlier passes; first stage remains immutable. The shared Gaussian
primitive already checks cancellation propagation; add only caller-phase cancel/
stale publication checks for the newly exposed smoothing stage, not another
kernel campaign. Inspect the already captured scrolled controls before packaging.

Pre-wheel handoff: old batch boundary/native batch role, two smoothing-phase
cancel/stale tests, two actual GUI flows and valid/three exact invalid typing
checks pass. The scrolled capture shows readable Sigma(px)=1.7, threshold0.66
and the expected913 kept pixels. One body-only budget repair was needed, caught
before packaging; no second full binding compilation or stub generation.
Plan adjustment: inline parameter owners now join the pre-package boundary trace.
Next freeze this repaired source, build/install one wheel and verify exact source/
RECORD membership plus selected final shipping checks. Reuse all130 staged passes.
