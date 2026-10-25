# Current implementation task

The full updated M0–M7 implementation goal remains active. All milestones remain
Partial. See the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
and [API delivery paths](api-delivery-plan.md) for the complete remaining scope.

One frozen candidate: integrated TMA source
`a29e4e1e49048488dd08dbd30e2efc83df1b8b55`, public ref
`validation/qptma-integrated-156`. Engine `40161970725` and Mac `40161976334`
completed successfully and their exact-source artifact audits passed. Portable
`40161982786` remains queued. The existing monitor is live; retain its exact
handles. Do not mutate that source or dispatch replacements after an observation
timeout. Promotion still awaits portable acceptance and all source-matched gates.

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

Next: require source-bound hierarchy/recovery cohorts in the engine/Mac/portable
artifact auditors, with missing/duplicate/skipped/mixed-source and disabled-
capability negative controls. Reuse small existing evidence primitives; preserve
all inherited scientific and runtime gates. Then freeze/qualify the integrated
source when the one-frozen-candidate slot is available. Do not claim platform
acceptance from local Windows tests. Unicode glyph delivery, broader migration,
sampling/scientific, licensing and supported-release requirements remain open.

Earlier task narrative is preserved in [history](current-task-history.md).
