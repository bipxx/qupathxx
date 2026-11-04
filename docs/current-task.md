# Current implementation task

Implementation follows the [R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and its [ordered remaining slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is complete; R1 inventory reconciliation is complete with owner decisions open.
R2-SI1 and [R2-META1](benchmarks/binaryonly-ome-r2/README.md) have local installed
evidence. All M0-M7 remain Partial.

Next task: **R2-UNIT2, remaining convertible non-SI OME length units**.

- Outcome/entry point: normalize eligible physical length units through the
  existing shared TIFF quantity parser for primary and companion metadata.
- Prerequisite: confirm symbols/factors against the pinned OME reference;
  no biological, hardware or release decision is needed for this engineering task.
- Checks: independent conversions, affected installed reader/companion cases,
  unchanged samples and existing SI/legacy arithmetic; retain nonphysical
  pixel/reference-frame and positive-finite/overflow behavior. Select native
  targets from the actual change, with one cached build/install after code settles.
- Stop: close this conversion gap with scoped evidence and unchanged public
  signatures; then select one demonstrated eligible vendor/representation gap
  under R2. No full platform campaign is triggered by this task.

Latest task closeout (2026-11-04, completion-plan update):
Outcome/evidence: the delivery plan now orders the remaining R1-R16 slices,
records easiest shared implementation routes and stopping/dependency rules.
Waste/cost: documentation only; no builds, test campaigns or fixture recaptures.
Removed a stale instruction to execute a completed B6 qualification. Plan
change/next action: finish UNIT2, then select a bounded reader gap; avoid an
unbounded reader campaign before annotation/persistence and analysis work.
After every task, including a failure or blocked attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update the
next action/checks before starting it. Preserve raw failures and existing evidence.

[Qualification308](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md)
retains its own frozen-source scope. New code does not inherit that acceptance.
Biological labels/tolerances, eligible hardware, project license and exact release
support remain owner decisions affecting only their dependent claims. Only
openly licensed readers and required codecs are eligible. Requirements remain in
[project-plan](project-plan.md); coverage remains in [feature-matrix](feature-matrix.md).
