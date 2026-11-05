# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-DATAPROP1 is locally verified in its recorded scope.

Current: **R5-TMASAVED1 — native saved-data TMA grids** (ready for contract preparation).

- Outcome: retain a recognized pinned saved-data TMA grid, its core identities,
  geometry, ordering, missing/case state and supported child hierarchy through
  native project import, existing Python/ImGui TMA use and saved reopen. Preserve
  explicit losses/refusals for unsupported representations; do not substitute
  annotation-only or image-only import for retained grid semantics.
- Entry/evidence: cpp/qupath_saved_data.cpp still emits tma_grid_dropped and its
  object decoder admits root/annotation/detection/cell classes, with no TMA-core
  retention route. Existing QPTMA0.7 fixture/probe covers text exchange, not native
  .qpdata. Reuse the accepted TMA native model, geometry, commands and store.
- Next action: inspect pinned hierarchy/grid/core serialization and current TMA
  creation/adoption APIs before choosing a decoder mapping. Capture one synthetic
  saved-data grid covering fractional geometry, missing/case labels and children,
  reading it back for reference. Do not recapture/reimplement the QPTMA codec.
- Dependencies/reuse: pinned runtime and project fixture source, existing stream/
  ROI/identity parsing, loss-aware import publication, TMA editor/batch/store and
  native ownership. Prefer existing TMA state to another document field or model.
  Record actual unsupported variants before freezing one coherent candidate.
- Checks/stop: independent core count/order/IDs/geometry/missing/case/child
  expectations; guarded preview and atomic refusal; actual inspect/use and
  save/reopen retain the same grid. Add only uncovered migration/ownership faults
  and affected consumers. No new detector, biological TMA acceptance, wide reader
  campaign or repeated text-codec qualification is implied. Other R4/R5 losses,
  source races, R6-R16 and owner research/license/release decisions remain open.

Latest closeout (2026-11-04, R5-DATAPROP1):
Outcome/evidence: [typed ImageData properties](benchmarks/image-data-properties-r5/README.md)
retains null/bool/exact signed64/binary64/UTF-8/list/map values through pinned
native import, Python deep copies, history, immutable snapshots, schema21 and
ImGui inspection/save/reopen. Unknown/cyclic/non-string-map Java values require
explicit whole-property loss review; malformed/oversize payloads refuse.
Final nativecb693c5a:40 new headless cases and one actual desktop pass, four
native suites, five packaged-stub checks, positive/three-diagnostic negative
typing and installed typed-import smoke.258 unchanged legacy passes/13 skips
retain source3d272074 scope; reconciled headless union298 passes/14 declared skips.
All461 compiler inputs and65 hashed installed wheel members match. Capture read.
Waste/action: **reuse** parser/list, UTF-8/SHA, native factory/history/snapshot/
store authorities and cache. Fact maps cannot preserve these value types; keep
one bounded typed model/codec shared by consumers. **Retain** baseline and two
initial bad-cast failures. **Fix** checked integer conversion instead of weakening
assertions. One679.95s build plus34.28s binding-only repair; one stub generation
and one final install. Only the new property module and current native roles were
rechecked after that compiler change; unchanged legacy evidence was reconciled.
Plan change/next: DATAPROP1 closes in local declared scope. Prepare TMASAVED1
above from an actual remaining decoder loss. Reuse existing TMA state to avoid
another private-field/header-wide build when the current model suffices. No
full qualification, overall speed claim or milestone/release completion follows.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
