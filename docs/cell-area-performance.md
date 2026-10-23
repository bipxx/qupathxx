# Physical cell-area request observations

The [record validation](benchmarks/physical-cell-area-profile-validation.json)
binds the [Windows](benchmarks/physical-cell-area-profile-windows.json) and
[Linux/WSL](benchmarks/physical-cell-area-profile-linux.json) raw samples to the
tested native artifacts and profiler. Both were collected on an Intel i7-12700K,
Windows first and Linux second, after local GUI tests finished. Other workstation
activity was not controlled. These observations do not accept hardware budgets.

For each size, one fresh-process warmup is excluded, followed by eight fresh
processes with a paired sampled/physical comparison and balanced first-arm order.
Each child excludes setup and per-arm warmup. Constructor timing averages 500
calls; detection timing includes request creation and native detection. Each pair
uses exactly the same resolved 64–96 sample limits, corresponding to inclusive
8–12 square-micrometer bounds at 0.5 by 0.25 micrometers per source pixel.
Gaussian sigma is zero and expansion is three sampled pixels.

All 48 retained pairs have identical nucleus labels, cell labels and numeric
compartment features. Those hashes also match across platforms at each size.
The unit-bound resolution is performed once; the detector uses its ordinary
integer size filter. Median paired constructor overhead is approximately
1.2–1.8 microseconds on this workload.

| Side | Cells | Windows extra constructor time (us) | Linux extra constructor time (us) |
| ---: | ---: | ---: | ---: |
| 128 | 64 | 1.28 | 1.20 |
| 512 | 1024 | 1.65 | 1.31 |
| 1024 | 4096 | 1.76 | 1.43 |

Whole-detection observations have broad ranges. They do not establish speedup or
performance parity. In particular, Linux's largest physical-case median is higher
in this sample. Paired differences are calculated before taking the median and
need not equal the difference of the two marginal medians. Raw samples are retained.

| Platform | Side | Sampled median (range), ms | Physical median (range), ms | Paired physical-minus-sampled median (range), ms |
| --- | ---: | ---: | ---: | ---: |
| windows | 128 | 10.45 (9.77–12.65) | 10.52 (10.14–12.48) | -0.05 (-0.97–0.76) |
| windows | 512 | 182.93 (176.12–195.99) | 184.58 (179.10–205.86) | 3.44 (-4.89–9.87) |
| windows | 1024 | 828.08 (753.13–1070.46) | 818.36 (762.86–1072.24) | 5.75 (-50.83–86.70) |
| linux | 128 | 6.23 (5.96–6.77) | 6.26 (6.19–8.29) | 0.17 (-0.58–2.06) |
| linux | 512 | 139.51 (113.38–600.30) | 126.43 (106.69–858.87) | -5.00 (-145.30–258.57) |
| linux | 1024 | 640.24 (567.42–2097.23) | 715.06 (597.36–2523.91) | 52.68 (-242.63–789.40) |

The [first attempt](benchmarks/physical-cell-area-profile-first-attempt.txt)
reached the 4,096-cell case and hit the default 32 MiB contour work limit before
measurement. Its timings are excluded. The final complete protocol explicitly
sets 64 MiB per contour pass and retains the 256 MiB overall work cap for both
arms at every size. Production defaults are unchanged. Work estimates are not RSS.

The initial profiler is preserved at commit
`43735d1a69d124334306b6d3bff1b9b6e1ac318e`; the corrected profiler's byte hash is in
the validation receipt. The local builds report an unknown Git revision; native
binary hashes, source-byte fingerprint and source-content commit identify them.

From a checkout with a physical-area-capable installed build and NumPy, use a new
output directory:

```sh
python -I tools/profile_cell_area_limits.py --output-directory build/cell-area-profile
```

Scientific segmentation, biological cohorts, arbitrary grids, physical radii,
hardware/frame budgets and supported releases retain their separate open gates.
