# Shared masked Gaussian kernel candidate

Cell detection and classifier pixel features share a native, domain-normalized
Gaussian kernel. Profiling the existing implementation exposed repeated boundary
tests and strided vertical dot products. The candidate computes valid offset
ranges once per output position. For long vertical kernels it accumulates contiguous
256-column spans; short kernels retain scalar dot products to avoid output-array
traffic. Each output still adds the same coefficients in ascending offset order.
Truncation, domain weighting, final normalization and scratch allocation sizes
remain unchanged. No new threads, approximation, parameter tuning or schema
changes are introduced.

The [local profile](benchmarks/masked-gaussian-local-windows-linux.json) records
paired baseline/candidate measurements on an Intel Core i7-12700K under WSL2.
Both functions are compiled into one executable with GCC 13.3, `-O3`, C++17 and
`-ffp-contract=off`. A warm-up precedes seven measured repetitions, alternating
which implementation runs first. Timing includes kernel creation, scratch
allocation and convolution; input construction/copy and result checking are outside
the timer. Every paired output is compared byte-for-byte before recording a case.

| 1024² input | Sigma | Baseline median (ms) | Candidate median (ms) | Ratio |
| --- | ---: | ---: | ---: | ---: |
| Full domain | 1 | 21.49 | 19.24 | 1.12× |
| Full domain | 4 | 58.01 | 30.86 | 1.88× |
| Full domain | 16 | 314.61 | 105.24 | 2.99× |
| Holes and sparse exclusions | 1 | 24.06 | 21.31 | 1.13× |
| Holes and sparse exclusions | 4 | 52.86 | 34.31 | 1.54× |
| Holes and sparse exclusions | 16 | 280.38 | 97.93 | 2.86× |

The 512² paired cases also preserve every output bit. A separately compiled
MSVC `/O2 /fp:precise` executable checks the six 512² cases against the baseline;
its timings are excluded from the speed claim because concurrent validation was
running. These observations are specific to these inputs and compiler options;
they do not establish whole-workflow throughput, viewer frame time, process RSS
or performance on other hardware.

The independent native test directly sums a two-dimensional Gaussian rather than
copying the production separable traversal. Its 189 cases cover empty/full/holey
domains, one-pixel axes, tiny sigma, the short/long-kernel transition and widths
straddling the 256-column block boundary. Cancellation propagation is also checked.
The standalone test passes ASan/UBSan. Full native suites pass 42 Windows and 41
Linux checks. The Windows isolated analysis/pixel-feature/QuPath comparison suite
passes 57 tests with the same four strict expected failures from the
[cell comparison corpus](qupath-cell-comparisons.md).

This remains a candidate. A Windows full-suite process is stalled in an existing
asynchronous ROI measurement wait. Fresh direct probes, collection followed by
the same job, and isolated pytest checks complete successfully; the cause remains
unresolved. A separate 2,000-job Windows stress process also completes. The live
process and its stack evidence are retained, and no full Windows pass is inferred
from the isolated checks. Linux full regression passes 1,623 tests with 389 optional
skips and four strict expected failures. Existing cell-comparison metrics and
shared-ROI results are unchanged on both platforms; seeded classifier model,
training and prediction identities also match the prior baseline. The profile
does not accept the candidate for release or
close any outstanding M0–M7 gate.

## Reproducing the paired kernel probe

From a Bash checkout, preserve a baseline header before compiling the candidate:

```bash
git show b8648fe:cpp/masked_gaussian.hpp > build/masked_gaussian_baseline.hpp
g++ -O3 -std=c++17 -ffp-contract=off \
  '-DQPX_GAUSSIAN_BASELINE_HEADER="../build/masked_gaussian_baseline.hpp"' \
  tools/benchmark_masked_gaussian.cpp -o build/benchmark_masked_gaussian
build/benchmark_masked_gaussian 1024 7 > build/masked_gaussian_profile.json
```

The benchmark exits nonzero if any paired output bit differs. The normal native
test target is `masked_gaussian`; it needs neither the GUI nor Python.
