# Spatial analysis delivery contract

SPAT1 is the next prepared SCI1-B workflow: measure each selected object's
centroid distance to a selected target cohort, inspect the immutable table,
export it and save/reopen it. The [native geometry/query foundation](benchmarks/spatial-native-263/receipt.json)
is locally verified at 3af9572. [Capture264](benchmarks/spatial-capture-264/receipt.json)
adds owner-captured IDs/revisions/centroids/geometry hashes, immutable results,
budgeted nearest queries and owner jobs at2b9c5d7. Four affected native suites pass
each Windows GEOS-ON/OFF profile. Workers retain no live Document or reader and
perform zero pixel IO. Geometry freshness binds document identity/revision; a
closed image source alone does not invalidate a geometry-only request.

Next implement versioned retained records/archive and bounded atomic CSV export,
then thin Python/CLI/ImGui/managed delivery. Native oracle coverage is18 geometry
cases ON/16 OFF plus four separately checked upstream storage-conversion cases;
four nonzero-plane cases are explicitly unsupported. Corrected902 tooling controls
pass at7bb8d78 without rebuilding unchanged native inputs. Frozen candidate262 has
separate complete engineering acceptance; none transfers to spatial264.
Neighbourhood counts, signed geometry distances, density maps and tiled feature
aggregation remain required subsequent spatial work.

## Observable behavior

- Use true geometry centroids. Prefer a cell's nucleus when present; otherwise
  use its body. Polygon area, line length and multipoint centroid semantics
  differ from the centre of a bounding rectangle.
- Compute 2D Euclidean distance after independently scaling X and Y. Pixel
  units use `(1, 1)`; micrometres require both finite positive calibration axes.
  Unknown calibration must fail before worker execution, never assume isotropy.
- Select source and target IDs explicitly. Self inclusion/exclusion is an
  explicit identity policy. Excluding self must retain a distinct coincident
  object. Scalar minima may tie; native nearest-target reporting must choose a
  deterministic ID and document that upstream captures prove only the scalar.
- An empty eligible target cohort produces an absent distance with a recorded
  status. It must not become a zero, NaN CSV value or omitted source row.
- Preserve captured source/target IDs and revisions, centroid selection,
  calibration, units, algorithm version and result identity. Editing or replacing
  the live document cannot silently make a historical result current.
- The present document represents a single analysis plane. The upstream oracle
  contains Z/time refusal controls; they do not imply native multiplane support.
  Future plane selection must filter targets before nearest-neighbour search.

## Independent reference and precision

The [contract](../tests/fixtures/qupath-spatial-0.7.0/contract.json) fixes analytic
expectations and geometry tolerances at `1e-9` absolute / `1e-12` relative.
The [capture tool](../tools/capture_qupath_spatial.py) authenticates the existing
QuPath 0.7.0 distribution and all 496 runtime files before executing an independent
Groovy script. It never imports qupathxx. The committed reference covers 26
cases, including anisotropy, source/target nuclei, identity exclusion, ties,
missing targets, planes, nonrectangular centroids and over 1,000 targets.

The first numeric capture failed the fixed tolerance. Default float measurement
storage rounds the scalar. An explicit `DOUBLE` list also rounds in this pinned
release: its setter casts through float. `GENERAL` storage retains the expected
geometry precision. Capture geometry with `GENERAL`; separately require exact
float32 results and runtime storage classes in four named storage cases. The
native table should retain double precision and report export conversions.
Do not loosen the geometry tolerance to reproduce an upstream storage loss.
See pinned [DistanceTools](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/analysis/DistanceTools.java)
and [measurement setters](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/measurements/NumericMeasurementList.java).

Failed setup and numeric captures remain under `build/spatial-oracle-261/`;
the [preparation receipt](benchmarks/spatial-oracle-261/receipt.json) records
their identities. Synthetic reference agreement is not biological validation.

## Easiest implementation path and exits

| Step | Existing owner and smallest extension | Decisive exit |
| --- | --- | --- |
| Geometry | Extend immutable `Roi` in `geometry.hpp`; use analytic rectangle/ellipse centres, polygon area moments, length-weighted polylines and point means | Native analytic fixtures, holes/multipart and degenerate/overflow cases. A triangle and uneven polyline must reject a bounding-box approximation. Preserve optional-GEOS capability behavior |
| Capture | Follow `ObjectFeatureRequest`'s owner-thread immutable capture and source/revision matching pattern; retain shared ROIs and explicit budgets | Mutation, stale delivery, missing IDs, invalid calibration and snapshot-budget refusal before work. Worker never consults a live `Document` or reader |
| Query | Extend the existing `SpatialIndex` traversal for nearest-point queries over captured centroids; bound pruning distances using the same per-axis metric | Exhaustive independent all-pairs oracle on small fixed scenes, deterministic ties, coincident objects, cancellation and large-scene work/memory evidence. No dense N-by-M matrix |
| Result and persistence | Reuse native immutable result, retained-record, archive and incremental table-export conventions | One row per source, nullable distance/status, stable units/schema/identity; save/reopen, historical inspection, currentness checks and bounded atomic export. No ad hoc frontend CSV |
| Python and CLI | Thin nanobind request/result/job surface and the existing batch-command route after native behavior stabilizes | Installed origin, GIL/lifetime/typing, direct/job parity, interruption and immutable output checks |
| ImGui and managed viewer | Existing owner jobs, selection controls, poll/cancel/take and table/export actions | Actual select/run/cancel/retry/export/save/reopen interactions; edits refuse stale publication, shutdown joins before document/reader release |
| Integration | Package the completed workflow once, then freeze its own source for required platform roles | Exact native/installed/front-end/persistence cohorts, source identity and full applicable audits; candidate262 acceptance cannot transfer to changed spatial code |

Keep `query_region` as the documented inclusive bounds-candidate query and
`pick` as exact point-to-geometry refinement. Neither API is a centroid-distance
measurement. Signed point-to-area distance needs its own calibrated geometry
contract; density needs a defined area/edge normalization; neighbourhood and
tiled aggregation need separate metric, quota and persistence controls. Those
requirements remain in the full project backlog.
