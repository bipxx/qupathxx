# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-CALIB1 is locally verified in its
[recorded scope](benchmarks/axis-calibration-r5/README.md).

Current: **R5-ENTRYMETA1 — QuPath project-entry metadata** (contract inspection ready).

- Outcome: retain supported per-image project metadata independently of native
  ImageData properties and historical root/ROI facts, with exact save/reopen.
- Entry/evidence: cpp/qupath_project.cpp reports image_metadata_dropped for
  nonempty metadata/tags/description; cpp/include/qupathxx/project.hpp defines
  ProjectImage, with existing project listing/snapshot/save/restore consumers.
- First action: inspect those existing entry fields/APIs and the pinned QuPath
  metadata-map representation. Check current fixtures before capturing missing
  metadata variants. Keep unsupported tags/description or other representations
  explicitly tracked rather than claiming the whole metadata family complete.
- Contract: settle scope/type/UTF8/size limits, owning copies, update semantics,
  project-versus-document lifecycle, snapshot accounting, old defaults and
  malformed/source-stale atomic refusal. Use Project as authority; no duplicate
  ImageData/root-fact encoding or UI model.
- Reuse: existing project manifest/source binding, ProjectImage/snapshots/store,
  loss review and inspector/Python consumers. CALIB1 and prior native/assisted
  state receipts retain their original source and scope. No reader/math recapture.
- Checks/stop: independent exact metadata values and source-ID binding, immutable
  listing/capture, failed apply/save preservation and round-trip, actual affected
  Python/desktop inspection. Settle interfaces before one cached candidate and
  one necessary stub/install pass. Close only this declared entry-map workflow.
- Dependencies: contract/engineering can proceed now. Research labels, project
  license and release support choices block only their affected claims. Only
  openly licensed readers and required codecs are eligible.

Latest task review (2026-11-04, CALIB1 closeout):
Outcome/evidence: saved Z/time state retains all seven units, offsets/irregular/
nonmonotonic times, partial prefixes and explicit unknown values. First product
has254 unique staged passes/12 skips after two observer repairs. Final decoder
cohort117 passes/five skips, six native roles, actual desktop, installed snapshot/
upgrade plus two coordinate/missing-source controls, five public-stub checks and
positive/three-negative typing pass. Receipt separates original/repaired source;
all465 compiler inputs and65 installed hashed members match. Screenshot inspected.
Waste/action: **reuse** seven successful captures and unchanged component checks;
**merge** all state at Document/current project/import APIs. One ABI/header build
687.96s and one targeted missing-unit/witness repair41.08s; one stub pass and one
uv install. Failed NaN capture, newline edit, builder-substitution reference,
missing-unit admission and guessed observer APIs remain recorded. Use exact
symbol discovery and literal normalized edits; no full-corpus replay/campaign,
parallel state codec or rebuild for stubs/observer limits. Stable provenance
keeps source acquisition and historical recipe meanings unchanged.
Next: inspect ENTRYMETA1's actual ProjectImage and upstream metadata contract;
reuse accepted axis/state routes without widening their claims. All broader
R5, scientific/resource, host support and release requirements remain queued.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
