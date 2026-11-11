# Matrix and annotation format catalogs

The selected private native catalog milestone is complete. The
[family guide](../../format-family-catalogs.md) describes the typed contracts.
Existing factories remain the scientific and publication authorities; their
public signatures, stored schemas and exact source/result/export identities stay
unchanged. This work adds no formats or family-specific binary plugin ABI.

## Implemented scope

| Family | Providers | Existing consumers |
| --- | --- | --- |
| Matrix | `qmx` open; `matrix-market` and `h5ad` indexed import | Native spatial import jobs and backed-matrix opening; association requests keep their existing authority |
| Annotation | `geojson` text/file/clipboard and QuPath class context; `measurements-csv` export-only | GUI file import/export and clipboard; native QuPath project class-context import |

One fixed table per family owns membership and capabilities. Availability is
independent of provider recognition and never probes input files. Explicit typed
requests preserve H5AD interpretation, Qpx/Tenx profiles and annotation transport
policies. Synchronous text, selections, class maps and protected-path sets are
borrowed without extra large-input copies; all results remain owned. Dispatch
delegates native error ordering, limits, cancellation and atomic publication.

Region, Batch and App expose the same `--formats` JSON report with schema
`qpx.format-catalog/1`. It does not load or freeze the unrelated image-plugin
catalog. Image `--readers` behavior remains separate.

## Source-scoped validation

Evidence is in `build/format-family-catalogs` in the owner workspace. The final
853-input frozen source is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The cached Windows x64 MSVC Release build uses native storage/interchange/H5AD/
GEOS/GUI backends with Python OFF. All38 retained binding object hashes are exact.

Fifteen distinct focused CTests pass across the recorded runs:

- `matrix_format_registry`, `annotation_format_registry`,
  `format_family_catalog`, `format_catalog_routes_cli`;
- `expression_matrix`, `h5ad_matrix`, `expression_matrix_project`,
  `expression_matrix_spatial`, `backed_spatial_import`;
- `spatial_counts`, `spatial_counts_cli`, `interchange`, `qupath_project`,
  `application_entry`, `gui_spatial_import`.

The matrix registry test passes at initial source
`1f2c88b6ccd22e066f10d96f60eb180b973169c5de556f7a49acc0fcca92fecc`.
It executes storage assertions and seven H5AD oracle cases. Remaining14 CTests
pass at final source. The only intervening source change corrects an annotation
test fixture to use the existing rectangle constructor plus classification.
No product behavior is changed to satisfy that test; initial passing objects are
retained and core source metadata is refreshed before the remaining build.

Decisive checks include exact frozen QMX bytes/hash
`174b4ea68d6022fbab3a488b0a070d0a550853e42e410880707e6f5d8b0b6018`,
uint64 counts above2^53 and at the type limit, sparse/cohort/order/chunk identity,
explicit H5AD domains/layers/frames and coordinates, source currentness,
failure/cancellation/budget/new-only refusal, and exact direct-versus-catalog
GeoJSON/CSV/reports and object/class/plane/property identity. Injected availability
combinations cover known disabled backends. The native-only CI adds these targets
and the CLI driver to its existing job; no separate OFF/platform campaign ran.

Seventeen existing static UI contracts pass unchanged. Two focused native GUI
replays also pass through the actual handlers:

- Export/import replay checks exact native GeoJSON bytes, seeded file import,
  create/replace/cancel/Escape behavior, protected project/recovery destinations
  and unchanged project bytes. Unchanged table routes retain their native owners.
- Clipboard replay checks real shortcut counters, exact hierarchy/geometry/
  nucleus/class ARGB/properties, new native IDs, compound undo/redo, a differently
  calibrated destination, atomic class-conflict refusal and displayed-plane paste
  from a channel-specific TIFF object. Source project and TIFF bytes stay exact.
  The test restores the previous GLFW-supported textual clipboard state.

No screenshot/pixel/layout equivalence is claimed; captures support these
meaningful geometry, control, result and source-data assertions.

## Cost and handoff

The first cached build passes core, CLI and matrix test compilation, then finds
an unsupported fourth rectangle-constructor argument in the new annotation
fixture. Correcting the fixture preserves the public API. One cached continuation
reuses passing objects and completes affected native/GUI targets. The earlier
task-card patch also used incorrect context and was corrected before product work.
Logs, both source snapshots and all first failures remain preserved. No tests or
scientific assertions were weakened, and no binding/install/package step ran.

`validation.json`, `source-identities.json`, `format-catalog.json`, native CTest
logs, `gui-replays.json` and replay stdout/stderr record the final scope. Runtime:
`build/format-family-catalogs/runtime/Release`. The separately admitted
[a10 image-plugin products](../reader-plugin-project/README.md) and candidate8
assets remain frozen; this source does not inherit their qualification.

Stop this selected milestone. Additional formats, external matrix/annotation
modules, a public family SDK/ABI and publication require their own workflow.
