# Native public OMERO integration

The optional C++ client connects to public OMERO.web JSON API v0 servers. It
browses projects/datasets/images, reads typed original-image metadata, downloads
rendered JPEG thumbnails, saves owned preview projects, retrieves bounded raw
blocks from explicitly linked open NGFF stores, and exchanges supported ROIs
and results through native GeoJSON and versioned offline files.
All interpretation, identity checks, output and project work is native. It uses
the existing admitted JPEG plugin; no file-reader ABI or automatic reader
selection changed. OMERO is a service, not another file format.

This is a public, read-only Windows x64 profile. Private login/SSO, uploads,
server write-back and whole-slide streaming are not implemented. A preview is explicitly
`uncalibrated-server-thumbnail-rgb8`; it is not a quantitative substitute for the
original image. Original dimensions, channel/type/plane counts and physical
quantities are reported separately. No plane selection, spatial registration or
physical scale is inferred from a thumbnail.

## Use

The locally checked executable is
`build/gui-aggregation/gap-closure/cleanup/runtime/qupathxx_omero.exe`.

```powershell
qupathxx_omero projects --server https://idr.openmicroscopy.org/ --limit 1
qupathxx_omero datasets --server https://idr.openmicroscopy.org/ --id 51 --limit 1
qupathxx_omero images --server https://idr.openmicroscopy.org/ --id 51 --limit 1
qupathxx_omero image --server https://idr.openmicroscopy.org/ --id 1884807
qupathxx_omero preview --server https://idr.openmicroscopy.org/ --id 1884807 --output preview-new.jpg
qupathxx_omero preview-project --server https://idr.openmicroscopy.org/ --id 1884807 --output preview-new.qpx --reader-plugins C:/ABSOLUTE/PATH/reader-plugins.json
```

The last command requires an admitted `org.qupathxx.jpeg` provider in the supplied
manifest and a reader-plugin-enabled host. The existing
[JPEG source/license/profile](../plugins/jpeg/README.md) applies unchanged.
The decoder validates the image; downloading an encoded preview alone validates
its declared media type, framing and bounds, not its full codec structure.

Use `--offset` and `--limit` for one explicit page (limit1..200); the client does
not silently scrape every page. `--timeout-ms` controls per-I/O timeouts and a
request deadline (default15000ms, maximum60000ms). JSON responses default to2MiB,
preview transfers to16MiB; `--preview-bytes` can lower or raise that transfer cap
up to64MiB. Preview-project decoding additionally admits at most1M pixels by
default, independently of the existing codec's bounded private workspace.

HTTPS certificate validation stays enabled. The client does not accept URL
credentials, use implicit Windows authentication or retain cookies. Redirects
are explicit, limited to three and restricted to the selected HTTPS server/base
path. HTML login/viewer pages, unsupported API versions and HTTP errors refuse;
server error bodies are not printed. Cancellation is checked around requests,
response chunks and output; an in-flight synchronous Windows call remains subject
to its configured I/O timeout.

## Snapshot and project contracts

An encoded preview owns its bytes and SHA-256, selected server/image/Pixels IDs
and canonical image-metadata SHA-256. Matching metadata observations bracket
the thumbnail request. This detects reported metadata drift; it does not claim a
server-side transaction, original-pixel identity or reproducible rendering settings.
Rendered bytes have their own identity even when source metadata is unchanged.

Preview projects pin temporary decoder bytes to that exact JPEG digest, reuse
the native reader, then embed decoded RGB pixels. The saved project records the
server, exact IDs, source dimensions, metadata/JPEG digests and preview domain.
It has no raw quantitative capability and no physical calibration. Reopening
uses those owned pixels and properties without OMERO or the JPEG plugin. It
does not silently reconnect or refetch. Existing outputs refuse before network
work and native publication is atomic/new-only. No credentials are saved.

## Original raw blocks

```powershell
qupathxx_omero raw --server https://idr.openmicroscopy.org/ --id 15190881 --x 256 --y 256 --width 128 --height 128 --channels 2 --z 12 --output raw-new.npy --receipt raw-new.json --reader-plugins C:/ABSOLUTE/PATH/reader-plugins.json
```

