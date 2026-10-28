# Spatial analysis delivery contract

SPAT1 is the current SCI1-B spatial workflow: measure each selected object's
centroid distance to a selected target cohort, inspect the immutable table,
export it and save/reopen it. The [native geometry/query foundation](benchmarks/spatial-native-263/receipt.json)
is locally verified at 3af9572. [Capture264](benchmarks/spatial-capture-264/receipt.json)
adds owner-captured IDs/revisions/centroids/geometry hashes, immutable results,
budgeted nearest queries and owner jobs at2b9c5d7. Four affected native suites pass
each Windows GEOS-ON/OFF profile. Workers retain no live Document or reader and
perform zero pixel IO. Geometry freshness binds document identity/revision; a
closed image source alone does not invalidate a geometry-only request.

[Persistence265](benchmarks/spatial-records-265/receipt.json) adds immutable
retained records, QSD1, schema14 project save/reopen and atomic CSV at1738022.
Ten affected native suites pass each Windows GEOS-ON/OFF profile, including
all13 legacy upgrades, failed-migration rollback,10000-record admission, stale/
historical and export fault controls. Final902 tooling controls pass;400 compiler
inputs and503 raw sources authenticate. [Python266](benchmarks/spatial-python-266/receipt.json) now installs the thin
Python API at9fa565a:53 native suites,404 installed passes/eight skips,902 observer
controls and exact typing diagnostics pass. [CLI267](benchmarks/spatial-cli-267/receipt.json) now verifies native measure/save,
historical export and fresh verification in both Windows GEOS profiles. ImGui/
managed controls, complete packaging and own-source qualification remain. Frozen candidate262 retains its separate complete engineering scope.
Native oracle coverage stays18 geometry cases ON/16 OFF plus four separate
upstream storage cases, with four nonzero-plane cases unsupported.
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

## Native retained tables and export

`Document.keep_spatial(request, result, name)` requires an exact current capture
and advances the canonical document revision only after admission. Immutable
`SpatialDistanceRecord` values retain result, name, producer and captured next-ID
bounds. Storage is limited to10000 records/256 MiB conservatively accounted
payload per document. Removal never destroys external historical handles.

`spatial_status(id)` reports `unverified`, `current`,
`objects_or_calibration_changed` or `verification_failed`. Copies and reopened
projects begin unverified. `SpatialDistanceRequest::replay(document,id)` captures
the saved explicit cohorts and options anew, refusing changed geometry/revisions
or calibration; `verify_spatial` compares its immutable native result without
changing the saved record or canonical revision. Unrelated new objects do not
change an explicit cohort. No pixel read or live reader is needed.

Project schema14 adds `spatial_distance_records(image_id,id,payload)`. Its QSD1
payload is little-endian with bounded counts, captured object snapshots/cohort
indices, source/build provenance, float64 results and recomputed input/result/
archive checksums. Derived row metadata cannot diverge from its snapshot. Structural
distance validation allows a few ULPs of foreign-libm rounding and preserves the
archived value/hash; independent scientific tolerances above remain unchanged.
Old schemas1–13 open without writes and migrate transactionally on save. Historical
records do not assert that their old source or analysis is currently verified.

`spatial_distance_csv(result, byte_limit, cancelled)` returns a bounded string;
`save_spatial_distance_csv(result,path,byte_limit,replace,cancelled)` streams one
row at a time through the existing atomic exporter. Replacement defaults to false.
The22 columns include source/nearest-target IDs/revisions/kinds/nucleus choices,
centroids, nullable float64 distance, `px`/`um`, `matched`/`no_target`, algorithm,
input/result hashes, X/Y scales and nucleus/self policies. Object kinds0/1/2 mean
annotation/detection/cell. Absent target/distance columns are empty; zero is a
matched numeric distance. The default cap is64 MiB; allowed caps are1 byte..1 GiB.
Cancellation or failed admission/publication leaves an existing destination intact.
These native C++ APIs now have the installed Python surface below. Application
ImGui/managed controls now have local Windows268 verification below; native CLI267 is documented separately.

## Installed Python surface

