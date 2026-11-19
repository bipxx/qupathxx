# Native matrix and annotation plugins

The FP04 host adds two optional, independent C++ import providers:
[dense counts TSV](../plugins/counts_tsv/README.md) and
[pixel point TSV](../plugins/point_tsv/README.md). Both use the
[typed C ABI](../cpp/include/qupathxx/format_plugin.h). The public
[native services](../cpp/include/qupathxx/format_import.hpp) own validation,
publication, class mapping, persistence and undo. The previously delivered image
ABI remains separate. Use a matching FP04 host/SDK; frozen earlier hosts do not
implement these new entry points.

The later selected [Loom matrix workflow](benchmarks/loom/README.md) adds a
separate matrix ABI2 for explicit UInt64 counts or Float64 continuous values,
without changing ABI1 counts layouts or stored identities. Its native/CLI/SDK
and local material/package/install/removal checks pass. Use its
source-matched typed addon and required frozen companions, not a header swap in
an older SDK.

Configure absolute `qpx.format-plugins/1` manifest paths before querying family
providers. Use `configure_format_plugins(path)` for one manifest or
`configure_format_plugin_manifests(paths)` for up to 32. In the CLI, repeat
`--format-plugins` to use independently installed packages together.

```text
qupathxx_format_import --format-plugins ABS_ANNOTATION_MANIFEST --format-plugins ABS_MATRIX_MANIFEST catalog
```

Each entry binds domain, provider/format IDs, display name, version,
contract, compiler/CRT profile, module SHA-256 and exact dependency paths/hashes.
The loader resolves paths within each package and verifies the PE/import graph,
callback ownership and descriptor agreement. Cross-manifest claims to the same
provider ID or the same format ID within a domain are all refused before loading
their DLLs. A malformed manifest or invalid module generates a diagnostic and
leaves unrelated packages available. Multiple-manifest diagnostics identify
the originating path. Modules remain pinned for the process lifetime. Close
the host before replacing or removing module files.

The initial modules have no third-party parser or codec dependency. Each module,
its independently authored format definition, helpers and test controls retain
[AGPL-3.0-only](../LICENSE). Commercial use follows those terms, including source
and notice obligations. No NC/research-only, proprietary or unknown material is
admitted by this workflow. Container support does not grant rights to its contents.

## Counts and positions

Select `org.qupathxx.counts-tsv`. The exact header starts with `observation_id`,
followed by unique feature IDs. Every data row contains its unique observation
ID and exactly one unsigned decimal UInt64 per feature. Counts, leading zeros
in IDs, UTF-8 bytes, axis order and explicit zeros remain exact. No floating
conversion, duplicate aggregation, trimming or missing-value inference occurs.

Supply a separate position TSV with exact `observation_id`, `x_px`, `y_px`
columns. The existing native position parser joins by the complete exact ID set;
position row order may differ. Supply an explicit native `CoordinateFrame` in the
SDK; the CLI derives the level-zero pixel frame from its selected image.
This records source coordinates and does not infer registration.

The DLL accepts text fields through4096 UTF-8 bytes; the existing QMX/native
catalog requires IDs/display names through256 bytes. Longer target identifiers
refuse. Feature display names use their source name or exact ID when empty;
feature type is the neutral string `feature`. Observation display names must be
empty because this QMX target has no separate observation-name field.

The native Writer publishes a complete new-only QMX store, profile
`external-dense-counts/1`, schema4. Receipts bind the complete provider/module/
dependency identity, actual normalized options, counts source SHA, position
source SHA/columns and coordinate frame. Legacy schemas1–3 retain their own
identity rules. Once indexed, queries/reopen use the verified QMX store and need
neither the original TSVs nor the provider DLL. Reimporting changed source/module
bytes creates a distinct identity even when provider ID/version are unchanged.

## Typed CSV and TSV expression matrices

Select `org.qupathxx.csv-matrix` for a wide UTF-8 CSV with explicit observation
and ordered feature columns. The optional ABI2 provider reads quoted fields,
exact UInt64 counts and finite binary64 continuous values, and uses the native
position join and owned QMX replay. [CSV reader profile](../plugins/csv_matrix/README.md)

