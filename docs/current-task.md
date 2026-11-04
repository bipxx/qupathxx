# Current implementation task

Use the [R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and its [ordered slices](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work).
R0 is engineering accepted; R1 inventory is reconciled with owner decisions open.
R2-SI1, R2-META1 and [R2-UNIT2](benchmarks/ome-nonsi-r2/README.md) have local
installed evidence. All M0-M7 remain Partial.

Next: **R2-CODEC1, JPEG-compressed quantitative TIFF**.

- Outcome/entry: add the missing codec route through cmake/TiffReader.cmake and
  the existing native TIFF reader, preserving raw/display separation.
- Prerequisite: pin an openly licensed decoder/dependency/notices route and
  independent scalar/RGB fixtures. Inspect static symbol/runtime coexistence
  with OpenCV/OpenSlide before choosing integration; no new reader model.
- Checks: reproduce current refusal, then compare affected decoded samples,
  mapping/edge behavior, invalid inputs and packaged runtime/notices. Use one
  settled cached build/install; extend host checks only for a concrete link risk.
- Stop: finish the declared JPEG slice and its affected consumers, review waste,
  then move to R4/R5 unless the capability inspection finds a blocking reader gap.
  Other codec/photometric forms remain recorded separately.

Latest closeout (2026-11-04, R2-UNIT2):
Outcome/evidence: [ten conversions and pinned reference](benchmarks/ome-nonsi-r2/README.md),
380 targeted passes including114 unit cases, six declared desktop skips and two
native suites. Installed/source hashes agree; API/stubs unchanged.
Waste/cost: one six-target cached build/install; reused shared parser, fixtures
and admission. Initial probe used an unavailable Groovy JSON module; bundled
Gson succeeded, with both attempts retained. No passing tests were rerun.
Plan change/next action: reuse that serializer for later captures, and address
the explicitly disabled JPEG route as one slice before editing/persistence.

Apply the [waste review](api-delivery-plan.md#task-closeout-and-waste-review)
after each task, including failures, and update next action/dependencies/checks
before starting it. No local task completion triggers a full qualification.
[308](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md) retains
its frozen-source scope; changed code does not inherit it. Biology/tolerances,
hardware, project license and exact release support remain owner decisions.
Only openly licensed readers and required codecs are eligible.
