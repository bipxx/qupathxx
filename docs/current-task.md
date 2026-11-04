# Current implementation task

Use the [R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and its [ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is accepted; R1 inventory is reconciled with owner decisions open.
R2-SI1/META1/UNIT2 and [CODEC1](benchmarks/tiff-jpeg-r2/README.md) have local
installed evidence. All M0-M7 remain Partial; other R2 format gaps remain open.

Next: **R4-MERGE1, merge annotation objects**.

- Outcome/entry: add a true merge through immutable ROI union, the document
  command journal, bindings and existing selection/ImGui controllers. Current
  apply_boolean retains target/operands; keep that behavior intact.
- Prerequisite: capture pinned QuPath0.7.0 mergeAnnotations for area/point,
  common/mixed classes, metadata, descendants and selection. Define an explicit
  child/identity policy compatible with native containment and history.
- Checks: independent geometry and pinned object outcomes; atomic failure,
  compound undo/redo, metadata/selection and save/reopen; affected installed
  Python and desktop/managed consumers. Build/install once after APIs settle.
- Stop: complete the merge user flow and its waste review, then choose the
  next actual R4/R5 gap. No reader or full platform campaign replay.

Latest closeout (2026-11-04, R2-CODEC1):
Outcome/evidence: [JPEG TIFF closeout](benchmarks/tiff-jpeg-r2/README.md),409
targeted passes/6 declared desktop skips, two native suites, two coexistence
workflow cases and eight exact Linux codec-only decodes. All62 installed members,
notices and stubs verify. Linux app/Mac support are not qualified by these checks.
Waste/cost: one cached six-target package build/install and one Linux codec build.
Avoided a new test codec dependency with a scalar fixture writer. Corrected a
parent/child CMake-scope mistake in the Linux harness; retained the failure and
reused all passing Windows checks. No production rebuild after that correction.
Plan change/next action: read generated feature headers for harness admission;
preserve products with explicit cache_path; move to MERGE1 instead of expanding
this into an all-codec campaign. Missing NASM leaves SIMD/performance unclaimed.

Apply the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
after each task, including failures, and update next action/dependencies/checks
before starting it. [308](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md)
retains its frozen-source scope. Biology/tolerances, hardware, project license
and exact release support remain owner decisions. Only openly licensed readers
and required codecs are eligible.
