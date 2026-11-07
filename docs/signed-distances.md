# Signed Cell-to-annotation boundary distances

Select Cells as sources and area Annotations as targets. The native distance
workflow returns one row per Cell, using the nucleus centroid where available
and the body centroid otherwise. `prefer_nucleus=false` selects body centroids.
No image pixels are read. All selected objects must share a Z/time plane.

Magnitude is the shortest Euclidean distance from the source centroid to any
boundary segment of the chosen annotation, including hole and multipart rings.
Negative means inside that annotation; positive means outside (including holes);
zero means on its boundary. The target with minimum absolute distance wins;
exact ties choose the lower object ID. Overlapping annotations remain individual
regions. This is not distance to their union or distance between object centroids.
An empty target cohort produces `no_target` with absent target/distance fields.
CSV `source_x/y` and `target_x/y` retain the source and annotation centroids for
identity/context; target coordinates are not a nearest boundary contact point.

Pixel units need no calibration. Micrometres require positive finite X and Y
calibration; each coordinate axis is scaled independently **before** measuring
segments. There is no mean-pixel-size approximation. Ellipses are refused unless
`polygonize_curves=true`; this explicitly uses the existing 512-segment polygon
for both boundary distance and membership. Rectangles and polygon/multipart areas
use their represented rings directly. Inputs are not clipped or repaired.

```python
import qupathxx as qpx

options = qpx.SpatialDistanceOptions()
options.metric = qpx.SpatialDistanceMetric.SIGNED_BOUNDARY
options.units = qpx.SpatialDistanceUnits.MICROMETRES
request = qpx.SpatialDistanceRequest(document, cell_ids, region_ids, options)
result = qpx.measure_spatial_distances(request)
record_id = document.keep_spatial(request, result, "Signed boundary distances")
qpx.save_spatial_distance_csv(result, "new-distances.csv")
```

Use existing `SpatialDistanceJob` for background progress/cancel, and
`SpatialDistanceRequest.replay` plus `Document.verify_spatial` after reopening a
saved project. Requests retain immutable geometry, calibration and identity;
document changes invalidate collecting/keeping a stale result. Replay checks
current selected objects and policies against the saved snapshot. Stored SHA
and CSV bytes remain exact across save/reopen; a fresh replay can have a new
revision-bearing input SHA and is compared through native verification.
Opening a record validates its identity, geometry, selected target distance and
segment count. It retains an unverified historical status until explicit Verify
recomputes the complete nearest-boundary search, as with centroid records.

```text
qupathxx_batch --measure-spatial source.qpxproj new-result.qpxproj \
  --image 1 --source 12 --target 3 --operation signed-boundary --units um
qupathxx_batch --export-spatial new-result.qpxproj new-distances.csv \
  --image 1 --record RECORD_ID
qupathxx_batch --verify-spatial new-result.qpxproj --image 1 --record RECORD_ID
```

Repeated IDs or existing bounded `--source-ids`/`--target-ids` files select the
cohorts. `--body`, `--polygonize-curves`, capture/work limits and
`--max-distance-tests` reuse existing admission and publication rules. For this
metric, distance tests count boundary **segments**, not source/target pairs;
every segment checks cancellation. A target is limited to 10000 represented
vertices. Budget exhaustion fails without publishing a partial measurement.
Work/snapshot allowances bound admitted payload, not whole-process memory.
The initial kernel scans the selected boundaries under that segment-test limit;
large cohorts may require narrower selections. No indexed-search speed claim is
made by this implementation. Unresolvable metric underflow/overflow is refused.

In ImGui, open **Spatial...**, set sources from the selected Cells and targets
from selected area annotations. Choose **Nearest distance**, then **Signed Cell
to annotation boundary** in Distance metric. Choose units and run; the existing
result table, Keep, saved-record Inspect/Verify and CSV controls apply. The
algorithm label and sign explanation identify displayed signed results even
when controls have subsequently changed.

Signed records use `QSD2` inside the existing project distance-record table;
centroid records continue using `QSD1` and their historical hashes/CSV schema.
Signed CSV appends `metric` and `geometry_policy` to the existing columns and
names algorithm `qpx.signed-boundary-nearest/1`. Older published runtimes refuse
QSD2; use the matching new runtime for projects containing signed records.
Published candidate wheels, SDKs and prior app bundles remain frozen.

This workflow measures selected geometry; it does not segment cells, infer
tissue, establish biological validity or make a QuPath speed/parity claim.
