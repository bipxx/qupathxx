# Current implementation task

The full updated M0–7 implementation goal remains active. All milestones remain
Partial. See the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
and [API delivery paths](api-delivery-plan.md) for the complete remaining scope.

The previous integrated TMA candidate `a29e4e1` passed engine `40161970725`,
Mac `40161976334` and portable `40161982786`, including artifact audits. It is
promoted to main; documentation commit `c07e778` publishes all receipts. The
validation ref is unchanged. No qualification remains live for that candidate.

Frozen candidate: `feature/annotation-migration-158` at `207f008`. Nested GeoJSON export
is shared by C++/Python/file/ImGui and actual QuPath preserves the captured six
objects, four parents, nucleus and point coordinates. Flat defaults are unchanged.
The recovery path-length failure is now fixed: shared physical-path conversion
covers private work/snapshots, locks, SQLite reopening, publication, discovery,
reviewed deletion, retention and rescue, while public logical paths remain exact.

Final installed engine `df0733c3782f99518e89d7e2f40787dbb2be3094` passes all
50 native suites and 305 installed cases with three declared host-capability
skips. The ten long-path cases also pass after a portable-layout assertion delta.
The passing integration root has the same 79-character absolute length as the
original failing root. Packaged stubs, strict positive/negative typing and two
actual pinned QuPath round trips pass. All 268 compiler inputs match the compiled
fingerprint, verified with one batched Git process. See the
[current local receipt](benchmarks/annotation-migration-recovery-local-windows.json)
and [migration contract](annotation-migration.md). The old failure receipt is
retained; no failed run is relabelled as passing.

The source-bound hierarchy/recovery gates are now wired into all three artifact
auditors, with 101 headless and 105 GUI required cases including 43 negative/
positive evidence controls. The shared QPTMA primitive retains its original
defaults. Positive typing checks require the four actual producer examples.
Local gate verification now passes 140 cases. All 55 bound source files match,
and old source/native artifacts are rejected for the new scope. See the
[qualification receipt](benchmarks/annotation-migration-qualification-local-windows.json).
Hosted engine `40166860999` failed one synthetic auditor fixture in each installed
configuration: it emitted a three-file typing log where the checker correctly
requires four. Mac `40166866585` passed its source-bound audit (49 native suites
per architecture); portable `40166872891` failed the same obsolete fixture in
both builders, so fresh consumers did not run. Both failed logs are retained.
The frozen checkout stays unchanged. This campaign does not qualify hierarchy
for main promotion.

Active integration source is now `feature/scientific-grid-159`. Commit `5474c3f`
corrects the stale typing fixture and adds an explicit rejection control; 89
affected/shared auditor tests pass locally (149 with broader inherited auditor
and mathematical checks). The shared
[scientific-grid contract](scientific-grid-contract.md) now has an independent
Fraction oracle and C++ geometry consumer. A standalone native harness passes
exact grid, rounding, context and admission checks; all 49 repository native suites
also pass in the intentional minimal build (GUI/Python/project/interchange/readers
disabled). Feature-disabled suites do not qualify those capabilities. These are
geometry checks, not scientific detector acceptance.
The [local geometry receipt](benchmarks/scientific-grid-geometry-local-windows.json)
retains raw tests, the 271-file compiler-input proof and the failed hosted log.

Source `7573a94` adds a bounded level-zero RGB sampler and owning nanobind grid/
result types. Native comparisons pass all 14 exact reference scenarios, including
fractional means, alpha/missing support and a large-origin reader window; reader
faults and cancellation reject. The sampler accumulates integer area weights and
rounds once to float64. Final installed engine `4bdfa98` passes 95 installed cases
with two explicit project-capability skips, all 49 native suites, packaged stub
checks, four positive typing examples and 19 expected negative errors (six new
grid, 13 inherited). The compiler-input lock covers 275 files with digest
`a748067e7301b332e7144c21fd1e393097bf697e3ea70a1d253a8cb985782516`.
This is an additive sampling utility. The new detector recipe, physical anisotropic
parameters, digest/archive/replay and required frontends remain next in the same
workflow; existing cell recipes and archive behavior are unchanged.
See the [sampling receipt](benchmarks/scientific-grid-sampling-local-windows.json)
for exact wheel/module/source identities and raw evidence. GUI, project,
interchange, GEOS and file readers were disabled in this local build; their
feature-disabled tests are not coverage for those capabilities.

Internal physical-kernel preparation now resolves per-axis calibration/sigma,
weighted squared distances and validity-normalized anisotropic Gaussian filtering.
An extreme-aspect reference exposed a nearest-owner error in the initial floating
intersection method; exact portable comparisons now preserve the owner even when
rounded total distances coincide. The regression is retained. Local verification
passes 36 exact distance scenarios, 517 wide-product comparisons, 1,024 exhaustive
native site scans, nine Decimal Gaussian references and 16 oracle/provenance tests.
All 49 minimal native suites pass, including old recipe/archive checks. These
kernels are not yet called by a new detector request or exposed as Python APIs.
See the [physical-kernel receipt](benchmarks/physical-grid-kernels-local-windows.json).

