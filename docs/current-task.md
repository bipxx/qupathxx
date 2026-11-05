# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. Completed workflows retain their recorded source and
scope in the delivery plan reuse index. Broader R2-R16 work remains open.

Current: **R7-AREA1 — calibrated tissue component area limits** (in progress).

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

Implementation attempt review: one read-only search used a Windows wildcard as
an explicit filename and failed; no build or product changed. Remove that form:
search the cpp directory with -g globs. The authoritative cell implementation
already rejects a zero physical maximum under underflow; reuse that behavior
rather than adding a speculative repair. Next extract the shared count resolver
and add the tissue factory and stale-calibration guard before consumer changes.

Trace correction before implementation: a second read guessed a nonexistent
cell_sampling.cpp path. The valid companion search still returned the needed
field, but future filename discovery must use rg --files before explicit reads.
No runtime attempt was made. Shared resolver extraction preserves the already
implemented cell zero-bound policy; tissue uses the same resolver and existing
count recipe, with an ephemeral calibration revision only.

Native/consumer implementation review: shared resolver extracted without changing
cell count-recipe semantics; tissue factory and all native/SDK/desktop/viewer/
batch entry points are wired. No new persisted result fields or codecs were
added; only ephemeral TissueRequest calibration state changed, and source search
found no sizeof(TissueRequest) quota charge. Tiny-mask checks use independently
known 1/2/4-pixel components. Reuse existing cell regressions for the extraction.
Next run affected native builds and inspect failures before any retry; bindings,
stubs and installed flows follow after native contracts pass. No wheel/install
has run. Keep bounded search output to avoid repeated truncation.

Static-read attempt review: a nonexistent tissue_tables.cpp path was included in
a search alongside the real measurement_tables.cpp; no product/build failed.
That is a repeated filename-guessing error, so subsequent reads use only discovered
paths and narrow line ranges. The SDK tests now cover batch resume/recipe identity,
CSV, CLI and both desktop routes as well as physical conversion and saved replay.
The running native build is reused; no replacement build or install was started.

Native foundation review: the cached native build completed successfully,
including app/batch consumers. Tissue, cell_calibration and project_batch native
roles pass (3/3, native-tests.log). The shared cell resolver retains existing
boundary behavior. A helper-discovery search also passed PowerShell-only syntax
to rg; it failed read-only and is removed, with direct discovered paths used
instead. No failed build or numerical retry occurred. Next build the settled
bindings and region target in the same cache, stage once, generate stubs once,
and run the selected installed workflow. Preserve the first successful native
logs; no full qualification or new environment is needed.

Binding-stage review: native numerical gates passed; binding compilation remains
live in session 53853 (observed cl process CPU advancing), so no restart is
justified. Source tests now also include async calibration-stale rejection.
Plan adjustment: stage and verify the native product before making the single
wheel, so observer/setup corrections do not force another package. Reuse the
probe environment and preserve the final60a baseline in its existing stage.

Stage smoke attempt review: the binding build and CMake install succeeded and
the isolated probe imported the intended new-stage native module. The observer
then asked for a nonexistent BuildEnvironment.revision field; this is a harness
error, not a product failure. Inspect the exposed environment fields before the
corrected identity check. Reuse this stage/build; do not recompile or reinstall.

Stage verification attempt review: 121 checks passed, 7 GUI cases skipped and
one new CLI observer failed after a successful CLI save. It guessed checkpoint
files lived directly in the output root; the product uses a nested layout.
Inspect the existing CLI result/path contract and use that authoritative path
before retrying only the failed node. Keep all 121 passed results and the first
log; no native edit, rebuild, stub generation or install is justified. Then run
the two new actual desktop nodes explicitly with GUI enabled.

Staged workflow review: the corrected single CLI node passes using the emitted
report image path; 121 earlier passes are retained. Both new actual GUI nodes
pass with GUI explicitly enabled. Strict typing passes the positive fixture and
reports exactly the three intended invalid calls. One stub generation completed.
Waste/action: remove guessed checkpoint layouts and environment field names;
use observed API/report fields. Reuse the staged build and earlier passed checks.
Next freeze stubs/tests, create one cached wheel and install once; compare the
compiler-input identity and packaged members, then run only shipping checks
covering the final product. Broader skipped GUI cases remain unclaimed.
