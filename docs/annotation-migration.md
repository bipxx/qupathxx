# Annotation migration and hierarchy

The pinned QuPath 0.7.0 [hierarchy corpus](../tests/fixtures/qupath-hierarchy-0.7.0/manifest.json)
is produced by an actually executed [Groovy generator](../tools/upstream/qupath_hierarchy_reference.groovy).
It contains six synthetic objects, two roots, four parent links and three object
levels, including a cell with an independent nucleus, a detection, points,
Unicode names and a flat class/color. No real patient images or QuPath runtime
files are included.

The current migration route uses existing GeoJSON and native project storage:

1. Export the intended objects from QuPath. Ordinary `exportObjectsAsGeoJSON`
   flattens hierarchy. To retain parents, the generator uses QuPath's public
   hierarchy serializer and wraps the serialized root's `childObjects` as a
   GeoJSON FeatureCollection. This is an explicit Groovy export recipe; it is
   not a claim that QuPath's ordinary export includes those links.
2. In **Import / export**, choose the file, enable **Assume image pixels** for
   untagged QuPath output, and import with F7. Keep **Allow lossy conversion**
   disabled unless the reported losses are acceptable. Image coordinates do
   not supply physical calibration; set or preserve calibration separately.
3. Review the imported objects, edit, undo/redo, then save a native `.qpxproj`.
   Native project restart preserves the tested parent links, IDs, class/color,
   body/nucleus geometry and anisotropic calibration.
4. For a return to QuPath, enable **QuPath hierarchy (nested export)** before
   F6. This writes the existing GeoJSON geometry with nested `childObjects`.
   Actual pinned QuPath imports retain all six tested objects and four parent
   links, including the cell nucleus and point coordinates. Python/C++ use the
   same exporter with `nested_hierarchy=True`. The default remains flat native
   GeoJSON; QuPath ignores its `qupathxx.parentId` extensions. The UI explains
   both choices before export. Never guess parents from containment.

| Field | QuPath nested → native → native project/reimport | Native flat GeoJSON → QuPath |
| --- | --- | --- |
| Rectangle/point body geometry and cell nucleus | Preserved for the committed corpus | Preserved for the committed corpus |
| Names and flat class/RGB | Preserved in the data | Preserved |
| Object kind | Annotation, detection and cell preserved | Preserved |
| Parent links | Four links preserved, including three object levels | Four links lost; native extensions are ignored |
| UUID/native IDs | Source UUIDs reported; new native IDs allocated and retained by native project | Native numeric IDs replaced by QuPath UUIDs |
| Object color, lock, measurements | Reject by default; explicit opt-in drops three properties and reports `properties_dropped` count 3 | These fields are absent from the native export |
| Metadata | Pinned QuPath raw export repeats the `metadata` member; native import rejects the duplicate even with loss opt-in | Native object metadata is not represented |
| Calibration | Set/preserved in native project, not inferred from pixel-coordinate GeoJSON | Not conveyed by this object export |
| Unused class definitions | Native complete collections retain them; this upstream corpus does not exercise unused definitions | Not qualified by this corpus |
| Holes, multipart and ellipse precision | Covered separately by the existing [shape comparison corpus](qupath-comparisons.md) | Do not infer additional hierarchy coverage from those shape tests |

The raw `metadata-raw.geojson` is retained exactly. QuPath's pinned serializer
writes identical metadata twice in this path; rewriting the file through a JSON
dictionary would hide the invalid duplicate. Strict rejection preserves the
native duplicate-member contract. General metadata preservation needs a separate
object-model contract.

## Review and import a QuPath project

QuPath0.7.0 `.qpproj` manifests can be reviewed and imported directly. Saved
annotations in pinned version3 `.qpdata` files are decoded natively when no exchange
is supplied. Assisted exchange remains available for its explicit compatibility route:

1. With QuPath0.7.0, run `QuPath script --args /path/project.qpproj`
   followed by the path to [export_qupath_project.groovy](../tools/upstream/export_qupath_project.groovy).
   The exporter writes a new sibling `qupathxx-exchange` directory and refuses an
   existing one. It reads saved image data directly: corrupt input fails rather
   than falling back to an empty hierarchy. It does not rewrite the source project
   or create server caches. Only eligible simple URI builders are accepted.
