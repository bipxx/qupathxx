# Focused Windows ROI performance — 2026-11-07

Completed the selected benchmark and a bounded development fix. This is one
512 x 512 level-zero ROI on CMU-1.svs, not whole-slide segmentation or overall
project speed acceptance. [Machine-readable results](summary.json) include stage
ranges, source identities, inputs, controls and exact comparison checks.

| Observed result | Development qupathxx | QuPath 0.7.0 |
| --- | ---: | ---: |
| Cells, all four paired runs | 2,237 | 2,237 |
| Cell capture/detection/retention/publication median | 1,079 ms | 486 ms |
| Peak process working set, one separate workflow including startup | 217 MiB | 1,262 MiB |
| Full workflow median, different retained-data contracts | 2,317 ms | 927 ms |
| Saved project bytes | 6,148,096 | 2,247,927 |

Native cells remain about 2.2 times slower on this case. All four paired runs
have identical source RGB, calibration, ROI area, cell counts and pixel-center
nucleus/body partitions (zero differing foreground pixels). Labels are compared
as partitions, independent of numeric IDs. The QuPath request trace established
that it reads the ROI without context; native context is therefore zero for
paired measurements. Earlier 32-pixel-context results are retained separately.
Density uses the ROI as explicit analysis support, not inferred biological tissue.

One warmup precedes three timed workflows with fresh image objects and warm
filesystem caches. Correctness rasterization is outside timers. Native retains
raw masks, statistics and replay identities; QuPath saves its hierarchy/imageData.
Their statistics methods and archive validation differ. Consequently the full
workflow/save/reopen numbers do not support an equivalent-work speed ratio.
QuPath uses its public WatershedCellDetection API; both use OpenSlide, explicit
stains, threshold 0.4, sigma 1, areas 16–512 pixels and expansion 5 pixels.
Native capacity is 50,000 raw basins, 512 MiB work and 128 MiB contours; these
admission allowances are not measured resident memory.

## Changes and validation

Published candidate5 saved a real grid-cell project that it could not reopen.
The decoder imposed quantitative one-marker-per-component rules on compatibility
labels after clipping/refinement, and replayed the wrong expansion recipe.
The development fix follows the declared versioned producer policy while keeping
quantitative checks, exact masks, archive bytes/hashes and body equality. Both
compatibility versions now have a clipped-maximum regression. The exact affected
2,210-cell candidate5 file reopens unchanged under the development runtime,
including the native app smoke. **Published candidate5 binaries remain affected;
these fixes are development-only and do not replace its release assets.**

Profiling identified contour construction/reconstruction as the major native
cost, including reopen (about 1,088 ms in the final workflow); density is about
10 ms. Direct GEOS construction now consumes existing binary64 polygon rings,
retaining WKT/path output and topology/prepared-geometry validation. No public
header, schema or binding changed; cached bindings were reused.

Full-workflow medians were 3,011 ms before, 2,317 ms after, and 2,615 ms in a
later original-code control: an observed **11% improvement against the later
control**. All four before/after outputs retain exact masks, RGB and full density
CSV, and ordered contour WKT hashes match. Isolated contour probes were
inconsistent and sometimes slower; this is a local workflow observation, not a
universal kernel improvement or statistically established speed guarantee.

Five affected native suites pass: geometry, geometry_edits, tissue_contours,
cells and cell_records (2.27 seconds). The initial constructor check exposed open
stored rings; closure was added only to temporary GEOS sequences. Failed logs
remain alongside passing logs; assertions were not relaxed. Initial capacity
refusals and the QuPath CPU-timer harness failure remain in the local evidence.

Resource-only runs omit mask oracles and perform one fresh workflow per engine.
The Windows high-water working set is sampled every 25 ms for the actual
worker-reported PID, including Python/NumPy or JVM/Groovy startup. QuPath runtime
defaults differ. This is one observation per engine, not kernel-only memory or a
cross-platform guarantee. The raw QuPath resource receipt retained an obsolete
scope string because its CLI flag was not last; verified resource mode emitted
no oracle masks. summary.json corrects that description without changing numbers;
measure.py now detects the flag anywhere in the command.

## Evidence and reproduction

[Development receipt](development-receipt.json), [522-input source manifest](source-inputs.sha256),
[runtime manifest](runtime-final.sha256), [archive recovery](candidate5-recovery.json)
and [coverage checklist](../../candidate5-performance-coverage.md) identify the
new runtime independently of the published wheel. Its compiled revision is
4234e8a7+dirty; its raw compiler-input SHA256 is
`dddf5bb3412a58e2ade9317f46e21d06d2857ca387ba40db37d0fd5fbedeaa41`.
The final source commit adds documentation; the raw manifest is authoritative
for the measured binary. Baseline raw tree is b7294a7c81ae6edae4dca0f90d89430400dc5eef9ce4fe40951a458e0b35989d.

From the active checkout, using the existing tools Python and new output names:

```powershell
$py = '../../../.venv/Scripts/python.exe'
$bench = 'docs/benchmarks/candidate5-performance'
$work = '../../candidate5-performance'
& $py "$bench/measure.py" "$work/native-new" -- $py "$bench/native.py" '../../fixtures/CMU-1.svs' "$work/native-new" "$work/runtime-final" --expected-tree dddf5bb3412a58e2ade9317f46e21d06d2857ca387ba40db37d0fd5fbedeaa41 --context 0
& $py "$bench/measure.py" "$work/qupath-new" -- '../../upstream/qupath/runtime/QuPath-0.7.0 (console).exe' script "$bench/qupath.groovy" --args '../../fixtures/CMU-1.svs' "$work/qupath-new"
```

Add `--resource-only` to native.py, or a third script argument `resource` to
QuPath, using separate fresh output folders, for the resource-only mode.
`summarize.py` uses the preserved named experiment blocks in the work directory;
`requests.groovy` and `contours.py` are untimed diagnosis tools, not release gates.
The source snapshot and baseline module are retained locally beside raw projects,
masks, failed attempts and logs. Large payloads are deliberately outside Git. Repository log copies normalize
line endings and trailing whitespace; original logs remain in the local work directory.
No additional feature or binary release is selected. A future speed task should
target the demonstrated contour/reopen cost; a future release must include the
archive repair and its own immutable package identity.
