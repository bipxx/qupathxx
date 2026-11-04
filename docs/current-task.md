# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. SOURCEAXES1 has local Windows engineering evidence.

Current: **R4-CLIP1 — annotation copy/paste** (implementation in progress;
combined build and affected verification pending).

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
- Next action: review the settled native/binding/UI changes and independent
  contracts, then freeze source for one cached build. Run new and older affected
  contracts staged before installing; generate final stubs once and reuse the
  compiled product for desktop verification.
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

Latest closeout (2026-11-04, CLIP1 contract investigation):
Outcome/evidence: pinned clipboard/type-adapter behavior is inspected and the
bounded transport contract above is settled. Native wrappers now reuse the
existing decoder/exporter and atomic append; Python exports and ImGui consumers
are drafted. Verification is pending; no runtime completion is claimed.
Waste/decision: **reuse** GeoJSON authority, object remapping/history and saved
property/plane mechanisms; **merge** native/API/UI compilation into one build.
**Retain** existing loss refusals and strict file dimension checks. Avoid a
second clipboard model, decoder or per-frontend numerical suite. No build yet.
The old installed eb53f2bc product fails the new selection contract because
copy_objects is absent (build/object-clipboard-r4/baseline.xml); this expected
failure is retained. Reuse the installed baseline; do not reinstall to repeat it.
Plan change/next action: inspect the drafted changes/contracts, freeze source,
then run affected staged contracts, package/stub consistency and two actual
clipboard desktop flows. Other R families and M0-M7 remain open.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
