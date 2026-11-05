# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1, CLASSHISTORY1 and
[CONTEXT1](benchmarks/cell-context-r6/README.md) are locally verified within their
recorded scopes. Full R6 and the broader R2-R16 inventory remain open.

Current: **R6-SAMPLING1 — lossless pyramid/reader equivalence** (ready for trace).

- Outcome: one named uncovered calibrated nominal-cell workflow reads the same
  declared level-zero pixels through an eligible lossless pyramid reader and a
  dense reference, retaining sampling/recipe/result and save/reopen/replay identity.
  This establishes engineering equivalence only, not vendor-wide/biology/speed.
- First action: inspect existing RGB nominal reader/halo, lossless TIFF pyramid,
  nominal-cell frontend and retained real-slide results. Select only a missing
  representation/boundary. If the outcome already has compatible evidence, link
  it and advance to the next R6 gap without a new implementation or capture.
- Expected contract: pin pixels, dimensions/axes/calibration, explicit ROI/lattice,
  context and sampling policy before comparing. Native request/sampler/reader
  authority owns mapping; thin Python/CLI/ImGui consumers reuse it. Use an existing
  procedural lossless fixture where possible. Compare pixels and supported-domain
  masks/features independently; input hashes may legitimately differ with reader
  identity. Do not demand byte equality across different lossy decoder policies.
- Decisive checks: if uncovered, dense versus declared lossless-reader sampling and
  cell outputs agree under the pinned contract; pyramid metadata cannot silently
  substitute another input level. Retained request/source identity, save/reopen and
  replay preserve the chosen route. Add damage/cancellation/axis checks only where
  the actual changed boundary requires them. No new biological acceptance claim.
- Dependencies: use openly licensed readers/codecs only. Pull an R2 slice forward
  only for a demonstrated adapter gap. Missing research labels/tolerances block
  that biological claim, not this independent engineering workflow.
- Cost/stop: trace needs no build/install/capture. Runtime baseline is installed
  faa71c1f, shared build cache and existing isolated environments. Reuse CONTEXT1's
  sampling/support/codec results and pinned pixels. Build only for missing native
  behavior; group dependent codec/consumer work before one candidate. No second
  sampler, general reader audit or repeated qualification. Close the selected
  complete workflow, then take the next R6/R7 slice in the single queue.

## Latest task review — CONTEXT1 closeout (2026-11-05)

Outcome/evidence: [faa71c1f](benchmarks/cell-context-r6/README.md) corrects signed
nominal support without changing RGB bytes, ROI geometry or detector arithmetic.
White8x8 now has zero cells/20 samples at step1.6 and zero/380 at step.4,
context0/2. QCL6 fixtures replay their original known-wrong1-cell/25-sample output;
new QCL9 and mixed QCA9 origins retain policy. Five candidate native roles plus a scoped mixed-origin observer,
105 staged SDK tests,85 installed cases and five selected actual ImGui flows
pass.23 desktop skips in the broader installed selection remain explicitly
separate from the five exercised flows.476 compiler inputs and65 installed/wheel
members match source/RECORD; stubs unchanged, one cached wheel/install.

Waste/action: **reuse** existing sampler, fixtures, caches, environments and
byte/halo expectations; **merge** identity/codec/consumer integration before one
wheel/install. **Remove** guessed targets/filenames and large unfiltered outputs:
search directory/content with explicit globs, then read returned paths/sections.
The initial native failure was an unrelated ROI-plane observer setup; correct
its authoring scope without weakening the guard. A misnamed save target was
corrected from CMake without repeating completed builds. The added mixed observer initially guessed a cross-fixture record ID; obtain the
actual restored record ID instead. Its repair passes explicit per-row support
policy assertions. Only that observer input differs from installed faa71c1f;
no second wheel/install. Failures stay retained in the closeout. Only the missing patterned legacy archive reused existing
pixels to test mixed origins; no fresh image corpus or general campaign.

Plan change/next: CONTEXT1 leaves the active implementation queue and enters the
reuse index. R6-SAMPLING1 starts with a bounded coverage trace, not a rebuild.
After each bounded task or failed/stopped attempt, replace this latest review
with scoped outcome/evidence, actual waste/action and changed next step before
continuing/retrying. Update this queue row only for changed scope/order/dependency;
update coverage only for changed evidence. No extra review/reporting framework.
