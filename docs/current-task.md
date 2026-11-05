# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-SETTINGS1 is locally verified in its recorded scope.

Current: **R5-DATAPROP1 — saved ImageData properties** (ready for contract preparation).

- Outcome: retain supported saved ImageData properties through native import,
  Python/ImGui inspection and project snapshot/reopen, keeping them separate from
  root/ROI facts, analysis defaults and executable workflow history. Unsupported
  representations need an explicit review/refusal policy; no silent narrowing.
- Entry/evidence: cpp/qupath_saved_data.cpp::properties currently discards every
  value except the reserved PROJECT_ENTRY_ID association. The existing pinned
  saved-data losses fixture contains synthetic="value" and reproduces this loss.
- Next action: inspect the pinned ImageData property serialization, existing
  stream node/value support, property/fact codecs and document/history ownership.
  Identify scalar/null/container types actually encountered, their bounds and
  loss policy before choosing a native value/API. Confirm consumer/observer
  members from the actual runtime or stubs rather than assuming telemetry.
- Reuse/dependencies: same pinned QuPath0.7 runtime, existing fixture and native
  stream/import-review authority; existing history, sparse transactional storage,
  native ownership and inspector patterns. No new reader, kernel or decoder.
  New capture is justified only for an uncovered type/behavior. Owner release,
  license and biological decisions do not block this engineering slice.
- Checks/stop: independent typed/key/value expectations; source-guarded review;
  clear/no-op/history and snapshot final release; exact save/reopen plus affected
  corruption/migration refusal; thin Python types and actual inspect flow. Settle
  interfaces before one cached build and reuse unaffected numerical evidence.
  Close only the declared property route; other R4/R5 formats/records/races and
  R6-R16 remain in the same queue.

Latest closeout (2026-11-04, R5-SETTINGS1):
Outcome/evidence: [saved defaults](benchmarks/image-settings-r5/README.md) locally
verifies six image types, two/three-stain profiles, full normalized basis, names,
white/residual identity, explicit request precedence and GUI adoption, history,
schema20 migration/refusal and owning snapshots. Nativec68bc511:257 staged passes/
13 declared skips (29 new headless cases), six native suites, two actual desktop
cases, five packaged-stub checks, positive/three-diagnostic negative typing and
one installed import smoke.457 compiler inputs and65 hashed installed members
match; native bytes are reused in the final wheel. Capture inspected.
Waste/action: **merge** native/bindings/store/UI into one764.38s cached build;
**reuse** stain math, pinned capture/runtime and unchanged passing scopes. One
stub generation and one install. **Retain** baseline loss and the initial two
GUI-observer failures. **Remove** the assumed GuiReport.analysis_result field:
corrected tests use the existing retained-result API, without a new native field
or rebuild. **Remove** guessed documentation filenames by using discovered
existing evidence links; the final link check also found a stale recovery anchor
and redirected it to the actual evidence heading. Repeat only affected link/
desktop checks. Preserve executed probe bytes in Git to avoid a needless recapture.
Plan change/next: SETTINGS1 closes in this local scope. Prepare DATAPROP1 above
from the existing demonstrated loss. Broader ImageData records, interchange,
science/resources, platforms and release remain open; no overall speed claim.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