Select `org.qupathxx.tsv-matrix` for tab-separated tables with the same
explicit column and value-domain options. TSV quote characters remain literal.
[TSV reader profile](../plugins/tsv_matrix/README.md)

## NumPy counts archives

Select `org.qupathxx.counts-npz` for a NumPy archive with named `counts`,
`observation_ids` and `feature_ids` arrays. The reader supplies exact UInt64
pages and UTF-8 IDs to the same native position join and QMX writer as the
counts TSV provider. [Reader profile](../plugins/counts_npz/README.md)

## Typed Loom matrices

Select `org.qupathxx.loom` with explicit matrix/layer, row/column ID datasets,
optional feature names, `domain` and `value_domain` options. The
[provider profile](../plugins/loom/README.md) defines Loom3 marker, dtypes,
encoding, filters and omitted objects. File rows are features and columns are
observations; the provider supplies observation-major pages. The same complete-ID
position TSV join and explicit native frame remain authoritative.

Separate matrix ABI2 negotiates exact UInt64 or finite binary64 transport and a
bounded explicit domain label. Counts use `raw-counts`; continuous labels cannot
claim that domain. Source selection and conversion are provider-owned native
interpretation, not host inference. Inexact integer-to-continuous conversion,
nonfinite values and ambiguous/duplicate option keys refuse. Catalog membership
or ABI2 alone does not establish which value kinds a particular provider admits.

On the selected x64 profile, ABI1's 56-byte table/40-byte metadata/32-byte values
buffer remain exact. ABI2 uses a separate 56-byte table, 72-byte open request with
independent file/work allowances, 64-byte metadata and 48-byte typed values buffer.
Only the selected counts or continuous pointer is supplied. No STL/allocator
ownership crosses the C boundary; host fields, lengths, kinds, currentness and
cancellation are independently checked.

The existing native Writer owns both domains. ABI2 persists
`external-dense-typed/1`, schema5, with new kind/label-bound recipe/source hashes.
ABI1 continues its exact schema4/hash path. QMX public DTOs, options, indexed
query interface and project ownership service retain their layouts. Saved typed
receipts and owned results remain inspectable without the Loom provider or
original inputs. The [Loom guide](benchmarks/loom/README.md) records native,
loader-OFF, exact legacy and fresh-process consumer evidence, plus 284 admitted
package/ZIP members and 12 separate installed/removal checks. The selected workflow
is delivered locally; the broader catalogue remains open.

## Sparse single-file matrices

Matrix ABI5 embeds the ABI2 file table and adds the same sorted stored-entry
callbacks as the directory ABI4. The host uses the guarded file receipt and
publishes CSR/CSC inputs as schema 5 `external-sparse-file/1`. The entry limit
applies to stored values, including explicit zeros. Dense input through ABI5
continues to use the typed dense profile and logical-entry limit. Both paths
reopen through the same QMX reader. [MuData usage](../plugins/mudata/README.md)

### Index against a saved project image

Use `project-create` to create a linked single-image project from the native
CLI. With an explicit reader plugin, `--reader-options` selects a view for
the new project. Subsequent commands use the saved reader and view.

```powershell
qupathxx_format_import --reader-plugins READER_MANIFEST project-create INPUT.vti NEW_PROJECT.qpxproj --reader org.qupathxx.vti --reader-options '{"spacing_unit":"mm","point_array":"Auxiliary"}'
```

The command prints the saved path and native image ID. The destination must
be new. The original image and its declared companion files stay linked.

Use `project-add-image` to append an image to a new copy of an existing
project. Its existing images keep their IDs and reader selections. Owned
matrix assets are copied by the native project writer when the new project
is saved in another directory. Existing project and source files are
protected against replacement.

```powershell
qupathxx_format_import --reader-plugins READER_MANIFEST project-add-image INPUT_PROJECT.qpxproj NEXT_IMAGE.mhd NEW_PROJECT.qpxproj --reader org.qupathxx.metaimage
```

Use `project-images INPUT_PROJECT.qpxproj` to find the saved image IDs before
indexing, relinking or exporting. The JSON report includes the active image ID,
image names, linked locations, reader IDs, dimensions, object counts and
saved TIFF or OME-Zarr views, including display channels and contrast bounds.
IDs and metadata are available when a source
has moved. `available` checks the linked path type and, for plugin images,
the configured reader implementation. Supply its manifest to check that
implementation. This command reads the project without opening source pixels.
Optional readers include a `reader_plugin` object with the saved normalized
options, provider version and exact implementation binding. These fields are
available offline. A reader with empty options reports an empty JSON object.
Use these fields to distinguish, for example, APNG frames or FITS
HDUs selected from the same file.

