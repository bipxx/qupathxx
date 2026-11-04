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
- Next action: source corrections are batched and ready for a cached correction
  build: explicit/parent creation planes, tissue/RGB/raw/streamed publication,
  lazy unchanged-plane edits, mixed-plane 2D distance refusal, loss-aware QPTMA
  and visible-only automatic import selection. A changed-input staged recheck
  passed three repaired probe cases and confirmed the actual streamed-plane bug
  in4265ff15. The derived saved import explicitly acknowledges its existing
  saved-calibration override loss. Probe the correction before packaging, then
  generate stubs from the exact product and install once; do not reinstall the
  first candidate or repeat unchanged migration-observer controls.
- Checks/reuse: one actual import/view/select/analyze/save/reopen/export workflow;
  exact plane pixels/coordinates/calibration, absent/out-of-range refusal, stale
  plane/result rejection and legacy default-plane replay. Reuse existing native
  request, job, source guards, immutable facts and snapshot authorities. Batch
  interfaces/casters/storage before one cached integration build/install.
- Stop: the declared non-default plane workflow works through native/Python/UI
  with unsupported cases explicit. Other R5 versions/settings, remaining R4,
  science/resource/release and R6-R16 requirements stay in the queue.

Latest attempt review (2026-11-04, PLANE1 first staged candidate):
Outcome/evidence: build4265ff15 succeeded; staged probe41 passes/nine failures/
one desktop skip. Raw results are retained under root build/object-planes-r5;
this is an incomplete workflow, not engineering acceptance. Migration observer
controls passed60 cases before the build; their unchanged tool-only scope stands.
Waste/cost: **retain** the first build/probe failures. **Merge** the source fixes
identified during publication review into one correction before packaging.
The791.53s build began before all object-producing consumers were reviewed;
review those consumers before freezing future cross-cutting object APIs.
**Remove** the false assertion that an explicitly selected raw request becomes
stale merely because the display changed; retain object/source-change rejection.
**Reuse** the cache and dedicated interpreter; no candidate installation or stub
pass was spent on this incomplete product. Earlier truncated/guessed-path reads
were unproductive; use discovered paths and bounded UTF-8 reads.
Plan change/next action: apply the batched correction above; keep PLANE1 open
through the final native/Python/desktop/save/reopen/export workflow. Preserve
prior native EDITSTATE1 evidence at its original source/product scope.

After each task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
