# Binary tissue median cleanup

TissueParameters.median_cleanup adds optional fixed3x3 median cleanup of the
binary threshold mask. It defaults to false. Filtering order is scalar feature,
optional Gaussian smoothing, inclusive threshold, binary median, opening,
closing, hole filling, components and pixel/physical size limits.

The pinned [QuPath0.7.0 tissue plugin](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core-processing/src/main/java/qupath/imagej/detect/tissue/SimpleTissueDetection2.java)
uses post-threshold ImageJ radius1 median. [ImageJ1.54p RankFilters](https://github.com/imagej/ImageJ/blob/v1.54p/ij/plugin/filter/RankFilters.java)
constructs a full3x3 neighborhood at that radius and replicates cropped image
borders.524 captured rectangular-mask outputs, including all512 three-by-three
patterns, match independent nine-point majority calculations. The capture uses
the retained QuPath runtime and recorded source hashes, not a new dependency.

## Native support and ordering

For each eligible center, at least five of the nine binary neighborhood values
must be foreground. Crop edges repeat the closest border sample;1x1 and narrow
rasters use the same rule. Excluded ROI/alpha samples contribute background and
never become output. Thresholding already applies native ROI-center and opaque
eligibility, so the median also receives a clipped mask. This native support
policy applies to holes and nonrectangular ROIs. The ImageJ comparison covers
the binary primitive on full opaque rectangles. Upstream ROI-clear ordering,
preprocessing, context and annotation assembly retain their separate contracts.

Horizontal votes followed by vertical summation produce the nine-point rank.
The existing reserved flood-fill queue supplies scratch storage; it is cleared
before later cleanup/components. No extra source reads or raster allocation are
introduced. All decode tiles share the captured raster, so the filter crosses
tile boundaries. The selected native pyramid level defines the sampled pixel
size. Gaussian smoothing, when enabled, remains before thresholding.

Median work participates in the existing cleanup phase/timing. threshold_pixels
counts the pre-median threshold mask; cleaned_pixels counts all cleanup before
component size limits; foreground_pixels counts the final retained mask. Median
can add or remove eligible foreground. Samples, connectivity and hole-fill
counters retain their existing definitions. Cancellation checkpoints guard both
vote passes; existing request identity rejects stale publication.

## Persistence, consumers and budgets

Median uses algorithm5/winding-5 and QTM4, retaining sampling policy, Gaussian
sigma (possibly zero) and the enabled median flag. A QTM4 record must enable
median. QTM1/2/3 remain canonical when median is disabled. Replay/verification
include median state. Raw input digests still identify pixels/lattice; recipe
parameters identify processing. Batch identity includes a true median flag.

Selections containing median results use tissue-table/3 or its project form,
appending smoothing_sigma_sample_px and median_cleanup. Older rows in a mixed
selection carry their actual sigma and zero median flag. Gaussian-only selections
keep CSV2, and unsmoothed selections keep CSV1. No project schema bump is needed.

The shared TissueParameters field is exposed to Python, managed viewer and SDK
batch callers. GuiOptions.tissue_median_cleanup and a visible3x3 checkbox use
the same request. Native CLI --tissue-median enables cleanup for --kind tissue;
other kinds reject an enabled tissue median.

The ordinary cleanup9-byte raster estimate covers labels, scratch and domain.
Gaussian+median keeps the existing Gaussian41-byte estimate; scratch is reused.
A shared offset-based metadata policy replaces scattered hardcoded deductions.
Inactive extensions remain in the existing allowance; enabled Gaussian/median
groups charge aligned storage fully. Old unfiltered and Gaussian record/table/
batch/work boundaries were captured before editing, including compact decoded
component capacity. Exact byte witnesses are MSVC/x64 scoped.

Verification status lives in current-task.md until closeout. Median at other
radii, arbitrary-grid/context semantics, specimen comparisons, whole-slide
resources and broader hosts/release support remain in the project queue.
