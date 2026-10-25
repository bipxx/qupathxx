# Current implementation task

The full updated M0–M7 implementation goal remains active. All milestones remain
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
per architecture); portable `40166872891` is still observed through its original
handle. The frozen checkout stays unchanged. No hierarchy main promotion is
authorized by this incomplete campaign.

Active integration source is now `feature/scientific-grid-159`. Commit `5474c3f`
corrects the stale typing fixture and adds an explicit rejection control; 89
affected/shared auditor tests pass locally (149 with broader inherited auditor
and mathematical checks). The shared
[scientific-grid contract](scientific-grid-contract.md) now has an independent
Fraction oracle and C++ geometry consumer. A standalone native harness passes
exact grid, rounding, context and admission checks; all 49 repository native suites
also pass in the intentional minimal build (GUI/Python/project/interchange/readers
disabled). Feature-disabled suites do not qualify those capabilities. These are
geometry checks, not native float sampling
or scientific detector acceptance. Python bindings, the sampler, new recipe/
digest/archive/replay and all required frontends remain next in the same workflow.
The [local geometry receipt](benchmarks/scientific-grid-geometry-local-windows.json)
retains raw tests, the 271-file compiler-input proof and the failed hosted log.

Next: complete that bounded sampling path using the existing native authority,
retain the failed campaign evidence, and qualify a clean integrated candidate
after installed workflow checks. Do not transfer the old Mac acceptance to new
compiler inputs. Unicode glyph delivery, broader migration, scientific, licensing
and supported-release requirements remain open.

Earlier task narrative is preserved in [history](current-task-history.md).