Use `project-set-view INPUT_PROJECT.qpxproj IMAGE_ID NEW_PROJECT.qpxproj
--z 2 --time 1` to set a saved TIFF or OME-Zarr view. Use
`--display-channels 1,0,-1` to map source channels to red, green and blue.
Plane and channel indices are zero-based. Use `-1` to disable a display
component. `--display-lower` and `--display-upper` each take three
comma-separated bounds in RGB order in the source sample units. The
resulting bounds must define finite, increasing ranges. Omitted settings
retain their saved values. The new project keeps the series and object planes.
`qupathxx_region --project-region` uses this saved display view. Raw export
uses the saved Z and time unless explicitly overridden and returns original
samples.

Use `matrix-project-index` to take the coordinate frame and reader from an
image already in a project. Supply the reader manifest when the saved image
uses an optional reader.

Project-backed file and store indexing and `matrix-attach` check any saved
image-source verification before publication. Review and adopt a changed
source before indexing or attaching against its new frame.

```text
qupathxx_format_import --reader-plugins READER_MANIFEST --format-plugins FORMAT_MANIFEST matrix-project-index PROVIDER INPUT POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX --plugin-options-json JSON
```

The manifests are absolute paths. The native operation reads the template and
writes a new QMX with the saved image's level-zero frame. Then use
`matrix-attach` with the reported QMX hash to publish a new project. The
attachment needs the saved image reader and QMX. Its original matrix, provider
and position table can be offline. [Attachment workflow](benchmarks/matrix-attachment/README.md)

To recover the association ID from a saved project and inspect its matrix:

```text
qupathxx_format_import project-matrices INPUT_PROJECT IMAGE_ID
qupathxx_format_import project-matrix-info INPUT_PROJECT IMAGE_ID ASSOCIATION_ID
```

Both commands read the project without journal recovery. The list reports saved
associations and resolved QMX locations even when a matrix is offline. Inspection
opens the associated QMX using the saved SHA-256 and validates its identity and
dimensions. It reports the value domain, saved frame and available import
receipt. Inspection needs the QMX file. Both commands work without the original
matrix inputs, format plugin or image pixels.

Export selected features from an associated QMX in the requested column order:

```text
qupathxx_format_import project-matrix-export-csv INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_CSV --feature Gene.B --feature Gene.A
```

The CSV starts with `observation_id` and keeps the native observation order.
UInt64 counts use exact decimal text. Continuous values use round-trippable
decimal text, including signed zero. Absent sparse entries are emitted as
measured zero. The output is a dense value projection for 1 to 63 explicit
features and at most 100,000 observations, with a 64 MiB file limit. Use the
QMX to retain sparse entry presence, coordinates, calibration and provenance.
The JSON report includes the selected feature IDs, saved matrix digest and
value domain. A new destination is required.

Export a complete integer count matrix for exchange as a sparse ZIP:

```text
qupathxx_format_import project-matrix-export-mtx INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_ZIP
```

The archive contains `matrix.mtx` with one-based feature and observation
indices, a headerless `features.tsv` with ID, display name and type, and a
headerless `barcodes.tsv` with observation IDs. It preserves original axis
order, empty rows and columns, stored zeros and exact unsigned 64-bit counts. The native
CLI re-indexes the extracted files with the Tenx catalogue option:

```text
qupathxx_format_import matrix-market-index matrix.mtx features.tsv barcodes.tsv NEW_QMX --catalogue tenx
```

Use `--catalogue qpx` for the QPX single-column catalogs with `feature_id` and
`observation_id` headers. The command reports the new QMX SHA-256, source
identity and axis counts. Export is limited to a 1 GiB ZIP and both commands
require new output paths.
Use the position export below for the matching coordinates and the QMX backup
when you need the original project association and provenance.

Export the associated positions for re-import alongside the CSV:

```text
qupathxx_format_import project-matrix-export-positions INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_TSV
```

