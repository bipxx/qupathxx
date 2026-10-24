# Arbitrary-size RGBA region resampling

`read_resampled_region(server, request, width, height)` reads the explicit
natural pyramid rectangle in `request` and maps its complete extent to an
owning `width` by `height` RGBA8 buffer. The source origin remains in level-zero
coordinates and its dimensions count pixels at the selected natural level.
Output pixel edges partition that rectangle uniformly. Enlargement, reduction,
noninteger ratios and independent horizontal/vertical ratios are supported.

```python
import qupathxx as q

server = q.open_slide("sample.svs")
request = q.RegionRequest(1024, 2048, 0, 1536, 1024)
pixels = q.read_resampled_region(server, request, 640, 480)
view = pixels.numpy()  # owned, read-only RGBA view
server.close()        # pixels and view remain valid
```

`resample_area(pixels, width, height)` applies the same filter to an existing
`PixelBuffer`. The thumbnail workflow uses this shared native implementation.
The bindings release the GIL during decoding/filtering; Python implementations
of readers are not introduced. Source buffers remain unchanged. The existing
natural-level `read_region` API and thumbnail planning policy retain their
coordinate conventions.

The standalone native command exposes the same operation:

```text
qupathxx_region --resample IMAGE X Y LEVEL SOURCE_WIDTH SOURCE_HEIGHT WIDTH HEIGHT OUTPUT.pam
```

The output PAM header records the resized dimensions. The source image cannot
be overwritten, and invalid dimensions fail before output creation.

## Filtering and alpha

Each output pixel integrates the covered source pixel cells with exact integer
overlap weights. RGB channels are accumulated with alpha weights, then converted
back to straight RGBA with round-half-up integer division. Alpha is the area
mean. Pixels whose rounded alpha is zero have zero RGB. Transparent padding is
included in the output pixel's total area; it does not darken retained color.
Enlargement uses these same piecewise-constant pixel cells. An integer scale
replicates cells; a fractional scale can mix cells across an output footprint.

The filter operates in the reader's device color space. ICC conversion, linear
light filtering and arbitrary rotated/affine grids require separate contracts.
This API does not silently change scientific detector/classifier recipes or
select a natural pyramid level. Those workflows still require their own grid,
provenance and reference comparisons.

## Admission, ownership and cancellation

Source and output dimensions must each fit the existing 256 MiB RGBA buffer
limit. Before decoding, admission computes the exact number of overlap visits:

`(source_width + width - gcd(source_width, width)) *
 (source_height + height - gcd(source_height, height))`.

At most 268,435,456 visits are allowed. This rejects extreme opposite-axis
resizing even if both buffers individually fit. Division checks avoid overflow.
Invalid sizes, levels or work requests fail before the reader is called.
Both native buffers can coexist (up to 512 MiB); reader temporaries and decoder
storage are additional, so this is not a process-wide memory budget.

Native callers may pass an atomic cancellation flag. It is checked before and
after synchronous decoding, per output row and every 4,096 overlap visits.
Cancellation cannot interrupt an underlying synchronous reader. Results are
returned only after filtering completes; decoder failures and unexpected reader
dimensions/byte counts propagate without returning a partial buffer.

## Validation status

Candidate 154 passes all 47 local minimal native suites, including 2,205
independent all-source overlap comparisons. The installed minimal Python wheel
passes 125 focused tests with seven optional reader/feature skips; all 13
mandatory resampling cases pass. They cover exact-fraction filtering, fractional
ratios, enlargement, unequal axes, identity, alpha, padding, lifetime, quota
rejection, closed/invalid readers and standalone CLI output/source protection.
The shared filter retains the existing thumbnail regression coverage. Generated
stubs match the installed module; three typed workflows pass and the seven
intended invalid calls remain rejected. The [local validation receipt](benchmarks/region-resampling-validation-local-windows.json)
records the configuration, skips and initial corrected exception expectation.

Hosted auditors require the native oracle marker and all 13 named installed
cases in Windows/Linux and portable builder/consumer evidence. The Mac auditor
requires the native suite; it does not claim Mac Python bindings. Source binding
normalizes Git text checkout CRLF only and records both committed and raw hashes.

An identity fast path copies the buffer and canonicalizes fully transparent RGB.
The [component pilot](benchmarks/region-resampling-identity-local-windows.json)
retains all 66 observations (two warmups and nine timed calls for each of three
cases before/after). On the local 2048×1024 input, opaque identity median falls
from 28.83 to 2.35 ms and mixed-alpha identity from 28.66 to 2.50 ms. Fractional
resampling medians are 8.09/8.33 ms. Every input/output digest agrees across the
two runs, and identity also matches an independent array expectation. Reproduce
with `python -I tools/profile_region_resampling.py --label NAME --output FILE`.
These measurements cover the native filter call only; decoder, UI, process
memory, hardware budgets and end-to-end performance remain unaccepted.

Exact-source [Windows/Linux](benchmarks/region-resampling-engine.json),
[Mac native](benchmarks/region-resampling-macos.json) and
[portable builder/consumer](benchmarks/region-resampling-portable.json)
engineering audits pass at `f2e8c2f`. The [promotion receipt](benchmarks/region-resampling-main-promotion.json)
records publication to `main`. All seven installed Python cohorts pass the 13
named resampling cases and 18 auditor controls, alongside the 389 inherited
runtime-inventory cases. Native suites pass on all configurations, including
both Mac architectures and sanitizer builds. Mac relocated package probes pass.

Real vendor resampling, scientific recipe integration, physical OS/hardware
budgets, broader biology and supported releases remain open. The full M0–M7
goal is still partial.
