# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. SOURCEAXES1 has local Windows engineering evidence.

Current: **R4-CLIP1 — annotation copy/paste** (ready).

- Outcome: copy a declared annotation selection and paste it through shared
  native/Python/ImGui commands, with correct fresh IDs/hierarchy, intended state,
  selection, one undo/redo transaction and saved reopen behavior.
- Evidence/entry: core.hpp/document_edits.cpp provide ROI/object commands,
  immutable facts and append_objects remapping; gui.cpp has drawing/edit/merge
  but no annotation clipboard flow. Reuse those authorities and existing nested
  geometry fixtures. Do not create another UI object model or decoder.
- Next action: inspect the pinned upstream clipboard behavior for selection/
  descendants, classes/state and planes, then settle native snapshot/transport,
  coordinates/calibration, paste-plane and unsupported-case contracts. Inspect
  every producer/consumer before freezing an additive API.
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

Latest closeout (2026-11-04, SOURCEAXES1):
Outcome/evidence: [retained source planes](benchmarks/source-plane-replacement-r5/README.md)
closes publication through relink/review, current plane/undo races, compatible
reductions, reader handoff and saved replay. Native sourceeb53f2bc, observer1b359d96:
93 staged passes/eleven skips,15 new headless passes, three native suites and two
actual desktop flows. The one changed compiler input is project.cpp;452 source
inputs and62 installed members/RECORD are checked. Public API/schema stay unchanged.
Waste/cost: **reuse** staged contracts, installed product, stubs/typing and passed
native/refusal checks. One37.37s cached build, one install, zero stub generations.
**Retain** six baseline failures and three failed desktop attempts. A Python copy
of GuiOptions.inputs swallowed save actions; construct the list and assign once.
Inspect ownership before timing retries. Separate GUI viewport dirtiness from
source publication; headless refusal still checks complete unchanged state.
Plan change/next action: proceed to CLIP1, the actual missing editing flow above;
source/plane/recovery mechanisms remain closed within their recorded scope.
Previously blocked deletion of extracted wheel copies was not retried.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
