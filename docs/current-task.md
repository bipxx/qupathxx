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
