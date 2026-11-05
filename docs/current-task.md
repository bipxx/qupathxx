# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. [CLASS1](benchmarks/class-hierarchy-r4/README.md) and
[CLASSHISTORY1](benchmarks/class-history-r4/README.md) are locally verified;
full R4/R5 and the remaining R2-R16 families stay open.

Current: **R6-CONTEXT1 — cell context/halo sampling and replay** (ready).

- Outcome: one uncovered cell-context/halo workflow has an explicit sampling,
  boundary and replay contract, with retained request/recipe/results and declared
  engineering expectations. This does not imply representative biology or speed.
- First action: trace existing CellRequest.for_grid_roi/for_nominal_roi/replay,
  GridCellSampling and NominalCellSampling against [sampled recipes](sampled-cell-recipes.md)
  and the [current baseline](api-delivery-plan.md#analysis-baseline-to-preserve).
  Select one uncovered ROI-boundary/context case; if existing evidence already
  satisfies it, link/reuse that result and advance rather than rewriting it.
- Reuse: shared scientific/nominal sampling, reader halo, cell kernels, archive/
  replay and thin native/Python/CLI/ImGui consumers. Detector context is in output
  samples; interpolation halo is a separate support requirement. Preserve V1
  recipes and nominal/grid coordinate/axis policies. No second sampler/cell model.
- Contract/checks: name exact pixels/lattice/ROI, context sizes, edge/alpha/missing
  support, rounding and tolerance before comparison. Use an independent small
  expected result and retained source/recipe; check core/context mapping and
  save/reopen/replay where affected. Biological labels/tolerances block their
  associated accuracy claim, not independent sampling/replay engineering.
- Stop/cost: complete one named workflow through affected APIs; keep other R6
  cases in the existing row. First trace needs no build/capture/install. Build
  only for demonstrated missing behavior, group interfaces/consumers before a
  candidate, and select changed-boundary checks. Reuse the installed2892db74
  baseline and existing environments; no general audit or repeated campaigns.
- Dependencies: reader/format gaps pull only their eligible R2 slice forward.
  Only openly licensed readers/codecs. Research/release choices remain in R1/R13;
  no unsupported host, scientific or overall speed claim is added here.

Latest task review (2026-11-05, CLASSHISTORY1 installed closeout):
Outcome/evidence: [source2892db74](benchmarks/class-history-r4/integrity.json)
fixes the demonstrated rename admission defect. The exact installed2000-vertex
probe moves from atomic budget refusal at8722d96d to1419 accounted history bytes
under16KiB, retaining geometry and undo/redo. Native subtree/property coverage
uses3392 bytes and preserves parent/raw/absent-color resolution, probability,
geometry/index, copy/history/snapshot and refusal semantics. Four native roles,
45 staged Python/stub cases (four declared desktop skips), the exact installed
probe and two installed ownership/history checks pass. All65 installed hashes
match wheel/RECORD; full families/milestones remain Partial.
Waste/action: **merge** rename into existing field preparation/atomic toggle;
no geometry payload or property retention in that journal, no new model/journal.
**Reuse** public headers, binding sources, stubs, cache, environments and fixtures.
One33.46s cached consumer candidate/install, no binding recompile or stub work;
no repair loop, pixel capture, broad import/schema/numerical/host qualification.
These are history accounting results, not deep-copy/RSS/CPU speed measurements.
Next: the single R6 contract/evidence trace above; keep owner-dependent claims
scoped and update this card after the next bounded task or failed attempt.
