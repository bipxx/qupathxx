# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. SOURCEAXES1 has local Windows engineering evidence.

Current: **R4-CLIP1 — annotation copy/paste** (affected checks passed;
evidence and plan handoff pending).

- Outcome: copy a declared annotation selection and paste it through shared
  native/Python/ImGui commands, with correct fresh IDs/hierarchy, intended state,
  selection, one undo/redo transaction and saved reopen behavior.
- Evidence/entry: core.hpp/document_edits.cpp provide ROI/object commands,
  immutable facts and append_objects remapping; gui.cpp has drawing/edit/merge
  but no annotation clipboard flow. Reuse those authorities and existing nested
  geometry fixtures. Do not create another UI object model or decoder.
- Investigation completed: pinned QuPath v0.7.0
  [clipboard commands](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-gui-fx/src/main/java/qupath/lib/gui/commands/Commands.java)
  use GeoJSON text and separate ordinary/current-plane paste. The pinned
  [type adapter](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/io/QuPathTypeAdapters.java)
  distinguishes flat and hierarchy serialization. Selection-only is the default;
  descendants are explicit. Do not repeat that investigation.
- Next action: verify retained source/package identity and archive the one
  workflow evidence summary. Preserve failed attempts, record the waste-driven
  handoff, then take the demonstrated R5 root-properties gap. Reuse all healthy
  checks; no additional build/install/stub generation is needed.
- Contract: level-zero image-pixel coordinates, no automatic physical rescaling,
  target calibration unchanged. Clipboard source dimensions can differ when
  actual geometry fits; ordinary file-import dimension checks remain strict.
  Ordinary paste preserves planes; explicit displayed-plane paste overrides them.
  Copy requires nonempty IDs; optional descendants are deduplicated. Selected
  parent links are retained; omitted parents detach with an informational report.
  Existing unsupported-field/ellipse losses require explicit permission.
  Limits are 100,000 objects and 16 MiB text by default (configurable to 256 MiB).
  Preserve properties/classes/nuclei/supported state and refuse before mutation.
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

Latest closeout (2026-11-04, CLIP1 installed attempt):
Outcome/evidence: five packaged-stub contracts pass; first installed typing and
desktop attempts are retained under build/object-clipboard-r4. Mypy refused an
explicit site-packages MYPYPATH before checking code. Desktop copy/paste counters
succeed; final selection after undo/redo is empty by the existing selection
policy (test_selection.py). The plane fixture used hidden channel1, so Select All
correctly selected no objects and copying refused.
Waste/decision: **remove** the invalid MYPYPATH override; **reuse** five native
suites, 173 distinct staged passes, packaged-stub passes, final stubs and the
single installed product. **Retain** failures. **Split** the desktop assertions:
check selected pasted IDs before history, then actual history/save/reopen; make
source channel visibility an explicit fixture precondition. No native rebuild.
Plan change/next action: typing-retry and both desktop-retry flows now pass.
Selection is checked immediately after paste; history changes revision twice and
save/reopen retains both objects and state. The source channel is explicitly
visible before copying. Captures are inspected. Keep the existing selection
policy; finalize package/source evidence and the next R5 root-properties card.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
