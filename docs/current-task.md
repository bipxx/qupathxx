# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ENTRYDETAILS1 is locally verified in its
[recorded scope](benchmarks/entry-details-r5/README.md).

Current: **R5-ENTRYEDIT1 — GUI authoring of entry metadata/details** (in progress).

- Outcome: a researcher edits short metadata, nullable description and ordered
  tags in the existing sidebar, with Apply/Cancel and saved/reopened exact state.
- Entry/evidence: cpp/gui.cpp currently exposes read-only entry fields; Project
  now provides owning getters and atomic set_image_metadata/set_image_details.
  Snapshot/schema24/import/SDK contracts already have scoped evidence.
- First action: inspect current ImGui text-edit/replay helpers and sidebar state.
  Design bounded transient drafts and separate native apply actions for metadata
  versus the atomic description/tag pair. Reuse the settled APIs; no public header,
  native model, persistence format or codec change for an already supported edit.
- Contract: preserve UTF8, None/empty and first tag order; duplicate-key feedback,
  clear/cancel/no-op and native refusal without partial live edits. Bind drafts
  to project/image; detect replacement/removal or changed baseline fields before
  stale application. Refresh existing dirty/catalog state on success.
- Reuse: current fixture/reference and installed entry-details product fb5d714d
  within unchanged native scope, project/store validators and inspector. Avoid
  repeating migrations/numerics/typing or generating stubs for a UI-only change.
- Checks/stop: actual add/edit/remove/clear/cancel/refusal and stale-context flows,
  metadata versus details namespace independence, exact save/reopen and affected
  package/UI lifetime behavior. One cached candidate after consumer design settles;
  record scoped evidence and waste handoff, keeping broader parity open.
- Cost boundary: expected UI-only compiler changes; one cached candidate and
  affected installed UI/package checks after drafts and interactions settle.
  No new capture, schema migration, stub generation or numerical campaign unless
  a demonstrated change invalidates the retained evidence. Record the reason
  before any repeat build/install/check.
- Scope stop: close when the declared authoring/save/reopen and stale-draft flows
  pass. Put newly discovered independent gaps back in R4/R5 rather than extending
  this task. Reconcile that remaining inventory once, then take the next ready
  gap or R6 context/halo replay.
- Dependencies: engineering can proceed now. Research labels, license and release
  support choices block only their claims. Only openly licensed readers/codecs
  are eligible. Global metadata/other R4/R5 and R6-R16 work remain in the queue.

Latest task review (2026-11-04, ENTRYDETAILS1 closeout):
Outcome/evidence:203 staged passes/seven GUI skips, four native roles covering
all23 older migrations, combined changed-metadata/description/tag UI save/reopen,
installed snapshot/separate budgets, five public-stub and positive/three-negative
typing checks pass. Nullable/empty text, first-occurrence tag order and actual
v1/v2 saved geometry routes retain exact state. All465 compiler inputs and65
installed hashed members match; combined screenshot inspected.
Waste/action: **merge** both attributes before one585.09s ABI/header build;
**reuse** captured builder/source and current Project/snapshot/store/inspector.
One entry-only supplement, one stub pass and one uv install; no new codec/model
or pixel/ROI/math recapture. Retain baseline count/glob failures and correct via
explicit directory/file checks; short semantic IDs prevent huge payload logs.
Next: reuse these now-settled native APIs for UI authoring, avoiding another
public-header/layout or binding change unless a concrete missing operation is
found. Scope remains retention/native-Python editing/read-only inspection until
ENTRYEDIT1's actual authoring flow passes; full R5/science/resources/release open.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.

Plan-maintenance review (2026-11-04): remaining R2-R16 families stay in the single
queue. Added explicit task scope/cost boundaries and a one-time R4/R5 handoff
reconciliation. Waste/action: reuse the existing queue, receipts and closeout
format; remove any need for a second backlog, cost ledger or review campaign.
Documentation-only change: check links/consistency/diff; no build or tests needed.
Next implementation remains ENTRYEDIT1 with the first action and stop above.

ENTRYEDIT1 inspection review: native getters/setters and existing replay helpers
are present. Two guessed paths (gui.hpp and build/entrydetails_r5_backend.py)
were absent; discover files with rg --files and use the actual build/ci helper.
No build was attempted. Waste/action: remove path guessing; implement local GUI
drafts through settled APIs, with no binding/schema change. Next: editor patch,
then one affected cached build and actual UI interactions.

ENTRYEDIT1 build checkpoint: product/observer committed at9abb5450; cached GUI
observer/app build is live in session28466. Compiler scope is GUI plus changed
provenance, with no public API or schema changes. A later guessed cpp/app.cpp
lookup also failed; use the CMake target source listing instead. This recurring
inspection waste requires changing the lookup method, not retrying guessed names.
Next: poll this same build, then run the existing GUI observer with actual ImGui
activation/text input; preserve any failure and diagnose before retry.

ENTRYEDIT1 attempt review: cached GUI observer/app build passed. First real-ImGui
observer passed metadata drafting, text input and duplicate refusal, then failed
its details apply assertion. Preserve build/entry-edit-observer-attempt1.log.
No passing status or package install is claimed. Waste/action: retain this focused
failure; inspect actual details and widget visibility before changing native code
or repeating the whole build. Next: add observer-only diagnostics and rebuild
only its changed target to distinguish input targeting from product behavior.

ENTRYEDIT1 diagnosis update: attempt2 shows exact multiline description persisted,
but two tags instead of three; popup has no vertical clipping (scroll max zero).
This rules out hidden Apply. Preserve attempt2; next inspect actual tag values
and selection/text-field activation before a targeted repair. Observer-only rebuild
was retained; no wheel/install/native contract changes.

ENTRYEDIT1 attempt3 diagnosis: retained tags are z-last/a-first; the newly added
empty row was overwritten by the still-active ImGui tag text cache, then native
first-occurrence deduplication correctly removed that duplicate. Fix the GUI
buffer rebind by clearing active text state when selecting/loading a draft row.
Native normalization remains unchanged. Waste/action: merge diagnostic values
into future first-failure output instead of successive observer-only probes.
Next: rebuild affected GUI/observer once for this concrete input-state repair.

ENTRYEDIT1 repair review: attempt4 passes all original actual ImGui checks,
including saved/reopened values, no-op dirty state, UTF8/multiline input,
metadata duplicates, null/empty/tags order, cancel, stale details, project/image
replacement and removed entries. Buffer rebind repair fixes the empty-tag loss.
Waste/action: retain attempts1-3; no native API/schema/model changes. Group final
tab-input support with explicit metadata-stale/clear and native aggregate-budget
refusal controls before final packaging; reuse existing SDK/store/numerical
results. Next: one frozen cached wheel/observer build, install once and inspect
installed UI. No new stubs or capture required.
