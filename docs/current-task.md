# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. SOURCEAXES1 has local Windows engineering evidence.

Current: **R4-CLIP1 — annotation copy/paste** (contract preparation in progress;
implementation and verification pending).

- Outcome: copy a declared annotation selection and paste it through shared
  native/Python/ImGui commands, with correct fresh IDs/hierarchy, intended state,
  selection, one undo/redo transaction and saved reopen behavior.
- Evidence/entry: core.hpp/document_edits.cpp provide ROI/object commands,
  immutable facts and append_objects remapping; gui.cpp has drawing/edit/merge
  but no annotation clipboard flow. Reuse those authorities and existing nested
  geometry fixtures. Do not create another UI object model or decoder.
- Investigation completed: pinned QuPath v0.7.0
  [clipboard commands](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-gui-fx/src/main/java/qupath/lib/gui/commands/Commands.java)
  use GeoJSON text and separate ordinary/current-plane paste. Reuse this finding;
  descendant serialization still needs a targeted adapter check before freeze.
- Next action: settle selection/descendant and supported-state contracts, then
  implement bounded native copy/paste wrappers around the existing GeoJSON
  authority and atomic append. Keep file-import dimension checks; clipboard
  cross-image paste needs its own explicit coordinate/bounds policy.
- Proposed contract to freeze: level-zero image-pixel coordinates, no automatic
  physical rescaling, target calibration unchanged; ordinary paste preserves
  planes, displayed-plane paste overrides them. Fresh IDs and selected parent
  links use append remapping; excluded parents and unsupported metadata have
  explicit reports/loss permission. Declare byte/object limits and refusal before
  mutation. Preserve properties, classes, nucleus and supported object state.
- Consumer route: expose thin nanobind/Python exports and system-clipboard ImGui
  actions through the same native functions. Keep keyboard shortcuts out of text
  editing, synchronize visible selection, and use one paste history transaction.
  Settle native declarations, casters and all consumers before building.
- Checks: independent nested/overlapping geometry, source snapshot independence,
  fresh IDs/parent mapping, properties/nucleus/plane handling as applicable,
  refusal before mutation, compound history and save/reopen. One actual desktop
  copy/paste flow and installed Python use the same native authority.
- Build/reuse: combine settled interfaces/casters/UI changes before one cached
  integration build. Run new and older affected contracts staged before install.
  Configure native vector properties by assigning a prepared Python list once;
  do not mutate a returned copy. Reuse valid evidence in its actual scope.
- Stop: the declared clipboard workflow is usable and verified, with unsupported
  cases explicit. Other R4/R5 and R6-R16 requirements remain in the queue.

Latest closeout (2026-11-04, remaining-plan update):
Outcome/evidence: the R0-R16 queue still covers the full remaining target; all
M0-M7 remain Partial. SOURCEAXES1 remains closed within its
[recorded scope](benchmarks/source-plane-replacement-r5/README.md).
CLIP1 investigation is recorded above; no clipboard implementation is claimed.
Waste/decision: **merge** duplicated task-handoff instructions into one closeout
procedure; **reuse** completed investigation and prior evidence; **remove** full
build/test work for this documentation-only update. Broad file reads produced
truncated output; use bounded section reads for subsequent edits. No build,
install, stub generation or qualification was needed.
Plan change/next action: finish the CLIP1 descendant/contract decision, implement
the shared native/Python/ImGui flow, then run only the listed affected checks.
After CLIP1, take the first ready slice in the existing queue; decisions block
only their consumers. No second backlog or per-task review campaign is added.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
