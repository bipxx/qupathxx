# Native stain estimation

The OD-plane estimator produces two reusable stain directions from a selected
area ROI or nucleus. C++, Python and ImGui run the same bounded native kernel.
It supports existing pyramid levels, deterministic reservoir sampling, progress,
cancellation and stale-result rejection. White RGB is supplied explicitly.

```python
import json
import qupathxx as q

params = q.StainEstimateParameters()
params.sample_limit = 200_000
params.max_read_pixels = 16_777_216
params.seed = 0
request = q.StainEstimateRequest(doc, object_id, params, white=[255, 250, 245])
job = q.StainEstimateJob()
job.request(request)
status = job.wait(doc, object_id)  # releases the GIL
assert status.state == "ready", status.error
estimate = job.result

# Use the exact fitted basis in the existing native analysis workflow.
analysis = q.AnalysisRequest(doc, object_id, estimate.transform)
record = doc.keep_analysis(analysis, q.analyze(analysis))
q.save_stain_estimate(estimate, "stains.json")

# Reload the normalized basis without another fitting or normalization pass.
data = json.load(open("stains.json", encoding="utf-8"))
model = q.StainTransform.from_normalized(data["stains"], data["white"])
assert q.stain_model_sha256(model) == data["model_sha256"]
```

`estimate_stains(request)` is the synchronous entry point and also releases the
GIL from Python. A request owns immutable source/ROI/parameter snapshots. Its
`matches(document, selected_id)` method checks the original live authority,
source, selected ID and object revision. The controller retains its source
request on the owner thread until joining the worker; worker teardown cannot
release a Python-owned reader. Ready results are native values with immutable
geometry and capture-time provenance, independent of later document removal.
An independently retained result remains usable as a model on other images.

## Algorithm and scope

