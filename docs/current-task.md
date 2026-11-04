# Current implementation task

Use the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [ordered remaining slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is accepted; R1 inventory is reconciled with owner decisions open. The four
recent R2 slices and [R4-MERGE1](benchmarks/annotation-merge-r4/README.md) have
local installed evidence. All M0-M7 remain Partial.

Current: **R5-QPPROJ1 closeout**, implemented with local results, not yet closed.

- Outcome: reviewed QuPath0.7.0 manifest plus hash-bound, QuPath-assisted
  annotation import through native/Python/ImGui, followed by native save/reopen.
  Native source is ee7555e8; final observer/exporter/stub edits remain pending.
- Reuse: pinned project/qpdata/exchange fixtures, existing source guards and
  transactional snapshots, retained build and installed environment under
  `build/qupath-project-r5` and `build/ci/qupath-project-r5-env` at the root checkout.
- Remaining actions: inspect final exporter controls and successful UI capture;
  cover only a missing optional-metadata/unknown-calibration assertion if needed;
  verify source/package/fixture identities; integrate pending changes and publish
  one scoped evidence summary and contract update. Preserve original failures.
- Checks: reuse model/native/typing/stub results for unchanged inputs and the
  corrected desktop apply case. Run only a newly added or affected case. Native
  rebuild only for changed compiler inputs/configuration; no platform campaign.
- Stop: supported assisted import is reviewable, loss-aware and evidence-linked.
  Direct Java .qpdata decoding, additional builders/properties/planes stay open
  under R5. Full migration, scientific acceptance and overall speed remain open.

Next after closeout: inspect remaining R4/R5 inventory and select one uncovered
source-replacement/relink race or edit/persistence behavior. Name the actual gap,
existing authority, decisive check and stop condition before coding. Then follow
R6/R7, R8-R10, R11, R3, R12 and R13; pull required R2 inputs forward and retain
R14-R16 parity work. Blocked decisions affect only their dependent claims.

Latest task closeout (2026-11-04, remaining-work plan update):
Outcome/evidence: reconciled the queue with implemented QPPROJ1 and pending
closeout; preserved full R0-R16 scope and explicit milestone exits.
Waste/cost: removed stale fixture-capture/implementation instructions from the
active next task; reused retained results and exporter controls. This planning
change needs diff/link/consistency review only; no build or test campaign.
Plan change/next action: finish QPPROJ1 closeout before opening another slice;
after each task or failed attempt, record remove/merge/reuse/defer/retain decisions
and update the next behavior, dependencies, reusable product and selected checks.

Apply the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
before the next task or retry. Keep this card short; technical lessons belong
in the existing compilexx notes. Only openly licensed readers/codecs are eligible.
Biology, hardware, project license and exact release support remain owner decisions.
