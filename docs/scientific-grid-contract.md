# Scientific sampled-grid contract, proposal 1

This is the preparation contract for queue item 5. It is **proposed**, not a
shipped detector API or scientific acceptance. C++ geometry and a level-zero
float64 RGB sampler are implemented on the integration branch, with owning
nanobind values undergoing installed-package validation. The full detector
workflow remains pending. All M0–M7 criteria remain in scope.

The current cell path selects natural levels in `cells.cpp::choose`, pads a
natural-index rectangle in `cell_recipe.hpp::processing_grid`, rounds each read
origin with `round(index * downsample)`, and evaluates ROI membership at ideal
level-zero centers. `input_digest.cpp` independently repeats the origin logic
and includes tile size in the old digest. These are historical semantics; an
arbitrary grid must not silently replace them under an existing algorithm ID.
Physical nucleus-area limits already exist in `with_nucleus_area_limits`; reuse
their inclusive-bound and calibration checks rather than implementing them twice.

## Grid and reader coordinates

Represent a base **integer level-zero extent** `(x0, y0, width, height)` and
explicit output counts `(nx, ny)`. The actual axis spacings are the exact rational
values `width/nx` and `height/ny`. A sample `(i,j)` covers half-open footprints
`[x0+i*width/nx, x0+(i+1)*width/nx)` and the corresponding Y interval. Its center
is the midpoint. Source pixels occupy integer unit squares. This retains the
accepted resampler's extent-driven definition and admits anisotropic output.

An optional requested physical spacing is a planning target, not permission to
mislabel the result: compute counts explicitly and retain both the request and
actual spacings. Multiplying actual spacings by each captured calibration axis
gives actual micrometres per sample. Never replace two axes with their mean.
Zero/unknown calibration is not a physical value; reject physical-parameter
requests without both finite positive axes. Pixel-unit requests can remain valid.

The first new reader adapter uses explicit **level zero**, with a bounded decoded
source window. This is an implementation step, not completion of pyramid support.
It must reject an inadmissible source window before decoding, rather than silently
coarsen the requested grid. A later natural-level adapter must capture source
level, decoder/input identity and read-origin phase, and prove its tiling rules.
Fractional natural downsample plus rounded level-zero read origins is not an
exact global source grid. Do not claim level-zero numerical equivalence from a
pyramid's coordinate alignment; differently encoded levels contain different data.

## Context, masks and numeric data

Context `(cx,cy)` extends sample indices to `[-cx,nx+cx)` and `[-cy,ny+cy)` on
the **same** grid. Conservative read bounds use mathematical floor/ceiling of
those rational endpoints, including negative values. Clip IO to image bounds;
retain missing support explicitly. Cropping a source window and resizing it to
the context size would shift the grid and is forbidden.

Context supplies algorithm input outside the selected ROI. Publication eligibility
requires a core-grid index, membership of the original ROI at the captured center,
and valid source support. Capture `RoiSamplingPolicy`; start with the existing
GeometryCoverage predicate and separately enforce half-open ROI bounding limits.
Do not infer polygon/hole boundary behavior from rectangle fixtures.

The proposed RGB reader adapter averages opaque RGB8 source footprints with exact
area weights, yielding **unrounded float64 RGB**, then applies OD/stain transforms.
Any contributing nonopaque or missing source pixel invalidates that sample; do
not renormalize around missing support. Record this policy in the recipe. This
differs from display RGBA8 resampling and must not pass through `PixelBuffer` byte
rounding. Typed quantitative channels require a distinct dtype-preserving adapter;
they must never be routed through display RGB or have their validity inferred
from display alpha. Both adapters share grid geometry, not an implicit data type.

The native `read_scientific_rgb(server, grid)` path reads one bounded, clipped
level-zero window. Captured image dimensions must match a unit-scale RGBA8 level
zero. Invalid samples contain canonical zero RGB with validity zero; callers
must use validity rather than treating zero intensity as missing. Validity alone
does not encode ROI membership or core-grid eligibility. The output includes
context at array origin `(-cx,-cy)` and retains its immutable grid.

`ScientificRgbGrid.numpy()` exposes an owning-lifetime, read-only float64 HWC
view; `valid_numpy()` exposes uint8 HW validity. Explicit `copy=True` creates
independent mutable arrays outside the sampler's retained-memory budget. The
sampler does not round to RGBA8 or apply OD/stain transforms itself. The exact
overlap cap bounds the integer channel numerator to `255 * 2^28`, allowing one
correctly rounded binary64 division after exact accumulation. Decoder errors,
wrong-sized/short buffers, closed or mismatched readers and cancellation reject.

