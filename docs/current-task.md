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
- State: native plane model, import/export, sparse schema18 storage, view/selection
  filtering and shared RGB/raw analysis guards are implemented but unverified.
  Independent multi-plane, invalid-axis, hierarchy, raw-mask, snapshot/restart and
  desktop workflow cases are prepared. The original invalid saved-plane fixture
  remains a refusal; a clearly derived valid source/metadata case is separate.
- Next action: all three desktop flows pass on source774abdc4, including real
  ImGui plane import/view/measurement and managed hidden-plane merge. The parser
  exception correction is ready as one private interchange.cpp change; bindings,
  storage, kernels and generated stubs are unchanged. Build it using the same
  cache, run the one failed compatibility assertion on the staged new product
  before a second install, and rerun the affected native interchange suite.
  Preserve all prior passing results at their tested source/dependency scope;
  no repeated stub generation, typing campaign or full model/desktop cohort.
- Checks/reuse: one actual import/view/select/analyze/save/reopen/export workflow;
  exact plane pixels/coordinates/calibration, absent/out-of-range refusal, stale
  plane/result rejection and legacy default-plane replay. Reuse existing native
  request, job, source guards, immutable facts and snapshot authorities. Batch
  interfaces/casters/storage before one cached integration build/install.
- Stop: the declared non-default plane workflow works through native/Python/UI
  with unsupported cases explicit. Other R5 versions/settings, remaining R4,
  science/resource/release and R6-R16 requirements stay in the queue.

Latest attempt review (2026-11-04, PLANE1 broader installed checks):
Outcome/evidence: corrected source774abdc4 passes50 staged headless cases, nine
native suites and stub/typing checks. Installed model.xml retains921 passes,
one failure and24 declared skips. The failure is an external-input exception
compatibility regression, not a reason to weaken the existing ValueError test.
Waste/cost: **retain** both builds/probes and this failure. The second build took
724.83s. **Merge** any actual desktop correction with the parser fix before the
next build; **reuse** passing numerical/ownership/persistence and typing results
where their dependency inputs are unchanged. First packaging already installed
this candidate once, so a correction will require another install. Run broader
contract regressions on the staged product before installation in future shared
API work; a new-feature-only probe did not cover this older rejection contract.
**Remove** repeated healthy cohorts and repeat stub generation: this private
exception fix changes no binding signatures. Preserve the first review's lesson
to inspect all object-producing consumers before freezing a shared object API.
Plan change/next action: finish pending desktop checks, localize the correction,
then verify the changed exception on the corrected product and close this slice
only with its actual frontend/persistence evidence and updated next action.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