2. In the native UI, put the `.qpproj` path in the project-open field and press
   Ctrl+O to review. Inspect available images, object counts and explicit issues.
   Choose a new `.qpxproj` destination. A second Ctrl+O imports the reviewed
   candidate; the usual dirty-workspace replacement confirmation still applies.
   Refresh the review after changing source inputs. Without an exchange, native
   saved-data decoding applies the contract below; unsupported data fails explicitly.
3. Keep loss admission disabled unless the listed omissions are acceptable.
   Unsupported or missing images can be omitted only with explicit loss admission;
   malformed/stale exchange data remains fatal. No usable images is always fatal.
   Import never overwrites an existing destination, and a failed attempt leaves
   the current workspace and destination intact.

Python uses the same native authority:

```python
import qupathxx as q

preview = q.preview_qupath_project("/path/project.qpproj")
for image in preview.images:
    print(image.source_id, image.source_path, image.available, image.objects)
for issue in preview.issues:
    print(issue.code, issue.message)
# Call after reviewing issues; default refuses any reported loss.
project = preview.import_new("/path/imported.qpxproj")
```

`source_overrides={"1": "/new/source.ome.tif"}` supplies an explicit source path
by project entry ID. Relative relocation from the saved project URI is attempted
when an original image path is missing. `exchange_directory` selects a different
exchange directory. Paths are local files; arbitrary remote URIs and transformed
builders are unsupported. Simple URI arguments are empty or a TIFF `--series N`.
Native adapters determine actual supported formats; recognizing a QuPath provider
name does not load its Java implementation or add its formats.

The preview holds a private native candidate. Properties return snapshots;
modifying the Python lists cannot alter it. Each image reports source entry ID,
new native image ID, source path/reader and parallel source-object/native-object
ID lists. The new native identities persist through save/reopen. A preview is
consumed only by successful publication and cannot be imported twice. Input
versions and expected absences are checked again before publication; changed
source images, metadata, .qpdata or exchange require a fresh review.

The exchange binds project, optional classes/metadata, per-image .qpdata and
GeoJSON hashes. Nested objects use existing strict loss-aware import semantics.
Unused project class definitions and tested XY calibration are retained; explicit
unknown XY remains unknown. Image settings, metadata and unsupported object
properties produce issues. Saved Z/time calibration overrides are not applied;
source calibration remains authoritative there. Unknown builders, further planes,
full project properties and additional saved-data versions/records remain R5 gaps.

Admission limits are16 MiB manifest,1024 images,4 MiB index,1 MiB each optional
class/metadata document,256 MiB per GeoJSON/native qpdata input and512 MiB per
qpdata fingerprint in the assisted route;
existing object/depth limits also apply. The [scoped evidence](benchmarks/qupath-project-r5/README.md)
proves two synthetic image entries and nested annotations, rather than complete
QuPath project migration or arbitrary vendor-builder compatibility.

## Native saved-data decoding

The existing preview/import API and ImGui review/apply flow also decode pinned
QuPath0.7.0 data-file version3 `.qpdata` directly when no exchange entry exists.
A supplied exchange remains hash-bound and must be current; it is not silently
ignored on failure. No Java runtime is needed for native import.

Supported hierarchy records retain source UUID-to-native-ID mappings, names,
flat classes/colors, parent links and annotations/detections/cells with an optional
nucleus. Supported ROI proxies are rectangle, ellipse, polygon, polyline, points
and GeometryROI WKB for 2D Point/MultiPoint, LineString, Polygon and MultiPolygon,
including polygon holes. MultiLineString/geometry collections are not represented. Analytic rectangle/ellipse parameters remain analytic rather
than inheriting polygonized export precision. Supported object planes retain
c/z/t and must be compatible with the source; unavailable planes and unsupported
ROI/stream representations fail explicitly. See [object-plane evidence](benchmarks/object-planes-r5/README.md).
A malformed/truncated stream or missing terminal EOF never publishes
an image-only project, even with loss admission.

XY calibration comes from saved server metadata; unknown calibration remains
unknown. Source dimensions, axis/channel counts and sample type must match.
Supported object metadata, measurements, lock/color and reported class probability
are retained through [object properties](object-properties.md). Root historical
facts use [image properties](image-properties.md); recognized image types and stain
profiles use [saved defaults](image-analysis-settings.md). Supported custom values
use [typed ImageData properties](image-data-properties.md); unsupported whole
properties and executable workflow history report losses before apply.
Derived classifications retain their full display name/color
with a class-hierarchy flattening notice. Saved workflows are never executed.
Recognized native [saved TMA grids](qupath-saved-tma.md) retain core identities,
slots, missing/case state and children through the existing TMA model. Invalid or
unsupported grids and top-level records fail rather than becoming an empty
hierarchy. Assisted exchange remains a distinct state-retention contract.

