# Focused post-consolidation speed comparison

Current native cell-processing median is **324.8ms**, versus **314.1ms QuPath**:
an observed **10.7ms /3.4% native overhead** on this512x512 ROI. Native samples
range320.4-330.3ms; reference294.9-346.3ms. The ranges overlap. This one sequential
local block does not establish a reliable remaining wall-time gap, parity or
speed superiority. No product optimization was made from this noisy difference.
[Summary and samples](summary.json), [source/runtime receipt](receipt.json).

## What is comparable

Both engines use the authenticated CMU-1.svs ROI[29185,2048,512,512], context0,
nominal step1,0.499um XY and the retained QuPath0.7.0 OD-sum/sigma1/threshold0.4/
area16-512/expansion5 recipe. They retain all2,237 Cells. Final RGB bytes and
canonical nucleus/body partitions are exact across engines; native masks also
match the earlier accepted ordered hashes. Calibration, analysis area and RGB
sums agree. One final output pair is captured outside all timed stages; unchanged
scientific qualification is reused rather than repeating a corpus campaign.

Native cell timing sums capture, detection, keep and publication; reference timing
covers the native plugin's cell detection/publication. Each engine opens/closes
its source per iteration. Six native warmups and24 JVM warmups precede six measured
samples each, using the prior settled warmup policy without searching for a
favorable median. [Native report](native-report.json), [reference](qupath-report.json).

| Observation | Native | QuPath |
| --- | ---: | ---: |
| Cell pipeline wall median |324.8ms|314.1ms|
| Cell pipeline process CPU median |445.3ms|390.6ms|
| Other-process logical-CPU fraction over complete process lifetime |23.5%|26.8%|

CPU includes all process threads; Windows counters are quantized and JVM JIT/GC
can contribute. Background load includes startup/warmups and the final archive/
oracle, not just measured cell stages, and is not attributed to a named process.
[Native resources](native-resources.json), [reference resources](qupath-resources.json).
The higher observed native CPU is a reason for a focused internal profile if
further CPU/latency reduction is needed, not proof of a particular inefficient
algorithm or an established14% CPU regression.

## Native costs and different work

Median native detection is320.5ms, publication4.3ms, cell capture0.06ms and keep
0.02ms. Open/read are15.6/7.8ms, native OD+RGB statistics17.7ms and density9.4ms.
There is no large Python submission/publication overhead exposed by this probe;
internal detection stages are the useful target for any subsequent profile.
Reference scripted RGB statistics and native OD+RGB statistics perform different
work and have no paired kernel ratio.

CSV/archive/reopen run once per engine after the last measured processing sample:
native CSV3.9ms, save67.6ms, reopen241.2ms; reference CSV5.2ms, save60.9ms,
reopen187.1ms. QPX retains masks/identities/statistics and QPData different hierarchy
content. These single observations are descriptive; they do not establish an
archive, cold-disk or overall-workflow speed ratio. Warmed filesystem caches apply.

Only one native and one reference archive plus one oracle pair were retained
(about14MiB total local evidence), avoiding repeated large project copies. No
rebuild, new runtime, wheel/release, dependency change or test campaign was needed.
Native source tree remains15f6a9baf4ce114189cdbdcb00ccc565062e84a670fdb1348eeb393eb7856011.

## Reproduction

Use the current selected development install and retained QuPath runtime. Choose
fresh output paths; preserve completed output if a checker fails.

```powershell
$py='../../../.venv/Scripts/python.exe'
$bench='docs/benchmarks/post-consolidation-speed'
$work='../../post-consolidation-speed-new'
New-Item -ItemType Directory -Path $work
& $py -I "$bench/measure.py" "$work/native" -- $py -I "$bench/native.py" ../../fixtures/CMU-1.svs "$work/native" ../../consolidation/development --steady
& $py -I "$bench/measure.py" "$work/qupath" -- '../../upstream/qupath/runtime/QuPath-0.7.0 (console).exe' script "$bench/qupath.groovy" --args ../../fixtures/CMU-1.svs --args "$work/qupath" --args steady --args 24 --args 6
& $py -I "$bench/summarize.py" $work
```

The summarizer currently names the selected native/qupath output subfolders and
last iterations11/29; retain that layout when choosing a new parent directory.
Large projects/masks/raw logs remain local under build/post-consolidation-speed.
Stop this measurement. A subsequent optimization should first profile the current
native detector's internal stages; do not change scientific safeguards or add
parallel workers merely to chase this overlapping10ms difference.
