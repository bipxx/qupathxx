# Current implementation task

The full updated M0-M7 goal remains active. All milestones remain Partial.
The qualified application source on public main is `3fc3bcb`; the following
commit publishes documentation/evidence only. The user's divergent local main
is preserved. See the [full plan](project-plan.md), [inventory](feature-matrix.md)
and [API delivery paths](api-delivery-plan.md) for remaining work.

Hierarchy/recovery and the explicit-grid RGB-cell workflow now pass complete
source-bound engineering qualification. The shared native grid, float64 channels,
physical kernels, QCL5/QCA5/CSV, typed Python/stubs, CLI, batch, managed viewer and
ImGui paths retain their existing immutable capture and publication authority.
The [workflow guide](grid-cell-workflows.md) describes the entry points.

The [combined qualification receipt](benchmarks/scientific-grid-169-qualification.json)
records all required audits at the same frozen source:

- [Engine 40188586241](https://github.com/bipxx/qupathxx/actions/runs/40188586241):
  all five jobs accepted. Windows GUI passes 50 native suites and 3,256 installed
  cases; Windows/Linux headless pass 49 suites each and 2,831/2,829 installed
  cases. Minimal and ASan/UBSan jobs pass 49 suites each. Runtime inventories,
  forwarder/loader probes, readers, hierarchy, TMA, shape/frontier, archives,
  classifiers, batch, rendered workflow and strict typing checks remain required.
- [Mac native 40188591097](https://github.com/bipxx/qupathxx/actions/runs/40188591097):
  ARM64 and x86_64 each pass 49 native suites, uploaded binary/notice inventories,
  relocation and fresh native runtime probes. This excludes Mac Python, GUI and
  OpenSlide qualification.
- [Portable extended 40190126665](https://github.com/bipxx/qupathxx/actions/runs/40190126665):
  both CPython 3.11/3.12 manylinux builders and fresh Debian consumers pass the
  complete inherited cohorts, including required builder NDPI coverage and static
  runtime/ABI inventories. The earlier green portable run omitted extended
  readers and is retained as an under-scoped attempt, not full qualification.

The scientific installed contract requires 107 named cases: all pass in GUI;
headless requires 105 passes and exactly two declared desktop skips. Missing
binding casters and Darwin type deduction found in candidate168 are fixed and
covered by runtime getter/lifetime/None checks, strict typed returns and a guard
against unbound C++ annotations. The [correction receipt](benchmarks/scientific-grid-portability-fix-local-windows.json)
retains before/after failures, local installed checks and the precise source proof.
Both old candidate158/168 failures remain unqualified; their frozen sources and
raw evidence are preserved.

Next work preserves the full original scope: scientific masks/features and
split/merge tolerances; vendor/pyramid/tiled and typed quantitative sampling;
ICC/fluorescence, hierarchy/interchange fault coverage, automation/TMA/classifier
extensions; real hardware/memory/performance campaigns; rights/labels, licensing,
Mac Python/GUI/OpenSlide, supported OS/ABI matrix, installers/signing and clean
machines. Three strict isolated-cell scientific gates remain unresolved.
Engineering qualification does not establish biological equivalence or release
readiness. Continue through M0-M7 rather than closing them from green CI.

Earlier source-specific narrative remains in Git history and
[current-task history](current-task-history.md).

## Active comparison integration after source169

The feature branch adds an independent pinned comparison for the existing
explicit-grid production detector. Seven natural-grid cases require exact
nucleus/body masks and matched features at the frozen tolerances. All nine
retain independent feature arithmetic and context checks; eight isotropic
cases compare equivalent pixel and micrometre recipes. The clipped ROI nucleus
and requested-resampling differences remain visible. No native detector or
reference fixture changes are involved. The older distance recipe's three
strict expected failures remain unchanged; the existing LoG recipe already
passes those isolated-cell checks.

The comparison and public ROI debug capture join the named installed
scientific-grid contract, raising its next-candidate requirement to
185 GUI / 183 headless passes plus
two declared desktop skips. Source169's accepted 107-case contract remains
historical evidence for that source. New comparison tools, regression source
and immutable capture dependencies are bound by the next source contract.
The dispatch-profile helper also remains a post-source169 feature-branch change.
Do not transfer source169 platform acceptance to this newer harness.

The [local comparison receipt](benchmarks/grid-cell-qupath-comparison-local-windows.json)
records 252 passing installed/auditor checks and two declared desktop skips,
including all 27 new comparison cases. All 298 compiler inputs and the installed
native binary are unchanged from the retained source169 local artifact; a
426-file source proof binds the new harness and capture dependencies. No native
rebuild or duplicate platform campaign was used to establish this local scope.

The pinned public ROI debug capture now reproduces the ordinary output exactly
and shows a 192x128 cropped processing image. A second capture reproduces all
twelve non-log payloads byte-for-byte. The explicit reference-ROI/no-context
comparison removes the 24 native-only pixels but retains four reference-only
pixels. Their small positive upstream LoG responses differ from zero/negative
responses in the unchanged native C++ numerical probe. Bodies match exactly.
The new capture verifier rejects byte changes, recipe/identity changes and
re-sealed semantic corruption. Production arithmetic and old fixtures are
unchanged; no tolerance is fitted to accept this discrepancy.

The [ROI stage/domain receipt](benchmarks/grid-cell-roi-debug-local-windows-linux.json)
records 275 passing installed/auditor cases on Windows and on a fresh WSL Linux
environment, each with two declared desktop skips. Both execute all 32 grid
comparison and 14 debug-capture checks, and pass the 151-case headless named
cohort. The Linux wheel/native hashes match qualified engine run 40188586241;
the 298 Windows compiler inputs and installed native hash remain unchanged.
A 444-file proof binds the new harness and capture. These are local comparison
checks on retained artifacts, not a full new engine/Mac/portable qualification.

The resampled public capture now records the actual 171x128 RGB input and
nominal 1.5 coordinate step. Geometry-lattice and pixel controls distinguish that
step from 256/171 despite equal array shapes. Captured-input detector masks and
features match independently reconstructed processing geometry; the original
source area sampler remains explicitly different. The 26 new checks reject
mixed identities, re-sealed metadata/pixel/geometry changes and wrong mapping or
rounding assumptions. Fourteen non-log capture payloads repeat byte-for-byte.
No native production sampler, detector or original fixture is changed.

The [resampled processing receipt](benchmarks/grid-cell-resampled-processing-local-windows-linux.json)
records 305 passing installed/auditor cases on Windows and on the retained
source169 Linux wheel, each with two declared desktop skips. All 26 resampled
checks and the 177-case headless named grid cohort run on each platform. The
462-file source proof binds the new fixture/tools; all 298 compiler inputs and
both installed native module identities remain unchanged. The source169 full
platform campaign remains distinct from this newer local comparison scope.

## Active typed raw integration

The private exact-mean dependency supports all ten raw dtypes. Its independent
Fraction oracle supplies 2,891 vectors, checked in both input orders. The
[numeric receipt](benchmarks/raw-scientific-means-local-windows-linux.json)
retains standalone MSVC/GCC/ASan/UBSan results and the initial strict MSVC warning
failure. The existing raw statistics recipe remains unchanged.

`read_scientific_raw` and its owned result are now implemented and pass native
resampling on Windows and Linux. The [reader receipt](benchmarks/raw-scientific-sampling-local-windows-linux.json)
records 94 independent sampling/hash cases, 3,732 per-channel results, canonical
raw-bit/source hashes, selected channels/Z/T, invalid/nonfinite precedence,
pre-pixel-IO admission, malformed-reader and cancellation checks. The same target
also executes the exact-mean and legacy RGB/area references. New reader/tests
pass selective ASan/UBSan instrumentation; inherited linked core code is not a
full sanitizer campaign. The 123 affected Python checks include six oracle checks.

Both native targets report source `c4a9a21` and compiler fingerprint
`976fe2deceb8d117c92f93f50f60cce6696499e32eea22808e0754ce22e98605`.
All 305 compiler inputs match a 475-file source proof. Windows reused the active
cache after retaining/hash-verifying 63 older artifacts; the updated core and
resampling executable coexist with older cached executables/module. The latter
are not relabelled, and installed Python remains the source169 API.

The [typed contract](scientific-grid-contract.md#typed-raw-sampling-active-contract-and-exact-means)
keeps strict footprint support and per-channel finite reasons. Data snapshots
carry original dtype/axes/calibration and remain independent of display RGB.
The bounded native reader does not finish actual TIFF-backed scientific
sampling, raw analysis/retention/replay, Python/CLI/batch/ImGui delivery or any
M0-M7 milestone. Full native/installed/platform qualifications remain required.

The shared raw capture and `digest_scientific_raw` are now implemented at
`b247880`. The [digest receipt](benchmarks/raw-scientific-digest-local-windows-linux.json)
records passing Windows/Linux native resampling, all 94 independent hash pairs
in both full and hash-only paths, 3,732 retained sample results, and 85 Python
oracle/auditor checks. Both native engines report fingerprint
`478d81adf4f57c6f884967847eb3a60f7770c8ef130d06617fd79e99fc765fcd`;
a 475-file proof binds the source. Hash-only allocates no output HWC arrays and
computes no means. Metadata is snapshotted before pixel IO; decoder mutation
checks prevent mixed metadata epochs. Lower output-free budget admission,
malformed reads, close and cancellation are covered. Selective ASan/UBSan passes
for new sources/tests linked with the older unsanitized core, whose reported
identity is retained rather than relabelled.

Next: compose the existing immutable `RawAnalysisRequest`/result/job path with
the grid reader and version retained records while preserving QRA1. The digest
is not yet connected to batch resume. Complete native save/reopen/replay and CSV
before grouping new bindings/stubs, CLI, batch and GUI changes into one installed
candidate. Actual TIFF-backed sampling, vendor/pyramid/tiled, biological,
hardware, licensing and the full M0-M7 scope remain open.
