# Current implementation task

Implementation is active under the [R0-R16 queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
R0 is complete; R1 reconciled all23 Partial rows. R2-SI1 and R2-META1 have local
installed evidence. Next: R2-UNIT2 confirms the remaining convertible non-SI
OME length-unit factors, then extends the existing quantity parser/tests.
Keep pixel/reference-frame units nonphysical and preserve supported arithmetic.

[R2-META1](benchmarks/binaryonly-ome-r2/README.md) at native `4e7b6e8d` resolves
BinaryOnly headers through XML or TIFF masters, tracks metadata in the same
guarded source set, and preserves project/CLI/desktop/batch semantics. Observer
`51b51725` records the final independent fixtures and exact admission contract.
[Qualification308](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md)
retains engineering acceptance at its own frozen source, not the newer reader.

Latest task closeout (2026-11-04, R2-META1):
Outcome/evidence:51 BinaryOnly cases,355 distinct targeted passes, two native
suites and exact OME admission of184 installed/six desktop cases; hashes and59
installed members verify, stubs unchanged. Waste/cost: one six-target build,
shared guards/parser/codecs/controllers and existing fixture/desktop helpers.
Two failed batch expectations were corrected after checking saved calibration
ownership; only changed cases were rerun on the same product. Plan change/next
action: preserve semantic hash-only reuse, select affected targets, skip desktop
fixture generation before capability admission, and proceed with R2-UNIT2.

All M0-M7 remain Partial. Biological labels/tolerances, eligible hardware,
project license and supported-release decisions remain explicit without blocking
independent engineering. Mac Python/GUI/OpenSlide, broader context/features,
whole-slide resources/seams and general speed remain open. Review
[waste after every task](api-delivery-plan.md#task-closeout-and-waste-review).
The [project plan](project-plan.md), [inventory](feature-matrix.md) and
[history](process-status-history-through308.md) retain their distinct roles.
Only openly licensed readers and required codecs are eligible.
