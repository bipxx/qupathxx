# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1, CLASSHISTORY1,
[CONTEXT1](benchmarks/cell-context-r6/README.md) and
[SAMPLING1](benchmarks/cell-pyramid-r6/README.md) are locally verified within their
recorded scopes. Full R6 and the broader R2-R16 inventory remain open.

Current: **R6-FEATURE1 — retained compartment measurements** (in progress: definition and shared reducer).

- Outcome: select and finish one missing nucleus/cell/cytoplasm measurement
  definition through native results, owning Python inspection, retained records
  and table/consumer paths, preserving historical output semantics.
- First action: inspect CellFeature, natural/grid accumulation, archive validation,
  cell tables and classifier origins against the pinned QuPath/reference feature
  definitions. The existing native fields retain counts and RGB/channel means;
  do not recreate those. Identify the concrete missing distribution statistic or
  feature-domain behavior before adding fields or parameters.
- Contract before editing: define source values (RGB/OD/stain), masks, population
  versus sample statistics, units, zero/one-sample behavior, numerical tolerance,
  ownership and retained identity. Use a small independent expected result and
  the existing pinned source/segmentation for integration. Do not infer new
  historical statistics from old means or silently reinterpret old records.
- Shared route: native accumulation is the authority; nanobind/CLI/ImGui and
  classifier/table paths consume its retained values. Reuse existing cell jobs,
  records, contours, schemas and atomic publication. Version persistence only
  where the selected new data requires it; avoid a parallel measurement model.
- Checks/stop: complete the selected feature through affected APIs, save/reopen
  and export with its definition/identity intact; retain legacy replay and exact
  existing means. Biological acceptance waits only for its actual labels and
  tolerances. No claim of all features, vendor coverage or whole-slide speed.
- Cost/dependencies: the trace needs no build/capture/install. Reuse installed
  faa71c1f, pinned pixels and current build/probe environments. Group interfaces,
  persistence and consumers before one necessary candidate; generate stubs once
  only if the Python surface changes. Use openly licensed readers/codecs. Reader,
  resource and research decisions stay with R2/R12/R1 where actually required.

## Latest task review — FEATURE1 numeric foundation (2026-11-05)

Outcome/evidence: cached QuPath0.7 RunningStatistics generated six pinned scalar
vectors, establishing sample SD n-1 and unavailable singleton SD. The shared
private reducer [passes the cells native role](benchmarks/cell-statistics-r6/README.md), including those references,
independent compartment values, byte/float and double input, large offsets,
background/nonfinite rejection, exact budget admission and cancellation.
Production behavior remains unchanged: no retained fields, archives, SDK or CSV
integration is complete. The [measurement contract](cell-intensity-statistics.md)
settles the native result/vector, replay, QCL10/11, QCA10 and opt-in CSV13/14 route.

Waste/action: **reuse** existing decoded samples/labels, runtime and cached build;
**merge** public API, persistence and consumers before a wheel/stub/install cycle.
The first reference script used an unavailable Groovy JSON module; preserve its
failure and use existing GsonTools, with no extra dependency. Web cache misses
and older cell94 source do not prove current definitions: use actual0.7 runtime
and pinned detector. Search actual filenames/API calls before writing observers.
No new pixel corpus, second reducer/model or product install was needed here.

Plan change/next: expose typed result statistics in a compact per-result vector,
fuse the moments into existing accumulation without changing canonical means,
and include the recorded measurement policy in saved replay/verification. Then
complete the versioned codecs and consumers in the same FEATURE1 workflow. Keep
legacy mean-only records small; do not insert a large empty optional statistics
aggregate into every CellFeature. Do not close FEATURE1 or repeat sampling/host
campaigns merely because the reducer passes. After each bounded task or failed/
stopped attempt, record outcome/evidence, actual waste/action and next step here
before continuing/retrying.