The image must explicitly identify an anonymous NGFF externalInfo association.
The client does not infer a store from image names or IDs. Its selected profile
is the base level of open NGFF0.4/Zarr2, interpreted by the existing native
`OmeZarrSource`. Dtype, byte order, C/F order, axes, units, channel/plane selection,
scale and translation remain native. Inconsistent OMERO/store dimensions, sample
type or supplied XY calibration refuse. This is original-channel data; thumbnail
rendering settings do not participate.

Only an in-bounds level-zero rectangle, contiguous channel range and one Z/T
plane are admitted. Original metadata and only the required chunks are staged
privately. Null/zlib/gzip chunks reuse the existing native decoder. BloscLZ/LZ4
Blosc1 chunks use optional `org.qupathxx.blosc-bytes` through the existing image
reader ABI; see its [profile, source lock and notices](../plugins/blosc-bytes/README.md).
It returns lossless flat bytes; the native Zarr authority supplies their scientific
interpretation. No new compression ABI or Python/Java runtime is involved.

Missing/inaccessible/corrupt chunks refuse rather than becoming fill values.
Metadata, selected chunk bytes and the image association are checked before and
after retrieval. The owned result records their hashes, original metadata,
selection, transforms, codec-module identity and exact raw/validity digest.
This is observed-byte stability, not a server-wide transaction guarantee.
The returned buffer is native-endian HWC with preserved floating-point bits.

Exports use the existing native `.npy` writer and an independent JSON receipt.
Both paths must be new and distinct. Each file is published atomically; the pair
is not a single filesystem transaction. A receipt failure can leave the completed
`.npy`, which the CLI reports as an error. Partial staging stores are never
published as complete images or raw projects. The original image coordinates and
channel/Z/T selection in the receipt locate the block; the block's local origin
is not a replacement coordinate frame for full-image annotations.

Defaults are256MiB native sampling workspace,256 selected chunks and64MiB total
NGFF response bodies including stability re-reads. Individual metadata bodies are
bounded to2MiB and chunks to64MiB. Codec opening has the ABI host's independent
bounded workspace; these budgets do not promise a total process RSS. CLI
`--transfer-bytes` and `--max-chunks` set the selected transfer/count bounds.
Anonymous external-store requests have independent HTTPS validation, no cookies
or credentials and no followed redirects.

## Original-coordinate ROI and result exchange

```powershell
qupathxx_omero rois --server https://idr.openmicroscopy.org/ --id 9528943 --output rois-new.geojson
qupathxx_omero roi-exchange --server https://idr.openmicroscopy.org/ --id 9528943 --input rois-edited.geojson --output updates-new.json
```

Native services support Rectangle, Ellipse, Point, Line, Polygon, Polyline and
Label shapes. IDs remain exact Int64 values. Analytic rectangles/ellipses retain
the existing native hints; original OMERO shape facts, ordering, styles and
TheC/TheZ/TheT fields are preserved separately from their editable XY projection.
An omitted/null plane field still means all corresponding OMERO planes. The
GeoJSON is an original-image XY projection, not a scientific Document assigned
to Z=0. Preserve its `omero`, `omero_source_shapes` and native GeoJSON extensions.
No thumbnail coordinates, physical scaling or arbitrary target-image registration
are inferred. Native interchange and `Roi` own all geometry interpretation.

`--z N --t N` explicitly select a plane; shapes applying to all planes are
included. The return command must use the same selection. Without these flags,
all planes are collected. Limits are10000 shapes and16MiB per exchange file.
Nonidentity affine transforms and unrepresentable edits (for example polygon
holes mapped to one OMERO Polygon) refuse without dropping geometry.

The return command retrieves a fresh native collection and checks exact image,
Pixels, metadata, selection, collection and original shape identities. Stale
files, changed source facts, duplicate IDs and preview frames refuse. Unchanged
shapes retain their original facts exactly. Vector geometry edits, names, lock/
stroke-color changes, class names, scalar measurement values, probability and
metadata facts are associated with the original ROI/shape IDs. Nonfinite scalar
facts use explicit strings. The file is `qpx.omero-roi-exchange/1`: supported
shape updates plus associated results, with its own digest. A selected subset
means updates to that subset, never deletion of omitted server ROIs. This profile
does not create new server identities or implement remote writes.