The public package exports `SpatialDistanceUnits`, `SpatialDistanceOptions`,
`SpatialObjectSnapshot`, `SpatialDistanceRequest`, `SpatialDistanceRow`,
`SpatialDistanceResult`, `SpatialDistanceRecord`, `SpatialDistanceStats`,
`SpatialDistanceJob`, `measure_spatial_distances`, `spatial_distance_csv` and
`save_spatial_distance_csv`. Document exposes spatial_records/spatial_record_count,
spatial_record/spatial_status, keep_spatial, verify_spatial and remove_spatial.
AnalysisProvenance.source_only explicitly
identifies whole-source capture without fabricating an object ID.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
a = doc.add_rectangle(q.Rect(20, 20, 10, 10))
b = doc.add_rectangle(q.Rect(50, 60, 10, 10))
request = q.SpatialDistanceRequest(doc, [a], [b])  # Pixel units by default.
job = q.SpatialDistanceJob()
job.request(request)
stats = job.wait(doc)
result = job.result
if stats.state != "ready" or result is None:
    raise RuntimeError(stats.error or stats.state)
row = result.row(0)
record_id = doc.keep_spatial(request, result)
q.save_spatial_distance_csv(result, "distances.csv")  # Refuses an existing file.
project = q.Project()
image = project.add_document(doc)
project.save("study.qpxproj")
```

Use `result.row_count`, `row(index)` and `rows(start=0,limit=256)` for inspection;
pages are limited to4096 copied rows. No whole-table copy is implicit. Snapshot
centroids, row Points and options are detached values, so Python mutation cannot
edit captured native inputs. Target ID/revision/centroid/kind and distance are
nullable; do not replace absence with zero. Result/snapshot/record values are
native-only constructions. Strict stubs preserve nullable job.result and rows.

Request capture and owner job/Document operations hold the GIL. The job's Python
wait releases it only while waiting for native readiness; both document checks
hold it. Direct kernels and CSV conversion/export release it over immutable data.
Cancel is thread-safe; owner operations from another thread refuse. Worker teardown
owns no Python Document/reader, and shutdown joins before releasing native state.
Keeping a record advances the document revision, invalidating the earlier request;
the retained record's status/replay uses its saved geometry/cohort/calibration.

Installed evidence covers Windows CPython3.12 only with shared test dependencies;
OpenSlide and large-fixture skips are explicit. The native binary stays at9fa565a,
generated stubs at2a16bd2. All401 compiler inputs stay identical; the final wheel
updates only the two stub payloads/RECORD, with every other payload unchanged.
This Python packet alone does not imply CLI/UI delivery, multiplane support, general performance,
biological validation or supported release readiness.

## Native command line

The native `qupathxx_batch` supports these commands. CLI267 was locally tested
after relocation with Windows GEOS ON/OFF. Frontend268 later verifies the CLI
in its installed Windows package; neither source inherits platform qualification.

```text
qupathxx_batch --measure-spatial study.qpxproj measured.qpxproj --image 1 --source 5 --target 9 --units um
qupathxx_batch --export-spatial measured.qpxproj distances.csv --image 1 --record RECORD_ID
qupathxx_batch --verify-spatial measured.qpxproj --image 1 --record RECORD_ID
```

Measure returns the new record_id in one JSONL `qpx.spatial-cli/1` report. It keeps
one immutable native result in a new project, preserving the input project, other
images, active image, objects and calibration. Units default to px; um requires
both calibration axes. Prefer nuclei by default; --body uses bodies and
--include-self permits the same object identity. Omitting targets creates absent
distances. --name supplies bounded UTF8 record text. No linked-image pixel decoding
is needed; project loading may read embedded image payloads.

For large cohorts use --source-ids FILE and/or --target-ids FILE. Each cohort uses
repeated --source/--target IDs or one file, never both. Files contain positive
unsigned decimal IDs separated by space/tab/CR/LF; UTF8 without BOM works because
tokens are ASCII digits. Default/max --ids-bytes is4 MiB per file and the count cap
is100000 IDs. Duplicate, zero, negative, overflow and malformed tokens refuse;
source files cannot be empty, target files can. A verified bounded file snapshot
is parsed once, with version guards retained through publication. This bypasses
Windows argv limits without inventing a second cohort model.

Analysis options reuse --snapshot-bytes, --work-bytes and --max-distance-tests.
Parsing buffers are separately bounded by --ids-bytes; these are payload/input
limits, not OS RSS claims. --save-snapshot-bytes and --project-bytes bound new-
project publication. --csv-bytes bounds export(default64 MiB, max1 GiB).

Export reads a retained historical result and streams its existing22-column CSV;
it does not silently reverify changed/deleted objects. Verify explicitly replays
the captured cohorts and compares a fresh result, modifies no project bytes and
persists no verified authority. Saved/reopened records still begin unverified.
JSON includes command/state, image/record IDs, row/cohort counts, units, algorithm,
input/result hashes and historical status; output paths/CSV bytes appear where
applicable. Exit0 is success,1 runtime/refusal,2 arguments and130 cancellation.

Outputs always refuse replacement. The input project, ID files, linked/captured
image files, path aliases and project journal/WAL/SHM sidecars are protected.
Project opens use the existing source-preserving path without hot-journal repair.
File versions are rechecked before publishing. CSV's native publication callback
runs after streaming and before its atomic commit, sharing the same final checks.
Project and CSV outputs are separate commands, so a partially published pair is
never reported as one successful operation. Runtime errors and argument errors
produce the existing structured stderr record. Input files and failed outputs
remain untouched under the tested guards and quotas.

The Python266 runtime used by the process harness is only a schema14 fixture
creator/QSD1 reader at9fa565a. Both tested CLI executables run native9468ae3 and
report it in saved provenance. Observer65f2c5d changes only fixture code; all404
compiler inputs and both binaries remain unchanged. ImGui/managed delivery and
full workflow qualification follow; no new wheel or SDK build was needed here.


## Desktop and managed viewer (local Windows268)

The toolbar's Spatial button opens the distance table. Select objects and use
Use selection as sources/targets, choose units/nucleus/self policy, then Run.
An empty target cohort produces absent distances; zero remains a matched distance.
Cancel cancels live work. Keep result stores the immutable table in the project.
Inspect reads retained history; Verify performs fresh replay without rewriting
historical values. Saved/reopened history begins unverified. Export CSV streams
the current table to a new file and protects project/source/sidecar paths.
Starting another analysis/inspection/verification resets the previous export
report; retain the CSV path or export after verifying for a final saved report.

For scripted desktop setup, assign a GuiSpatialOptions value to GuiOptions.spatial
(open/run, source/target IDs, native analysis options, record_id, CSV path/byte cap).
Assign None to clear it. The configuration is copied; read-modify-reassign nested
options rather than relying on a live mutable configuration pointer.
GuiReport.spatial contains the immutable result, counts, historical status and
export outcome. Rows in the desktop are virtualized; Python uses bounded pages.

Managed producers submit to the existing ViewerSession owner queue:

```python
measure = session.spatial_distances(image_id, sources, targets, keep=True,
                                    expected_revision=revision)
# Wait/take from a producer thread while show(session) runs on its UI owner.
kept = session.wait(measure, 15)
if kept.state == "succeeded":
    kept = session.take(measure)
    inspect = session.inspect_spatial(image_id, kept.record_id)
    verify = session.verify_spatial(image_id, kept.record_id)
    export = session.export_spatial(image_id, kept.record_id, "distances.csv")
```

Consume every terminal ticket with take to release its retained budget. Use
session.cancel(ticket) from producers; workers never receive a live Document.
Measurement result admission precedes Keep, so a refused viewer payload leaves
canonical history unchanged. expected_revision guards capture; identity/revision
checks guard delivery. Retained immutable results outlive the viewer. A committed
CSV is still reported committed if cancellation/replacement arrives afterward;
replacement makes it detached rather than granting it current-document authority.

[Receipt268](benchmarks/spatial-frontends-268/receipt.json) binds native d9bba6b,
stub observer c360ac9,55 native suites and132 installed passes on Windows GUI/GEOS.
Initial failures, detached-export red/green regression and both inspected captures
remain preserved. Headless/GEOS-OFF, expanded desktop faults and full own-source
qualification remain. No biological, general speed or supported-release claim.
