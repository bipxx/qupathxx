# ImGui density resume coverage

Selected outcome: previous-GUI-run folder, unchanged result reuse, changed input
recomputation and explicit UI/CSV/report labels. All10 requirements are closed.

| Requirement | Evidence |
| --- | --- |
| Optional previous run and separate new output controls | Actual desktop compute/resume route; nested-folder rejection tests |
| Exact engine and native captured-recipe eligibility | Source/compiler/config/platform/GEOS comparator; changed engine/geometry/calibration tests |
| Smaller limits use fresh native admission | Reduced work-budget test recomputes both images |
| Unchanged results skip density kernels | Native progress observer records0 kernel calls; desktop0 computed/2 reused |
| Reuse references original checkpoints, without pixel/QPX copies | Native chain test and desktop folder inventory |
| Partial run needs no final report | Per-image-only receipt test reuses1/computes1 |
| Corrupt receipts/checksums/stale saved geometry fail explicitly | Duplicate JSON, changed file bytes and native replay refusal tests |
| Relevant versus unrelated edits handled correctly | Metadata rename reuses2; changed window computes1; calibration computes affected image |
| Clear computed/reused labels and immutable previous run | Actual saved/reused rows, summary, CSV reuse column; all baseline file SHAs unchanged |
| Existing cancellation/error/source/publication contracts retained | Existing density_panel suite and new density_resume suite both pass |

[Native tests](../cpp/tests/density_panel_resume_tests.cpp),
[test log](benchmarks/gui-density-resume/tests.log),
[desktop/source receipt](benchmarks/gui-density-resume/desktop.json),
[computed screenshot](benchmarks/gui-density-resume/desktop-computed.png) and
[reused screenshot](benchmarks/gui-density-resume/desktop-reused.png).
Both affected suites pass in1.64s. Density kernels/codecs, SDK/bindings, published
candidate4 and the Python runner remain unchanged. Native replay here validates
captured inputs; reuse is not numerical re-verification. GUI receipt format and
engine eligibility are documented in the [guide](gui-project-density.md).
