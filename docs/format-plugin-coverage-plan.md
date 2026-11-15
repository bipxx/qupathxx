# Format-plugin coverage and next delivery

Status2026-11-14: coverage inspection is complete and the selected native
[PNG](benchmarks/png-reader-plugin/README.md) and
[JPEG](benchmarks/jpeg-reader-plugin/README.md) plugins are delivered locally. No publication
is claimed. This is the non-UI follow-up after native integration. UI safety
remains owned by its separate workflow; Java/Groovy compatibility is excluded.
The implementation goal has since delivered FP00's complete scoped census and
the FP01 [PNM/PAM](benchmarks/pnm-reader-plugin/README.md),
[BMP](benchmarks/bmp-reader-plugin/README.md) and static lossless
[WebP](benchmarks/webp-reader-plugin/README.md) plugins. The three demonstrated
ordinary-input gaps are closed within their declared profiles; later packets
still require a selected workflow. The owner selected NRRD, ICS and external
matrix/annotation workflows, one at a time. The first FP02
[scalar NRRD](benchmarks/nrrd-reader-plugin/README.md) and
[ICS fluorescence](benchmarks/ics-reader-plugin/README.md) profiles are delivered;
external matrix/annotation plugins are next.

Owner update2026-11-14: plan all supportable format families. The
[full roadmap](#full-format-roadmap-2026-11-14) below owns future format intake,
plugin boundaries and delivery order. It is a planning catalogue; a candidate
row does not establish implemented support or select its implementation.

Use the [reader licensing policy](reader-license-policy.md), the existing
[plugin architecture](format-reader-plugin-plan.md) and
[family contracts](format-family-catalogs.md). Every additional implementation
is a native plugin with its actual component licenses. No noncommercial or
research-only implementation, dependency, required asset or restricted derivative
is eligible. Unknown terms remain unreviewed.

## What is delivered

Source baseline: main `7c981bf9`. The retained integration executable was queried
with `--readers` and `--formats`; both exited0. The exact executable/source hashes,
commands and JSON are in
[inspection results](../build/format-plugin-coverage/results.json).
Availability below describes that build, not every possible build configuration.

| Family / provider | Delivered behavior | Boundary |
| --- | --- | --- |
| Image: `ppm` | Binary P6 uint8 RGB input and display | No quantitative raw capability in the catalog |
| Image: `openslide` | Native whole-slide display via the configured OpenSlide SDK | SDK backend availability is not evidence for every vendor, codec or specimen |
| Image: `tiff` | Quantitative TIFF/OME-TIFF, display and typed raw access | Existing declared axes, series, codecs and metadata contracts |
| Image: `ome-zarr` | Local NGFF0.4/Zarr2, display and typed raw access | Existing null/zlib/gzip chunk profile; no general NGFF/Zarr3 claim |
| Matrix: `qmx` | Open an indexed expression sidecar | Exact indexed identity; opening is distinct from import |
| Matrix: `matrix-market` | Native Matrix Market import with explicit QPX/10x catalogs | Exact counts/IDs and declared coordinate/domain interpretation |
| Matrix: `h5ad` | Native AnnData import into the indexed store | Explicit dataset keys, counts/continuous domain and coordinate frame |
| Annotation: `geojson` | File/text import/export and clipboard | Native losses, hierarchy, classes, planes and undo authority |
| Annotation: `measurements-csv` | Measurement export | Export-only; not an annotation-import capability |
| External image provider: P6 example | Independently built DLL, relocated kit and saved-source workflow | Existing-format architecture proof; does not add another input format |
| External image provider: `org.qupathxx.png` | Static PNG, legal color/depth combinations and Adam7, typed original samples and separate RGBA8 display | Explicit manifest/provider selection; [declared profile](../plugins/png/README.md), Windows x64 recorded compiler only |
| External image provider: `org.qupathxx.jpeg` |8-bit Huffman DCT Gray/RGB/YCbCr, baseline/progressive, canonical decoded samples, oriented RGBA8 and explicit physical calibration | Explicit manifest/provider selection; [declared profile](../plugins/jpeg/README.md), lossy decoded samples, no ICC/CMYK; Windows x64 recorded compiler only |
| External image provider: `org.qupathxx.pnm` | Declared single-image P1–P7 UInt8/UInt16 source codes, PAM alpha and explicit display transfer | Explicit provider/options; [profile](../plugins/pnm/README.md), [receipt](benchmarks/pnm-reader-plugin/README.md); no inferred calibration or automatic suffix change |

Image plugins have a delivered dynamic C ABI. Matrix and annotation providers
currently have fixed native catalogs and typed dispatch, **not independently
loadable family plugin ABIs**. A selected external matrix/annotation importer
would need its own typed boundary; it must not be passed off as an image reader.
The image ABI is read-oriented and does not imply an image-writer plugin API.

## Confirmed gap and proposed order

A repository-generated2x2 RGB8 PNG was refused by the retained automatic image
route: `OpenSlide cannot open or recognize`. The probe's bytes, expected RGB
values and exact native diagnostic are retained in the inspection results.
An [independent Pillow decode](../build/format-plugin-coverage/png-oracle.json)
confirms the PNG is valid RGB2x2 and all12 decoded RGB bytes match the authored
values. Pillow is a development oracle, not a native product fallback.
This establishes an input gap on the named build; it is not evidence that an
owner's real PNG workflow or every other format has been evaluated.

The owner selected PNG after this inspection. Its declared profile is now
implemented and verified through the existing kit and host. The baseline PNG
refusal remains evidence of the original gap; legacy automatic selection is
unchanged, so use the explicit plugin provider. Other scanner, omics or annotation
inputs require their own selected workflow; this delivery starts none of them.

| Candidate / area | Current finding | Native route and licensing | Next decision or check |
| --- | --- | --- | --- |
| Standalone PNG | Delivered locally through `org.qupathxx.png`; legacy automatic-open route unchanged | Existing image-plugin ABI; locked libpng1.6.59/zlib1.3.2, separate licenses, exact source kit | Completed:33 positive profiles,10 refusals, native project/raw/replay, Unicode paths, relocation, admission and removal; see the receipt |
| Standalone JPEG | Delivered locally through `org.qupathxx.jpeg`; ordinary JPEG gap confirmed against the retained automatic route | Existing image ABI; private pinned libjpeg-turbo3.1.3, [declared profile](../plugins/jpeg/README.md), actual component licenses/source kit | Completed:40 exact profiles,29 refusals, native replay/identity, orientation/calibration, PNG coexistence, relocation,199-member admission and removal; see the [receipt](benchmarks/jpeg-reader-plugin/README.md) |
| OpenSlide scanner formats and DICOM WSI | Existing `openslide` route, not automatically a missing parser | Inspect the actual installed SDK and required codec/license set first | Use one intended source to determine whether the gap is availability, metadata, source identity or genuinely absent decoding |
| Additional OME-Zarr codecs or Zarr versions | Outside the delivered null/zlib/gzip Zarr2 profile | A directory-store/codec plugin needs an explicit compatible source contract; the current external image pilot is file/companion based | Do not force a directory store through a single-file ABI or rebuild the delivered Zarr reader without a selected need |
| Additional matrix / annotation formats | Existing formats already dispatch through shared native services | A family-specific native plugin boundary is needed for a selected independently distributed importer | Preserve exact IDs, integer counts, coordinates, loss reports and atomic host publication |
| Java/Groovy bridge, closed required decoder, NC/research-only component | Excluded by owner scope | No implementation | Keep excluded entries out of the required backlog and package inputs |

OpenSlide's [upstream format list](https://openslide.org/formats/) includes
multiple scanner backends and DICOM. That is upstream capability information,
not proof that the retained SDK provides every current backend. Its
[DICOM contract](https://openslide.org/formats/dicom/) is specifically whole-slide
microscopy and can involve companion files; it is not general medical-image
DICOM support. Keep those distinctions in any capability report.

## Candidate dependency evidence

Checked2026-11-13. These records are research inputs for a selected package;
they do not admit an unbuilt plugin or all members of an upstream archive.
Reuse the existing inventories and material guard, not a second licensing store.
The [existing guard](../tools/ci/check_material_staging.py) already recognizes
`libpng-2.0`; the OpenSlide notice inventory also has that license. Neither admits
the candidate1.6.59 bytes without a new exact material record.

| Component | Exact reference | Terms and package handling |
| --- | --- | --- |
| libpng candidate | [v1.6.59 LICENSE](https://raw.githubusercontent.com/pnggroup/libpng/v1.6.59/LICENSE); [upstream release/checksums](https://www.libpng.org/pub/png/libpng.html) | PNG Reference Library License2 grants use for any purpose subject to its stated conditions. Retain the full license, origin and modification notices. Other contributed/generated files can have separate terms; inspect selected members |
| libpng source selection | [Exact member lock](../plugins/png/libpng-source-lock.json), commit `cd952f49f95bb27154ae77dbb103032d95f6e580` |25 selected code/header/notice files, no upstream images/contrib. The earlier candidate full-archive pin was not downloaded or substituted for this admitted selection |
| PNG format | [PNG second-edition specification](https://www.w3.org/TR/2003/REC-PNG-20031110/) | Published lossless raster format; declaring a PNG profile does not establish support for every depth, color type, animation or color-management feature |
| zlib | Existing admitted pin in [PinnedZlib.cmake](../cmake/PinnedZlib.cmake) and [reader inventory](reader-license-policy.md#current-reader-inventory) | Preserve Zlib terms and the actual package identity; use a compatible plugin-local build or declared dependency without changing host resolution |
| libjpeg-turbo | Existing3.1.3 archive/hash and notice installation in [TiffJpeg.cmake](../cmake/TiffJpeg.cmake); upstream [LICENSE.md](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.1.3/LICENSE.md) and [README.ijg](https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/3.1.3/README.ijg) | Retain the applicable IJG/BSD/Zlib component notices. The selected API/build members determine the inventory; a TIFF qualification is not a standalone-JPEG qualification |

Repository-authored adapter code keeps its repository license unless explicitly
licensed otherwise. Record that separately from dependency licenses. Plugin
isolation does not waive compatibility, attribution, source or distribution
obligations. Use repository-generated fixtures initially; do not copy an upstream
test image, specimen or trained model merely because its decoder is eligible.

## Implementation packet for the first selected image plugin

1. Name the actual input workflow and freeze its profile: color types/bit depths,
   transparency, orientation, coordinates, physical calibration, display transforms
   and raw-sample capabilities. Unsupported cases refuse explicitly; no silent
   uint16-to-uint8 conversion or inferred physical scale. A display RGBA buffer
   does not establish quantitative sample access or existing recipe admission.
2. Pin and admit the exact plugin/decoder/dependency source and binary members in
   the current material inventories, including notices and synthetic fixtures.
   No optional download or runtime fallback introduces an unreviewed dependency.
3. Build the module against a relocated existing ReaderPluginKit using the
   recorded Windows x64 MSVC Release `/MD` profile. Give it a distinct provider
   ID, version and semantic contract; generate its hash-bound manifest after
   linking. It does not link the application core or alter built-in selection.
4. Use the retained host and existing native authorities for file observations,
   metadata, buffers, region reads, cancellation and saved-source identity.
   Preserve exact currentness/implementation checks, including dependency changes.
   A missing module leaves the saved source unavailable with an honest diagnostic.
5. Settle the interface, codec configuration and tests before one plugin-only
   cached build. Verify matching headers/profile and all frozen host/core/binding
   hashes. Rebuild a host only for a demonstrated missing contract; such a change
   gets a separate source identity and affected validation.
6. Exercise independent expected pixels, alpha and supported raw samples, cropped/
   padded regions, axes/calibration, malformed/truncated inputs, unsupported
   encodings, overflow/work bounds and cancellation. Keep invalid samples,
   channel meaning and color-management limitations explicit.
7. Exercise plugin-specific project attach/save/reopen, offline missing-provider
   handling, changed implementation/dependency refusal, and the selected retained
   analysis/replay. Check unaffected built-ins; no broad qualification campaign.
8. Stage a relocated plugin package with its exact manifest, source/notice/license
   inventory and dependencies. Run the existing material/member guards; verify
   use/removal preserves host and user files. Record source-scoped evidence and
   stop at that delivery. No wheel, new platform or publication is implicit.

## Planning handoff

- [x] Inspect native source dispatch and the installed image/matrix/annotation catalogs.
- [x] Distinguish the external image ABI from fixed typed family catalogs.
- [x] Demonstrate a PNG input gap with generated bytes and retain exact diagnostics.
- [x] Check primary candidate license evidence and existing codec pins.
- [x] Define the first-plugin delivery and decisive checks without reopening UI work.
- [x] Owner selected PNG; its exact static image/sample/display/calibration profile is documented.
- [x] Implement and qualify the selected plugin through the existing kit; retain the local package/source and frozen-host evidence.

Inspection correction: the first probe controller expected a guessed diagnostic.
The existing native source showed the actual OpenSlide refusal; the controller
now checks that exact message. No product assertion or source was changed.

## Full format roadmap (2026-11-14)

Deliver independently selectable native format plugins under explicit profiles,
with useful scientific inputs and repeatable saved-source workflows. Coverage
includes images, matrices, annotations, project exchange and eventual exports.
Existing supported formats retain their authorities and frozen artifacts. Do not
rewrite built-ins simply to place all legacy code behind DLLs.

"All supportable" means every candidate in the scoped research-format families
gets an honest disposition and a feasible native route where established. It
does not mean every suffix, encoding, vendor product or Bio-Formats reader is
already implementable in C++. The following families form the roadmap;
FP00's [dated census](format-plugin-census.json) now retains/classifies every
entry from the inspected upstream indexes. That census is planning metadata,
not runtime registration or acceptance of every variant in an entry.
Formats outside these families enter through the same intake card, not an
automatic decoder campaign.100% QuPath parity remains outside the owner's goal.

### Status and admission rules

| State | Meaning |
| --- | --- |
| Delivered | Only the existing source-scoped profile/platform and receipts qualify |
| Reuse first | An existing route may suffice; prove the intended variant before adding code |
| Candidate | A plausible native implementation/schema route exists; exact package/fixture admission and validation are still required |
| Contract first | Current source, metadata, import/export or publication contracts cannot honestly represent the requested behavior |
| Unreviewed | Native path, schema reuse terms, dependency rights, maintenance or reference inputs remain unresolved; no support claim |
| Excluded | Current owner policy disallows the necessary implementation/material or closed format in the proposed workflow |

### FP00 implementation and first selected gap

FP00 is complete for the2026-11-14 snapshot:165 Bio-Formats8.5.0 format rows
and13 OpenSlide sections,178 retained/classified entries,282 listed extension
aliases and195 distinct case-folded suffixes. All57 roadmap groups remain linked.
The [census](format-plugin-census.json) records per-entry disposition and exact
source URL/date/hash; aliases never choose a runtime parser. UltraVIEW's literal
`etc.` notation remains explicit and unexpanded. Independent source-table/link/
section checks match every row; offline reparse is byte-deterministic.

The actual retained OpenSlide API reports4.0.1; three DLL copies match SHA
`dc8e35ad0ce001c4e878f2bebc23e87fb85d2bdebec5a542af2ecec3cb5cd635`.
Matching source archive/Meson and CRC-bound debug evidence contain14 linked source
units (13 indexed backends plus internal synthetic). This establishes source/build
presence, not new per-vendor/codec input qualification. Existing Aperio and one
brightfield NDPI receipts retain their narrower scopes.45 inspected runtime/SDK
artifact hashes are unchanged. Paired SDK codec-version metadata is explicitly
distinguished from separately queried runtime APIs.

Actual library/source notices identify OpenSlide LGPL-2.1-only; an older aggregate
source-archive inventory label says or-later and mixes component terms. The
census preserves both provenance facts without widening the library's terms or
changing frozen packages. Future distribution must use exact component notices.
No new third-party implementation, specimen or material admission occurred here.

| Rank | Demonstrated gap | Selection / next action |
| --- | --- | --- |
|1 | Valid authored PGM grayscale cannot open through the retained automatic route; built-in P6 only exposes maxval255 RGB display | Delivered [PNM/PAM plugin](benchmarks/pnm-reader-plugin/README.md):79 profiles,36 refusals,nine native routes, exact source/option identity and20-member admitted offline source/package handoff |
|2 | Valid authored BMP fails the same route | Delivered [BMP plugin](benchmarks/bmp-reader-plugin/README.md):62 profiles,40 refusals,eight native routes, exact RGB/palette-index/calibration semantics and20-member admitted offline source/package handoff |
|3 | Valid authored lossless WebP fails the same route | Delivered [static lossless WebP plugin](benchmarks/webp-reader-plugin/README.md):19 profiles,28 refusals,eight native routes, exact RGBA/alpha semantics, fully charged codec scratch and90-member admitted offline source/package handoff |

Each control independently decodes to its literal expected pixels; the native
diagnostic is exactly `OpenSlide cannot open or recognize`, and direct
`openslide_detect_vendor` is null. This is an ordinary-input gap, not a vendor
specimen qualification. FP01 preserves the frozen host/built-ins and uses the
existing external image ABI. Native code/tests/packaging have disjoint ownership.

Retained evidence: `build/format-plugin-coverage/fp00/upstream/validation.json`,
`upstream/source-manifest.json`, `runtime/summary.json`, `runtime/source-records.json`
and `runtime/results.json`; the adjacent runtime README records individual backend
and fixture/license scope. [Source extractor](../tools/ci/format_inventory_upstream.py)
and [planning census builder](../tools/ci/build_format_census.py) are development
audit tools; they do not implement or drive product format dispatch.

Every new format uses C/C++ application services and optional native plugins.
Python may drive builds/tests or act as an independent oracle; it is never the
shipped parser, scientific authority or fallback. Java/Groovy compatibility is
excluded. Group related extensions into one semantic provider when appropriate;
a codec or suffix is not a separate scientific feature.

Apply [per-plugin admission](reader-license-policy.md) independently to schema,
implementation, every required codec/filter, fixture and derived material. Preserve
actual licenses, revisions/hashes, notices and source/distribution obligations in
the existing inventories. Do not create a second license database. Unknown/NC/
research-only/closed required materials cannot enter a build, test corpus or
package. Copyleft is eligible in principle; exact integration terms still matter.

For new fluorescence/spatial-omics, the format/schema must also satisfy the
[open-format rule](fluorescence-omics-plan.md#format-admission). An open HDF5,
TIFF, CSV or Zarr container does not admit a restricted inner schema or vendor
bundle. Legacy brightfield vendor input can be evaluated through an eligible
open native reader under the separate existing-reader policy. Record that exact
workflow distinction rather than declaring all vendor formats allowed/excluded.

### Architecture to preserve and extend

| Family or requirement | Existing authority / proposed boundary | Implementation consequence |
| --- | --- | --- |
| Single-file image and bounded file set | Existing [image C ABI](../cpp/include/qupathxx/reader_plugin.h), loader and native source/project services | Reuse v1; module-private decoder; host-owned buffers and file observations. No application-core link for an independently built reader |
| Companion files | Existing optional source-files extension: primary plus at most63 companions, bounded source payload | Observe every semantic input, reject aliases/escapes/changes. Larger collections cannot hide files behind an unobserved sidecar |
| Directory/chunk stores | Existing built-in OME-Zarr contract; no general external store ABI | Define an additive native store inventory/snapshot extension for the first selected external store. Include metadata, chunk/shard identity, traversal bounds and currentness; do not enumerate arbitrary directories through v1 |
| Multidimensional image views | v1 raw reads have channel/Z/T selection; RGBA requests require all selectors zero | A raw capability is not arbitrary-plane display support. Bind a representable selected view through explicit native options, or introduce the missing native view/ABI contract before claiming display/replay |
| Affines, irregular time, complex/half samples, anisotropic pyramid transforms | v1 has per-axis scalar calibration and one scalar downsample; no general affine/origin or complex/float16 descriptor | Start with explicitly representable subsets. Preserve declared sample semantics; refuse unsupported transforms/types. Never silently resample, drop affine information or reinterpret arrays as channels |
| Matrix importer plugins | Existing [typed matrix authorities](format-family-catalogs.md); no dynamic matrix ABI | Add a versioned C boundary only with the first selected external importer. Stream bounded typed values/IDs/options into host-owned staging; native services validate and publish/index QMX atomically |
| Annotation importer/exporter plugins | Existing native geometry/interchange and preview/undo authorities; no dynamic annotation ABI | Return bounded typed geometry/properties/planes, mappings and losses. Host validates IDs/classes/references, obtains required loss acceptance and commits one native transaction. Plugins do not mutate a Document/project |
| Image/matrix writers | Image reader ABI is read-only; existing export services remain authoritative | Add a separate typed writer contract for a named export workflow. Require exact target schema/loss report and protected new-only/atomic output; no read-to-write inference |
| Remote sources | No generic file-plugin URI/store authority | Separate native source-provider design for authenticated access, immutable/versioned object identity, caching/offline behavior, cancellation and source review |

New boundaries must use versioned C data, owned snapshots, bounded streaming and
explicit lifetimes/status/cancel rules, without STL/allocators/exceptions or retained
host pointers crossing modules. Keep v1 layouts/entries and old saved projects
compatible. Source/implementation identity includes exact module/dependency hashes,
semantic contract and canonical options; paths alone are not implementation identity.

Keep one optional package per coherent provider/profile, with explicit private
dependencies and no arbitrary system-codec discovery. Share internal native codec
utilities when this reduces duplicate logic, without merging scientific identities
or forcing unrelated formats to install together. No universal-decoder bundle or
opaque conversion subprocess is selected. Runtime automatic suffix routing stays
unchanged until a separately tested native dispatch change is actually needed.

### Image and scientific-array candidates

Every candidate row below is read/import planning unless an export is explicitly
named. Raw/display/recipe availability must be recorded separately.

| ID | Format/profile | State and native route | Required profile decision |
| --- | --- | --- | --- |
| I01 | PNG | Delivered `org.qupathxx.png` | Preserve static legal depth/color/Adam7 samples, RGBA8 and pHYs profile; new ICC/APNG handling is a separate contract |
| I02 | JPEG/JFIF/EXIF | Delivered `org.qupathxx.jpeg` | Preserve canonical decoded8-bit Gray/RGB, orientation and density contract. CMYK/ICC/arithmetic/other precision remain outside it |
| I03 | PBM/PGM/PPM/PAM | Delivered `org.qupathxx.pnm` selected P1–P7 profile; legacy P6 route unchanged | Exact UInt8/UInt16 source codes, PBM/PAM polarity, maxval, alpha and explicit BT709/sRGB/linear display; [receipt](benchmarks/pnm-reader-plugin/README.md) |
| I04 | BMP/DIB | Delivered `org.qupathxx.bmp` selected native profile | BM/40-byte BITMAPINFOHEADER BI_RGB1/4/8/24/32, original indices/RGB, canonical rows and per-axis ppm; X is unused, other headers/compression/alpha profiles refuse |
| I05 | WebP | Delivered `org.qupathxx.webp` static lossless profile | Simple single-VP8L version0, exact RGBA/hidden RGB and hint-independent alpha; lossy/VP8X/metadata/animation variants remain separate unimplemented contracts |
| I06 | JPEG2000 JP2/J2K | Candidate with maintenance risk: native OpenJPEG or another admitted implementation | Lossless/lossy, signed precision, components, color/palette, resolution levels and bounds; no automatic JPX/HTJ2K claim |
| I07 | GIF/APNG and animated WebP | Single-frame subset Candidate; animation Contract first where timing/view metadata is missing | Refuse animation in a static contract; explicitly define disposal/compositing and nonuniform frame timing before animation support |
| I08 | TGA, PCX, QOI | Unreviewed, lower-priority native raster profiles | Select exact palette/RLE/alpha variants and establish schema/code terms for each; do not adopt an unrestricted multiparser bundle |
| I09 | JPEG XL | Candidate via libjxl after exact dependency audit | Integer/float/lossy/alpha/color/HDR and animation are separate profile choices |
| I10 | AVIF | Candidate via libavif plus one selected open native AV1 decoder | Exact bit depth, range/primaries/transfer, YUV conversion, alpha and still-versus-sequence semantics |
| I11 | HEIF/HEIC | Unreviewed exact native stack and format/codec obligations | A libheif wrapper does not admit every codec or establish all commercial-use rights; no closed decoder fallback |
| I12 | OpenEXR, Radiance HDR | Candidate after native schema/codec admission | Keep linear/HDR values separate from display tone mapping; half/complex/unrepresented channels need an explicit native contract |
| I13 | TIFF/BigTIFF/OME-TIFF | Reuse delivered quantitative profile first | Inventory actual qualified dtypes/layouts/axes/series/compressions; add only missing variants through a new optional native profile, preserving frozen reader |
| I14 | ImageJ/LSM/STK/QPTIFF and other TIFF metadata dialects | Reuse pixel decoder; each metadata/schema variant Unreviewed until inspected | Generic TIFF pixels do not qualify vendor channels, Z/T, registration, labels or calibration; apply open-format policy to the intended workflow |
| I15 | ICS/IDS v1/v2 | Delivered `org.qupathxx.ics` selected uncompressed profile | Independent native parser, guarded ICS1/IDS or inlineICS2, scalar/channel-first XY/XYZ, UInt8/UInt16/Int16/Float32, exact byte permutations/C/Z and explicit calibration/window; libics1.7.2 development oracle only |
| I16 | NRRD/NHDR | Delivered `org.qupathxx.nrrd` attached scalar profile | NRRD0005 raw XY/XYZ UInt8/UInt16/Int16/Float32, explicit kinds/endian/window and exact raw bits/Z/calibration; detached/compressed/general affine/frame profiles remain unimplemented |
| I17 | NIfTI1/2, Analyze7.5 | Candidate native selected slices; general spatial interpretation Contract first | qform/sform, scaling versus stored values, endian, intent and voxel/world frame; preserve or refuse oblique/unsupported data |
| I18 | FITS | Candidate via admitted CFITSIO or a bounded native subset | Explicit HDU, BITPIX, BSCALE/BZERO/BLANK and axis/WCS meaning; astronomical coordinates are not microscopy calibration |
| I19 | MRC/CCP4 | Unreviewed native/schema subset | Axis permutation, origin, mode/endian and supported dimensional projection; no inferred registration |
| I20 | MetaImage MHA/MHD, VTK VTI, ENVI | Unreviewed, separate native profiles | Header/payload identity, strides/endian/dtype and physical transform; shared raw storage does not make these one schema |
| I21 | Explicit raw binary plus metadata | Candidate only with a complete named sidecar schema | All dimensions/strides/endian/type/offset/units supplied and hashed; no suffix-based scientific guesses |
| I22 | OME-Zarr NGFF0.4/Zarr2 | Delivered local null/zlib/gzip subset | Reuse existing identity/axes contract; new codecs, labels, collections or versions require their own declared coverage |
| I23 | OME-Zarr NGFF0.5/Zarr3, shards and additional open codecs | Contract first for independent directory-store plugin, then Candidate per version/codec | Metadata version, chunk/shard inventory, coordinate transforms and budgets; preserve old0.4 interpretation |
| I24 | N5 and BigDataViewer XML/HDF5/N5 | Contract first for directory stores or large file sets; exact native/schema path Unreviewed | Explicit array/multiscale selection and transform semantics; no Java runtime dependency |
| I25 | CellH5, published HDF5 image schemas | Unreviewed per schema; reuse admitted native HDF5 where compatible | HDF5 is a container. Select datasets, typed axes/units and exact allowed filters; no arbitrary external filter loading |
| I26 | Micro-Manager / HCS / TIFF image collections | Reuse OME exports first; each published open collection profile Candidate after review | Collection membership, positions, plate/well/field/series, acquisition metadata and view model; large sets need the source contract first |

### Whole-slide and vendor-format coverage

The current [OpenSlide upstream index](https://openslide.org/formats/) names
Aperio `.svs`, ARGOS `.avs`, DICOM `.dcm`, Hamamatsu `.ndpi/.vms/.vmu`, Huron
TIFF, Leica `.scn`, MIRAX `.mrxs`, Philips TIFF, Sakura `.svslide`, Trestle TIFF,
Ventana `.bif/.tif`, Zeiss `.czi` and generic tiled TIFF. These are thirteen
upstream backend entries, **not thirteen newly qualified qupathxx readers**.
FP00 must check the actual pinned SDK, enabled backends/codecs and intended
sources; newer ARGOS/Huron/Zeiss entries cannot be inferred from CI's requested
OpenSlide4.0.1 pin. The runtime version/build needs its own evidence.

| ID | Area | Disposition and next action |
| --- | --- | --- |
| W01 | OpenSlide-backed brightfield scanner variants above | Reuse first. Record individual backend/codec/layout and companion-set evidence. Add a separate native provider only if the installed route has a demonstrated gap |
| W02 | DICOM whole-slide microscopy | Reuse first, then qualify exact transfer syntaxes/photometric interpretations, sparse/multiframe layout and required companions from the [WSI contract](https://openslide.org/formats/dicom/). No general radiology/SEG/SR claim |
| W03 | Philips iSyntax | Unreviewed brightfield candidate: [libisyntax](https://github.com/LUMCPathAI/libisyntax) has native decoding and [BSD-2-Clause code](https://raw.githubusercontent.com/LUMCPathAI/libisyntax/main/LICENSE.txt). Audit exact native dependencies, schema path and fixtures; existence of this reader is not admission or support |
| W04 | CZI, ND2, LIF/LOF/XLIF, VSI/OIR/OIB/OIF, NDPIS, IMS | New fluorescence use is Excluded where the inner format fails the open-format rule; otherwise Unreviewed pending actual reusable schema/native-stack evidence. [libCZI](https://github.com/ZEISS/libczi) is a native option, not a policy waiver |
| W05 | DM3/DM4, DeltaVision, MetaMorph, Andor SIF, SPE, SlideBook and remaining microscopy/acquisition dialects | Unreviewed individually; reconcile the [Bio-Formats index](https://bio-formats.readthedocs.io/en/stable/supported-formats.html) at FP00. Native implementation, schema rights, codec and fixture evidence are independent requirements. Do not promise a port of every Java reader |
| W06 | General medical DICOM / volumetric series | Contract first: native series/file identity, pixel/affine/spacing/photometric interpretation and typed view. This is a separate workflow from WSI, with no clinical claim |

Candidate discovery must use current primary sources: QuPath's
[format guide](https://qupath.readthedocs.io/en/stable/docs/intro/formats.html)
describes a useful reference, not our acceptance target. An older claim that no
open reader exists is not a permanent exclusion when a newer native implementation
can be evaluated. Conversely, the native [OME Files](https://www.openmicroscopy.org/ome-files/)
project covers TIFF/OME-TIFF; it is not a native replacement for the complete
Bio-Formats catalogue. Private research inputs do not become redistributable fixtures.

### Matrix, spatial-data and annotation candidates

| ID | Format/profile | State / native route | Required scientific contract |
| --- | --- | --- | --- |
| M01 | QMX | Delivered native indexed open | Preserve exact stored identity, indexed access and offline-original independence; no plugin rewrite |
| M02 | Matrix Market + QPX/10x TSV | Delivered declared native import | Preserve exact UInt64 counts, sparse zeros, feature/observation IDs, explicit catalog/coordinate options; separately scope any new Matrix Market variant |
| M03 | AnnData/H5AD | Delivered declared native profile | Reuse explicit dataset keys, dense/CSR/CSC and value/coordinate domain; additional encodings need a new profile |
| M04 | Dense CSV/TSV matrices and coordinate tables | [Declared counts TSV profile delivered](format-family-plugins.md); other delimiters/domains remain conditional | Exact observation-major UInt64 counts/IDs; complete native position-TSV join and explicit frame; native QMX target256-byte IDs, no double/missingness inference |
| M05 | Loom HDF5 | Candidate after schema/terms review and matrix plugin boundary | Explicit matrix/layer, row/column IDs, domain and coordinates; ignored graphs/metadata must be declared |
| M06 | 10x feature-barcode HDF5 | Unreviewed open-format eligibility, then Candidate using native HDF5 | [Published layout](https://www.10xgenomics.com/support/software/cell-ranger/latest/analysis/outputs/cr-outputs-h5-matrices) alone is not reusable-terms admission; preserve CSC indices/counts/barcodes/features and exact IDs |
| M07 | AnnData Zarr | Contract first: matrix directory-store identity, then selected native [on-disk encoding](https://anndata.readthedocs.io/en/stable/fileformat-prose.html) | Matrix family, not ImageServer; select version/keys/encodings/domain and all metadata/chunk identities |
| M08 | Arrow/Feather/Parquet | Unreviewed native dependency/schema profile | Explicit integer/null/continuous semantics, dictionary IDs, row groups and coordinate schema; no arbitrary table-to-expression inference |
| M09 | MuData H5MU, NPY/NPZ | Unreviewed exact schema/native path; multimodal composition Contract first | Explicit selected modality/array and separate ID catalogs; do not flatten modalities or infer a cell axis from array dimensions |
| M10 | Visium, Xenium, CosMx, MERSCOPE and other platform bundles | Reuse admitted generic open exports only; proprietary schema/bundle compatibility is not admitted by MTX/CSV/HDF5/Parquet storage | Exact stable-ID association, units/coordinate frames and supplied registration; no invented spatial alignment or platform/biological acceptance |
| A01 | GeoJSON; measurements CSV | Delivered GeoJSON import/export; measurements CSV export-only | Preserve image-pixel frame, native loss/class/plane/property/undo behavior; CSV export is not object import |
| A02 | ImageJ ROI and ROI ZIP | Candidate native parser after typed annotation boundary and exact terms | Version/ROI/subpixel/plane/group/style and supported composite shapes; ZIP budgets/member paths and loss reporting. No Java runtime |
| A03 | WKT/WKB | Candidate using existing eligible geometry utilities | Explicit coordinate frame/units, geometry dimensions and class metadata mapping; no inferred geographic CRS or silent geometry repair |
| A04 | OME ROI XML | Candidate native XML profile after schema/terms check | OME image/shape/plane IDs, units, supported primitives and loss acceptance; not a new image decoder |
| A05 | ASAP/Aperio XML and vendor annotation dialects | Unreviewed per schema/terms; excluded where proposed use violates open-format policy | Class/region/negative-space/plane/color mappings; XML parser availability does not establish the dialect's contract |
| A06 | Explicit point/box/polygon CSV/TSV | [Declared point TSV profile delivered](format-family-plugins.md); boxes/polygons/CSV remain conditional | Exact IDs and image-pixel coordinates/C/Z/T; explicit existing string-class mapping, guarded native bounds/append/undo/receipts; no heuristic columns, clamp or silent row loss |
| A07 | COCO JSON polygons/RLE | Candidate after schema/native-code/fixture admission | Image identity, category/instance IDs, RLE ordering/crowd/holes and pixel lattice; no COCO dataset/license inheritance |
| A08 | TIFF/PNG/NGFF label masks | Candidate composite import through admitted image reader plus native annotation service; NGFF labels need a declared store profile | Exact label IDs/type/background/plane/reference-image registration; vectorization is an explicit derived result, not original object identity |
| A09 | SVG subset, GeoPackage, NGFF tables/shapes | Unreviewed typed schema/native dependencies | Coordinate transforms/CRS, metadata/losses and safe bounded payload; full vector documents are not automatically scientific objects |
| A10 | DICOM SEG/SR | Contract first, separate from WSI pixel reading | Referenced SOP/frame/segment IDs, patient/slide coordinates, segmentation type and coded measurements; no incidental support from `.dcm` |

New family ABIs must preserve exact integers above2^53 through `UINT64_MAX`, source
axis order, distinct stable IDs versus display names, explicit counts/continuous
domains, absent sparse entries versus missing data, and exact coordinate frames.
The current duplicate-coordinate refusal and empty-versus-omitted cohort semantics
are not relaxed. Host authorities own identity/reuse, native result publication,
QMX persistence, project preview and annotation undo. A plugin must not duplicate
those state machines or use Python to consolidate its results.

### Project exchange, exports and long-tail intake

| ID | Area | Plan |
| --- | --- | --- |
| X01 | Native QPX projects and existing QuPath project/GeoJSON exchange | Reuse delivered project preview/save-new authorities. Any extra project format needs typed project-preview/publication, not an annotation/image parser. No claim of arbitrary `.qpdata` or Java-extension serialization support |
| X02 | Image exports: PNG/JPEG/TIFF/OME-TIFF/OME-Zarr | Inventory existing export services first. Future writer plugin per target profile must declare precision/alpha/color/loss/axes/metadata and refuse unsupported serialization; reader support does not qualify writing |
| X03 | Matrix/annotation exports | Preserve current QMX/GeoJSON/measurement CSV behavior. New MTX/H5AD/Loom/ROI/XML/label writers need their own exact mapping, losses and protected atomic publication checks |
| X04 | OMERO, DICOMweb, IIIF, HTTP/object stores/S3 | Remote-provider roadmap only. Explicit native API, rights/authentication, object version/content identity, availability/cancel and offline behavior precede plugins; these are transports/services, not file formats |
| X05 | Camera RAW/DNG, AVI/MOV/video, PDF/EPS/full SVG, spreadsheets/R/Seurat serialized objects and remaining Bio-Formats entries | Unreviewed lower-priority intake. Identify native decoder/schema/terms and a representable research output before promotion. Demosaicing, frame timing, vector rasterization or model projection must be explicit; no executable object deserialization or language-runtime fallback is selected |

### Candidate source evidence and risk decisions

Checked2026-11-14. These are planning references, not newly admitted dependency
pins. Before build selection, pin the actual revision/archive and inspect all
selected members/dependencies/fixtures in the existing material inventory.

| Candidate | Primary evidence | Decision carried into a packet |
| --- | --- | --- |
| libwebp | [Exact1.6.0 member lock](../plugins/webp/libwebp-source-lock.json), commit4fa21912, BSD-3-Clause with PATENTS/AUTHORS | Selected generic decoder/common DSP/utilities only;63 exact upstream source/notice members, no upstream image/test asset or encoder/mux/demux; [local receipt](benchmarks/webp-reader-plugin/README.md) |
| OpenJPEG | [License](https://raw.githubusercontent.com/uclouvain/openjpeg/master/LICENSE), [upstream maintenance statement](https://github.com/uclouvain/openjpeg) | BSD-2-Clause code; repository declares itself unmaintained. Compare maintained native alternatives and ownership of fixes before choosing a new dependency; existing OpenSlide evidence stays source-scoped |
| libjxl | [BSD-3-Clause text](https://raw.githubusercontent.com/libjxl/libjxl/main/LICENSE) | Inspect every selected third-party component and sample asset, not only the top-level license |
| libavif | [License and third-party notices](https://raw.githubusercontent.com/AOMediaCodec/libavif/main/LICENSE) | Core terms do not select/admit an AV1 codec stack or all format variants |
| libics | [Reference implementation and LGPL2.1 terms](https://svi-opensource.github.io/libics/) | Candidate for open ICS; packaging must satisfy the exact linked library's source/relink/distribution obligations |
| NRRD / native NrrdIO | [Format](https://teem.sourceforge.net/nrrd/format.html), [selected implementation license reference](https://raw.githubusercontent.com/InsightSoftwareConsortium/ITK/master/Modules/ThirdParty/NrrdIO/src/NrrdIO/LICENSE) | Native subset candidate; affine/dtype/compression semantics still need host representation and exact source selection |
| NIfTI / FITS | [NIfTI definitions](https://nifti.nimh.nih.gov/nifti-1/), [FITS standard](https://fits.gsfc.nasa.gov/fits_standard.html) | Published formats; not a blanket admission of a particular native library, sample collection or all coordinate conventions |
| NGFF0.5 / N5 | [NGFF specification](https://ngff.openmicroscopy.org/0.5/), [N5 specification repository](https://github.com/saalfeldlab/n5) | Distinct version/store contracts. Implement native access; Python/Java reference tools may only be independent development oracles |
| Loom / ImageJ ROI | [Loom layout](https://linnarssonlab.org/loompy/format/), [ROI header reference](https://imagej.net/ij/developer/source/ij/io/RoiDecoder.java.html) | Review reusable schema/code terms before a native parser; do not import reference runtimes or datasets |
| libCZI | [Native library](https://github.com/ZEISS/libczi), [LGPL3-or-later and bundled notices](https://raw.githubusercontent.com/ZEISS/libczi/main/COPYING) | Native reader availability does not resolve the new-fluorescence open-format restriction; no commercial-license branch/closed SDK is selected |

### Delivery sequence and parallel ownership

Priority is a recommended order within the appropriate family, not an instruction
to implement every row. Start each packet with an intended input/output workflow,
an admitted reference and a demonstrated gap. These planning rows alone do not
reopen the frozen native runtime or select a new host build.

| Packet | Deliverable / dependency | Decisive exit |
| --- | --- | --- |
| FP00 | Delivered dated per-reader census:178 upstream entries,57 roadmap groups, actual OpenSlide4.0.1/source/codec facts and three independently validated ordinary-input gaps | All source rows retained/classified; duplicate aliases and upstream support separated from qualified profiles; deterministic offline rebuild and existing source/notice scope pass. See [census](format-plugin-census.json) |
| FP01 | Three demonstrated ordinary-input gaps delivered: optional [PNM/PAM](../plugins/pnm/README.md), [BMP](../plugins/bmp/README.md) and static lossless [WebP](../plugins/webp/README.md) | PNM79/36/nine routes, BMP62/40/eight and WebP19/28/eight pass, with exact source/sample identity and20/20/90-member offline source/package/admission/removal evidence. Other raster variants are unselected; JPEG2000 still needs maintenance review |
| FP02 | Scientific image/volume and fluorescence file-set profiles delivered: [NRRD](../plugins/nrrd/README.md) and [ICS](../plugins/ics/README.md) | NRRD50/56/nine routes/20 members and ICS66/60/11 routes/22 members pass, with exact stored bits/C/Z/window/calibration, guarded source identity, independent offline builds and admitted package/removal evidence; no hidden resampling or new host |
| FP03 | First selected directory-store or missing metadata/view outcome; design only the additive boundary it needs | Contract/limits/identity/lifetimes and old-plugin/project compatibility tests settled before one matching native host/SDK candidate build; preserve old frozen runtime |
| FP04 | Delivered external counts TSV and point TSV importers with typed family C ABIs and separate native host/SDK | [Receipt](benchmarks/format-family-plugins/README.md):76 workflow commands,18 positive/50 negative profiles and23 selected native suites; exact UInt64/IDs/position joins, guarded oneundo annotation publication, schema4/per-object receipts,32-member admitted offline source/kit/package, independent builds and seven-image-plugin coexistence |
| FP05 | Selected store/family plugins after FP03/FP04 respectively | NGFF/N5/AnnData-Zarr or Loom/table/ROI packets qualify only the named version/profile. No automatic all-store/all-importer campaign |
| FP06 | Selected advanced raster, vendor brightfield, writer or remote-provider outcome | Close actual admission/representation gaps first, then its own scientific and package acceptance; no prerequisite built-in rewrite or broad parity qualification |

Dependency graph: FP00 precedes each selection. FP01 and representable FP02 are
independent; FP03 is needed only for a missing store/view contract; FP04 only for
the first external typed-family importer. FP05 depends on the relevant boundary,
not on every previous packet. Writer/remote work requires its own contract in
FP06. Avoid building three speculative ABIs as one prerequisite platform.

For a selected packet, root owns shared ABI/registry/identity changes, inventory
merge, current card and integration. One worker owns provider/decoder files;
another owns independent fixtures/oracles and native SDK/ABI tests; license/source
intake can run concurrently when disjoint. For a boundary packet, the contract
worker owns native host interfaces and coordinates the provider worker; do not
let both edit the same headers. Workers are not alone and must preserve others'
edits. Parallelize independent research/code/tests, then freeze interfaces and
dependency configuration before one combined cached build. Keep one user outcome
in progress and one active integration/cache/runtime; document-only work has no
native build. No new UI lane, wheel, merge/push or release publication is implied.

### Per-format card and acceptance

Add the selected packet to [current-task.md](current-task.md), reusing this
roadmap and existing receipts. Record these fields once; no second capability or
licensing authority is introduced:

1. Format/schema revision, magic/detection, aliases, exact encoding subset and
   native provider/semantic-contract version; distinguish read/display/raw,
   import/export and actual platform/build availability.
2. Stored versus decoded/physical values, dtypes/channels/palette/alpha, color
   transforms/loss, axes/planes/series, units/affines/time and pyramid lattice.
   Unknown calibration remains unknown; unsupported representation refuses.
3. Native options/view selection and every primary/companion/store input;
   file/object/version/hash and exact implementation/dependency identity; budgets,
   cancellation, currentness, refusal and atomic publication boundaries. Large
   slides/arrays require bounded region/block access; do not densify an entire
   source to claim scalable input. Publish full-decode limits for small rasters.
4. Schema reuse, code, codec/filter, fixture and derived-material rights separately;
   exact source/member hashes, licenses/notices/source obligations, maintenance and
   distribution scope. Admission of one component never substitutes for the rest.
5. Intended workflow, independently derived expectations, necessary build/check,
   retained artifacts/failed attempts and explicit handoff/remaining scope.

The decisive validation set is selected from changed behavior:

- Analytic/literal anchors and an independent decoder/schema oracle; compare
  lossy inputs against declared decoded results, not precompression originals.
  Preserve raw integer bits, large counts, exact IDs and persisted identity.
- Correct channels/color/alpha/orientation, scale/units/coordinate frame, crop,
  seams/pyramid lattice, padding/invalid samples, selected planes/series and losses.
  No hidden fallback, duplicate summing, inferred units or unreported shape repair.
- Native trust-boundary malformed/truncation/warning/overflow tests, actual decoded
  allocation bounds, file/member/chunk limits, Unicode paths and before/during
  cancellation; failures expose no partial handle/project/output/undo transaction.
- Shared native open/import or export, save/reopen/replay/source changes and
  equal-pixels-but-changed-implementation refusal. Offline provider tests must use
  valid requests and check the actual unavailable-provider error.
- Unchanged built-ins and coexisting plugins; disabled/missing provider and
  bad dependency/module identity. New host boundaries get separate candidate/SDK
  identities and old v1/project compatibility tests; never relabel frozen evidence.
- Offline source-kit build, complete component licenses/source, existing stage/ZIP
  material guards and install/use/removal preserving user files. Packaging/docs
  repairs reuse qualified module bytes; do not rebuild codecs to edit a notice.

Follow the [validation policy](api-delivery-plan.md#validation-and-build-policy):
affected tests and compatible receipts first, full platform/corpus campaigns only
for a named concrete risk or release scope. Stop when the selected packet meets
its declared scientific/deployment conditions. Remaining unreviewed/excluded or
unselected rows stay explicit and do not automatically start another format.

### Initial planning verification

The initial roadmap handoff was documentation only. Actual source inspection distinguishes v1
file-set/RGBA/raw limits, fixed family catalogs and existing typed authorities.
Primary references above were checked2026-11-14; moving upstream links are
research evidence, not release pins. No candidate dependency, fixture, decoder,
ABI, binary package or material-inventory admission was added in this pass.

The Claude peer-plan skill's referenced helper files were absent. Its read-only
CLI fallback first failed bare-mode authentication; a normal authenticated
Fable/max attempt then returned usage-limit429. Both results are retained under
`build/format-plugin-coverage/all-formats-peer*`. There is no independent Claude
endorsement; no alternate model or repeated review campaign was run. Local link,
diff and source-preservation checks own this planning handoff.

Local verification passes for57 distinct coverage entries, seven dependency-aware
packets,164 local links and28 Markdown anchors. The17 delivered PNG/JPEG product
source inputs match their qualified hashes; native root sources are unchanged.
The broad anchor check repaired two existing current-card links to historical UI
headings now named Previous, without changing UI code. Evidence is retained in
`build/format-plugin-coverage/all-formats-plan-results.json`. This closes the plan,
not decoder implementation. FP00 is now delivered above; FP01 is tracked in the
current task with its own changed-input validation and candidate identity.
