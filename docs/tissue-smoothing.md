# Pre-threshold tissue Gaussian smoothing

TissueParameters.smoothing_sigma is an optional Gaussian sigma in sampled
pixels, from 0 through 16, finite and nonnegative. Zero preserves the previous
thresholding, work estimate, algorithm, QTM1/2 archive and unfiltered-only CSV1.
GuiOptions.tissue_smoothing_sigma and the native batch --tissue-sigma option
supply the same parameter; managed viewers and SDK batches already accept the
shared TissueParameters value.

## Numerical and support contract

The kernel first computes the existing scalar feature: double-precision sum
of the three float32 optical densities, or one float32 stain concentration.
For positive sigma, it applies the existing masked Gaussian with radius
ceil(3*sigma), separable exp(-0.5*(distance/sigma)^2) coefficients and final
normalization by eligible support. The inclusive threshold is applied to the
smoothed scalar, followed by existing opening, closing, hole filling, connected
components and pixel/physical component limits, in that order.

Only fully opaque finite nominal ROI-center samples contribute. Excluded
samples have zero weight, never become output, and do not act as zero-valued
measurements. Filtering is clipped and renormalized at the captured ROI raster,
image boundaries, holes and excluded alpha samples. It does not read an outside
ROI context halo. Those support semantics are explicit native workflow policy,
not a claim of arbitrary QuPath/ImageJ filtering equivalence. A future context
recipe must declare different support and identity rather than altering replay.

All decode tiles populate one captured raster, so the filter crosses their
boundaries without independent tile halos or additional source reads. Sigma is
measured at the chosen native pyramid level; its level-zero scale follows that
level's downsample. No arbitrary-grid tissue API is introduced in this task.
No eligible samples still fails; an eligible image with no foreground still
returns an empty mask. Existing background/component and stale/cancel rules stay
in force, with cancellation checkpoints in the shared Gaussian. Jobs expose a
smoothing phase. threshold_pixels now counts the thresholded smoothed feature
for this recipe, before morphology; samples and later cleanup counters retain
their existing meanings. elapsed_ms includes filtering, cleanup_ms only morphology.

## Retention, versions and budgets

Positive sigma uses qpx.threshold-components/4 (or winding-4), with QTM3 carrying
sampling policy and sigma. QTM3 requires a positive supported sigma; QTM1/2 decode
with zero and remain canonical when re-encoded. Saved replay and verification
include the smoothing parameter. Raw input digests continue to identify the
captured pixels/lattice; algorithm and parameters identify the processing recipe.
Batch parameter identity includes a positive sigma so different recipes cannot
reuse each other's checkpoints. Other batch kinds reject a tissue sigma.

Tissue table selections containing smoothed results use tissue-table/2 (or its
project form), appending smoothing_sigma_sample_px. Mixed unfiltered rows carry
zero. Selections with no smoothing retain the original table1 header and values.

A shared request/decoder estimator charges 41 bytes per raster sample for labels,
queue, Gaussian values/weights/scratch and domain, plus the bounded kernel and
existing component/tile/bookkeeping allocations. Unfiltered 8/9-byte paths retain
their old estimates. The new parameter's metadata is fully charged for smoothed
results; unfiltered legacy metadata bookkeeping stays stable within the existing
fixed allowance, including the three scientific/nominal grid mask estimators.
The old retained-result and exact table snapshot witness was captured at 40ece8a9
and measured after reopen before this implementation. Its original capture
sizes are also preserved, because archive decoding compacts component capacity.

## Verification status

The independent native test uses direct 2D weighted convolution, independent
OD/stain scalar values, crop/ROI/alpha eligibility, noninteger levels, short and
long kernel paths and multiple decode tile sizes. It also checks QTM3 canonical
replay, checksum-resigned invalid recipes and work admission. Five affected
native regression roles pass. tests/test_tissue_smoothing.py adds installed SDK,
legacy quota, mixed CSV, save/reopen, batch/CLI/resume and actual desktop checks;
current-task.md records which installed checks remain pending. Median filtering,
arbitrary-grid/context semantics, specimen accuracy, other hosts and whole-slide
resource/speed claims remain in R7/R12 and the wider queue.
