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

Latest closeout (2026-11-04, remaining-work plan update):
Outcome/evidence: consolidated the execution sequence in the
[delivery plan](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and made start/implement/verify/waste-review/update handoffs explicit. No native
code or product changed. Review of changed links and diff is the applicable check.
The latest native delivery remains [EDITSTATE1](benchmarks/object-edit-state-r5/README.md);
its source/product and validation scope remain unchanged.
Waste/cost: **remove** the duplicate next-actions table; **reuse** existing workflow
evidence and the installed product within its original scope; **retain** affected
correctness checks. No build, install or platform campaign is needed for this edit.
No new cost measurement was made.
Plan change/next action: continue R5-PLANE1 with the card above. After that outcome,
choose the first demonstrated remaining R4/R5 gap, then follow the ordered slices.
Update this card and the affected queue entry after each task or failed/stopped
attempt, applying the waste decision before the next task or retry.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
