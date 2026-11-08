# Closing the cell speed gap - 2026-11-07

The selected optimization batch roughly halves native cell-pipeline time against
a late unchanged-code control. **Speed parity is not yet achieved:** final native
medians remain 26-31% slower than paired QuPath on this one 512px ROI.
[Results](summary.json), [coverage](../../speed-gap-coverage.md) and the
[current task](../../current-task.md) keep that remaining goal open.

| Matched cell pipeline, median milliseconds | Native | QuPath | Native / QuPath |
| --- | ---: | ---: | ---: |
| Starting baseline | 1,077 | 431 | 2.50 |
| Selected candidate, first pair | 447 | 354 | 1.26 |
| Selected candidate, later pair | 439 | 336 | 1.31 |

The unchanged starting native implementation measured 906 ms in the late
control, versus 439-447 ms for the new candidate. That supports an observed
roughly 51% native time reduction against the late control. Host load changes
remain material: other-process CPU averaged about 12-17% of all logical CPU
time during final/control blocks, versus about 23-28% initially and up to 41%
in an intermediate reference block. This is not a statistical or universal
speed guarantee. Both engines were retimed; an old QuPath number was not reused
to claim the new ratio. Each block has one warmup and three measured workflows.

## Appropriate optimization choices

The prior contour-only optimization was useful but incomplete. This batch
addresses structural work and measures the whole cell pipeline:

- Run the independent nucleus/body contour conversions concurrently for rasters
  of at least 256x256 samples with more than one cell. This adds at most one
  worker; the existing admission model already charges both contour budgets.
  Tiny workloads stay serial. Join before publication, including failure unwind.
- Join directed, unambiguous nominal boundary segments into closed rings before
  GEOS constructs graphs. Ambiguous vertices retain the segment route. A single
  closed ring can directly create its polygon; multiple rings retain GEOS hole/
  nesting discovery. Existing clipping, area, validity and preparation checks stay.
- Format polygon WKT only when requested, using the same classic-locale17-digit
  writer and exact path order. Binary coordinates remain authoritative; no shared
  mutable text cache was introduced. Payload accounting now counts the actual
  retained data rather than an unused text duplicate. Repeated explicit text
  exports may pay formatting again; lazy formatting's separate speed gain was
  small/noisy and is not claimed independently.
- Reuse the nucleus's existing binary GEOS geometry for every body containment
  predicate. The old path formatted WKT and parsed it again for each cell. Both
  context mutexes protect geometry caches; identical backend self-queries return
  true. The same GEOS Covers predicate remains for distinct geometries. Primitive
  shapes without a backend retain their previous conversion path.

This targets algorithm/representation overhead and independent work. It does
not depend on Python binding rewrites, a GPU, new dependencies, fewer measurements,
smaller images, relaxed thresholds, lower precision or skipped topology checks.

## Exact output and failure evidence

All recorded full native blocks preserve byte-identical RGB, nucleus/body label
arrays and complete density CSV. The final two native/reference pairs each
include four warmup/measured comparisons: all eight have exactly 2,237 cells,
identical source RGB/calibration/area and exact nucleus/body partitions. Ordered
contour labels, pixel counts and full WKT hashes match the frozen pre-optimization
runtime. Save/reopen succeeds throughout. Density support is the explicit ROI,
not inferred biological tissue; this is not whole-slide segmentation.

Six affected native suites pass in 2.32 seconds: cells, cell_records,
tissue_contours, geometry, geometry_edits and project_save. Tests include the
existing 84-pattern general-versus-specialized topology comparison, a new large
nominal serial-versus-concurrent contour/archive case and worker-failure joining,
and opposing-direction concurrent Covers/self queries on shared snapshots.
Literal hole/multipart text checks also pass. Cached binding objects were reused;
no public header, binding interface or archive schema changed.

One attempted shortcut skipped overlay for faces enclosed by a rectangular clip.
It passed the small patterns but failed exact real nucleus geometry identity.
That implementation was discarded and the earlier passing source/runtime reused.
[Rejected output](rectangle-contours.json) and its source manifest remain evidence;
it was not relabeled a success and no assertion was weakened.

A [coarse public CellJob probe](phase-profile.json) helped expose downstream
work absent from direct contour microprobes. It ran the pre-binary-containment
6073f04e runtime; its numbers do not qualify the final binary. Its phase names
are coarse: reading includes compatibility segmentation, and contours includes
containment/normalization, not just tracing. Do not infer fine kernel costs from
those labels. The source of that diagnostic is retained for a narrower follow-up.

## Scope, reproduction and remaining work

[Source/runtime receipt](receipt.json), [522-input manifest](source-inputs.sha256)
and [runtime members](runtime.sha256) identify the measured module separately
from published candidate5. Compiled revision is f55da4b7+dirty; raw compiler-input
tree is `4290404591e945023f8b9ebbb946cda9cc524534a5c263ebb6db37ef3c926ca0`.
The source handoff adds documentation after building; the raw manifest owns the
measured binary's identity. Raw projects/masks, failures, intermediate runtime
copies and logs stay in build/speed-gap; compact local file hashes are retained.
No cleanup rejection was retried and no published release asset was changed.

From the active checkout, using a new output directory:

```powershell
$py = '../../../.venv/Scripts/python.exe'
& $py docs/benchmarks/candidate5-performance/measure.py ../../speed-gap/native-new -- $py docs/benchmarks/candidate5-performance/native.py ../../fixtures/CMU-1.svs ../../speed-gap/native-new ../../speed-gap/runtime-selected --expected-tree 4290404591e945023f8b9ebbb946cda9cc524534a5c263ebb6db37ef3c926ca0 --context 0 --label selected
& $py docs/benchmarks/candidate5-performance/measure.py ../../speed-gap/qupath-new -- '../../upstream/qupath/runtime/QuPath-0.7.0 (console).exe' script docs/benchmarks/candidate5-performance/qupath.groovy --args ../../fixtures/CMU-1.svs --args ../../speed-gap/qupath-new
& $py docs/benchmarks/speed-gap/compare.py ../../speed-gap
```

The last command checks/summarizes recorded named blocks and does not launch
another campaign. CLI documentation now correctly repeats --args for QuPath.
The monitor records actual worker PID CPU plus GetSystemTimes deltas. Its
other-process fraction estimates system busy time minus engine CPU across all
logical CPUs over startup/warmup/oracles; it does not identify named applications.
Resource RSS includes oracles here: no new resource-only RAM advantage is claimed.
Native/reference statistics and saved representations still differ, so overall,
save and reopen times do not support equivalent-work ratios.

The remaining selected goal is the 26-31% cell gap. The easiest next path is one
precise profile of this selected runtime that separates numeric segmentation,
per-object GEOS creation/preparation, overlay and final containment. Measure
context/allocation costs before introducing sharing or more workers; shared
contexts require an ownership/locking audit. Keep exact legacy/model identity
and reuse current masks/reference outputs for development. Use a short decisive
component check for intermediate experiments and one fresh paired end-to-end
check after integration. Do not start another reader, feature or release phase.

Waste review: fresh reference timings and a late original-code control were
necessary because host conditions changed. More full save/reopen workflows were
run per variant than necessary; retain them as controls, then use narrower probes
until final integration. The failed rectangular shortcut cost a bounded cached
build and exact gate, rather than a package/qualification campaign. Private CPP
changes reused expensive bindings throughout. Repository log copies normalize
whitespace; originals remain outside Git.