The float64 OD/stain path is now implemented through native/Python
`ScientificChannels`, retaining the grid, validity, mode, white and normalized
basis. It transforms the sampled RGB mean directly and keeps signed stain values.
Engine `bbf3f61` passes 163 installed cases with two declared project-capability
skips, all 49 native suites, packaged stub checks, four positive typing examples
and 24 expected negative errors. Native comparisons cover 44 independent Decimal
cases and 262,144 pre-refactor byte/float32 compatibility probes. The O(1) work
estimator accepts a grid before IO. See the
[channel-stage receipt](benchmarks/scientific-channels-local-windows.json).
No per-axis cell raster or new archive is exposed by this transform API.

Shared C++ raster integration now retains the complete explicit grid, separate
X/Y sample steps, capture-time provenance and exact edge mapping through GEOS
contours. Native construction rejects labels in context, missing support or
excluded ROI samples. Scalar-only access and old tissue retention/archive paths
reject these new masks. Input identity binds the decoded RGBA snapshot during
its existing read and the grid/ROI/transform/calibration/actual eligibility;
different source pixels remain distinguishable even when sampled means coincide.
Source `ff3f0a6` passes 49 GEOS-enabled native suites and 23 independent contract
tests. All 286 compiler inputs match the configured raw-byte fingerprint
`2892d84ff4bf1d0c85da05b410022ae8f3272c1c619eb3b7997d870a56f43c0b`.
See the [native raster receipt](benchmarks/scientific-raster-local-windows.json).
This layout change has not been rebuilt or qualified in Python/GUI/project
configurations; the older installed channels engine is not evidence for it.

The native grid cell recipe is now connected in source `956bd18`. A typed C++
request captures the scientific grid directly, including source snapshots without
natural-center/sample-count admission. Pixel or micrometre scales resolve per
axis; inclusive nucleus-area limits reuse calibrated product rules. The pre-IO
ledger includes float64 input/channels, kernels/heaps, masks/features, decoded
source and both contour budgets. Detection reads once, uses metric LoG/shape/body
operations and returns per-axis masks, compartment features and GEOS contours
through the existing CellJob lifecycle. Tiny ROIs and a bounded grid on a
10^12-by-10^12 source pass without full native raster allocation.

All 49 GEOS-enabled native suites and 27 contract tests pass, including 32 exact
LoG signs, 128 rational-distance expansion references, hand-calculated native
workflow cases and inherited recipe/archive checks. All 291 compiler inputs
match the configured fingerprint
`9b5a9463e12c0d08821ba4d35f4f1c3ac5b2deb4d92f3455590851f67038fd57`.
The [cell-stage receipt](benchmarks/grid-cells-local-windows.json) retains both
development failures and final evidence from that native detector stage.

Source `929cbd0` now implements QCL5 cell retention, record matching, publication,
verification and replay. The complete grid, typed requested/resolved parameters,
capture-time QPV/calibration, input hash and historical eligibility persist in
the run. Canonical decoding checks label/feature/domain consistency and reruns
body expansion against the historical metric. Loading does not reread pixels;
fresh replay detects changed pixels and rejects changed calibration. Project
external provenance must exactly match the embedded capture.

All 49 native suites pass with project storage and GEOS enabled; 30 independent
contract tests pass. Native checks include a Python-packed byte vector,
re-signed malformed records, a plausible-count but incorrect body mask,
rectangle/ellipse/hole and winding-policy geometry, real SQLite save/reopen,
fresh verification and a second save. All 294 compiler inputs match fingerprint
`3de23796c8f4d55478619dc91259110da2415761bac45891320c0931a5d49c23`.
See the [QCL5 receipt](benchmarks/grid-cell-archive-local-windows.json).
Python/GUI/interchange/readers/training remain disabled in this qualification;
their earlier installed artifacts do not qualify these layouts or this workflow.

Source `f8fbc98` connects grid-cell measurements to native classifier provenance
and CSV export. QCA5 carries typed grid manifests alongside tagged natural-grid
sources, validates resolved/calibration metadata and uses core sample limits for
upsampled results. CSV schema 5/6 preserves both axes, all grid fields, requested
and resolved units and captured calibration; grid rows leave scalar-only legacy
fields blank. Mixed natural/grid, quoted manual/unresolved and project rows use
the shared immutable/atomic export path. No-grid requests retain prior formats.

All 49 project/GEOS-enabled native suites and 33 contract tests pass, including
an independently packed QCR1/QCA5 vector, backend-free prediction snapshots,
mixed/upsampled sources, malformed descriptors, row/column checks and classifier/
CSV project save/reopen. The shared parameter serializer still reproduces the
independent QCL5 vector exactly. All 297 compiler inputs match fingerprint
`e3fdcb3991c06a83821e19e8930c3f945ac5f5ebf77b544a7f3c36d4e2c54598`.
See the [consumer receipt](benchmarks/grid-cell-consumers-local-windows.json).
Python scalar guards were edited but not compiled in this native configuration;
typed metadata exposure and installed validation remain unfinished.

