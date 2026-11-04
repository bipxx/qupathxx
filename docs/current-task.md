# Current implementation task

Use the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is accepted; R1 inventory is reconciled with owner decisions open. Recent R2,
R4-MERGE1 and R5-QPPROJ1/QPDATA1/RELINK1 have local evidence. All M0-M7 remain Partial.

Current: **R5-SOURCE1, persist and review source verification**, implementation in progress.

- Outcome: fingerprint an image using the existing worker; retain the association
  with its project entry; save/reopen; compare current bytes explicitly and review
  changed/missing/moved sources before replacement. Structural relink alone still
  admits same-sized different pixels; this is the remaining concrete gap.
- Entry/dependency: inspect ReaderSource, FileFingerprint/SourceFileAssociation,
  existing JSON codecs, project schema/migrations, native job publication and UI
  fingerprint controls. Settle additive API/storage/replacement semantics before
  coding. No biological or release decision blocks this engineering contract.
- Current contract: native opaque review holds a compatible candidate and a
  comparison against the saved baseline. Apply rechecks input versions/current
  image generation and requires explicit acceptance for changed bytes. Schema15
  retains checksummed immutable associations; old schemas migrate transactionally.
  Ordinary opening stays lazy. Public additions and UI/schema assertions are
  batched before the first cached build; stubs will come from that exact product.
- Failed attempt closeout: a1d812a8 cached build stopped with nanobind enum
  borrowing errors after668.13 seconds. The wrapper cannot be extended like a
  class wrapper; register values in its existing declaration. No product installed.
  Retry also corrects changed companion/observed-source reporting and reuses a
  stable live reader for verification, avoiding a duplicate decode and no-op dirty
  state. Stable current-reader verification also preserves its generation; unchanged
  rechecks avoid dirty/autosave churn. Reuse completed objects; one corrected
  build/install remains, with original
  failures/logs retained. No platform or reader campaign is justified.
- Current checks: corrected native a66c8af5 built successfully in546.58 seconds;
  one final wheel install and byte-preserving stub repackaging are complete.
  Model687 passes/ten declared skips; stubs and positive/three-negative typing pass.
  Four native suites pass. The project suite's final test call forgot explicit
  acceptance for an observed-source change. Correct that test caller only, rebuild
  its executable against retained core/package bytes, and rerun only that suite.
  Keep the failed XML and separate product/observer source identities. Desktop is
  running on the installed product; no product rebuild/reinstall is required.
- Desktop failure closeout: two apply/accept cases time out because the new
  modal inherits the editor's disabled scope; the review-only case passes. Move
  popup creation/rendering outside that scope beside existing dialogs and use the
  shared modal sizing helper. This changes GUI only. Preserve687 model passes,
  typing/stubs and corrected native observer evidence; relink a corrected product,
  then rerun the changed desktop flow and source/build identity checks only.
- Reuse: immutable native source/fingerprint facts, existing worker/cancellation,
  SQLite snapshots and verified relink handoff. Keep ordinary open lazy; distinguish
  primary-file proof, metadata association and compound-source coverage. Historical
  associations never establish new proof of already-decoded pixels.
- Checks: save/reopen exact association; byte-changed, moved/missing and compound
  cases; explicit replacement and old-analysis provenance; stale/cancel/fault
  preservation; compatible migration and actual Python/UI workflow. Batch settled
  changes into one cached build/install and record elapsed time in its wrapper.
- Stop: the declared verification/review/save workflow is usable. Remaining source
  policies, recovery faults, properties and planes stay in the same R5 inventory.
  Avoid duplicate per-frontend hashing, a new scheduler or a provenance framework.

Then select uncovered R4/R5 work, followed by R6/R7, R8-R10, R11, R3, R12 and R13;
pull required eligible R2 inputs forward and retain R14-R16 parity.

Latest closeout (2026-11-04, R5-RELINK1):
Outcome/evidence: [verified-reader handoff](benchmarks/project-relink-r5/README.md)
has four new model cases and two observer controls within226 passes/four declared
skips; three native suites and two existing successful/failed desktop relink flows
pass. Source26b4f228 binds442 compiler inputs and all62 installed members/RECORD.
Waste/cost: removed the discarded candidate/second open and merged compatibility/
source publication. One cached build/install; no shared public-header/binding edit,
stub regeneration/repackaging, fixture capture or passing-check replay. Existing
UI relink tests cover the changed behavior. Build elapsed remains unknown.
Plan change/next action: extend the existing fingerprint association into project
persistence/review rather than adding another hashing mechanism. Record future
command elapsed in existing wrappers; no separate timing campaign is necessary.

Perform the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
after each task or failed attempt; update this card before continuing/retrying.
Only openly licensed readers/codecs are eligible. Source308 retains its own
platform scope. Biology, hardware, license and release support remain owner choices.
