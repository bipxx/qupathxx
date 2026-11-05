# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. [FILTER1](benchmarks/tissue-smoothing-r7/README.md) is
locally verified within its recorded scope; broader R2-R16 work remains open.

Current: **R7-MEDIAN1 — median stage and support contract** (ready for trace).

- Outcome: finish one demonstrated missing median tissue workflow through the
  existing native request/kernel, retained result and affected thin consumers.
- First action: trace the existing tissue/Gaussian/morphology filters, neighborhood
  helpers and pinned upstream tissue median workflow. Identify whether the needed
  median acts on scalar intensity or a binary mask, and its stage/order. Reuse
  existing evidence if it already proves the proposed case; do not duplicate it.
- Contract before editing: pin pixels/feature/units, sampling level, neighborhood,
  radius/order statistic and ties, image/crop/ROI-hole/alpha support, filtering
  order, expected mask and tolerance. Do not infer a median contract from Gaussian
  or morphology names. Keep arbitrary context/halo recipes explicit and separate.
- Easiest route: extend the current raster/filter/cleanup authority and shared
  jobs, archives, tables and SDK/managed viewer/ImGui/batch/CLI parameter routes.
  Reuse compatible neighborhood/workspace code rather than a second segmenter.
  Readers and required codecs must remain openly licensed.
- Checks/stop: one complete selected median workflow, an independent tiny-array
  expectation and relevant edge/level/tile, persistence/consumer and cancel/stale
  checks. Version only changed recipes; keep older unfiltered/Gaussian replay.
  Close only that slice. Broader grid/context, stains, models/specimens and
  whole-slide resources stay in R7/R12 and the wider queue.
- Cost: trace requires no build/install/capture. Reuse final7873a3a5 and the current
  caches/isolated environments. Group native API/consumer fields before binding
  compilation. Trace every inline parameter owner that affects quotas, including
  batch/table captures; pin valid saved old boundaries before packaging. Inspect
  actual scrolled controls before the wheel; generate stubs/package only as needed.
  Root .venv is backend/static tooling only, never the runtime editable hook.

## Latest task review — FILTER1 closeout (2026-11-05)

Outcome/evidence: [final7873a3a5](benchmarks/tissue-smoothing-r7/README.md) delivers
pre-threshold masked Gaussian smoothing across native, owning SDK, managed viewer,
ImGui and batch/CLI. Explicit cropped support crosses decode tile boundaries;
QTM3/CSV2 retain sigma while old QTM1/2 replay and encode canonically. Six native
roles pass.130 first-stage cases, one repaired old-batch node, two actual GUI
flows and two phase cancel/stale cases pass. Strict typing passes with exactly
three intended invalid assignments. Eight installed shipping checks and two
old-format canonical cases pass.481 source/build inputs and65 wheel/installed
members match; repaired-stage source matches final code. Other skipped checks
and broader science/host/resource claims remain unclaimed.

Waste/action: **reuse** shared filters, raster, morphology, codecs, parameter
routes, environments/caches and source-scoped passed checks. One full binding
compilation, one stub generation, one wheel and one install were used. A valid
old103012-byte batch witness caught8 extra inactive metadata bytes before
packaging; preserve legacy bookkeeping and fully charge new smoothed requests.
Only the affected core body/links, failed node and native batch role were retried;
no limits/assertions were weakened. **Expand** the pre-package trace to inline
parameter owners. A guessed roi.hpp read failed; discover paths/declarations.
The reopened fixture boundary correctly accounts for decoded component capacity.
Scrolled controls were inspected before packaging, avoiding another wheel.
The final audit added old QTM2 evidence using retained code and the same tiny
pixels, with no rebuild. Failures, both stages and original fixtures are retained.

Plan adjustment/next: FILTER1 enters the reuse index and the tissue coverage row.
MEDIAN1 first traces stage/order and support; no second backlog or general campaign
was added. After every bounded task or failed/stopped attempt, record outcome,
actual waste/action, plan adjustment and a concrete next task/retry before continuing.
