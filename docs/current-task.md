# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. FILTER1 is locally verified; MEDIAN1 is in progress.
The full R2-R16 inventory remains active.

Current: **R7-MEDIAN1 — post-threshold binary median cleanup**
(native implementation verified; installed verification pending).

- Outcome: optional median_cleanup in TissueParameters, shared by native, owning
  Python, managed viewer, ImGui and SDK batch/CLI, with retained/replayed/exported
  state. [Contract](tissue-median.md) pins units, order, support and versions.
- Contract: fixed3x3 binary median (five of nine foreground), replicated crop
  borders, excluded ROI/alpha values are background and output stays in domain.
  Runs after scalar/Gaussian thresholding and before opening/closing/hole fill.
  The primitive matches pinned ImageJ radius1 on full opaque rectangular masks;
  upstream ROI-clear/preprocessing/context semantics remain distinct.
- Implemented: two vote passes using existing queue scratch and cancellation;
  QTM4/algorithm5/winding-5, CSV3 with sigma/median flags; old QTM1/2/3 behavior
  remains when disabled. All consumers are wired. One offset-based helper now
  handles inactive metadata for masks, grid estimators and batch requests.
- Evidence:524 captured ImageJ1.54p cases match an independent majority oracle.
  Native tissue_median, tissue_smoothing, tissue, tissue_records,
  measurement_tables, project_batch and cell_calibration all pass. Logs live in
  build/tissue-median-r7/reference-capture.log and native-tests.log.
- First next action: resume the live bindings/region build in session58426
  (confirmed live; cl CPU advancing, no errors reported). Do not launch a
  replacement build. It uses the existing gridfrontends167-package cache.
  After terminal success, stage once into build/tissue-median-r7/stage,
  copy facades and use the existing isolated probe environment. Assert actual
  module/source identity, generate stubs once, then run selected SDK/CLI, old
  format/metadata and typing checks. Enable the two new GUI nodes explicitly and
  inspect the scrolled checkbox before packaging. Root .venv is backend/static
  tooling only; never use its editable hook for runtime tests.
- Pre-package compatibility: pre-median.qpxproj and manifest in
  tests/fixtures/tissue-median-v1 pin7873a3a5 unfiltered/Gaussian result, table,
  batch and work limits after reopen. Check both recipes, plus existing old
  QTM1/2/3 and tight grid/classifier witnesses. Preserve bounds, not raised limits.
- Stop: the complete selected median workflow passes installed verification,
  then close only MEDIAN1. Arbitrary radius/grid/context, measurements/stains/
  models/specimens and whole-slide resources remain in R7/R12 and the wider queue.
- Cost: one cached native build is done; no median wheel/install/stub generation
  has run. Group all native/consumer fields before the one full binding compile.
  Reuse compatible passed evidence; fix any observer/budget issue before packaging.

## Latest task review — MEDIAN1 native foundation (2026-11-05)

Outcome: pinned QuPath tissue code and ImageJ1.54p establish a binary cleanup
stage, avoiding an incorrectly placed scalar median.524 actual primitive outputs,
including every3x3 binary pattern, agree with independent edge replication and
rank calculations. The native implementation and seven affected roles pass;
SDK/desktop/typing, old exact byte limits and packaging remain pending.

Waste/action: **reuse** the current mask/cleanup pipeline, queue scratch, jobs,
archives, table layer and consumer parameter routes. **Consolidate** repeated
hardcoded metadata deductions using actual parameter offsets/aligned tails, with
both old unfiltered and active Gaussian witnesses captured before editing.
A web source read had a cache miss; fetching only the pinned file into the
existing cache resolved it without another checkout. The cached Java runtime
produced the reference once; no new corpus/reader/model/environment or generic
framework was added. No native failures or retries occurred.

Plan adjustment/next: MEDIAN1 is native-verified in the existing R7 row; coverage
acceptance stays pending installed evidence. Finish bindings, verify all promised
old formats/owners and inspect visible controls before one package. After every
bounded task or failed/stopped attempt, review actual waste and update this card/
queue before the next task or retry. Keep broader M0-M7 scope intact.

Live handoff: binding session58426 was re-polled and is still running. Native
source is frozen at4a252c6e plus this documentation handoff; no median stub,
wheel or install has run. The probe/installed runtimes still contain Gaussian
code and must not be mistaken for the new median product. Resume the same build,
then stage and verify old unfiltered/Gaussian boundaries before packaging.
