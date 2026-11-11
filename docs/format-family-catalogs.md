# Native matrix and annotation format catalogs

Matrix and annotation catalogs centralize discovery and typed dispatch over the
existing C++ services. They keep separate result types and workflow boundaries.
An expression matrix is an `ExpressionMatrixSource`; annotation import acts on a
native document or a separate project preview. Neither becomes an `ImageServer`.

This page records the delivered contract. The
[source-scoped receipt](benchmarks/format-family-catalogs/README.md) records
passing native, CLI and GUI checks; [current task](current-task.md) owns the
current outcome. The separately qualified image-plugin
[phase 4 products](benchmarks/reader-plugin-project/README.md), with native source
`a10f7146da05180ec8e5daa7fcc972f47f5fd8caca7b55fb65f6d0d743399b88`,
remain frozen. This work selects no package, wheel, schema change, family plugin
ABI, additional format or qualification campaign.

## Providers and native authorities

| Matrix provider ID | Operation | Existing authority | Availability |
| --- | --- | --- | --- |
| `qmx` | Open an indexed matrix sidecar | [open_expression_matrix](../cpp/include/qupathxx/expression_matrix.hpp) | Native expression matrix storage |
| `matrix-market` | Index Matrix Market plus explicit Qpx or Tenx TSV catalogs | [import_expression_matrix](../cpp/include/qupathxx/expression_matrix.hpp) | Native expression matrix storage |
| `h5ad` | Index the admitted AnnData/H5AD profile with explicit interpretation | [import_h5ad_matrix](../cpp/include/qupathxx/h5ad.hpp) | Native expression matrix storage and H5AD support |

The matrix request carries the existing native options for its operation. Opening
QMX does not index original inputs again. Matrix Market keeps its explicit
`ExpressionCatalogueFormat::Qpx` or `Tenx` choice. H5AD keeps `H5adOptions`, including
the selected matrix/spatial keys, value domain and coordinate frame. A suffix
never supplies those scientific choices.

| Annotation provider ID | Operation and transport | Existing authority | Availability |
| --- | --- | --- | --- |
| `geojson` | Import/export through text or files; copy/paste through clipboard transport | [GeoJSON and clipboard services](../cpp/include/qupathxx/interchange.hpp) | Native interchange support |
| `measurements-csv` | Export selected native object measurements to text or a file | [measurements_csv and native writers](../cpp/include/qupathxx/interchange.hpp) | Always available |

Measurement CSV is export-only. GeoJSON options retain pixel-coordinate
assumptions, explicit loss acceptance, hierarchy handling, ellipse precision and
selection semantics. A borrowed QuPath class context is used synchronously; the
catalog does not retain pointers into the caller's map. Text import borrows its
input string, and export borrows object selections and protected paths for the
synchronous call. Dispatch does not copy a bounded GeoJSON payload merely to
select the native operation. Clipboard import owns its transferred text.

[QuPath project import](../cpp/include/qupathxx/qupath_project.hpp) remains a
separate native preview/new-project workflow. It can use the annotation dispatch
authority for its existing GeoJSON conversion without turning a project bundle
into a current-document annotation operation.

## Discovery and dispatch

The shared `--formats` report exposes the separate families, provider IDs,
operations and actual build availability. A known provider remains listed when
its backend is disabled, with an owned reason. Availability describes service
readiness; it does not assert that a particular file, geometry or request is valid.
The existing image-reader listing and explicitly configured image-module
bootstrap keep their own behavior.

The family tables and request variants remain private C++ implementation details.
Existing public factories, SDK contracts and optional Python controls continue to
call the native authorities. No family DLL loader or extension ABI is introduced.

Dispatch uses an explicit provider and operation. An unsupported operation or
unknown ID refuses. Once a provider is selected, its existing error propagates;
dispatch does not retry another parser. Catalog labels and capability summaries
do not replace native request validation or grant additional scientific access.
GeoJSON availability does not require GEOS globally: an operation that needs a
geometry backend retains its existing operational refusal.

## Scientific and persistence invariants

Matrix dispatch preserves these existing contracts:

- Counts remain exact `uint64`, including values above `2^53` and `UINT64_MAX`.
  Continuous values retain their declared domain. Catalogs do not rescale,
  normalize, infer counts or convert indexed values into object measurements.
