# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R4-CLIP1 is locally verified in its recorded scope.

Current: **R5-ROOT1 — saved-data root metadata/measurements** (ready for contract
preparation; no implementation or verification claimed).

- Outcome: retain the pinned QuPath root metadata/measurements as image-level
  historical facts through native import, Python/ImGui access and saved reopen,
  rather than requiring loss permission and dropping them. Do not create a fake
  ROI or present historical values as newly computed image measurements.
- Evidence/entry: cpp/qupath_saved_data.cpp explicitly emits
  root_properties_dropped when root attributes/measurements are nonempty;
  QuPathProjectPreview owns nonmutating preview and atomic import. Reuse existing
  ObjectProperties facts/limits, decoder primitives, storage transactions and
  object-properties inspector/table patterns. The workflow inventory records
  this gap separately from already delivered object-level facts.
- Next action: inspect pinned root serialization and existing captured fixtures;
  record independent expected keys/values and the current loss. Trace preview ->
  native Document/image ownership -> persistence -> Python/UI before choosing an
  additive facts API and schema migration. Settle root versus ImageData settings
  semantics, history, limits, nonfinite values and legacy defaults before edits.
- Checks: captured root values and independent native fixtures; object facts
  unchanged; mutation/refusal/history where applicable; save/reopen and legacy
  project load; import preview/source preservation and loss reports; thin Python
  access/types and one actual inspector flow. Select the affected older contracts
  after tracing producers/consumers; no speculative full qualification.
- Build/reuse: reuse current installed CLIP1 product for baseline behavior and
  the existing cache. Settle interfaces/schema/casters/consumers before one
  combined build, run new and older affected contracts staged before install,
  and generate stubs once if public APIs change. Do not repeat clipboard or
  source-axis checks unless their inputs or a concrete concern change.
- Stop: supported root facts are retained and reviewable across that workflow,
  with unsupported values explicit and refusals atomic. Image analysis settings,
  additional saved-data versions/records, remaining R4/R5 gaps and R6-R16 stay in
  the existing queue; completing ROOT1 will not close all interchange or M4.

Latest closeout (2026-11-04, R4-CLIP1):
Outcome/evidence: [annotation clipboard](benchmarks/object-clipboard-r4/README.md)
locally verifies native/Python/ImGui copy/paste, graph/state/planes, fresh IDs,
selection, atomic refusal/history and saved restart. Native dd33c2b1 / observer
576b5b81: 173 distinct staged passes/fourteen skips (18 new headless passes),
five native suites, two desktop flows, five packaged-stub checks and positive/
four-diagnostic negative typing. All453 compiler inputs and62 installed members/
RECORD match; no schema change. Three existing captures are inspected.
Waste/decision: **reuse** the 170 initial staged passes and five native suites
while retrying three observer mistakes; **retain** all original failures.
**Remove** invalid MYPYPATH and assumptions about Python property calls, channel
visibility and automatic selection restoration. Verify fixture preconditions
and existing policies before composing consumer tests. **Merge** native/binding/
GUI compilation: one705.995s build, one install and one stub generation; observer
repairs reused the same native bytes. No full platform/science/speed campaign.
Plan change/next action: clipboard is complete within its declared scope; reuse
its [contract](annotation-clipboard.md) and evidence. Proceed to ROOT1, an actual
reported loss, through existing facts/import/storage authorities. Other editing
and image-settings gaps remain recorded; do not reopen completed mechanisms.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
