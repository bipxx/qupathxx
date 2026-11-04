# Current implementation task

Use the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue),
[ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial; SOURCE1 is locally verified.

Current: **R5-PROP1 — imported object metadata and measurements**.

- Outcome: import supported QuPath object metadata/measurement facts, inspect them,
  save/reopen and export without the current explicit property loss.
- Entry/dependency: `cpp/interchange.cpp` drops foreign properties;
  `cpp/qupath_saved_data.cpp` reports metadata/measurements dropped; `Annotation`
  currently has no representation. Trace object commands/history, snapshots,
  project codecs and table/export consumers. Reuse existing pinned synthetic
  QuPath0.7.0 data and GeoJSON fixtures; no new reference capture by default.
- Implementation status: immutable ObjectProperties, atomic property commands,
  history/snapshot charges, schema16 table/codec, GeoJSON/CSV and selected-object
  inspection are being wired together. Saved-data decoding retains the pinned
  metadata and measurement representations. No native build/install has run yet.
- Contract: bounded string metadata and binary64 measurements (canonical NaN,
  signed zero and infinities preserved); imported facts remain historical after
  edits. Unknown fields/locks/planes retain explicit refusal/loss. Ordinary edits
  preserve properties; explicit property clearing is undoable. Merge refuses
  nonempty historical properties rather than inventing aggregated measurements.
- Small failed edit-script review: source-closure helper expected a literal module
  path but the tools discover compiler files automatically. Keep that discovery;
  add only the new tests/typing dependencies. No build/check rerun is justified.
- Next action: freeze the settled compiler inputs for one cached build/install,
  generate stubs from that exact product, then run affected native/model/typing
  and desktop inspection/save checks. Baseline checks on SOURCE1 fail as expected
  because ObjectProperties/import retention are absent. The contract covers
  including supported values, numeric/nonfinite policy, immutable ownership,
  limits, duplicate keys and atomic import behavior. Then implement the shared
  authority and affected Python/ImGui/file routes together.
- Checks: independent exact fixture properties; lossless supported roundtrip,
  undo/redo, project migration/rollback and retained snapshot ownership. Preserve
  unsupported locks/planes/custom payload refusal or explicit loss reports.
  Settle native interfaces before one cached integration build/install; generate
  stubs once. Exercise changed installed import/inspect/save/export workflow.
- Stop: declared supported metadata/measurement workflow survives save/reopen and
  exchange with precise loss reports for everything outside its contract.
  Wider properties/locks/planes and broader R5 remain open.

Latest closeout (2026-11-04, recovery-gap reconciliation):
Outcome/evidence: [storage reconciliation](project-storage.md#recovery-gap-reconciliation-2026-11-04)
links retained passing hot-journal, abnormal-exit, whole-snapshot, cleanup lock/
retry and killed-native-autosave cases. Current assertions were inspected.
Version-2 cross-session discovery/rescue already exists. Active stale gaps fixed.
Waste/cost: **remove** another generic crash/cleanup campaign; **reuse** retained
results within original source scope; **retain** unknown debris and lock inodes
for their documented safety policy. XML search initially dumped giant single
lines; **replace** content search with filenames plus parsed named cases. No build,
installation or test replay was necessary. No hardware power-loss claim follows.
Plan change/next action: implement the demonstrated property-loss workflow using
existing native parsers/commands/storage, starting with the contract above.

Then finish demonstrated R4/R5 gaps, R6/R7, R8-R10, R11, R3, R12 and R13;
pull eligible R2 inputs forward, and retain R14-R16 full-target scope.
After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
