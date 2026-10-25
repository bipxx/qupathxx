# Current implementation task

Implementation of the full updated M0–M7 plan remains active. All milestones
remain Partial; the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
and [API delivery paths](api-delivery-plan.md) retain the complete scope.

One frozen candidate: integrated TMA source
`a29e4e1e49048488dd08dbd30e2efc83df1b8b55`, ref
`validation/qptma-integrated-156`. Its local gate passes 148 installed cases,
50 native suites and strict typing, with inspected captures and actual pinned
QuPath import of the GUI export. Exact hosted engine `40161970725`, Mac
`40161976334` and portable `40161982786` runs are still queued at the last direct
check. The existing monitor remains live. Do not change that source or duplicate
its dispatches. Promotion awaits complete source-matched acceptance.

One integration source: `feature/annotation-migration-158`, based on the frozen
TMA candidate. Bounded preparation for queue item 4 now captures actual QuPath
hierarchy and unsupported-property exports. Twelve new reference/persistence
cases and three real ImGui cases pass; with existing interchange tests the
headless regression is 30 passed, zero skips. These checks explicitly use the
accepted main154 full-feature Windows wheel, whose interchange implementation
is unchanged here. They do not qualify candidate158 binaries or other platforms.
See [migration contract and remaining gaps](annotation-migration.md) and the
[local evidence receipt](benchmarks/annotation-migration-local-windows.json).

The next implementation step is an explicit hierarchy-preserving export route
back to QuPath, with target/loss semantics in Python and ImGui. Ordinary flat
QuPath import currently drops all four tested parent links. Keep native flat
export defaults and project identity intact; reuse the existing native exporter
rather than adding a Python model or another file format. Add bounded/depth,
subset-parent, malformed and actual upstream round-trip checks before the final
installed integration gate. The captured default-font alpha glyph gap also
remains tracked; Unicode data is preserved.

The old current-task narrative is retained in [history](current-task-history.md).