The TSV contains `observation_id`, `x_px` and `y_px` in matrix observation
order, with round-trippable binary64 coordinates from the saved level-zero
pixel frame. The JSON report identifies that frame and its channel, Z and
time. Use the corresponding saved image and plane options when re-importing.
Position TSV import accepts one leading UTF-8 byte-order mark and includes it
in the original input digest.
The original positions file and format plugin can be offline during export.
QMX remains the source of the complete frame and calibration.

Restore a re-indexed QMX and its exported positions on the matching saved image:

```text
qupathxx_format_import project-matrix-import-positions INPUT_PROJECT IMAGE_ID QMX POSITIONS_TSV NEW_PROJECT
```

The native backed importer joins observation IDs exactly and saves the points
and an owned QMX in a new project. It takes level-zero pixel coordinates and
accepts `--frame-z` and `--frame-time` for the original plane. Supply the saved
image's admitted `--reader-plugins` manifest when the project uses a plugin.

Restore a missing owned QMX from an exact backup:

```text
qupathxx_format_import project-matrix-restore INPUT_PROJECT IMAGE_ID ASSOCIATION_ID BACKUP_QMX
```

The command verifies the backup against the saved association and copies it
atomically to the managed path. The project database stays read-only. Repeating
the command with the exact file already in place reports `already-present`.
A conflicting file or SQLite companion at the destination stops the restore.
Use `project-matrices` to find the association ID.

Create the backup directly from an owned or external association:

```text
qupathxx_format_import project-matrix-backup INPUT_PROJECT IMAGE_ID ASSOCIATION_ID NEW_QMX
```

The command validates the association, streams the full QMX to a new file and
checks the copied bytes against the saved SHA-256. The standalone copy keeps
the exact original identity and works with the restore and relink commands.

Relink an external matrix to an exact copy and publish a new project:

```text
qupathxx_format_import project-matrix-relink INPUT_PROJECT IMAGE_ID ASSOCIATION_ID EXACT_QMX NEW_PROJECT
```

Relink verifies the saved QMX digest and scientific identity. The new project
keeps the existing association ID and points to the replacement file. The
input project keeps its original locator. If the replacement is stored
relative to the new project, the link follows when both move together.

To give the new project its own exact copy, use the same arguments with
`project-matrix-own`. This keeps the association ID and stores the verified
QMX in the destination project's managed assets. The new project can reopen
with the original locator and replacement offline.

## AnnData directory matrices

The [AnnData Zarr provider](../plugins/anndata_zarr/README.md) uses matrix ABI4
for local stores. ABI4 embeds the ABI3 directory table and adds separate
sparse metadata and stored-entry callbacks. ABI3 providers keep their dense
route. Dense QMX still uses `external-dense-store/1`. The schema 6
`external-sparse-store/1` profile counts only stored entries, preserves explicit
zero values, and interprets absent coordinates as zero. The native host checks
sorted unique indices, values, store identity and the complete position join
before publishing. Both profiles use the same QMX reader and owned-project
services. See the [Windows workflow and checks](benchmarks/anndata-zarr/README.md).

To index a directory against an image already saved in a project, supply its
image ID and the store's primary metadata key.

```text
qupathxx_format_import --reader-plugins READER_MANIFEST --format-plugins FORMAT_MANIFEST matrix-project-store-index PROVIDER STORE PRIMARY_KEY POSITIONS INPUT_PROJECT IMAGE_ID NEW_QMX --plugin-options-json JSON
```

The reader manifest is needed when the saved image uses an optional reader.
The output QMX must be outside the source store. It retains the store receipt
and can be attached to a new project with `matrix-attach` after the original
store and positions are moved offline.

## Canvas-bound annotations

For canvas-bound SVG annotations, annotation ABI6 requires exact source
width and height in whole level-zero pixels. The host checks the selected
saved image dimensions before consuming object pages. Matching canvas
dimensions are retained in each annotation's native import metadata.
[SVG usage](../plugins/svg_annotations/README.md)

## Point annotations

Select `org.qupathxx.point-tsv`. The exact columns are `id`, `x`, `y`, `c`, `z`,
`t`, `class`. X/Y are finite nonnegative level-zero image pixels; C=-1 means all
channels, and Z/T are explicit nonnegative indices. The native target validates
each plane. Its image-bound geometry contract applies at atomic append;
outside-image points refuse without clamping or partial publication. Source IDs are
unique exact strings, subject to the native256-byte target limit.