Convert exact sample centers to binary64 at the geometry boundary using correctly
rounded rational values. Avoid platform-dependent `long double` arithmetic and
rounding a large integer product before division. The oracle retains rational
coordinates and binary64 hex values, including large origins and half-pixel ties.

## Filters, limits and reproducibility

Physical smoothing/expansion must use actual X/Y sample spacings. Unequal physical
axes require explicitly anisotropic kernels/metrics; a scalar pixel radius or a
geometric mean is not an implementation. Capture per-axis resolved parameters
alongside physical requests. Existing scalar pixel parameters keep their defaults.
Gaussian support, IO halo and detector context are distinct. A finite Gaussian
halo does not make global watershed/connectivity computations independently
tileable. First materialize one bounded processing grid; tile IO only, then use
the existing whole-grid kernels where their metric contract is correct.

Preflight coordinates, sample counts, context, source support, exact overlap work,
all live buffers, contour work and cancellation before IO/allocation. Reuse the
existing 256 MiB reader-buffer and `2^28` area-overlap ceilings; keep the existing
cell request's total work budget authoritative. The new peak ledger must include
float64 sampling buffers in addition to detector arrays. Proposed geometric
admission: integer extents/coordinates within `10^12`, axis sample counts at most
`10^6`, context at most 4096; reject arithmetic overflow and label-index overflow.
These caps are not a substitute for the stricter byte/work admission.

The geometry consumer admits a sampling budget (256 MiB default, at most 1 GiB)
covering one clipped RGBA8 source window plus 24 RGB float64 bytes and one
validity byte per context sample. The source window separately remains at most
256 MiB. This is a sampler budget, not a claim to include detector/kernel/contour
arrays; the future request must admit those additional live allocations.

For a window of indices `[a,b)`, exact 1D overlap visits equal the number of source
unit cells touched plus the number of noninteger internal sample boundaries.
For an unpadded extent this reduces to `extent + samples - gcd(extent,samples)`.
The 2D count is the product of the two axis counts. Validate a native efficient
formula against the oracle's enumerated rational intersections, including halo.

The proposed detector is additive (`GridLogWatershedBodies`, appended enum value;
`qpx.grid-log-watershed-bodies/1`), with explicit new input-digest and archive
versions. Names are proposals until implemented. The new digest captures grid,
context, ROI/membership, transform/calibration, validity, source-level/reader
identity and decoded input in a canonical order independent of IO tile size.
Old input formats intentionally include tile size and must remain replayable.
QCL5/QCA5 are proposed containers for the new recipe, not changes to QCL1–4/QCA1–4.
Unknown versions reject; no silent conversion of old provenance or historical masks.

## Implementation and exit evidence

1. Implement one native extent/grid-window planner against the independent
   [rational oracle](../tests/fixtures/scientific-grid-v1/reference.json). Bind
   owning immutable values thinly with nanobind. Geometry alone is not a detector.
2. Add the level-zero float sampler and calibrated/unknown-unit admission, with
   opaque/partial/missing support and tile-size invariance checks. Reuse exact
   overlap arithmetic, not the display filter's byte output.
3. Add one cell request/recipe through native kernels, digest, immutable result,
   archive/reopen/replay, Python, CLI and ImGui. Implement anisotropic filters
   before accepting unequal physical axes; preserve every old recipe byte contract.
4. Validate independent masks/features/split–merge metrics under predeclared
   tolerances. Keep legacy strict xfails and the 503/478-vs-506 NDPI observations.
   Synthetic grid arithmetic is not biological or QuPath segmentation acceptance.
5. Reuse the validated contract for tissue and classifier requests, then add
   proven natural-level/typed-raw adapters and remaining batch/release coverage.

Research review of physical interpretation and biological tolerance/labels remains
an M0 prerequisite for scientific acceptance. The oracle fixes exact mathematical
expectations; it does not select a biological tolerance after seeing native output.

## Geometry implementation evidence

`ScientificGridAxis`, `GridFraction` and `ScientificGrid` are owning C++ values
in `qupathxx/scientific_grid.hpp`. They compute exact rational edges/centers,
positive source overlaps, clipped read windows, overlap visits and buffer admission.
Rational conversion uses integer long division with one nearest-even rounding,
avoiding both large-product rounding and negative near-zero cancellation.

The independent Python Fraction oracle generates JSON and the C++ expectations;
the manifest binds their bytes, generator and this contract. Native checks cover
14 geometry scenarios, eight large-origin counterexamples, 521 exact conversion
probes and invalid/budget boundary controls. They run within the existing native
resampling suite. The oracle's RGB values are future sampler expectations;
passing geometry checks does not establish that native RGB sampling exists.
