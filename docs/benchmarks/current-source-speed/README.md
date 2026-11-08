# Current-source native / QuPath comparison

G01 selected-ROI measurement is complete. Native cell pipeline median **305.6ms**
versus QuPath0.7.0 **313.1ms**: observed7.46ms /2.38% lower native wall time.
Ranges overlap (native296.0-316.7ms; QuPath295.1-331.8ms), with substantial
background CPU. This supports comparable observed speed on this ROI, not a
statistical equivalence test, guaranteed parity or general superiority.
[Samples and scope](summary.json), [runtime/scripts receipt](receipt.json).

Unchanged CMU-1.svs ROI[29185,2048,512,512], context0, affine-inverse sampling,
QUPATH_070_V2 native and pinned upstream recipe,0.499um calibration. All2,237
cells, RGB bytes and canonical nucleus/body partitions match exactly. Native
ordered mask hashes also match retained accepted evidence. One archive/oracle
per engine is outside processing timers. Different archive content prevents an
end-to-end save/reopen speed ratio.

Six native and24 JVM warmups precede six alternating measured pairs. Both workers
stay alive but only one workflow is released at a time; ready/go waits occur
before open/setup and all timed stages. [Schedule](schedule.json) retains actual
nonoverlapping workflow windows; no build, kernel change or repeated timing
search. Native timing sums capture/detection/keep/publication; reference covers
plugin detection/publication. All engine samples remain in the reports.

Process CPU medians398.4ms native /367.2ms reference include all engine threads;
Windows counters are quantized. Lifetime other-process logical-CPU fractions
40.0% /34.8% include warmups, gate waits, final archive/oracle, and are not
per-pair load controls. Lifetime peak working sets135.6MiB /1289.9MiB are process
observations with different warmups/JVM allocation, not a whole-slide memory
acceptance or controlled memory ratio. Existing measure.py scope text omits
gates; this paragraph qualifies both resource reports.

Reproduce from active checkout with a new output path:

```powershell
../../../.venv/Scripts/python.exe -I docs/benchmarks/current-source-speed/run.py ../../current-source-speed-new
../../../.venv/Scripts/python.exe -I docs/benchmarks/current-source-speed/summarize.py ../../current-source-speed-new
```

Raw logs, final masks and projects remain under build/current-source-speed.
Stop this bounded comparison. Further optimization needs a demonstrated workload
bottleneck; general speed coverage remains under G02, not another G01 rerun.
