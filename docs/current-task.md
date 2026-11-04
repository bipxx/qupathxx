# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-PROP1 has scoped local engineering evidence.

Current: **R5-EDITSTATE1 — retain imported object editing state**.

- Outcome: preserve supported per-object lock, color and class-probability facts
  through import, inspection/editing, undo, save/reopen and export. These remain
  explicit saved-data loss codes; metadata/measurements are now retained.
- Entry/dependency: inspect `qupath_saved_data.cpp`, `interchange.cpp`, native
  ObjectProperties, object commands and overlay/UI authorities. Reuse pinned
  synthetic QuPath0.7.0 output. Check upstream lock/edit rules before enforcement;
  do not assume an ROI lock prohibits every scripted/classification action.
- Next action: settle typed fields, absence/default semantics and backwards codec/
  storage behavior. Define actual lock/color effects in the existing UI/commands
  and a usable explicit edit/unlock/clear path. Batch this related public API before
  one build; keep non-default planes in their separate display/source contract.
- Reuse/checks: immutable property owners, atomic history and snapshot/storage
  authorities from PROP1. Check actual imported fields and remaining loss reports;
  native editing/undo behavior, save/reopen/export, typing/stubs and changed UI.
  Reuse property fixture/kernel checks when their inputs remain unchanged.
- Waste control: include STL casters with new bound container types. At the next
  already-required header change, consider forward-declaring ObjectProperties in
  core.hpp and including its definition only in actual consumers, reducing future
  property-header rebuild coupling. No standalone refactor/rebuild campaign.
- Stop: the declared editing-state workflow works, with unsupported behavior
  explicit. Remaining source races, planes/versions, R4 and R6-R16 stay in the queue.

Latest closeout (2026-11-04, PROP1):
Outcome/evidence: [retained object properties](benchmarks/object-properties-r5/README.md)
has25 new model passes plus the desktop case,726 distinct selected model passes/
twelve skips, five native suites, two desktop flows and passing stubs/typing.
Product0effd30d, native-test3095e6f8 and observerac6b3089 remain distinct; all449
compiler inputs and62 installed members/RECORD verify. Schema16 migration and
historical-value semantics are explicit. One final product installation.
Waste/cost: **retain** first build953.27s/import failure and corrected457.69s build;
**reuse** completed core/native tests; **remove** full model/platform replay and
benchmark schema literals. Rerun only two benchmark cases after fixing the stale
literal and exact-name match. Verify actual package members without guessed
counts. No build/install for observer edits; no overall speed claim.
Plan change/next action: finish related missing editor-state fields using the
existing shared property/command path; settle container casters/contracts before
another expensive integration build. Whole-export resource bounds remain R12.

After every task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