The stream reader limits input to256 MiB, one million serialized values,
128 nesting levels, eight million array elements,16 MiB per serialized string and
256 MiB of accounted stream-node storage; this is not a bound on total process
memory. Native hierarchy depth remains32 and existing geometry/import limits
apply. Java modified UTF-8 surrogate pairs become validated UTF-8. Arbitrary Java
serialization compatibility, other data versions, full settings/measurements and
non-default planes remain explicit R5 work.

The [actual capture and installed checks](benchmarks/qupath-saved-data-r5/README.md)
cover the declared synthetic geometry, loss/refusal and save/reopen scope. They
establish native data import for that contract, rather than complete QuPath parity.

## Evidence and remaining work

The nested option is additive; omitting it keeps the previous flat output
byte-for-byte for the committed corpus. It retains selected objects only. A
missing selected parent requires `allow_losses=True` and reports
`parent_not_exported`; the descendant becomes a root, including when a more
remote ancestor is selected. No implicit ancestor expansion or reparenting occurs.
Nested output is bounded at one million objects, 32 object levels and 256 MiB.
The iterative writer avoids recursive subtree copies. The 32-level boundary is
native-tested; the actual upstream corpus has three levels. Conversion and depth
errors occur before file publication and preserve an existing destination.

The [current local receipt](benchmarks/annotation-migration-recovery-local-windows.json)
identifies the installed engine at compiled source
`df0733c3782f99518e89d7e2f40787dbb2be3094`, wheel/module hashes and all 268
compiler inputs. Fifty native suites and 305 installed cases pass. Three host
capability skips remain explicit: one OpenSlide fixture case and two file-symlink
cases unavailable on this Windows host. The existing hierarchy/interchange cases,
four desktop migration cases, project save/recovery and additional recovery
maintenance/retention/UI checks are included. Ten long-path cases also pass after
adding portable filename-limit assertions. Packaged stubs, four positive typing
files and all 13 expected negative errors pass. Two actual pinned QuPath processes
verify Python and GUI exports separately, including point coordinates and parents.

The earlier [partial integration receipt](benchmarks/annotation-migration-nested-local-windows.json)
retains the Windows recovery staging failure. The fix reuses the batch engine's
physical-path policy through `cpp/filesystem_access.hpp`. Physical work paths,
SQLite connections and recovery filesystem operations use extended Windows paths;
public project/base/image paths and recovery listings stay logical. Owned work
names/markers, writer leases, inventory checks, non-overwriting rescue and checked
deletion policy are unchanged. Tests cover 230/270-character checkpoint paths,
longer derived work/snapshot/rescue paths, reopening and discovery, changed-since-
review rejection, and live-session retention followed by release. Drive/UNC
conversion is native-tested; a real network share is not qualified by that test.

The old wheel fails all five original long-path reproductions. The corrected
engine passes the broader gate at the same 79-character absolute root length;
it does not hide the failure by shortening the test directory. Raw failures and
successful XML/logs are retained separately. The original
[main154 preparation receipt](benchmarks/annotation-migration-local-windows.json)
is still bound to its historical source `15f5888`.

The inspected final nested-export capture has readable mode/depth text and
`Exported 6 objects`, with body/nucleus/points visible. It is byte-identical to the
inspected initial capture. The default font displays the alpha in the parent
label as `?`; Unicode data remains exact. Unicode glyph delivery remains open.
Source-bound hierarchy/recovery qualification cohorts, broader hierarchy/property
contracts, direct `.qpproj`/`.qpdata`, full platform gates and supported-release requirements remain in the full plan.

Run the installed tests from the repository root:

```powershell
python -I -m pytest tests/test_qupath_hierarchy_reference.py tests/test_geojson_hierarchy_export.py tests/test_hierarchy_comparison.py -q
$env:QUPATHXX_TEST_GUI = '1'
python -I -m pytest tests/test_gui_hierarchy_migration.py -q
```

To regenerate with the verified Windows runtime, use `tools/fetch_qupath.py`,
then invoke the returned executable with `script --args NEW_OUTPUT` and the
generator path. A second invocation accepts `--args NATIVE_EXPORT` before the
generator path. The generator requires a new output directory. Preserve raw
logs and compare the declared contract before deliberately replacing fixtures;
tests never regenerate them implicitly. The comparison is image-object migration,
not Java serialization compatibility or a supported release claim.