Algorithm revision `qpx.od-plane-stains/1` uses the optical-density plane and
angular-percentile approach described in [Macenko et al. (2009)](https://wwwx.cs.unc.edu/~mn/sites/default/files/macenko2009.pdf)
and the explicit [Macenko algorithm in the 2019 normalization study](https://pmc.ncbi.nlm.nih.gov/articles/PMC6778842/).
This implementation fits the uncentered OD second moment, equivalent to the two
largest right-singular directions of the sampled OD matrix. Centered covariance
implementations have a different contract. QuPath v0.7.0 runtime agreement has
not been established; this estimator has its own revision and defined behavior.
It provides absorption directions for existing deconvolution. Intensity
normalization and semantic stain naming remain separate workflows.

1. Decode bounded RGBA tiles at the chosen existing pyramid level. Include only
   fully opaque pixel centers covered by the selected ROI. Coarse-grid centers,
   rounded level-zero tile origins and input-digest layout match tissue sampling.
2. Compute double-precision `max(0, -log10(max(1, intensity) / white))` per channel.
   Keep nonzero OD tuples whose three components each meet `min_od`.
3. Retain at most `sample_limit` tuples using Algorithm R reservoir replacement,
   SplitMix64 and rejection sampling for unbiased bounded draws. A fixed seed,
   tile size, level and source stream reproduce the same retained tuples.
4. Accumulate the uncentered 3x3 second moment with compensated sums. A native
   symmetric Jacobi solver yields descending eigenvalues and the two-dimensional
   plane. Reject a second direction or spectral gap at most 1e-6 of the largest
   eigenvalue. Reject the smallest eigenvalue / trace above the configured bound.
5. Anchor the first in-plane axis to the projected mean OD. Orient the plane
   normal so its largest absolute component is positive, and derive the second
   axis with a cross product. Require all retained projections to lie in the
   positive half-plane, avoiding a percentile cut across an angle wrap.
6. Use linearly interpolated `trim_quantile` and `1-trim_quantile` angular
   percentiles. Convert the endpoints back into unit OD vectors. Negative
   components below -1e-10 fail; roundoff within that tolerance is clamped to zero.
7. Sort the first two directions by descending lexicographic RGB components and
   validate their separation. The third direction is the normalized signed cross
   product used by the existing native `StainTransform`.

Blank, constant, effectively rank-one, poorly separated or strongly nonplanar
ROIs fail with an explicit error. No fallback basis is invented. A representative
ROI must contain both absorption directions. The ordering rule supplies stable
channel numbers, without assigning biological stain labels.

## Parameters, levels and memory

| Parameter | Default | Valid range / meaning |
| --- | ---: | --- |
| `min_od` | 0.05 | 0..3, required in every OD channel |
| `trim_quantile` | 0.01 | 0..0.25 |
| `min_angle_degrees` | 5 | 0.1..89 |
| `max_residual_fraction` | 0.05 | 0..0.25, smallest eigenvalue / trace |
| `min_samples` | 64 | At least 8, no more than sample limit |
| `sample_limit` | 200000 | `min_samples`..1000000 |
| `tile_size` | 256 | 16..1024 sampled pixels |
| `seed` | 0 | Unsigned 64-bit deterministic seed |
| `max_read_pixels` | 16777216 | 1..2**40, bounding sampled-grid pixels |
| `work_bytes` | 64 MiB | 1 byte..256 MiB |
| `level` | -1 | -1 auto, or an existing nonnegative level index |

Auto selection uses the finest existing level fitting the read and work limits.
A dense image with one level needs a smaller ROI or larger limits if its grid is
too large. There is no full sampled-raster allocation. Before reading, capture
checks dimensions, coordinate arithmetic, final tile origins and the sample
capacity. The reported `estimated_work_bytes` charges 32 bytes per possible
retained tuple (OD plus angle), two tile-sized RGBA allowances and 64 KiB of
numeric/controller scratch. The actual cap is the smaller of the bounding grid
and requested sample limit. This is bounded algorithm accounting, not a process
RSS cap; existing input geometry/provenance and decoder caches are separate.

Source descriptors are cached. Request capture performs no image decoding or
file hashing. Cancellation checks occur between tiles, rows and fitting phases;
the bounded angle sort and a decoder's current synchronous read finish before
the next check. Polling reconciles live identity before exposing a ready model.
A busy job rejects additional requests. Cancellation clears publication; a job
can be reused after its accepted work is joined. Native stats reuse `AnalysisStats`.

## Results, identity and persistence

Results expose vectors/white, descending eigenvalues, angular separation,
residual fraction, opaque/eligible/retained counts, tiles, actual level/downsample,
work estimate, parameters, source ROI and original analysis provenance. Estimates
remain separate from the document until explicitly used in an analysis request
or applied to the desktop controls.

`input_sha256` hashes the actual bounded RGBA tile stream: `qpx.rgba-tiles/1` at
level zero or `qpx.rgba-level-tiles/1` at coarse levels. The digest includes
bounding-box pixels outside the ROI or OD filter; ROI WKT and parameters identify
the selection/filter separately. It is not the full slide-file digest.

`model_sha256` hashes the tag `qpx.stain-model/1`, three white doubles and all nine
normalized basis doubles, each as little-endian IEEE binary64. Signed zeros are
canonicalized to positive zero. It identifies the exact reusable basis, not its
fit metadata. `stain_model_sha256(transform)` works for supplied bases too.
`from_normalized` validates unit vectors and nonsingular inversion while
preserving their stored values exactly.

`stain_estimate_json` / `save_stain_estimate` export schema
`qpx.stain-estimate/1`: parameters, quality, counts, ROI WKT, input/model digests,
normalized basis and full capture provenance. Output is bounded to 16 MiB and
publishes atomically. Referenced source paths and observed identity aliases are
protected from replacement. Existing ROI/tissue/cell archives retain the actual
basis/white when an estimated model is used; a loaded archive can recover its
model identity from those normalized vectors. The standalone JSON additionally
retains the fitting parameters and diagnostics. A project model registry and
complete portable execution manifests remain future plan work.

## Desktop

Select an area object, set **White RGB** and optional **Use nucleus**, then choose
**Estimate stains (F3)**. The optional **Stain estimation settings** section edits
OD cutoff, trim, separation/residual checks, read/sample limits and pyramid level.
**Use estimated stains (Shift+F3)** applies a current ready result to the shared
region/tissue/cell controls. The exact normalized basis is retained for subsequent
requests, avoiding an extra normalization pass. Existing analysis **Keep result**
and project save workflows preserve the basis actually used.

**Cancel estimate (Ctrl+F3)** stops publication. Selection/source/object changes
reject stale results; parameter changes cancel the old estimate. Estimation and
model fitting occur on a native worker, with no expensive per-frame refitting.
`GuiOptions.stain_estimate_parameters` sets initial parameters. Reports expose
`stain_estimate_state`, `stain_estimate_error`, `stain_estimate_result` and
`stain_estimate_applied`. Native replay adds estimate/apply/cancel key events and
STAINS_ESTIMATED / STAINS_APPLIED readiness gates.

## Verification

The focused installed-wheel suite passes 31 cases. Independent NumPy SVD and
quantiles check the native Jacobi solution; a separately written integer
reservoir checks fixed seeds, including the full uint64 range. Known 8-bit
synthetic absorber mixtures recover directions within a predefined 0.5-degree
fixture tolerance. Independent hashlib checks input and model digests. Other
cases cover nucleus/ellipse masks, real coarse-level pixels, exact work bounds,
invalid/degenerate inputs, atomic/alias-safe output, exact model reload, saved
analysis restart, GIL release, stale owner/selection/source rejection, native
worker lifetime and GUI estimate/apply/analyze equivalence. All 31 native full
suites and 30 core/minimal suites pass, including malformed reader buffers.

These checks establish this algorithm's synthetic contract. Numerical QuPath
workflow comparison, real stain variability, fluorescence/ICC and other supported
platforms remain part of the full project plan.


The final installed GUI wheel passes 1059 Python cases with two existing Windows
file-symlink permission skips in 244.13 seconds. A 1440x1200 visual capture verifies
the selected ROI, fitted values, estimate/apply controls and status in the sidebar.

## Recorded Windows workload

[Raw results](benchmarks/stain-estimation-view-windows.json) come from
`tools/benchmark_stain_estimation.py --size 1024 --objects 100000 --gui`.
The deterministic 1024x1024 8-bit mixture has 1048576 opaque centers and 915836
eligible tuples; the bounded reservoir retains 200000 over 16 tiles. Both
estimated directions meet the independent fixture recovery tolerance. The viewer
uses the exact same model and verifies its analysis means against a native
headless request. Capture is sampled 100 times after kernel warmup.

| Measurement | Recorded value |
| --- | ---: |
| Logical estimated working buffers | 6989824 bytes |
| Request capture p95 | 0.0071 ms |
| Worker submission | 0.0780 ms |
| Maximum sampled poll | 0.0128 ms |
| Estimation worker | 25.481 ms |
| Known-direction angular errors | 0.18382 / 0.00378 degrees |
| Plane residual fraction | 1.0618e-6 |
| Selected / visible objects | 100001 / 100001 |
| Viewer estimation | 26.786 ms |
| Viewer CPU frame work p95 / maximum | 0.620 / 1.200 ms |
| Cached overlay / GPU tile payload | 3675040 / 4194304 bytes |

The CPU frame metric covers UI/native drawing work before presentation; it is
not total frame wall time or a display refresh-rate measurement. Driver and
presentation behavior affect comparisons across recordings. This evidence is
one warm synthetic Windows workload on an RTX 4090. Real stain variability,
large complex masks, vendor catalogs and other operating systems remain
unverified. Exact native binary SHA-256:
`9e901bc510adb6824c97b9497906aed643724216602d9f9172c2ae133e6798b8`; source tree SHA-256:
`1b3652e6cdb626b7419ef768c2a076a4aae402fb10fd4a7b3153dcdcef60087e`.
