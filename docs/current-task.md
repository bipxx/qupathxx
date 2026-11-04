# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. EDITSTATE1 has local Windows engineering evidence.

Current: **R5-PLANE1 — source/view-aware object planes**.

- Outcome: support non-default planes through import, the matching source/view,
  object selection/analysis, save/reopen and export. Plane labels alone are not
  completion; both importers currently reject non-default channel/Z/T values.
- Entry/dependency: trace TiffView/reader axes, project/view switching, ROI/hierarchy
  and spatial queries, render/picking filters and shared analysis request capture.
  Reuse eligible multi-plane TIFF/OME fixtures and independent pixel expectations.
  Check actual source axis bounds; the existing saved-plane refusal fixture must
  not become a positive merely by deleting its guard.
- Next action: settle native plane/series identity and default behavior, hierarchy
  rules, view/selection eligibility, source generation/stale publication and
  backwards storage/API contract. Pull only required R2/R3 work forward.
- Checks/reuse: one actual import/view/select/analyze/save/reopen/export workflow;
  exact plane pixels/coordinates/calibration, absent/out-of-range refusal, stale
  plane/result rejection and legacy default-plane replay. Reuse existing native
  request, job, source guards, immutable facts and snapshot authorities. Batch
  interfaces/casters/storage before one cached integration build/install.
- Stop: the declared non-default plane workflow works through native/Python/UI
  with unsupported cases explicit. Other R5 versions/settings, remaining R4,
  science/resource/release and R6-R16 requirements stay in the queue.

Latest closeout (2026-11-04, EDITSTATE1):
Outcome/evidence: [editing state](benchmarks/object-edit-state-r5/README.md) has27 new
model passes/four desktop cases,764 selected passes/sixteen skips, seven native
suites and seven desktop flows. Stubs/typing pass. Source585d6ba2/observer9e72413b
bind451 compiler inputs and62 installed members/RECORD. One final install.
Waste/cost: **reuse** public API/UI compilation after a real compact-history budget
regression; **remove** unconditional property pointers from class-only rows; use
sparse owner changes. Old64KiB/1000-object case now passes. First build681.98s,
private correction56.56s. **Retain** wrong-interpreter and staged regression logs;
use a dedicated interpreter with asserted source for probes. Facts share maps,
bindings are localized and consumers explicitly include the property definition.
No repeated healthy runtime cohort or full platform campaign was needed.
**Remove** duplicated completed-workflow counts from the active plan; keep only
reuse links, dependencies and the next steps there.
Plan change/next action: source/view-aware planes are the next actual import gap.
The CSV column union is now bounded at4096 before row materialization; wider R12
export memory/latency remains open. No general performance claim follows.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
