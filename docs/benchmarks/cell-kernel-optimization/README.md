# C++ cell kernel optimization - L1/L2/L4

Native cell-pipeline median is **327.8 ms**, versus **348.2 ms** for the retained
pre-change module: an observed **5.9% improvement** in this matched Windows ROI.
The extended QuPath reference median is347.5 ms, with a305-403 ms range. Native
has the lower median in this run, but overlapping/variable samples do not confirm
a reliable general speed advantage or strict parity. [Samples](summary.json).

## Changes and correctness

- Watershed interior pixels use the same eight occupancy bits via direct offsets.
  Border pixels retain the original neighbor path. Fate table, pass order, delayed
  writes, traversal limits and cancellation are unchanged.
- Windows SHA batches short updates using its existing64-byte block. Large writes
  remain direct; finish flushes pending bytes. The canonical stream, integer
  encoding, object allocation and portable SHA algorithm are unchanged. No new
  cache, trusted-hash parameter, public API or processing policy is introduced.

Seven affected suites pass in4.17 s: sha256, log_watershed, qupath_response,
log_cell_recipe, cells, cell_records and project_save. Tests retain26 independent
ImageJ masks/maxima and add28 pre-change1/2-pixel/thin-grid captures. Mixed small/
large/zero chunks,64-byte boundaries, integer serialization, finalization refusals
and both Windows/portable hash backends pass. [Tests](native-tests.log).

Four full correctness workflows preserve exact RGB, nucleus/body label bytes and
full density CSV against the frozen native workflow. Four detector checks after
archive reopen preserve all2,237 cells and the prior ordered labels/pixels/WKT
hashes. All timed rows preserve RGB sums, calibration, count and analysis area;
these agree with the pinned QuPath recipe/reference. [Geometry](geometry-check.json).
The ROI is512x512 at[29185,2048], context0, nominal step1, .499um XY, the existing
OD-sum/sigma1/threshold.4/area16-512/expansion5 recipe. Density support is the explicit
analysis ROI. No whole-slide, biological or new RAM claim follows.

## Timing correction and scope

| Run | Warmups / measured | Pipeline median | Range |
| --- | --- | ---: | ---: |
| Native candidate |6 /6|327.8 ms|321.8-341.8 ms|
| Retained native control |6 /6|348.2 ms|340.3-350.3 ms|
| Initial QuPath, unsettled |6 /6|382.9 ms|318.0-411.9 ms|
| Extended QuPath |24 /6|347.5 ms|304.5-402.9 ms|

The initial measured QuPath block declined412 ->318 ms, so its apparent14%
native advantage was not accepted. Only that reference was repeated with24
warmups and explicit counts; the completed native captures were reused. Extended
samples fluctuate without the same downward trend but remain noisy. Other-process
CPU fractions across process lifetimes were13.6% native,15.3% control and18.0%
extended reference. These are sequential local observations, not confidence bounds.
Historical320/292 timings come from a different host-time window; compare this
candidate with its fresh retained348 ms control rather than interpreting raw
historical milliseconds as a regression. Pipeline work is matched as before;
statistics/archive/overall workflow timings still differ and are not ratios.

The reference script accepts optional fourth/fifth arguments after steady for
warmup/measured counts; default6/6 and ordinary/resource modes stay unchanged.
The monitor now describes explicit steady counts rather than hardcoding six.
Python only submits existing C++ operations and verifies/measures development
outputs. No product science/workflow implementation moved into Python.

## Source, reuse and reproduction

[Receipt](receipt.json), [530-input source manifest](source-inputs.sha256) and
[runtime](runtime.sha256) identify this candidate. Raw source tree:
`ef7338dd16763fc5c62b5aeb501a4b1eeb6540acb0f6bb5a17c343e175727584`.
Compiled revision is070503e5+dirty. The retained old module has tree d1bee927...
and shares unchanged DLLs from the cache. All37 binding objects remain byte-for-
byte identical; only affected native inputs/provenance compile before module
relink. [Native build](native-build.log), [module relink](module-build.log).

From the active checkout, choose fresh output names:

```powershell
$py='../../../.venv/Scripts/python.exe'
$cache='../../ci/gridfrontends167-package/Release'
$bench='docs/benchmarks/candidate5-performance'
& $py docs/benchmarks/cell-kernel-optimization/run-native.py "$cache/_native.cp312-win_amd64.pyd" $cache "$bench/native.py" ../../fixtures/CMU-1.svs ../../cell-kernel-optimization/native-new $cache --expected-tree ef7338dd16763fc5c62b5aeb501a4b1eeb6540acb0f6bb5a17c343e175727584 --context 0 --steady
& '../../upstream/qupath/runtime/QuPath-0.7.0 (console).exe' script "$bench/qupath.groovy" --args ../../fixtures/CMU-1.svs --args ../../cell-kernel-optimization/qupath-new --args steady --args 24 --args 6
```

The bootstrap directly loads one selected native module per process and aliases
it for the existing control script, avoiding another runtime copy/install. Use
the existing measure.py wrapper to capture actual engine/system CPU as in the
retained resource JSON files. All large projects/masks/control binaries remain
local in build/cell-kernel-optimization. No wheel or candidate5 asset changed.

L1/L2 and final measurement L4 are complete. Conditional L3 domain/identity sharing
is deferred until its remaining cost justifies a new ownership boundary; no
unmeasured framework was added. Next planned phase is L5-L7's shared native
workflow/import authority and thin controls. Strict speed parity stays unconfirmed.