- Feature IDs, display names, observation IDs and source-axis order remain
  distinct. Requested feature order is retained. Explicit observation cohorts
  stay sorted/unique; an empty cohort differs from an omitted cohort. Exact ID
  joins do not become positional joins.
- Sparse absent entries remain measured zeros. Explicit stored zeros remain
  valid. Duplicate coordinates refuse instead of being silently summed.
- Physical file hashes, logical source hashes, chunk/block identities, input
  profiles and H5AD dataset keys stay unchanged. Catalog metadata enters none of
  those hashes. Reopening an indexed sidecar does not require its original text
  or H5AD inputs.
- Coordinates retain their declared frame and units. Explicit calibrated unit
  conversion precedes alignment; no inferred registration is introduced.
- Work/file/store bounds, cancellation, currentness checks and atomic new-only
  publication stay in the existing services. Preview does not publish a project
  or an owned matrix copy.

Annotation dispatch preserves these existing contracts:

- Geometry, nuclei, parent links, object planes, names and exact source-to-native
  identity reports keep their existing meaning. Imported native IDs remain the
  IDs generated by the native append/import authority.
- Class ancestry, raw ARGB values and absent colors remain distinct. Object
  properties retain their existing special-number and negative-zero encodings.
- Loss reports, counts and explicit acceptance remain authoritative. The existing
  optional QuPath ellipse precision conversion keeps its declared numeric
  contract; catalog routing adds no rounding rule.
- Failed parsing, class collisions, rejected losses and unavailable operations
  leave the target unchanged. Successful document import retains its native undo
  transaction. Clipboard transfer uses image-pixel geometry without rescaling the
  target's calibration.
- QuPath project preview remains owned/read-only, source-guarded and consumed
  only by successful new-project publication. Existing outputs cannot be replaced
  through the catalog.

Legacy QMX/project encodings, source associations and annotation payloads remain
owned by their existing codecs. No catalog ID is added to persisted scientific
identity merely because dispatch is now centralized.

## Focused checks

| Boundary | Decisive check |
| --- | --- |
| Catalogs | `matrix_format_registry`, `annotation_format_registry` and `format_family_catalog`: exact IDs/operations, known versus available, disabled-feature combinations, unknown/incompatible request refusal and shared discovery |
| CLI discovery | `format_catalog_routes_cli`: Region and Batch share the native `--formats` report; an available App is checked by the same driver without making GUI a native-only CI requirement |
| Indexed matrices | `expression_matrix`: the existing frozen tiny QMX byte hash; exact large counts, sparse/cohort/order behavior, chunk checks, cancellation and no-overwrite |
| H5AD | `h5ad_matrix` when enabled: dense/CSR/CSC, explicit counts/continuous domains, dataset keys, coordinates and shared decoder/writer budgets |
| Matrix consumers | `expression_matrix_project`, `expression_matrix_spatial`, `backed_spatial_import`, `spatial_counts` and `spatial_counts_cli`: source association, exact ID joins, owned/linked save/reopen, preview and publication |
| Annotations | `interchange` and `qupath_project`: geometry/class/plane/property and loss behavior, atomic mutation/undo, source identities, preview/source guards and new-only publication |
| Existing desktop route | `gui_spatial_import`: native catalog/index/H5AD hooks, Preview/Publish controls, exact values, owned Save As and visible stale-source errors; `gui_gene` retains overlay registration/data checks |
| Annotation desktop routes | `qupathxx_gui_organization_tests WORK --export-sampling` checks native GeoJSON bytes, import, confirmation and protected outputs; `WORK --format-clipboard` checks actual Ctrl+C/V/Shift+V, exact object graph/pixels/classes/properties/calibration, fresh IDs, history and atomic class-collision refusal. These replays require `QUPATHXX_TEST_GUI=1`; the displayed-plane case also needs TIFF. Clipboard restoration uses GLFW's text transport only. |

The native-only CI job includes the three new catalog targets without another
engine configuration. Its normal H5AD-disabled profile checks that H5AD remains
known but unavailable. The existing integration cache has H5AD enabled, so its
affected native H5AD checks cover the enabled route.

Use direct-factory versus catalog comparisons on the same admitted inputs.
Retain exact existing scientific digests and stored payloads; compare owned
results and reports rather than timing fields. Reuse relevant source-scoped
receipts and affected native tests. Historical campaigns and screenshots do not
require replay for a dispatch change, and cosmetic equivalence is not a result
acceptance criterion.
