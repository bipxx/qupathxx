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
  metadata and measurement representations. The native implementation is built and its installed workflow checks are in closeout.
- Contract: bounded string metadata and binary64 measurements (canonical NaN,
  signed zero and infinities preserved); imported facts remain historical after
  edits. Unknown fields/locks/planes retain explicit refusal/loss. Ordinary edits
  preserve properties; explicit property clearing is undoable. Merge refuses
  nonempty historical properties rather than inventing aggregated measurements.
- Small failed edit-script review: source-closure helper expected a literal module
  path but the tools discover compiler files automatically. Keep that discovery;
  add only the new tests/typing dependencies. No build/check rerun is justified.
- Attempt closeout:3095e6f8 compiled successfully in953.27s; all five selected
  native suites pass. A pre-install import probe fails with `bad cast` because
  new dictionary defaults lack nanobind/stl/map.h. No product was installed.
  The build completed before a reviewed stop could target its old process;
  no process was stopped or source changed while it was live. Preserve the
  candidate wheel/probe and native results with their original source identity.
- Waste decision/next action: add the missing map caster and incrementally rebuild
  the binding/product using completed core objects. Reuse passing native results
  because core/header/test inputs are unchanged; verify final installed identity,
  model workflow, stubs/typing and desktop. Check STL caster includes beside new
  container arguments before expensive builds. No new reference/platform run.
- Latest attempt review: corrected0effd30d built in457.69s, one product install;
  stubs/typing and both desktop flows pass. Model724 passes/twelve declared skips;
  two descriptor-benchmark cases fail on a stale schema14 assertion. All new
  property cases pass. Reuse product/native/model results; remove the benchmark's
  duplicated version literal by reading the existing schema declaration, then
  rerun only those two cases. The first observer retry also matched the batch
  schema constant; anchor the exact name and preflight its single match before
  another run. Both failures are retained. No native input/build/install changes.
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
