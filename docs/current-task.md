# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R4-CLIP1 is locally verified in its recorded scope.

Current: **R5-ROOT1 — saved-data root metadata/measurements** (contract settled; implementation in progress).

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
- Investigation: a new synthetic pinned v0.7.0 capture independently records
  root metadata, finite/NaN/infinite/signed-zero measurements and distinct child
  facts. Cached PathObject/PathRootObject sources match the existing pinned
  hashes; root serialization has no ROI. No reader/runtime download is needed.
- Contract: Document.image_properties exposes immutable ObjectProperties maps;
  ROI lock/color/probability flags are invalid at image level. Empty facts become
  None. Set/clear is one ordinary history transaction, does not change object IDs,
  selection, ROI properties, calibration or live computed measurement values.
  Schema19 adds one sparse checksummed image_properties row per image; legacy
  1-18 projects default empty and migrate in the existing save transaction.
  Save snapshots share owning immutable facts and charge their bounded payload.
- Next action: extend existing staging/history, preview decoder, snapshot/store,
  thin binding and historical inspector together. Settle consumers/tests before
  one combined cached build. Broader ImageData settings retain explicit losses.
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

Latest closeout (2026-11-04, ROOT1 contract/capture):
Outcome/evidence: the pinned synthetic root capture independently retains Unicode
metadata, finite/NaN/infinite/signed-zero measurements and distinct child facts.
Cached serialization sources match the pinned hashes. The installed dd33c2b1
product reproduces root_properties_dropped (build/root-facts-r5/baseline.xml).
The shared native/history/import/store/binding/inspector changes and contracts
are drafted; runtime verification is pending.
Waste/decision: **reuse** the pinned runtime/source/corpus, immutable facts codec,
ordinary staged history and save snapshot authority. **Remove** a second facts
model/decoder and unnecessary ROI/render invalidation for image-only edits.
**Merge** schema/bindings/GUI into one build. Current-format expectations are
centralized independently in tests/project_schema_contract.py; legacy fixtures
remove the new table before downgrading. Only affected migration functions plus
actual changed workflow contracts are selected, not unrelated numerical cohorts.
Plan change/next action: freeze the settled compiler inputs for one cached build;
run ROOT1 and new/older affected contracts staged before installation. Preserve
this baseline failure and retry only diagnosed failures. Then generate stubs once,
install once, verify typing and inspect the real historical image/ROI facts UI.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
