# Shared masked Gaussian kernel candidate

Cell detection and classifier pixel features share a native, domain-normalized
Gaussian kernel. Profiling the existing implementation exposed repeated boundary
tests and strided vertical dot products. The current candidate retains the original
two-axis traversal for kernel radii at most six. Larger kernels compute valid offset
ranges once per output position and accumulate contiguous 256-column vertical spans.
Each output still adds the same coefficients in ascending offset order.
Truncation, domain weighting, final normalization and scratch allocation sizes
remain unchanged. No new threads, approximation, parameter tuning or schema
changes are introduced.

The initial candidate's [local profile](benchmarks/masked-gaussian-local-windows-linux.json) records
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

The initial candidate's first Windows full-suite process stalled in an existing
asynchronous ROI measurement wait. Fresh direct probes, collection followed by
the same job, and isolated pytest checks complete successfully; the cause remains
unresolved. A separate 2,000-job Windows stress process also completes. After more
than 20 minutes in the same wait, the specific stalled process was stopped with
final stacks and identity retained. A fresh full run passes 2,002 tests with ten
optional skips and four strict expected failures, including software-rendered GUI
checks. The original intermittent wait's cause remains unresolved. A separate complete Windows
headless suite passes 1,622 tests with 390 optional skips and four expected failures.
The [follow-up receipt](benchmarks/masked-gaussian-validation-followup.json)
retains both attempts and identifies the completed full-suite evidence. Linux full
regression passes 1,623 tests with 389 optional
skips and four strict expected failures. Existing cell-comparison metrics and
shared-ROI results are unchanged on both platforms; seeded classifier model,
training and prediction identities also match the prior baseline. The profile
does not accept the candidate for release or
close any outstanding M0–M7 gate.

## Complete workflow timing

The initial candidate's [workflow validation receipt](benchmarks/gaussian-workflow-validation-windows-linux.json)
binds the baseline and optimized wheels to their actual loaded native artifacts.
Their only changed runtime C++ file is `cpp/masked_gaussian.hpp`; the CMake/native
test additions are also identified. Separate persistent installed-package
interpreters process identical 512² synthetic inputs. Each workload has one warm-up
and seven paired measured calls, alternating baseline/candidate order. Final Windows
and Linux timing passes run sequentially; preliminary overlapping passes are excluded.

Pixel features include RGB and OD Gaussian, gradient and Laplacian extraction over
an input with transparent holes. Cell calls include request capture, decoding,
smoothing, watershed, expansion, geometry/contours and compartment measurements.
All cell runs retain 64 cells. Within each platform, full feature/validity arrays,
native result/input identities, nucleus/body labels, all compartment features and
contour WKT have matching output signatures across every measured call. Input
construction, worker startup, result hashing, IPC and output-file I/O are outside
the timer. The records retain all raw wall/native timing samples and work estimates.

| Workflow | Sigma | Windows median ratio | Linux median ratio |
| --- | ---: | ---: | ---: |
| Pixel features | 1 | 1.100× | 0.955× |
| Pixel features | 4 | 1.332× | 1.234× |
| Pixel features | 16 | 1.944× | 1.669× |
| Cell detection and contours | 1 | 0.959× | 0.945× |
| Cell detection and contours | 4 | 1.072× | 0.904× |
| Cell detection and contours | 16 | 1.287× | 1.267× |

A ratio below one means the candidate was slower. Small-kernel observations
include roughly 4–11% slower calls, and the shared workstation has wide timing
ranges. The records do not establish statistical significance or uniform workflow
improvement. In particular, the approximately threefold large-kernel result must
not be described as a threefold complete-cell-workflow speedup. The candidate stays
on a validation branch while representative performance acceptance remains open.

Reproduce using two installed-package environments without replacing either wheel:

```powershell
python -I tools/benchmark_gaussian_workflows.py --baseline-python BASELINE_PYTHON --candidate-python CANDIDATE_PYTHON --size 512 --repeats 7 --output build/paired-workflows.json
```

The benchmark refuses to record speed ratios if any paired result signature or
input/interpreter identity differs. Venv interpreter paths are preserved rather
than resolving Unix symlinks to the base Python. Neither interpreter opens a GUI.

## Short-kernel refinement

The [revised candidate receipt](benchmarks/gaussian-short-kernel-validation-windows-linux.json)
records source fingerprint
`afedc41994196ea996789808b0c5f8d030cb7339a6516a245929243df68744a3`.
The original traversal is restored for `ceil(3*sigma) <= 6`, covering the common
sigma values up to two sampled pixels. The larger-kernel block path is retained.
This responds to the mixed small-kernel complete-call observations rather than
assuming that a faster isolated kernel always improves the calling workflow.

The independent 2D oracle now covers 231 cases, including the adjacent floating-point
values below and above sigma two. ASan/UBSan passes. GCC and MSVC paired kernels
preserve every tested output bit against the original implementation. Native suites
pass 42 Windows and 41 Linux cases; focused Python checks pass 156 Windows cases
with four optional skips/four scientific expected failures and 50 Linux reader/runtime
cases with two desktop skips. Both installed workflows, stubs, strict typing,
QuPath characterization metrics and seeded classifier identities pass.

The complete-call comparison repeats the same fixed six cases against the
pre-optimization wheels, with one warm-up, seven alternating pairs and serialized
platform timing passes. All paired output signatures match and every cell run
retains 64 cells. The observed median ratios are:

| Workflow | Sigma | Windows median ratio | Linux median ratio |
| --- | ---: | ---: | ---: |
| Pixel features | 1 | 1.004× | 1.095× |
| Pixel features | 4 | 1.409× | 1.173× |
| Pixel features | 16 | 2.128× | 1.703× |
| Cell detection and contours | 1 | 1.011× | 1.065× |
| Cell detection and contours | 4 | 1.124× | 1.278× |
| Cell detection and contours | 16 | 1.442× | 1.225× |

No slower median is observed in these selected cases. The near-one Windows
short-kernel ratios are effectively neutral observations; these samples do not
establish statistical significance or uniform hardware performance. The original
mixed-result recordings remain available. Full regression for the revised source
is running, and the optimization remains on a validation branch. Project-wide
performance, scientific acceptance and the original intermittent wait stay open.

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
