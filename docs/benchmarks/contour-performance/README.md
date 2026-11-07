# Label-local contour performance - 2026-11-07

Delivered the selected follow-up to the [matched ROI benchmark](../candidate5-performance/README.md).
A private native change gives GEOS each nominal label's boundary graph instead
of one large graph. The general scientific-grid, ordinary tissue, pixel and
streaming routes retain the existing shared-network path. No public API,
archive schema, binding interface, wheel or published release changed.

## Why this change

Temporary phase timers on the existing 2,237-cell ROI measured polygonization
at about 160-180 ms for nuclei and 225-250 ms for bodies. Intersections together
cost about 120 ms; final geometry construction also remains significant. The
candidate targets the largest stage without replacing GEOS topology handling
or removing clipping/validation. [Raw phase log](profile-contours.log) includes
project loading followed by three direct contour probes.

Existing adjacent-label tags identify which edges belong to each label. The
candidate preserves their order, polygonizes each smaller graph, selects faces
by an interior-point lookup in the original raster, then uses unchanged world
mapping, clipping, area accounting and geometry validation/preparation. Background
and enclosed other-label faces are excluded. General masks keep their prior path.
The existing conservative edge/component work allowance covers the temporary
pointer lists; edge/face limits, cancellation and retained-geometry admission stay.

## Results and limits

[Machine-readable results](summary.json) contain all stage medians/ranges and
exact comparisons. Each block has one warmup and three measured workflows, using
the same existing CMU-1 ROI and recipe. No build ran during timed blocks.

| Median, milliseconds | Original before | Label-local candidate | Later original control |
| --- | ---: | ---: | ---: |
| Native detection stage | 1,450 | 1,219 | 1,720 |
| Native project reopen | 1,598 | 1,354 | 1,628 |
| Complete native workflow | 3,215 | 2,988 | 3,564 |

The complete workflow was about **7% faster against the first control and 16%
against the later control**. Initial direct contour probes fell from 339/440 ms
(nuclei/bodies) to 270/324 ms; final candidate probes were 284/340 ms, while a
later original probe was 413/667 ms. Host timings drift substantially. These
are local observations, not a statistical or universal speed guarantee. They
support retaining this bounded optimization; they do not establish a new ratio
against QuPath's earlier timing or imply superiority. No new RAM claim is made.

All four warmup/measured native comparisons preserve exact RGB bytes, nucleus
and body label arrays, calibration/area, 2,237 cells and full density CSV across
both controls and the candidate. Candidate partitions also match the frozen
QuPath masks exactly. Ordered contour labels, pixel counts and full WKT hashes
match across all four direct-probe blocks. Save/reopen succeeds in each workflow.
The prior QuPath output is reused for correctness; QuPath was not retimed.

The new regression compares label-local nominal and general scientific-grid
contours on 84 patterns, including all 81 two-label/background 2x2 patterns,
checkerboard contacts, nested holes/enclosed labels and disconnected parts.
It checks exact ordered WKT, labels and pixel counts. The first test setup used
an extra context pixel in the reference grid, causing a dimensions refusal;
correcting both axes to zero context resolved it without changing assertions.
[Failed setup log](regression-tests.log) is retained. The final five affected
suites pass in 2.15 seconds: tissue_contours, cells, cell_records, geometry and
geometry_edits. Profiling instrumentation was removed from the final source.

## Identity, reproduction and next boundary

[Receipt](receipt.json), [522-input source manifest](source-inputs.sha256),
[runtime manifest](runtime.sha256) and [coverage](../../contour-performance-coverage.md)
identify the measured module. Its compiled revision is 64986907+dirty and raw
source tree is `08f5967e9e7de1109a00f9985c9c98344d1c7ab9e6523413d9f7d76557d6c756`.
The source commit adds documentation after the build; the raw compiler-input
manifest owns measured identity. Only private CPP/provenance and the new native
test needed compilation; cached binding objects were reused.

From the active checkout, with the retained runtime and a fresh output folder:

```powershell
& ../../../.venv/Scripts/python.exe docs/benchmarks/candidate5-performance/native.py ../../fixtures/CMU-1.svs ../../contour-performance/native-new ../../contour-performance/runtime-candidate --expected-tree 08f5967e9e7de1109a00f9985c9c98344d1c7ab9e6523413d9f7d76557d6c756 --context 0 --label label-local-contours
& ../../../.venv/Scripts/python.exe docs/benchmarks/candidate5-performance/contours.py ../../contour-performance/runtime-candidate ../../candidate5-performance/native-geometry/0/study.qpxproj ../../contour-performance/probe-new.json
& ../../../.venv/Scripts/python.exe docs/benchmarks/contour-performance/compare.py ../../contour-performance ../../candidate5-performance
```

The last command summarizes the preserved named blocks and asserts equality;
it does not launch another benchmark. Temporary profiling can be reproduced
on parent 64986907 with the [saved patch](temporary-profiling.patch); its runtime
and source file remain local. Repository log copies normalize whitespace; raw
logs, projects, masks and comparison runtimes remain under build/contour-performance.
No cleanup command was retried after the earlier policy rejection.

This completes C1-C4. Polygonization is improved but clipping, geometry creation
and eager reconstruction on reopen remain costs. Further optimization or a binary
release requires a selected outcome; candidate5's newly discovered archive
limitation remains in its immutable binaries. These development sources contain
the prior repair and this optimization.
