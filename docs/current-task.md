# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1, CLASSHISTORY1,
[CONTEXT1](benchmarks/cell-context-r6/README.md) and
[SAMPLING1](benchmarks/cell-pyramid-r6/README.md) are locally verified within their
recorded scopes. Full R6 and the broader R2-R16 inventory remain open.

Current: **R6-FEATURE1 — retained compartment measurements** (native/API complete; installed consumers pending).

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

## Latest task review — FEATURE1 candidate handoff (2026-11-05)

Outcome/evidence: typed per-result statistics and selected-row classifier snapshots
are implemented, preserving CellFeature layout and canonical means. Both kernels
fuse moments into their existing accumulation loops. QCL10/11 and QCA10 retain
new fields while old mean-only replay remains absent. The five affected native
roles pass; independent moments span four natural, area and two nominal recipes.
Malformed-statistics checks pass. The old exact workspace boundary now reopens/
replays unchanged at18212166 bytes.11 initial SDK/ownership/mixed-history checks
and the repaired boundary pass. Stubs were generated once; positive typing passes
and negative typing reports exactly the three intended errors.

Waste/action: **reuse** existing source pixels, masks, caches and environments;
**merge** field/codecs/bindings/export/UI changes before one packaged candidate.
Do not repeat the slow public-header binding compilation: it completed once;
subsequent body/observer fixes only refresh affected objects/links. Preserve the
stale QCL2 algorithm-coupling observer failure and the32-byte legacy metadata
regression. The first typing command checked unchanged imported equality stubs;
a guessed CLI option then failed. Local mypy code/help established the required
config-only setting. Keep those logs; apply silent-import checking to stubs while
still demanding all three negative diagnostics. No broad suppression/API change.

Plan change/next: build one source-identified cached wheel with settled stubs and
all packaged executables relinked. Run the new shipping SDK/CSV/CLI/legacy and
selected desktop workflows plus package/stub integrity; install once only after
those candidate checks. Existing reader/sampling and unchanged numerical cases
retain their scoped evidence. FEATURE1 remains open until these consumers pass.
After every bounded task or failed/stopped attempt, update outcome/evidence,
actual waste/action and the changed next step before continuing/retrying.
