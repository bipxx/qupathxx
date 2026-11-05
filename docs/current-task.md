# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. Completed workflows retain their recorded source and
scope in the delivery plan reuse index. Broader R2-R16 work remains open.

Current: **R7-AREA1 — calibrated tissue component area limits** (ready).

- Outcome: a researcher supplies optional inclusive minimum/maximum component
  areas in square micrometers and gets the same tissue result through native,
  Python, managed viewer, ImGui and batch/CLI paths, with saved replay/export.
- First action: settle the physical-bound contract against TissueRequest and
  CellRequest::with_nucleus_area_limits; inspect affected bindings and consumers
  before editing. Establish independent component counts/areas on a tiny mask.
- Easiest route: resolve bounds once using captured per-axis calibration and the
  chosen level into TissueParameters.min_pixels/max_pixels. Reuse thresholding,
  morphology, connected components, jobs, retained results and table exports.
  Level selection currently does not depend on these count filters. Reuse shared
  calibrated numeric handling; do not copy a second rounding/search algorithm.
- Contract to settle: absent bounds preserve existing behavior; supplied bounds
  require known positive calibration and finite nonnegative values; minimum may
  not exceed maximum. Intersect physical and pixel limits. Define zero maximum,
  empty intersections, inclusive boundaries, anisotropy and extreme finite
  calibration explicitly. Capture calibration for stale-publication checks.
  Prefer the existing frozen count-recipe replay contract; add persistence fields
  only if a demonstrated requirement cannot use that contract.
- Checks/stop: independent tiny-array expectations at multiple levels and unequal
  axes; exact boundary/empty/invalid/unknown-calibration cases; affected stale
  publication and saved replay; thin owning SDK/typing and actual changed desktop,
  CLI/batch/managed flows. Close only AREA1. Other R7 features and broader R6
  vendor/context/specimen claims retain their existing queue entries.
- Cost: reuse installed 60a0274e for unchanged behavior and existing build caches
  and isolated environments. Group native API/bindings/consumers before one
  candidate build/install and one stub generation if the public API changes.
  Check affected native cases before packaging. Inspect any quota-sensitive
  layout and valid old boundary fixtures before packaging. No new reader,
  segmenter, corpus, environment or full platform campaign is planned.
- Handoff: after this task or any failed/stopped attempt, record outcome, actual
  waste, action, plan adjustment and concrete next task/retry **before continuing**
  using the [shared review procedure](api-delivery-plan.md#completion-gates-and-task-handoff).

## Latest task review — SPLIT1 trace and remaining-plan update (2026-11-05)

Outcome/evidence: existing [nominal reference assertions](../tests/test_qupath_nominal_cells_reference.py),
[policy-edge assertions](../tests/test_cell_policy_edges.py),
[body-boundary assertions](../tests/test_qupath_body_boundary_revision.py) and
[pinned cases](../tests/fixtures/qupath-cells-0.7.0/contract.json) already cover
split/merge, touching nuclei, clipped ROI/hole behavior and distinct feature
sampling domains. Reuse the recorded results within their original source and
fixture scope. This trace adds no runtime acceptance; intentional discrepancies
and biological/vendor gaps remain explicit. No uncovered engineering case was
selected, so advance to the demonstrated pixel-only tissue-filter gap.

Actual waste/action: **reuse** these references instead of inventing another
split/merge campaign. **Remove** oversized unfiltered document dumps: bounded
section searches are enough for planning. **Merge** upcoming physical-area API
and consumer work before packaging. No builds, installs, captures or numerical
reruns were needed for this documentation task; no artifacts were deleted.

Plan adjustment: R6 retains named uncovered cases and specimen acceptance; R7
now starts with AREA1. The existing ordered R0-R16 queue remains authoritative.
The shared closeout now requires an actual waste decision and changed retry
reason after every task or failed/stopped attempt, without another report cycle.
Next: settle AREA1 boundaries and inspect existing consumer APIs before coding.
