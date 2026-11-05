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