Raster Mask support requires admitted `org.qupathxx.png` in `--reader-plugins`.
The native mask endpoint must return exact-size RGBA8, transparent-black
background and one nontransparent fill matching the shape's declared color.
Ambiguous colors, fully transparent fill, fractional/out-of-image raster bounds,
empty masks and excessive work refuse. Existing native label contours preserve
pixel edges, holes, multipart coverage and exact area on the original grid.
Canonical MSB-first zero-tail binary coverage, observed PNG hashes and decoder
identity are retained. This is style-verified coverage from a PNG, not an assertion
of exact stored OMERO `Mask.bytes` or its padding. The return file preserves that
raster and its geometry; raster geometry edits require a separate explicit raster
editing workflow. The default aggregate mask budget is16M pixels/64MiB transfers,
including stability checks, so select a Z/T plane for larger collections.

IDR returned HTTP403 for mask rendering during this checkpoint. That live route
remains unavailable; it was not bypassed or replaced by bounding boxes. Authored
native PNG/plane/hole/raster round-trips pass, and public point ROI exchange is
verified separately. See the [source-scoped evidence](benchmarks/omero/README.md).

## Open-source and public-reference evidence

OMERO is offered under open-source licenses; a separate commercial license is
optional. See [OME licensing](https://www.openmicroscopy.org/licensing/) and
the [documented JSON API](https://omero.readthedocs.io/en/stable/developers/json-api.html).
This independent client is covered by this repository's AGPL-3.0-only license.
It links no OMERO SDK/server code and requires no local Java/Python runtime or
paid OMERO component. Windows WinHTTP is an operating-system transport; the
existing JPEG adapter/codec keep their own recorded licenses and notices.

The live reference is IDR project51, dataset51, image1884807, from Lawo et al,
*Subdiffraction imaging of centrosomes reveals higher-order organizational
features of pericentriolar material*, DOI10.1038/ncb2591. Its
[study annotation](https://idr.openmicroscopy.org/webclient/api/annotations/?type=map&project=51)
states CC BY4.0, copyright Lawo et al. Attribution and exact local JPEG identity
are retained in the [evidence](benchmarks/omero/README.md). Public accessibility
alone does not admit another study's data; licensing is checked per material.
No original vendor file was downloaded and this does not add a vendor reader.

The public legacy OME-TIFF export returned HTTP500 and full rendered-image
requests redirected to the viewer during the IDR migration. Those failures
were preserved. The explicit NGFF raw route above is independently verified; no fallback to
legacy exports, rendered pixels or fabricated full-image qualification is used.

## Build and validation

Opt in with `QUPATHXX_BUILD_OMERO=ON`. The CMake targets are
`qupathxx_omero_client`, `qupathxx_omero` and, with tests enabled,
`qupathxx_omero_tests`, `qupathxx_omero_exchange_tests` and `qupathxx_omero_cli_tests`.
The private `qupathxx_omero_cli` library keeps command parsing/reporting shared
with its tests. Extended suites need the corresponding native Zarr/GeoJSON/GEOS
features; feature-disabled builds report those suites as skipped (exit77). Ordinary builds still
default to Python and OMERO off. The optional Blosc byte provider is built
independently with the matching ReaderPluginKit and pinned local sources.
Enable `QUPATHXX_BUILD_OME_ZARR` for raw blocks and `QUPATHXX_BUILD_GEOS` plus
`QUPATHXX_BUILD_INTERCHANGE` for polygon/mask exchange. Native project storage
supports preview projects; Windows codec use also needs the existing reader-plugin
host. These optional capabilities are not enabled merely by selecting OMERO.

Native HTTPS transport is currently Windows-only; the authored protocol tests
use an injected native transport and do not contact external servers.
`preview-project` also needs native project storage and the configured JPEG
reader plugin. See the [evidence](benchmarks/omero/README.md) for the actual local
native/public/offline checks. No desktop controls, SDK/wheel, packaged release or
cross-platform/biological/resource qualification is claimed by this checkpoint.

The regular root CMake configuration has also been checked with OMERO/tests on
and Python off, using the existing cached dependency sources. The actual
`Omero.cmake` module compiles and links through normal CMake targets against the
retained core/dependency graph; CTest and offline project reopening pass. This
checks the new component's build declarations without recompiling the frozen
engine. It is not a full root product rebuild or a new SDK qualification.
