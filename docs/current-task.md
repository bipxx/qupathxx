# Current implementation task

The full updated M0–M7 implementation goal remains active. All milestones remain
Partial. Use the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
and [API delivery paths](api-delivery-plan.md) for the complete remaining scope.

One frozen candidate: integrated TMA source
`a29e4e1e49048488dd08dbd30e2efc83df1b8b55`, public ref
`validation/qptma-integrated-156`. Exact hosted engine `40161970725`, Mac
`40161976334` and portable `40161982786` runs are source-matched. At the last direct checks,
engine is in progress, Mac completed successfully, and portable remains queued.
Mac artifact acceptance is separate from its successful job conclusion. The
existing monitor is retained. Do not mutate the candidate or dispatch
replacement runs after an observation timeout. Promotion awaits complete
source-matched acceptance.

One integration source: `feature/annotation-migration-158`. Native/Python/file
and ImGui nested GeoJSON export now preserve the captured QuPath hierarchy.
Flat defaults remain unchanged. Final installed source
`50b44b099e0b67e60ea9e7830e4692017973a7fe` passes 50 native suites, 51
hierarchy/interchange checks, packaged stubs, strict positive/negative typing
and actual pinned QuPath import of Python and GUI exports. Four parent links,
six objects, nucleus and point coordinates match. The capture is inspected.
See the [workflow contract](annotation-migration.md) and
[partial local receipt](benchmarks/annotation-migration-nested-local-windows.json).

The broader integration run is not accepted: 215 passed, one failed, one skipped.
`test_project_recovery.test_unsaved_default_cache_survives_restart` exposes a
Windows staging path exceeding the legacy limit under the deeper test directory.
The OpenSlide startup case is an explicit local capability skip, not reader
qualification. The failure and raw evidence are retained.

Next: fix recovery physical path access, reusing the established Windows
`access_path` behavior in `cpp/project_batch.cpp`. Preserve public logical paths,
session ownership/locks, recognized work-directory inventory, cancellation,
non-overwriting rescue and maintenance. Check long work/snapshot/sidecar paths
and abandoned-work cleanup; do not change token naming or reduce the test root
to hide the failure. Then rerun the unchanged installed integration gate, add
source-bound hierarchy cohorts/negative controls to the final platform auditors,
and qualify when the frozen-candidate slot is available. Unicode glyph delivery
and broader migration/scientific/platform/release work remain open.

Earlier task narrative is preserved in [history](current-task-history.md).
