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
179 GUI / 177 headless passes plus
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

Next implementation: typed raw scientific sampling and its analysis workflow.
Reuse `ScientificGrid`, `RawRegionRequest`, `RawPixelBuffer` and the existing raw
analysis/record path; freeze per-channel finite/support policy, Z/T/channel
capture and exact integer-to-float64 mean semantics, including UInt64/Int64,
before wiring shared native/Python/CLI/batch/UI consumers. Do not pass typed
quantitative samples through RGBA8 or introduce a resize solely to fit this
synthetic RGB capture. Vendor/pyramid/tiled admission and all broader biological,
hardware and M0-M7 requirements remain. The legacy distance expected failures
remain separate from the newer recipes' successful isolated-cell comparisons.
