# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1, CLASSHISTORY1,
[CONTEXT1](benchmarks/cell-context-r6/README.md) and
[SAMPLING1](benchmarks/cell-pyramid-r6/README.md) are locally verified within their
recorded scopes. Full R6 and the broader R2-R16 inventory remain open.

Current: **R6-FEATURE1 — retained compartment measurements** (ready for trace).

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

## Latest task review — SAMPLING1 closeout (2026-11-05)

Outcome/evidence: [installed lossless TIFF integration](benchmarks/cell-pyramid-r6/README.md)
passes10 SDK/CLI cases plus one actual desktop case at unchanged faa71c1f.
Contiguous/separate tiled Deflate pyramids, IntegerOrigin/AffineInverse and
context0/3-4 match dense RGB, exact nuclei/bodies, retained compartment values and
calibrated sample areas. A white coarse level proves the original-source path
cannot silently substitute preview pixels. Native TIFF links, save/reopen/replay
and CLI fresh checkpoint/resume remain correct. Broader R6 remains open.

Waste/action: **reuse** existing source pixels, sampler/reader, expected masks,
installed runtime and package integrity. No native fix, new decoder/corpus,
build, wheel, stub work or install was needed. The initial integration observer
failed after numerical/save checks at a guessed ImageServer.reader property;
use actual tiff_view/metadata and ProjectImage.reader. Preserve its failed log.
Use existing GUI options and CLI terminal-JSON parsing. Test files generate
short-lived TIFF/project inputs instead of duplicating the image corpus.

Plan change/next: SAMPLING1 enters the reuse index; FEATURE1 first traces existing
compartment fields and reference definitions. Only the selected missing contract
justifies implementation. After every bounded task or failed/stopped attempt,
replace this review with scoped outcome/evidence, actual waste/action and changed
next step before continuing/retrying. Update existing queue rows for changed
scope/order/dependency and coverage only for changed evidence.