Empty class means unclassified. Map each nonempty opaque source class label to
an existing native **string class key**. The provider cannot create classes or
receive a mutable Document. Mixed row planes are permitted when the target
supports them. Coordinates are not rounded, clamped or silently rescaled.

`preview_annotation_plugin` owns reviewable points, receipts and a deterministic
preview identity. `apply_annotation_plugin` checks the original live document,
revision, image server/source/frame, class registry and input currentness. A stale,
cancelled or invalid preview leaves the document/history unchanged. A successful
apply returns exact source-ID/native-ID mappings and appends one undoable batch.
Existing object properties persist per-row receipts through QOP/project save and
reopen; no DLL is required to read the saved result. Preview is consumed only by
a successful apply. Cancellation polls parsing, pages, source checks and staging;
the final native append is an atomic transaction, without a fixed cancellation
latency claim inside that transaction.

The selected target scope is a guarded regular-file/file-set image or immutable
native RGB image. Directory-store targets explicitly refuse until an atomic
membership/chunk guard contract is selected.

## Native CLI and SDK

The later [pixel geometry TSV provider](../plugins/geometry_tsv/README.md) adds
rectangle, ellipse, points, polyline, polygon with holes and multipolygon through
a separate annotation ABI2 entry. Existing QRO/Roi services interpret geometry;
the same native preview/apply/class/source/undo/project authority publishes it.
Its version2 per-object/recipe/preview identities retain exact geometry bytes
alongside source/module/frame/plane/class identity. ABI1 points and matrices keep
their original layout and exact identities. See the
[source-matched delivery receipt](benchmarks/annotation-geometry/README.md).

Use `annotations-import` with the matching host, or the compatible `points-import`
alias. Both commands honor saved image-source verification. Review and adopt
a changed source before importing annotations against its new frame.

The new source kit includes a separate native service archive/config and a
[public-only C++ consumer](../examples/annotation_geometry/main.cpp), linked over
the exact frozen SDK. Matching headers and archive travel together; do not replace
only the header of an earlier SDK. Independent module builds still require only
the C kit. The native overlay's MIT JSON and public-domain SQLite interface
sources/notices are distinct from the standalone module's dependencies.

The [ImageJ ROI/ZIP profile](../plugins/imagej_roi/README.md) adds a separate
ABI3 entry for source display names/properties/colors and explicit versus
unpositioned/legacy planes. Stable source IDs remain independent of display
names. Source facts are stored below `qpx.annotation-plugin.source.` and cannot
replace protected native receipt/ID fields. Native code resolves unique Z/T
planes or requires typed explicit choices; legacy indices require a named axis.
Linear composites use a shared native even-odd value codec and existing Roi
topology. ABI1/2 layouts and original stored point identities stay exact.

Use the matching ABI3 kit and complete native SDK overlay; the earlier frozen
SDK/host does not gain ABI3 by replacing a header. The optional ImageJ DLL links
only its private openly licensed zlib. See its [source-scoped receipt](benchmarks/imagej-roi/README.md).

The [WKT/WKB image-pixel profile](../plugins/wkt_wkb/README.md) uses the same ABI3
host and a shared bounded producer/value helper with ImageJ. It adds no external
reader/codec dependency. Selecting that provider declares literal image-pixel XY
coordinates; dimensions/SRID/empty members refuse. Multi-lines and heterogeneous
collections retain explicit source member paths/counts when split into native
objects. New provider source/binary identities are separate from the earlier
frozen ImageJ delivery; see the [A03 receipt](benchmarks/wkt-wkb/README.md).

The [OME2016-06 ROI profile](../plugins/ome_roi/README.md) uses a separate ABI4
entry to preserve the existing native interactive lock state. ABI1–3 layouts and
stored identities remain exact. Use its matching ABI4 native service/header/kit;
the frozen base SDK alone does not gain a newer provider ABI. The shared bounded
producer delegates provider-owned options. Seven vector primitives, source facts
and structured annotations reuse native geometry and import/persistence services;
unsupported masks/transforms fail explicitly. Exact Expat MIT/CC0 source and OME
CC-BY-4.0 schema terms remain separate in the [A04 receipt](benchmarks/ome-roi/README.md).

