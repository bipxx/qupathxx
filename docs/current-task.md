# Current implementation task

The user resumed implementation of the updated plan on 2026-10-25. The active
workflow is direct QuPath TMA exchange through project, Python, CLI and ImGui.

Use the [workflow inventory](feature-matrix.md#current-execution-state-2026-10-25)
for current source/capability status, [project plan](project-plan.md) for the full
M0–M7 scope and [API delivery paths](api-delivery-plan.md) for the next workflow.
These files separate code availability, engineering/scientific evidence and
supported-release requirements. All milestones remain Partial.

The final project-enabled wheel gate passed. The integrated TMA desktop suite
now passes 148 installed cases without skips (80 required QPTMA cases, nine new
and 17 existing TMA GUI cases, project/save/recovery, stubs and native-stack
checks), plus all 50 native suites and strict typing. Actual pinned QuPath
imports the GUI-produced file with all supported fields exact. Captures have
been inspected. See the [local receipt](benchmarks/qptma-integrated-local-windows.json).

The next step is one frozen exact-source engine/Mac/portable qualification,
using the now mode-aware CI concurrency key. Do not mutate its source or restart
a live run after an observation timeout. Qualification does not close scientific,
hardware, licensing or supported-release gates.

The old current-task narrative is preserved in [history](current-task-history.md).
It is not the current backlog or a source for present completion claims.
