# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-DATAPROP1 is locally verified in its recorded scope.

Current: **R5-TMASAVED1 — native saved-data TMA grids** (contract settled; capture/implementation preparation).

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

Latest task review (2026-11-04, TMASAVED1 contract/capture/baseline):
Outcome/evidence: corrected capture succeeds with eight grid/membership variants,
including reversed hierarchy order, duplicate names, legacy cases and children.
Fixture/probe hashes pass. Previous installed product reproduces unsupported
TMA-core failure. All affected public cases collect; the new module has21 cases.
Decoder/import drafts use one grid layout and map UUIDs to existing native IDs,
then attach metadata with one staged edit inside the private preview.
Waste/action: **reuse** native TMA state/commands/store and shared stream/list/
ROI/fact/plane parsing. **Remove** nested ternaries next to Groovy list literals;
two failed parse probes are retained before the successful conditional form.
Use zero-prefixed decimals. No new public API, document field or schema was
needed; keep existing stubs and avoid broad binding recompilation or a detector/
text-codec/scientific campaign. Preserve the failing installed baseline.
Next: freeze four affected compiler files for one cached build; run new TMA and
shared import regressions, four native roles and one actual TMA export/save flow.
Then install once, check unchanged packaged stubs/RECORD and close only this route.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