The live checker verifies the complete pinned runtime and never updates fixtures:

```powershell
python -I tools/compare_qupath_hierarchy.py --output build/hierarchy-roundtrip-new
python -I tools/compare_qupath_hierarchy.py --output build/hierarchy-gui-new --native-input path/to/gui-export.geojson
```

Outputs must be new directories. The GUI-input mode records the checker engine;
its producer identity belongs in the originating capture receipt. The current
receipt binds both to the same installed module. This comparison qualifies the
committed object corpus, not arbitrary geometry, research workflows or releases.


## Assisted project exchange version2

`tools/upstream/export_qupath_project_state.groovy` is the separate exchange/2
producer for QuPath0.7.0. Its arguments are project path, optional mode
(`auto`, `native`, `geojson`) and optional new output directory. Pass script
arguments with repeated `--args` on the QuPath console. Existing output is refused.
The original exchange/1 exporter and bundle behavior are unchanged.

Each v2 image explicitly selects `native-saved-data` or `geojson`. Native entries
contain only `entry_id`, `representation` and `qpdata_sha256`; the current pinned
native reader supplies complete supported geometry and image state once. The
producer recognizes the saved-data header without opening a server or exporting
alternate geometry. A native decoder failure refuses import; it never switches
to GeoJSON. An unrelated GeoJSON file is not an import authority for that entry.

Auto mode selects GeoJSON for an unrecognized producer/header. Explicit GeoJSON
uses the existing QuPath API and reports image facts, defaults and properties it
cannot retain; a TMA grid refuses this representation. Forced native mode refuses
an unrecognized header. Header recognition is not full-stream validation.
GeoJSON entries bind saved data, exported geometry, XY calibration and explicit
losses. Metadata/project hashes, source guards and transactional publication
remain shared with the existing importer. Tests in
`tests/test_qupath_assisted_state.py` bind the six derived producer captures;
[local route acceptance](benchmarks/assisted-state-r5/README.md) records the candidate, affected tests and explicit remaining limits.


Saved physical Z/time calibration now has [scoped native/direct-v2 retention](image-axis-calibration.md),
including explicit unknown and partial timestamps. The original exchange1 and
explicit GeoJSON loss contract remains unchanged. Other project metadata and
unsupported representations still require their existing loss review.


Short project-entry metadata maps now have [scoped retention](project-entry-metadata.md)
through ProjectImage/Project and manifest/exchange1/2 imports, including actual
saved geometry routes. [Tags and description](project-entry-details.md) also have scoped retention
through their project authority. [Project-wide metadata](benchmarks/project-metadata-r5/README.md) now has its separate schema25/import/edit/snapshot/recovery workflow. Derived class ancestry and zero-image project class defaults remain distinct gaps.


## Saved PolygonROI vertex touches

Pinned QuPath0.7.0 PolygonROI uses nonzero winding and can serialize a walk with
repeated non-adjacent vertices. The native saved-data reader decomposes those
walks into simple cycles without moving, snapping or inventing coordinates.
Validated containment/winding determines exterior shells, holes and redundant
interior boundaries. Every cycle and the final polygon/multipolygon must pass
normal native topology validation; partially overlapping/crossing cycles,
backtracking spikes, adjacent duplicates and empty results remain fatal. No
GEOS MakeValid or general repair is applied. Normalization is limited to100,000
vertices,1,024 cycles and two million cycle-pair vertex work; cancellation is
checked while decomposing and comparing cycles. Ordinary valid polygons retain
the existing path and vertex limit.

Preview reports `polygon_vertex_touches_normalized` with a count. As with other
reported import losses, default publication refuses; the existing explicit
`allow_losses=True` API or native UI acceptance checkbox admits the reviewed
conversion. Source files remain unchanged and source guards/new-destination
publication remain in force. Existing stored measurements are retained exactly;
newly computed geometry metrics describe the valid filled-region representation,
not the original repeated walk's edge traversal. Unnamed source objects remain
unnamed instead of acquiring the generic GeoJSON Annotation default.

[Named-case evidence](benchmarks/all-five-gaps/README.md) compares all558 cells'
nucleus/body partitions,22,880 bit-exact stored measurements, names, hierarchy,
calibration and save/reopen. This is support for vertex-touch PolygonROI walks;
unsupported arbitrary self-crossings are still explicitly refused.
