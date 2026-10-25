# Current implementation task

The full updated M0–M7 implementation goal remains active. All milestones remain
Partial. See the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
and [API delivery paths](api-delivery-plan.md) for the complete remaining scope.

The previous integrated TMA candidate `a29e4e1` passed engine `40161970725`,
Mac `40161976334` and portable `40161982786`, including artifact audits. It is
promoted to main; documentation commit `c07e778` publishes all receipts. The
validation ref is unchanged. No qualification remains live for that candidate.

One integration source: `feature/annotation-migration-158`. Nested GeoJSON export
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
Next: finish source-bound local gate verification, freeze the integrated source
and run its full engine/Mac/portable qualification once. Do not claim platform
acceptance from local Windows tests. Unicode glyph delivery, broader migration,
sampling/scientific, licensing and supported-release requirements remain open.

Earlier task narrative is preserved in [history](current-task-history.md).