The [ASAP XML profile](../plugins/asap_xml/README.md) adds an independent ABI3
module over the same frozen native host and shared bounded XML/QOX2 parser.
Its source coordinates follow upstream binary32 interpretation, complete Order
permutations and explicit leaf-group class mapping. Preferred RGB follows the
topmost group color, with all original group/annotation metadata retained.
Unsupported spline/negative-space/unit/plane extensions refuse. OME retains its
separate schema/image/binary64/lock contract. The changed XML module sources have
new identities; the [A05 receipt](benchmarks/asap-xml/README.md) preserves old
products and qualifies the shared extraction without rebuilding the native host.

The [COCO instance profile](../plugins/coco/README.md) adds a separate ABI5 symbol
and QBM1 full-canvas binary masks. Polygon parts follow COCO rasterization/union;
compressed/uncompressed RLE retains column-major semantics. Native exact unit-cell
contours, image/frame checks, ID-based class mapping and preview/apply/undo/project
authority are shared. The [A07 receipt](benchmarks/coco/README.md) binds the new
host/CLI/SDK and module identities, including reused exact native mask objects;
ABI1–4 identities remain exact and frozen older hosts do not gain this capability.

The CLI uses the same native services:

```text
qupathxx_format_import --format-plugins ABS_MANIFEST catalog
qupathxx_format_import --format-plugins ABS_MANIFEST matrix-index org.qupathxx.counts-tsv COUNTS.tsv POSITIONS.tsv IMAGE OUTPUT.qmx
qupathxx_format_import --format-plugins ABS_LOOM_MANIFEST matrix-index org.qupathxx.loom INPUT.loom POSITIONS.tsv IMAGE NEW_OUTPUT.qmx --plugin-options-json EXPLICIT_OPTIONS_JSON
qupathxx_format_import matrix-info OUTPUT.qmx
qupathxx_format_import --format-plugins ABS_MANIFEST points-import org.qupathxx.point-tsv INPUT.qpproj IMAGE_ID POINTS.tsv OUTPUT.qpproj --class-map-json CLASS_MAP_JSON
```

`CLASS_MAP_JSON` is a flat string-to-string object, for example
`{"Tumor":"Existing native tumor class"}`. Project/image IDs are native IDs.
Outputs are new-only; input files and project/image sources are protected against
aliasing. Successful JSON reports include receipts, source identities and, for
points, the source/native mappings. Native failures remain visible. Optional
`--reader-plugins ABS_MANIFEST` configures the separate image-provider catalog
and can be repeated for up to 32 separately installed image-reader packs.

### Native GeoJSON project exchange

```text
qupathxx_format_import geojson-import INPUT.qpx IMAGE_ID ANNOTATIONS.geojson NEW.qpx --assume-pixels
qupathxx_format_import geojson-export INPUT.qpx IMAGE_ID NEW.geojson --nested-hierarchy
```

Use `--assume-pixels` for an untagged input whose coordinates you have confirmed
are level-zero image pixels. A native tagged export declares the coordinate
space, origin and image dimensions. Add `--allow-losses` only to accept an issue reported
by the native interchange service. Import also accepts `--qupath-precision` for
the existing QuPath ellipse admission policy. Export accepts
`--nested-hierarchy` to write child objects beneath their parents. Both commands
use new-only output paths and protect the project and linked sources. Import
reports incoming IDs and the native IDs assigned in the new project.

### Native saved object measurements export

```text
qupathxx_format_import measurements-export INPUT.qpx IMAGE_ID NEW.csv
qupathxx_format_import measurements-export INPUT.qpx IMAGE_ID SELECTED.csv --object 4 --object 9
```

The CSV includes saved object geometry, physical areas when calibrated, and
any applicable plane, editing state, metadata and imported measurement columns.
Intensity columns are blank. With no `--object` flags the command exports every
object in the selected image. Repeated IDs produce one row per object. Rows
are ordered by native object ID. The command reports the row and byte counts
after publishing a new file and protects the project and linked sources.

### Reader options for matrix and annotation workflows

Point and annotation imports can target a linked OME-Zarr project image.
`matrix-index` and `matrix-store-index` can use a local `.zarr` image as the
frame source. The native reader checks the observed store before publication,
and a new project or QMX must be outside the input store.

