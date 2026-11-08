# Native paired-raster identity optimization

The nominal nucleus/body raster-construction stage is **27.2% faster** in the
isolated alternating-order probe: **21.28ms ->15.49ms**, saving about **5.79ms**.
The native detector now computes their shared immutable source/domain digest once.
Each mask retains independent label/domain/counter/budget/component and expected-
identity validation. Canonical hash bytes, output values and resource charges are
unchanged. [Stage samples](isolated-raster-pair.jsonl), [summary](isolated-summary.json).

There is **no demonstrated end-to-end speedup** from the full A/B/A block:
329.8ms baseline-before,334.0ms candidate,330.0ms baseline-after. Other-process load
was31.2%,36.4%,35.3% of logical capacity over the engine lifetimes. The overlapping
samples and larger pipeline costs can obscure this small stage reduction. Retain
all results; do not repeat the full run to obtain a favorable median or extend
this result into QuPath parity. [Comparison](comparison.json).

## Why this change

A temporary private native profile measured RGB15.1ms, domain5.1ms, conversion3.3ms,
Gaussian4.0ms, Laplacian1.2ms, nucleus processing86.4ms, expansion15.3ms,
features21.7ms, raster construction20.9ms and contours129.1ms (trace about122ms,
containment about7ms). These are diagnostic stage observations including logger
overhead, not final candidate timings. [Profile](profile.log), [temporary patch](profile.patch).

Contours remain the largest stage, but prior shortcuts changed ordered geometry.
This batch keeps their clipping/topology path intact and removes a measured duplicate
identity pass instead. `ScientificRasterKernel::make_pair` synchronously receives
one immutable source/input and both owned label vectors. The first mask computes
its digest normally. A **private** helper reuses that internally produced value
for the second mask; neither public SDK nor Python can supply a trusted digest.
Expected external digest strings are still compared independently. General rational-
grid masks retain their original construction path. No persistent cache, extra
workers, public binding, schema, scientific algorithm or tolerance was added.

The isolated probe alternates separate-then-paired / paired-then-separate within
one process using the actual512x512/2,237-cell saved fixture. Four warmup pairs
precede12 measured pairs. Label copies/allocations are charged to both paths;
full result comparisons occur after timers. This establishes the specific stage
benefit when the full-block measurements did not resolve an overall gain.

## Correctness and source

[Three affected suites](diagnostic-removal-tests.log) pass (2.23s), and the
[final paired test](final-pair-tests.log) passes after developer-probe edits. New
cases compare nominal, fractional/context and general raster pairs with separate
construction; exact per-mask work limits remain enforced. Wrong expected digests,
changed decoded source, invalid/outside/non-dense/short labels and cancellation
refuse. Existing clipping/topology, archive, scientific numeric and cell checks
are retained rather than replaced by implementation assertions.

Full baseline/candidate/baseline outputs retain exact RGB, both mask arrays, full
density CSV, ordered nucleus/body WKT and all stored feature values. All2,237 Cells
remain. A fresh candidate replay after archive reopen verifies through the native
request/result contract. [Independent checker](compare.py), [installed check](installed-check.json).
The new test file is separate so value-heavy fixtures do not inflate an existing
large Windows stack frame.

[Final receipt](receipt.json), [568-input manifest](source-inputs.sha256) and
[generated build manifest](build-manifest.hpp) match installed tree
`a3cbffa4ae17136523f40464a09acfc5504a8132231694cfcd4d82efa8b3f6e7`.
Full timed candidate source is recorded separately; only opt-in test/probe code
and provenance changed after that timing, with the product implementation unchanged.
All38 [binding objects](binding-objects.sha256.json) are byte-identical. The same
cache/runtime is reused; app/batch/region/module relinked and the existing development
install refreshed. Frozen products/fixtures/source archives remain preserved.

## Diagnosed waste and reproduction

Restoring instrumented source with `copy2` restored an older timestamp as well as
its original bytes. MSBuild consequently reused a diagnostic contour object.
The initial candidate timing still emitted profile lines and is explicitly invalid;
touch the restored source and rebuild the affected object/relink. The later accepted
run has no diagnostics. Do not restart bindings or clear the cache to solve this.

The opt-in probe initially passed a stain array where StainTransform requires a
vector; use an explicit vector conversion. The first checker tried to verify a
decoded result with a new request ticket; compare decoded identities directly and
verify a freshly recomputed replay result. Failures and completed products remain
local in build/native-detector-optimization; only the affected action was repaired.

From the active checkout with the retained fixture:

```powershell
& ../../ci/gridfrontends167-package/Release/qupathxx_tissue_contour_tests.exe --benchmark-pair ../../native-detector-optimization/candidate-final/11/study.qpxproj
& ../../../.venv/Scripts/python.exe -I docs/benchmarks/native-detector-optimization/compare.py ../../native-detector-optimization
```

Ordinary tests have no timing assertion. Stop this bounded optimization batch.
Further contour/nucleus optimization needs its own measured opportunity and exact
identity evidence; general speed superiority remains unproven.
