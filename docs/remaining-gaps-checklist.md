# Remaining gaps checklist

Reviewed against development source `981086cd` (2026-11-08). This is the
selection index for remaining work, not another acceptance ledger. Detailed
checks and evidence stay in the linked workflow records. Update the selected row
and [current task](current-task.md) after each task; do not replay completed work.

## Completed baseline

- [x] Native application/workflow consolidation: **NC01-NC28, 28/28 complete**;
  C++ owns shipped logic and Python controls native services.
  [Acceptance](consolidation-coverage.md), [boundary review](consolidation-python-boundary.md).
- [x] Selected density, neighborhoods, signed distances, project-density/resume,
  NuCLS import and training-cohort consumers are delivered. Their broader
  inventory rows do not mean these workflows need implementing again.
- [x] Latest contour optimization: exact masks, geometry and measurements for
  2,237 cells; 596 topology/budget patterns; observed matched local detector
  median341.3 ->302.9ms (**11.2% reduction**).
  [Evidence](benchmarks/contour-optimization/README.md).
- [x] Windows candidate5 publication and its12 packaging checks are complete.
  [Frozen release coverage](windows-candidate5-coverage.md).

Development runtime source tree:
`2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e`
(568 inputs). Published candidate5 is older; its qualification does not transfer
to this development runtime.

## Priority gaps to select from

**G01 complete; G04 locally verified, publication pending; G02/G03/G05 partial.** Owner selected closure of the
priority gaps; optional expansion stays deferred. These
are evidence/delivery boundaries, not five known bugs or an effort estimate.
Only select a category for a required user workflow or claim.

| Done | ID | Remaining gap | Easiest next action and completion rule |
| --- | --- | --- | --- |
| [x] | G01 | Current-source selected-ROI comparison complete; general speed unproven | Six alternating pairs: native305.6ms /QuPath313.1ms; exact RGB/partitions for2,237 cells. Overlapping ranges/background load prevent a general speed claim. [Current evidence](benchmarks/current-source-speed/README.md). |
| [ ] | G02 | Partial: selected coarse whole-slide tissue passes; broader resources open | [Current case](benchmarks/gap-closure/README.md): exact256/512 tiles/independent threshold/cancel;32 repeats without post-warmup private drift; two settled viewer replays. Level-zero cells, dense scenes, long-duration memory and total GPU residency remain unqualified.  Choose one actual large-slide research workflow; reuse the tiled scheduler and existing fixtures. Check full/tiled seams and duplicate/lost objects where applicable, cancellation/stale results, process/GPU peaks, UI responsiveness and long-run memory for that workload. Optimize only the measured limiting stage. R12 in the [delivery plan](api-delivery-plan.md#dependency-lanes-and-immediate-queue). |
| [ ] | G03 | Partial: new558-cell anisotropic NDPI crop exact; generalization open | [Current case](benchmarks/gap-closure/ndpi-checks.json): exact masks/RGB/calibration and independent compartment RGB. No new pathologist/independent-specimen/context-halo acceptance.  Select a rights-cleared specimen/context/vendor/feature-domain case missing from existing evidence. Capture inputs, calibration and recipe first; compare masks/counts/measurements with declared tolerances and preserve replay. The13 held-out plus nine prior zero-context original-slide workflows already match; do not reopen their eight historical resolved differences. [Held-out scope](cell-policy-heldout-gaps.md), [public contract](original-source-cell-integration.md). |
| [ ] | G04 | Candidate6 local package/install/routes/removal verified; public handoff pending | [Evidence](benchmarks/gap-closure/README.md):69 immutable files, native/wheel closure, installed density/import/resume agreement,29 user files unchanged after removal.  Once a release is selected, freeze one combined source/runtime, update dependency/source/notices and support claims, then package matching wheel/native/source assets. Run only applicable changed-workflow and clean-host install/run/removal checks. Reuse candidate5 procedures; do not rebuild intermediate packages. [Packaging authority](windows-candidate5-coverage.md). |
| [ ] | G05 | Partial: current Windows/NDPI supported route checked; invalid-ring import and platform expansion open | [Actual upstream .qpdata](benchmarks/gap-closure/qpdata-limit.json) refuses a self-intersecting ring before publication; supported saved-data controls pass. Define explicit repair/admission semantics before accepting this case; no silent geometry change.  Pick a specific additional eligible reader/codec, project-file version, or Linux/macOS package route required by a user. Reuse current adapters, loss-aware import and packaging. Close with exact pixels/calibration or loss reporting for that format, or installed workflow evidence for that host/package. Historical platform checks do not qualify the latest source. R2/R5/R14 in the [inventory](feature-matrix.md). |

G01 is complete for the selected ROI; do not repeat it for status. The next concrete
compatibility task is G05's refused upstream invalid-ring file; generalization
and expanded support must keep bounded acceptance contracts. The latest11.2%
local improvement compares two native versions, not QuPath. The earlier direct
native324.8ms/QuPath314.1ms comparison predates the contour optimization and had
substantial host load. Comparing today's302.9ms with that older314.1ms would not
establish a speed win. More kernel work is not automatically required: profile
current code only if the selected workload still demonstrates a bottleneck.

G02/G03 must name a bounded workload/case before execution; they are not requests
for exhaustive whole-slide or biological qualification. G04/G05 become release
conditions only for the package/host/format support actually claimed.

## Optional feature expansion

**5 deferred categories. None is a mandatory parity requirement.** Select one
only for a concrete research need; then create its small acceptance checklist
using the existing shared C++ authority.

| Selected | ID | Optional scope | Easiest existing path |
| --- | --- | --- | --- |
| [ ] | O01 | Advanced fluorescence, explicit ICC policy, additional monitor/DPI/international text behavior | Extend current channel/display transforms and font/tile pipeline; preserve measurement pixels and overlay registration. R3. |
| [ ] | O02 | Additional classifier/model schemas and Cellpose/StarDist or OMERO/ImageJ integrations | Extend native model/import/export services for one named model or exchange contract; verify its feature/prediction or round-trip semantics. R8/R15. |
| [ ] | O03 | Tiled spatial aggregation or additional TMA study workflows | Reuse delivered spatial index/density/distances and TMA proposal/editor/batch APIs. Add only missing aggregation or case-pooling semantics. R9/R10. |
| [ ] | O04 | General multi-stage workflow grammar, dependencies/retries and broader pipeline replay | Build one versioned workflow over current native requests/jobs and persistence; keep CLI/ImGui/nanobind as consumers. Density resume/import are already delivered. R11. |
| [ ] | O05 | Distributed execution or broader extension infrastructure | Start only after one measured workload needs it; extend existing jobs/archives rather than adding a second scientific authority. R15/R16. |

The [workflow inventory](feature-matrix.md) and R0-R16 catalogue retain finer
possibilities. Cosmetic QuPath equivalence, proprietary readers, Java plugin
binary compatibility and100% feature parity are outside the selected scope.

## Execution and waste controls

1. Select one gap and state the user outcome, changed inputs and decisive check
   in current-task.md before implementation or expensive validation.
2. Reuse existing C++ APIs, fixtures, cache and installed-route evidence. Build
   changed targets once; preserve costly binding objects and frozen releases.
3. On completion or failure, update this row's scope/evidence and current-task's
   outcome, check, waste/correction and next action. Do not add another ledger.
4. Stop at that outcome. No automatic broad corpus/platform campaign, repeated
   archive publication or new parity phase. Documentation uses link/diff checks.
