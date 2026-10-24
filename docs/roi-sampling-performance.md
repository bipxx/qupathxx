# Polygon sampling performance observations

The [native profiler](../tools/profile_roi_sampling.py) compares coverage and
explicit winding on the corrected source
`14788f23325d74dcb2ff016d82d994786eecd999`. The
[Windows raw observations](benchmarks/roi-sampling-profile-windows.json) and
[validation record](benchmarks/roi-sampling-profile-validation.json) retain 32
paired fresh processes and four excluded fresh-process warmups. Both arms have
identical ROI sample counts/means and tissue counts/labels in every retained pair.

Each child warms both arms, then times three request-and-analysis calls per arm
for each workflow. First-arm order is balanced four each per case. Table columns
show medians in milliseconds. Paired differences are computed within each
process before aggregation; they can differ from the difference of arm medians.

| Shape | Side (pixels) | Workflow | Coverage | Winding | Paired winding minus coverage |
| --- | ---: | --- | ---: | ---: | ---: |
| Square with hole | 256 | roi | 8.050 | 8.343 | 0.366 |
| Square with hole | 256 | tissue | 8.981 | 9.808 | 0.754 |
| Square with hole | 1024 | roi | 125.225 | 142.139 | 15.538 |
| Square with hole | 1024 | tissue | 152.825 | 170.200 | 17.726 |
| 4,096-vertex polygon | 256 | roi | 6.938 | 8.461 | 1.631 |
| 4,096-vertex polygon | 256 | tissue | 7.833 | 10.002 | 2.124 |
| 4,096-vertex polygon | 1024 | roi | 116.236 | 140.126 | 22.657 |
| 4,096-vertex polygon | 1024 | tissue | 133.578 | 155.976 | 23.507 |

These measurements show a cost for explicit winding on these synthetic warm
workloads: paired median differences range from about 0.4 to 23.5 milliseconds.
Optimizing that cost remains open. This is not hardware performance acceptance,
a speedup claim, or evidence for equal runtime across policies.

The workloads use owned dense RGB inputs, natural-grid sampling, 256-pixel read
tiles and explicit 256 MiB tissue work budgets. Simple polygons have a hole;
complex polygons have 4,096 vertices. They avoid intentional boundary sample
centers and require exact output equality. Boundary semantics are checked
separately by the independent triangle and pinned compartment regressions.
Cold startup, slide decoding, boundary-heavy workloads, process RSS, biological
throughput and GPU/frame budgets are outside this measurement. Other workstation
activity is uncontrolled. No Linux timings are claimed: the local WSL filesystem
became read-only after host disk exhaustion.

The source record distinguishes actual checkout bytes from committed LF bytes:
[source projection](benchmarks/roi-sampling-source-projection.json). All 242
native-build input files match the exact source commit after CRLF normalization.
The profile binds its actual native binary, compiler-build source identity and
profiler bytes; captured Git identity is the corrected commit, not a reconstructed
label. Pre-fix observations remain separate and are not used in this report.

From an interpreter containing the matching wheel and NumPy, use a new directory:

```powershell
python -I tools/profile_roi_sampling.py --output-directory build/roi-sampling-profile
```

Corrected Windows native/focused, installed, stubs/typing and desktop checks pass.
Corrected full Windows regression and the [hosted artifact audit](benchmarks/roi-sampling-ci-windows-linux.json)
pass at this exact source. Engineering acceptance does not accept performance. Scientific segmentation, hardware and supported-release gates
remain separate requirements.


## Boundary-grid prototype: interleaved Windows observations

Native source `843973cd582b32531b386379c747d743e5815c4f` adds a lazy,
fixed-size conservative boundary grid under the existing GEOS context mutex.
Default coverage queries allocate no grid. Unmarked grid cells can avoid the
second topology predicate; marked cells retain exact GEOS/orientation tests.
Dense grids retain the original query order. Cache storage is bounded by a
640-byte compile-time assertion per activated polygon, excluding allocator and
existing GEOS state. This is not a process RSS measurement.

The [raw paired receipt](benchmarks/roi-sampling-grid-interleaved-windows.json)
retains all 108 fresh-process observations: six excluded engine warmup pairs
and 48 retained pairs. Both engine order and policy order are cross-balanced.
The two engines execute identical profiler bytes. Numeric outputs match exactly
across engines and policies on all six workloads. Native binary hashes, actual
checkout source hashes, build Git revisions, all samples and timing ranges are
retained. Positive changes below mean a slower candidate; values are medians
of paired candidate-minus-baseline winding times, not differences of medians.

| Workload | Size | ROI change ms | Tissue change ms |
|---|---:|---:|---:|
| square-hole | 256 | -0.1399 | -0.0731 |
| square-hole | 1024 | -2.6990 | -3.5741 |
| complex | 256 | -0.6265 | -0.7248 |
| complex | 1024 | -10.8755 | -8.3440 |
| dense-triangle | 1024 | -0.2725 | 1.3869 |
| edge-strip | 1024 | 0.0019 | -0.0034 |

The larger complex polygon improves by about 8–11 ms in this run. The dense
triangle's tissue workflow is 1.39 ms slower, while its ROI result is essentially
unchanged. The boundary strip differs by under 0.004 ms. Coverage controls and
per-engine overhead deltas remain in the receipt, including noise between runs.
The dense fallback control is mixed; these observations do not establish a
uniform speedup or performance acceptance.

Windows passes 43 native suites and 401 focused Python cases, with three strict
scientific expected failures; the new independent rational-reference geometry,
concurrent initialization and extreme-aspect tests pass. Full current-source
Windows/Linux, sanitizer and portable qualification remain pending for this
prototype. Cold first-query cost, RSS, real-slide throughput and GPU/frame
budgets remain open. The prototype is retained on its validation branch;
main's accepted engine remains unchanged.

To reproduce with two matching installed environments and a new output path:

```powershell
python -I tools/compare_roi_sampling_engines.py --baseline-python BASELINE_PYTHON --candidate-python CANDIDATE_PYTHON --output-directory build/roi-engine-comparison
```