Both standalone matrix commands also accept `--reader ID`. Use
`--reader-plugins ABS_MANIFEST` to configure an optional image reader, and
`--reader-options JSON` to replace that plugin's defaults for this invocation.
For example:

```text
qupathxx_format_import --reader-plugins FITS_MANIFEST --format-plugins COUNTS_MANIFEST matrix-index org.qupathxx.counts-tsv COUNTS.tsv POSITIONS.tsv IMAGE.fits NEW.qmx --reader org.qupathxx.fits --reader-options '{"hdu":2}'
```

An image plugin uses the `qpx.format-import-cli-image/2` frame in both standalone
and saved-project indexing. It captures the effective reader implementation and
normalized options. `matrix-attach` requires that exact frame for a plugin
image, even when another view of the same file has identical dimensions. For a
new plugin attachment from an older QMX, reindex against the saved image.
Older QMX files and existing attached matrices retain their stored identities.
Built-in readers continue to use `qpx.format-import-cli-image/1`.

For native TIFF, add `--reader tiff --series N` to choose a zero-based series
when creating or appending a project image, or when indexing a standalone file
or directory-backed matrix. Project-based indexing reopens the saved series.
The built-in TIFF frame already includes the series, so attaching a QMX to a
different series of the same file fails even when the dimensions match.

```text
qupathxx_format_import project-create multiplex.tif study.qpx --reader tiff --series 1
```

All four matrix indexing commands accept `--frame-channel`, `--frame-z` and
`--frame-time`. These bind observation coordinates to a native image plane.
Channel `-1` applies to all channels. Z and time are zero-based concrete
indices. The default remains C=-1/Z0/T0. ROI aggregation can use the stored
frame and requires an area annotation on that plane.

ABI3–5 `annotations-import` also accepts `--unpositioned-z INDEX`,
`--unpositioned-time INDEX` (native zero-based) and
`--legacy-position-axis channel|z|time`. Ambiguous planes refuse without a choice.
`--plugin-options-json JSON` forwards native normalized provider options through
the same SDK argument; OME multi-image input requires an explicit `image_id` or
`unbound_rois` selection. Format interpretation stays inside the native provider.
COCO requires an explicit unsigned `image_id`; its source class labels are
`category/ID`, independently of category display names.
The matching M05 host also forwards `--plugin-options-json` for `matrix-index`.
`matrix-info` uses native QMX metadata/receipt services without a provider.
When the QMX has coordinates, its `spatial_frame` report includes the frame ID,
source ID, unit, channel/Z/time plane, dimensions and available calibration.
Format-CLI errors, including existing-output refusal, return exit code 2.

Plain CMake keeps Python OFF. Enable `QUPATHXX_BUILD_FORMAT_PLUGINS=ON` for the
Windows x64 MSVC Release `/MD` host. Host configuration needs no prebuilt kit.
Install component `FormatPluginKit` for the two C headers, compiler profile and
license. Build either initial TSV provider separately against that kit, with no core/Python
or dependency downloads. Keep `plugins/format-common` beside each provider's
source directory. The NativeSDK owns the C++ import/receipt APIs and matching
static host services; it is distinct from the standalone C development kit.
The later Loom module instead uses its explicitly pinned frozen HDF5/zlib native
closure and header context. Its typed SDK addon links the separately retained
A08 application addon and frozen FP04 core; actual dependency terms/notices and
corresponding sources remain separate. No Python discovery or runtime is needed.

The initial providers use bounded full-input parsing, not a scalable out-of-core
claim. The DLL work allowance is64KiB–256MiB and charges its source, decoded
arrays/catalogs and sorting/bookkeeping. Native host limits separately charge
the full assigned DLL allowance, bounded ABI workspace, retained positions/
points and native publication. Headers are bounded to64KiB; unsupported headers,
encodings, options, domains or numeric forms refuse explicitly. LF/CRLF and a
final unterminated row are supported; quotes are literal, not CSV quoting.

The [current task](current-task.md) records validation/delivery state. The full
qualified-catalogue goal includes additional scientific formats, stores, named
annotation formats, writers, remote IO and application/integration gaps under the
[format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14).
Each profile still needs its own actual terms, representable contract and evidence.
