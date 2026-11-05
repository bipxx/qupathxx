# Current implementation task

Follow the [single queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1 and CLASSHISTORY1 are locally verified within
their recorded scopes; remaining R2-R16 inventory is still open.

Current: **R6-CONTEXT1 — cell context/halo sampling and replay** (in progress).

Outcome: new nominal cell requests use the actual RGB sampling support at a
signed boundary; retained old recipes still replay exactly. This is engineering
correctness, not biological acceptance or a speed claim. Ordinary context cores,
interpolation halo IO and replay already have evidence; reuse them.

Demonstrated gap: installed `2892db74`, white opaque8x8, in-image ROI(0,0,8,8),
nominal extent(-4,0,12,8), step1.6, context0/2 produces one false cell and25
eligible samples. Binary64 center eligibility disagrees with RGB fixed-point
phase. The correct new-request expectation is zero cells/20 eligible samples;
step.4 with origin-1 expects zero/380. Keep the in-image ROI guard and strict
opaque-source policy. Padding is opaque black; alpha/color is not a support mask.

## Remaining steps for this task

| Order | Work and easiest path | Decisive check / stop |
| --- | --- | --- |
| 1 — primitive verified | Reuse `rgb_nominal_source_support` and the existing sampler phase; no second coordinate algorithm. Retain legacy QCL6 fixtures captured with installed2892 | Explicit boundary bitmap passes both coordinate policies and context0/2. Combined resampling observer passes after its ROI-plane setup repair; production cells are still unchanged |
| 2 — next: settle compatibility | Trace input identity, CellRequest replay, QCL/QCA dispatch, classifier origins and request/result matching. Use a private support policy; fresh nominal requests select exact-phase support, old archives select legacy support. Version input format and archive envelope where required; do not expand public processing policy merely for sampling | Old envelopes decode/reencode with original meaning. New identity cannot be mistaken for an old recipe. Finalize versions from actual codec consumers before implementation; verify fixture hashes and pin binary attributes |
| 3 — implement shared route | Cache/reuse one sampling plan per grid; wire support into eligibility, hashing, archive validation and replay. Preserve area grids, pixel bytes, ROI geometry and detector arithmetic. Consumers reuse CellRequest/native records | Independent white-image core counts above, context invariance, no false cells, unchanged original pixel checks, atomic invalid-input refusal. No per-pixel plan construction |
| 4 — compatibility and consumers | Add narrow native/Python regressions for new save/reopen/replay and retained legacy fixtures; include classifier-origin persistence if affected. Exercise only changed CLI/UI plumbing | Legacy fixtures reproduce original1 cell/25 samples, explicitly compatibility evidence. New archives/replay retain corrected identity and results. Check actual installed source before SDK execution |
| 5 — one integrated candidate and handoff | Group settled changes in the existing cache; build affected native roles and packaged consumers together. Reuse installed/probe environments. Generate stubs only if bindings change; install once when native/codec changes settle | Record one scoped summary/source/configuration; run affected installed workflow, ownership and persistence checks. Update coverage only for demonstrated behavior; select first remaining R6 slice, then follow R7 onward |

Cost/dependencies: no new reader, image corpus, public API or general campaign is
needed for the demonstrated defect. Reader work, when required, must use openly
licensed readers/codecs. Existing installed/staged runtime2892 remains the baseline;
new primitive/observer builds have distinct dirty-source provenance. No wheel or
install has yet delivered this correction. Native integration is implemented. Do not call the primitive alone a
completed cell workflow. Owner research/license/release decisions remain R1/R13.

## Latest task review — plan and support-primitive handoff (2026-11-05)

Outcome/evidence: `build/cell-context-r6/phase-native.xml` passes the resampling
role after the phase extraction; `legacy-installed.xml` passes both installed2892
fixture cases, including save/reopen/replay. No wheel or stubs were rebuilt.
`build/cell-context-r6/support-observer.xml` reports one native
resampling role, zero failures, including exact-phase support and retained nominal
byte/halo checks. The original `support-native.xml` failure remains preserved.
Its raw observer authored a default-plane ROI but requested another retained Z/T;
the repair binds the ROI plane before capture and scopes the metadata-read counter
after authoring. No reader-plane guard or pixel/hash expectation was weakened.
Native cell integration and versioned replay now pass their affected checks;
installed candidate/consumer verification remains open.

Waste/action: **reuse** the passing primitive and ordinary halo evidence; **merge**
remaining native/codec/consumer work before one integrated candidate; **remove**
blind build retries and large document dumps by inspecting retained results and
bounded sections first. The earlier negative ROI probe hit the intended guard;
use the allowed explicit descriptor instead. No new audit, cost ledger or report.

Plan change/next: the delivery queue now names the actual signed-boundary gap,
full-inventory finish conditions and per-slice API/consumer handoff. The [support contract](cell-context-support.md) records the traced QCL9/QCA9
layout, including per-origin kind5 for mixed classifier histories. The new legacy
regression verifies retained masks/hash/measurements and save/reopen/replay.
A phase-only helper now shares RGB coordinate authority without a window scan.
Next wire the policy/identity through
all listed consumers before changing fresh-request defaults.
After each bounded step or failed/stopped attempt, replace this latest review
with outcome/evidence, observed waste and action, and the changed next step before
continuing or retrying. Update the existing queue row only for changed scope,
order or dependency; keep durable evidence at its original path.

Attempt review (integration build): core, cells, object_classifier and project_batch
compiled/linked; the final requested save target was misnamed and MSBuild rejected
it (`integration-build.log`). **Remove** guessed target names: CMakeLists.txt names
`qupathxx_save_tests`. Build that target only, preserving already built products;
then run the four affected roles. No source/codec change is justified by this
command failure. Full correction is still awaiting these native results.

Task review (native policy/codec integration): integration-native.xml passes cells,
object_classifier and project_batch; save-native.xml passes project_save. New
QCL9 exact support and pinned QCL6 legacy replay/cross-policy refusal are exercised.
**Reuse** native pixel expectations and old fixture pixels; capture only a missing
legacy patterned project from installed2892 to test mixed classifier origins, not
another image corpus. **Merge** SDK, CLI, app and required test relinks in one
cached wheel candidate. Binding signatures are unchanged; regenerate stubs only
if actual generated output requires it. Next: one candidate, narrow staged SDK
checks including mixed QCA9 origins, then install and affected consumer checks.
