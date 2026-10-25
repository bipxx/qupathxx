# Current implementation task

Integrated TMA source `a29e4e1e49048488dd08dbd30e2efc83df1b8b55` is now promoted
to public main after exact-source engine, Mac and portable artifact acceptance.
See the [combined local/hosted receipt](benchmarks/qptma-integrated-local-windows.json).
The frozen validation ref remains unchanged.

The accepted runs are engine `40161970725`, Mac `40161976334` and portable
`40161982786`. Engine includes 80 required QPTMA cases in each installed cohort
and nine desktop cases. Mac covers 49 native suites on arm64 and x86_64;
portable covers both Python builders and fresh consumers. These are engineering
acceptance claims; all M0–M7 milestones remain Partial.

Implementation continues on the public
[annotation/migration branch](https://github.com/bipxx/qupathxx/tree/feature/annotation-migration-158).
It contains nested QuPath hierarchy export and a Windows recovery long-path fix,
with passing local integration, typing and actual QuPath round trips. Those
changes are not included in this TMA main promotion; main retains the known
recovery long-path limitation until the next candidate is qualified. The next
step is source-bound hierarchy/recovery auditor cohorts and one complete
engine/Mac/portable qualification of that integrated source.

Use the [full project plan](project-plan.md), [workflow inventory](feature-matrix.md)
and [API delivery paths](api-delivery-plan.md) for remaining scope. Scientific,
hardware, fixture rights, licensing and supported-release gates remain open.
The old task narrative is preserved in [history](current-task-history.md).
