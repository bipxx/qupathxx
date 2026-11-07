# Why the native cell path is still slower

The settled integrated result remains **320 ms native / 292 ms QuPath** on the
selected 512x512 ROI. This investigation identifies a demonstrated native
inefficiency, not a new integrated speed result. Product sources/binaries are
unchanged from eb9db7a9; no release or qualification campaign was run.

## Demonstrated cause: general neighbor logic inside repeated watershed passes

`SampledShapeWatershed::grow_levels` repeatedly builds an eight-neighbor occupancy
mask. Each neighbor goes through `neighbor`, which derives coordinates using
runtime width division/remainder, tests borders and directions, and computes an
index. This happens inside repeated directional growth passes. C++ Release /O2
was already enabled; the algorithm's access pattern still matters.

The pinned ImageJ1.54p MaximumFinder.processLevel uses explicit neighbor offsets
and decodes packed x/y coordinates once per pixel. Its candidate/maxima routines
also distinguish interior pixels from border pixels. Native preserved behavior
but did not carry over those low-level access choices.

A private copied-header experiment computes the same eight occupancy bits with
direct offsets for interior pixels and keeps the original helper on borders.
It preserves scan order, fate-table decisions, delayed writes and traversal/cancel
checks. No watershed rule, ordering or precision changed.

| Isolated Release probe, median of last6 /12 runs | Original | Interior access | Later original control |
| --- | ---: | ---: | ---: |
| Response basins |25.51 ms|25.08 ms|27.50 ms|
| Filled basin regions |13.25 ms|13.15 ms|13.62 ms|
| Shape watershed, all stages |55.37 ms|39.09 ms|57.87 ms|
| Shape growth alone |22.78 ms|7.73 ms|23.36 ms|
| Final nucleus labeling |10.00 ms|9.73 ms|10.02 ms|
| Combined nucleus stages, per-run sum median |105.16 ms|87.03 ms|108.88 ms|

All36 runs produce exactly2,237 cells and byte-identical nucleus labels against
the frozen real workflow. The later control supports a real local effect rather
than warming the host. The interior probe reduces this nucleus subtotal about17%
against the earlier original; this is not a measured17% full-pipeline gain.
There is no paired Java sub-stage timing, so this does not establish how many
milliseconds of the native/reference difference each sub-stage causes.

## Demonstrated extra work: small hash calls, repeated mask construction checks

Both ScientificRasterKernel::make calls recompute the same input digest for the
same immutable source/RGB snapshot. Each digest loops262,144 samples and calls
Sha256::integer once per eligibility boolean. That emits an eight-byte little
endian integer, then calls BCryptHashData immediately. Nucleus and body together
therefore make524,288 tiny updates for this part of their input identity alone.

A standalone probe using the actual production sha256.cpp hashes exactly the
same262,144 eight-byte integers. Individual updates take3.480 ms versus0.920 ms
for8 KiB batches, with identical digests in every run. This isolates provider-call
overhead; it excludes ROI evaluation, metadata and raster construction. It suggests
about5 ms avoidable across two such loops, not a measured full-workflow saving.
The existing digest format must stay exact: batch the same serialized bytes,
then consider sharing the verified identity within one immutable construction.
Do not replace it with a different packed-bit format or trust caller strings.

Eligibility is also evaluated when constructing the detection domain, validating
each raster, and hashing each raster. These are the same internal immutable
snapshot but general helpers repeat bounds/ROI work. Sharing an already verified
domain is a separate measured-follow-up opportunity; this probe did not time it.

## Remaining geometry overhead and what the evidence does not prove

Native traces both label rasters, validates each face, maps it, intersects it with
the image/ROI clip, then constructs another validated ROI. QuPath's selected
plugin directly creates PolygonROIs from ImageJ label tracing; it does not take
this exact general native construction path. The previous diagnostic baseline
measured58-67 ms per mask in intersections and additional validity/construction
work. Those are older parallel-stage observations, not a current320 ms breakdown.

The general geometry path remains an opportunity, but the earlier rectangle
shortcut changed ordered stored geometry and was correctly rejected. Any fast
path must preserve exact masks, actual geometry and archive identity.

Nucleus box traversal is not catastrophically amplified on this ROI: accumulated
basin boxes cover174,902 samples, and final nucleus boxes286,874, against262,144
image pixels. Filling is not the largest measured nucleus cost here; avoid a
large connected-component rewrite before the demonstrated neighbor fix.

Existing integrated capture/keep medians are0.049/0.015 ms; publication is3.49 ms
and detection316.58 ms. Python crossing/ImGui are not the measured main bottleneck.
Both engines make measurements, but their result models and identity validation
are not identical; equal cell partitions do not mean identical internal work.
Do not ascribe the remaining gap to C++ versus Java or add a GPU/thread framework
without a corresponding measured bottleneck.

## Evidence and next action

[Raw and summarized probes](logic/summary.json), [hash result](logic/hash-summary.json),
[header hashes](logic/header-inputs.sha256) and [artifact hashes](logic/evidence.sha256)
retain scope. Baseline inputs are the unchanged production headers; instrumentation
is in logic/profile.patch, and the controlled access change in logic/interior.patch.
The probe rebuilds only two small translation units using the same MSVC toolchain,
/O2 /Ob2 /DNDEBUG /MD; ImageJ Gaussian uses /fp:strict as in production. It borrows
the retained rgb.u8/nucleus.u32. Six warmups precede six measured runs; correctness
comparison happens after each timed nucleus run. Local tools/runtime/project copies
stay outside Git under build/speed-gap-logic; no full module rebuild was needed.

The QuPath plugin source was fetched at the pinned04ccfa4 revision and matches the
retained source exactly after newline normalization. Its URL/hash is in
logic/reference-source.json. ImageJ's retained1.54p MaximumFinder is the production
behavioral reference. Private-header timing emits only a small number of stage
records; absolute times remain local observations.

Next: apply the narrow interior-neighbor optimization to production, check edge
widths/topology/cancellation plus existing numeric/native cases, then run one
source-scoped exact-output and settled paired check. Profile remaining repeated
neighbor work or hash buffering only after that result. Avoid summing isolated
savings and promising parity before integrated measurement.
