# Optical density and fixed stain separation

`qupathxx/stains.hpp` implements the first native M5 analysis primitive. It works
on owned RGBA region buffers, independently of display windowing, gamma, LUTs,
document edits and the shape-measurement cache. Nanobind invokes the same kernel
with the GIL released. No Python or image-server references enter the result.

## Numerical contract

For 8-bit channel intensity `I` and measured white `W`, optical density is
`max(0, -log10(max(I, 1) / W))`. Thus zero and one share the finite black value,
and values above white have zero OD. This matches the scalar convention in
[QuPath v0.7.0 ColorDeconvolutionHelper.makeOD](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/color/ColorDeconvolutionHelper.java).
White is explicitly supplied per RGB channel, finite and within `[1,255]`;
the default is `[255,255,255]`. The scalar `optical_density` additionally accepts
fractional intensities in `[0,255]` and rejects nonfinite/out-of-range input.
There is no ICC conversion or assumption of fluorescence applicability.

`StainTransform(stains, white=[255,255,255])` accepts two or three RGB OD
direction vectors. Each vector must be finite and nonzero. Normalization scales
by its largest component before finding its Euclidean norm, so very large or
very small finite input magnitudes do not change its direction. Two vectors
derive a third normalized cross-product residual in the supplied order.
This residual can have negative components. Three supplied vectors retain their
directions, including signs. The normalized basis determinant must have absolute
value at least `1e-8`; a two-vector cross-product norm must also reach `1e-8`.
These are explicit numerical rejection thresholds, not QuPath parity tolerances.

Let the normalized stain vectors be the columns of matrix `S`. Separation solves
`S * concentrations = RGB_OD`. Negative concentrations are retained, without
clipping or reconstruction to an 8-bit display. The third component for a
two-stain request is a residual coordinate, not an independently identified
biological stain. No automatic stain estimation or named stain preset is implied.

Construction prepares three 256-entry OD tables and three 256-entry tables of
three double-precision concentration contributions. Each valid pixel then needs
table lookups and additions, with no per-pixel logarithm or matrix inversion.
OD and separation both return three interleaved native-endian IEEE float32
components. Double-precision intermediates are rounded on output. All three
components are NaN when alpha differs from 255, including transparent region
padding. Partial alpha is excluded rather than interpreted as measured tissue.

## API and ownership

```python
import numpy as np
import qupathxx as q

server = q.dense_image_server(q.Image.demo())
pixels = server.read_region(q.RegionRequest(0, 0, 0, 256, 256))
# Example directions only; supply vectors validated for the actual experiment.
stains = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
result = stains.deconvolve(pixels)
concentrations = np.frombuffer(result.data, dtype=np.float32).reshape(
    result.height, result.width, 3)
rgb_od = stains.optical_densities(pixels)
```

`FloatRgbImage` owns its result and exposes `width`, `height`, `row_stride` in
bytes and `data`. The Python `data` property copies into an immutable `bytes`
object; NumPy may then read that copy without another copy. C++ `values()` is a
const reference with the result's lifetime. Closing/deleting the original server
or deleting the transform/input does not affect a result. Parameters returned
to Python are snapshots. There is no NumPy runtime dependency.

The transform retains no inputs, cached results or mutable scratch buffers.
Concurrent calls may share a transform and immutable `PixelBuffer`. C++ callers
must not mutate an input concurrently. Malformed native RGBA buffer lengths are
rejected. Each float result has its own 256 MiB allocation ceiling, checked
before allocating; this is separate from the input's existing ceiling. Callers
must tile larger regions and bound concurrent allocations. The kernel has zero
spatial halo: identical input pixels produce identical values in whole-region
and partitioned execution.

C++ calls accept an optional `const std::atomic<bool>*` cancellation flag,
checked before allocation, every 4096 pixels and before return. Cancellation
throws without publishing a partial result. Allocation itself is not
interruptible. Python calls currently run synchronously with the GIL released;
the future analysis request/job API must provide cancellation, source-version
provenance, stale-result rejection and GUI publication. These transforms do not
change any document or persist analysis results.

## Verification and scope

`tests/test_stains.py` checks all 256 intensities with unequal channel whites,
independent NumPy matrix solves for two/three/signed bases, reconstruction,
padding, closed-source/result lifetimes, concurrent reuse, exact tile-partition
equality, normalization range and invalid parameters. Native tests additionally
exercise partial alpha, malformed RGBA input and cancellation. Tests compare
float32 results with `rtol=1e-6, atol=2e-6` and scalar OD with `atol=1e-14`.
These are mathematical fixtures; a QuPath runtime comparison with recorded
stain definitions remains required for upstream workflow parity.

`tools/benchmark_stains.py` measures construction, warmed native transforms
(including result allocation), throughput and Python byte copying separately.
Image construction/read, reference solving and previous-result destruction are
outside the timed kernels. Every benchmark output is compared with the
independent formula/linear solve. Results do not establish detection throughput,
slide I/O performance, job scheduling, GUI responsiveness or whole-application
speedup. M5 also still requires tissue/cell algorithms, tiled reconciliation,
measurement publication, provenance, and the same jobs through C++/Python/GUI.

The recorded [Windows 1048576-pixel run](benchmarks/stain-transforms-1048576-windows.json)
uses twelve warmed calls for each transform. Median OD is 4.39 ms (239 Mpixel/s),
and separation is 5.49 ms (191 Mpixel/s), including each 12 MiB result allocation.
Transform construction is 0.028 ms; Python byte copies cost an additional
2.42/3.90 ms. Maximum absolute errors against the independent references are
`1.17e-7` OD and `2.38e-7` concentrations. This is one synthetic dense-buffer
recording on the identified machine, not a cross-platform performance guarantee.