The frontend integration below adds the typed request/result/source bindings
and managed viewer, CLI, ImGui and batch adapters. Finish current installed
workflow checks, generate/package stubs, and validate the final installed
artifact before the frozen complete source-bound qualification campaign.
Continue through the full original workflow;
retain the failed campaign evidence, and qualify a clean integrated candidate
after installed workflow checks. Do not transfer the old Mac acceptance to new
compiler inputs. Unicode glyph delivery, broader migration, scientific, licensing
and supported-release requirements remain open.

Frontend167 adds count-based ROI capture for native batch, managed viewer,
CLI and ImGui workflows; see the [API/workflow guide](grid-cell-workflows.md).
Typed nanobind request/result/classifier-source metadata and per-axis raster
accessors are installed and verified. ImGui grid controls and initial options
use the shared factory; preview geometry uses exact per-axis edges. Legacy
option conflicts reject before CLI project IO. Batch resume and QCL5/CSV
checkpoints preserve per-image calibration and the typed recipe.

The local Python/GUI/project/GEOS build passes all 50 native suites and the
33 independent contract tests. Six new installed integration tests cover metadata
ownership, batch replay/resume, classifier-source lifetime, actual grid preview/
publication and a managed Python producer retaining/exporting/saving a run.
The final installed wheel passes 50 serial desktop tests plus three additional
desktop checkpoint/table tests. Regression checks pass 224 tests with 20 initial
skips: 13 opt-in desktop cases are subsequently executed, while seven require
disabled OpenSlide/pyramid, interchange or classifier-training capabilities.
Five positive typing files pass; six negative files produce exactly the expected
31 errors. Packaged sources/stubs match and regenerate from the installed module.

The final stub-packaging pass preserves native module bytes exactly; both wheels
share compiler fingerprint
`dbd02bd3e38a4953fc79dca14ad75cbfc0a1529fdbbd57fc564ccd31ca975e02`.
See the [frontend receipt](benchmarks/grid-cell-frontends-local-windows.json).
These local checks do not qualify disabled readers/interchange/training, other
platforms, scientific segmentation tolerances, hardware/performance, rights,
licensing or supported release requirements. All M0-M7 remain Partial. Next:
freeze the integrated candidate for complete required platform cohorts, continue
scientific/pixel-source acceptance and the remaining original workflow.

CI integration168 adds an explicit 106-case scientific-grid installed cohort,
nine named native suites with capability-specific markers, and shared strict
typing checks for five positive files plus all three scientific negative fixtures.
Headless runs declare exactly two desktop skips; GUI runs require both to pass.
Engine, Mac native and portable builder/consumer auditors bind the scientific
sources and raw fixture inventory to the dispatched commit. New source checking
uses one batched Git blob read; synthetic acceptance fixtures advance with the
runner's five-file typing contract and reject stale three/four-file evidence.
Local checks pass 185 affected auditor cases and 104 installed headless cases
with two declared desktop skips. The new auditor also accepts the retained
106-case installed GUI cohort and the nine native suite markers from frontend167;
these are explicit reuse checks, not new platform acceptance. The frozen
candidate is `feature/scientific-workflow-168` at `5ebdb96`; its separate checkout
must remain unchanged. The complete campaign is dispatched as
[engine 40185494002](https://github.com/bipxx/qupathxx/actions/runs/40185494002),
[Mac 40185498517](https://github.com/bipxx/qupathxx/actions/runs/40185498517), and
[portable 40185503588](https://github.com/bipxx/qupathxx/actions/runs/40185503588).
Engine requests live reference and extended readers. Independent gate-mode
concurrency keys preserve all three runs. Their acceptance remains pending;
poll these exact IDs rather than dispatching replacements. See the
[preflight receipt](benchmarks/scientific-grid-ci-preflight-windows.json) for
source hashes, auditor/installed checks and recorded dispatch attempts. Full
M0-M7 scope remains open.

Qualification168 has exposed concrete defects and cannot be accepted:
Linux headless and both portable builders report platform-dependent generated
stubs, while Mac x86_64 fails a mixed `auto` declaration in QCL5 decoding.
Windows headless and the two Linux native jobs pass at that source; remaining
old jobs are retained for evidence, not restarted or relabelled.

Correction169 adds the missing string and optional nanobind casters and splits
the size/count declarations without changing the archive format or numeric recipes.
The old wheel fails two new runtime boundary tests; the corrected Windows wheel
passes them. Public signatures now expose `str` and `ScientificGrid | None`.
Stub generation rejects unbound C++ annotations, and packaged/strict typing
checks exercise these fields. The installed scientific cohort now has 107 cases.
Local checks pass 50 native suites, 108 installed checks with two declared desktop
skips, 50 desktop cases, and 193 auditor/signature-guard cases. A compile probe
using the actual archive declaration with distinct Darwin-like unsigned aliases
fails before the fix and passes after; it is not full Mac qualification. The
previous 56 native artifacts were hash-verified and retained before incremental
cache reuse. Source `3fc3bcb` and its [correction receipt](benchmarks/scientific-grid-portability-fix-local-windows.json)
record these checks. A corrected frozen candidate still needs complete platform
gates; the failed source168 evidence remains separate.

Earlier task narrative is preserved in [history](current-task-history.md).
