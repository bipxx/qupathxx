# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. [MEDIAN1](benchmarks/tissue-median-r7/README.md) is
locally verified in its recorded scope; broader R2-R16 work stays active.

Current: **R7-GRID1 — explicit tissue sampling and context contract**
(ready for trace).

- Outcome: finish one demonstrated missing tissue sampling/context workflow
  through the existing native raster, retained results and thin consumers.
- First action: inspect TissueRequest's native-level capture, scientific/nominal
  raster APIs, shared support/lattice descriptors, cell grid requests and mask
  archive restrictions. Compare their existing evidence with the R7 user outcome.
  Reuse a delivered route if it already satisfies that outcome; do not add another
  reader, sampler or result model. Pin the actual missing behavior before coding.
- Contract before editing: define requested origin/extent/spacing/counts, units,
  source plane/reader, ROI and opaque support, context/halo, threshold/filter
  sampling domains, edge ownership, calibration, errors and replay identity.
  Establish an independent tiny expected mask and declared tolerance. Preserve
  intentional native/nominal/grid recipe distinctions and old archives.
- Easiest route: reuse ScientificGrid/scientific raster, nominal support, tissue
  cleanup/filters, jobs, persistence and tables where compatible. Add only missing
  factory/descriptor/codec plumbing, then affected SDK/managed/ImGui/batch/CLI.
  Reader/codecs must remain openly licensed. Pull R2 forward only for a real gap.
- Checks/stop: one full selected workflow, exact lattice/pixel/mask and relevant
  seam/domain/calibration expectations, save/replay/export and changed consumer/
  cancel/stale/resource checks. Close only that slice; broader context, feature/
  stain/model/specimen and whole-slide work stays in R7/R12 and the wider queue.
- Cost: trace requires no build/install/capture. Reuse final34704c24, existing
  stages/caches and isolated environments. Settle coupled contract/interface
  fields before compilation; trace all inline quota owners and valid old bounds.
  Inspect actual scrolled controls before packaging. Reuse tests by source and
  configuration; no status-driven campaign or duplicate filter/reader work.
  Root .venv is build/static tooling only, not the runtime editable environment.

## Latest task review — MEDIAN1 closeout (2026-11-05)

Outcome/evidence: [final34704c24](benchmarks/tissue-median-r7/README.md) delivers
fixed3x3 post-threshold binary cleanup across native, owning SDK, managed viewer,
ImGui and batch/CLI.524 actual ImageJ outputs match independent rank/edge logic.
Seven native suites,148 staged cases, two actual GUI flows, strict typing with
three intended invalid assignments and eight installed shipping checks pass.
Old unfiltered/Gaussian record/table/batch/work limits and QTM1/2/3 replay remain
unchanged; QTM4/CSV3 retain enabled median.482 build inputs and65 installed/wheel
members match. Stage and final source/configuration agree, except revision stamp.
Other skipped checks and wider science/host/resource/speed claims stay unclaimed.

Waste/action: **reuse** existing mask/cleanup/jobs/archives/tables/parameter routes
and queue scratch. **Consolidate** five scattered size deductions into one shared
parameter-offset policy; pin both prior parameter groups before editing. No
native/staged/installed failure, byte repair or retry occurred. One full binding
compile, one stub generation, one wheel and one install were needed. The live
build was polled rather than restarted. A web cache miss was resolved by fetching
only the pinned source into the existing cache; no extra checkout. Scrolled
controls were inspected before the wheel, and existing RGB capture was converted
unchanged for viewing. No new specimen corpus, reader/model/environment or
reporting framework was introduced.

Plan adjustment/next: MEDIAN1 enters the reuse index and tissue coverage row.
GRID1 first traces existing sampling/retention authorities. Group coupled
contract/interface work when it is ready to avoid a build per parameter; do not
invent unsettled APIs. After every bounded task or failed/stopped attempt,
record actual outcome, waste/action, plan change and next task/retry before
continuing. Keep all unfinished M0-M7 and the wider queue intact.

Cleanup review: redundant build PPM bytes matched the retained PNG's decoded RGB
before removal. A PowerShell parent-path guard used an unsupported Split-Path
parameter combination; the resolved single named PPM was removed, and no recursive
operation occurred. Verify its fixed parent with System.IO.Path and its absence;
do not repeat deletion. Preserve the equality manifest/PNG and all source/fixtures/
logs/caches. Use one supported path API for future cleanup guards.
