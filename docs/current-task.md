# Current implementation task

Follow the [single R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue),
[concrete next actions](api-delivery-plan.md#concrete-next-actions-and-completion-gates)
and [change-based validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. SOURCE1 has local engineering evidence only.

Current: **R5 recovery-gap reconciliation**.

- Outcome: distinguish delivered crash/recovery/cleanup behavior from a genuinely
  missing fault boundary. Correct stale active claims before scheduling new work.
- Entry/dependency: read `tests/test_project_save.py`, `test_project_recovery.py`,
  `test_recovery_work_cleanup.py`, recovery docs and existing receipts. Native save
  hot-journal/process-death and abandoned-autosave cases already exist; SOURCE1
  adds association rollback/migration checks. No new scheduler or cleanup system.
- Next action: match each claimed missing recovery behavior to its actual
  assertion/evidence. Record a concrete uncovered boundary, or advance to a
  demonstrated edit/property/plane gap if current recovery scope is satisfied.
- Checks/reuse: documentation consistency and existing evidence inspection first.
  No build or passing-test replay for reconciliation. Add a deterministic fault
  case only for a named uncovered boundary, using the existing installed product
  when compiler inputs are unchanged. Preserve prior project/checkpoint on failure.
- Stop: covered versus missing is explicit and the next implementation outcome is
  selected. Process death does not establish hardware power-loss or network-FS
  durability; these claims need their own intended support scope.

Latest task closeout (2026-11-04, SOURCE1):
Outcome/evidence: [source verification](benchmarks/source-verification-r5/README.md)
now completes native/Python/ImGui review/apply/save/reopen with schema15. Final
product identity, stubs,18 selected passes/three declared desktop skips, project
native suite and three distinct corrected desktop outcomes pass; original687 model
passes/ten skips, typing and four native suites retain their source scope.
Waste/cost: **reuse** unchanged-core results and final installed product; **remove**
a full suite/platform replay and GUI-fix stub regeneration; **retain** final
identity checks for the distinct corrected build. Three native attempts took
668.13s failed,546.58s successful and70.11s GUI correction; native-test-only repair
17.10s. Two candidate installations total. Failures remain preserved.
Plan change/next action: source review is locally verified; inspect existing
recovery coverage before creating another fault test. Keep primary-only compound
coverage and immutable historical-association limits explicit. Broader R5 is open.

Then complete demonstrated R4/R5 gaps, R6/R7, R8-R10, R11, R3, R12 and R13.
Pull eligible R2 inputs forward as required; retain R14-R16 full-target work.
After every bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependency, reuse and necessary checks **before** continuing/retrying.
Keep only the latest closeout here. Only openly licensed readers/codecs are eligible.
