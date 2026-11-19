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
[ICS fluorescence](benchmarks/ics-reader-plugin/README.md) profiles are delivered.
The typed [matrix/point-annotation plugins](benchmarks/format-family-plugins/README.md)
and [QOI](benchmarks/qoi-reader-plugin/README.md) have since passed their local
native handoffs. The active full qualified-catalogue goal includes formats and
application/integration gaps; continue one concrete packet at a time under the
owner scope below.

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

Owner selection2026-11-16: the active goal is to close all remaining qualified
catalogue work, including eligible formats and application/integration gaps.
The [current card](current-task.md) owns one implementation packet at a time;
finishing one packet does not complete or narrow that goal. The tables below
remain the inventory. Investigate candidates and missing contracts under their
actual admission rules; unknown terms are unresolved, not delivered/excluded by
default. The owner's open/commercial-use, native-authority and scientific-fidelity
requirements still apply. The dated178-entry census remains its original evidence.

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
| Matrix importer plugins | Delivered [matrix ABI1 and ABI2](format-family-plugins.md); [ABI3 store profile](benchmarks/anndata-zarr/README.md) delivered locally | Bounded UInt64 counts and finite Float64 continuous values use native QMX staging, exact catalogs and position joins. ABI3 tracks complete directory content and filesystem observations separately. |
| Annotation importer plugins | Delivered point ABI1 and [geometry ABI2](benchmarks/annotation-geometry/README.md), sharing native geometry/preview/apply/undo authorities | Bounded image-pixel QRO geometry, exact source IDs/class labels and C/Z/T enter host-owned staging. Native code validates/maps/captures/commits one transaction. Additional properties, hierarchy, losses and exporter contracts remain explicit extensions; no mutable Document crosses the ABI |
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
| I05 | WebP | [Native lossless and lossy stills with selected EXIF orientation delivered locally](benchmarks/webp-reader-plugin/README.md#exif-orientation-in-120) | Simple VP8L version 0 preserves exact RGBA and hidden RGB. Simple VP8 and the alpha-only VP8X/ALPH/VP8 layout expose the pinned decoder's straight RGBA8. EXIF stills apply exact orientation 1–8 to dimensions, display and raw coordinates. Other EXIF tags, color management and animation remain separate contracts |
| I06 | JPEG 2000 JP2/J2K | [Baseline and tiled grayscale plus lossless JP2 sRGB profiles delivered locally](../plugins/jpeg2000/README.md) | Version 1.13.0 reads up to 4096 zero-origin tiles with up to 255 explicitly counted nonempty parts per tile, 1 to 16 layers and any single Part 1 packet order using default or explicit precincts. Explicit grids are limited to 2,097,152 packets. Declared SOP and EPH packet markers are validated. Advisory PLT lengths must cover the tile body. The private decoder refuses a closing marker before all declared packets are consumed. The reversible profile retains exact unsigned 8- to 16-bit codes in UInt8/UInt16 and signed grayscale codes in Int8/Int16. Enumerated JP2 gray and sRGB, with optional explicitly declared straight alpha, preserve exact packed raw samples and separate RGBA8 display. JP2 capture resolution supplies per-axis calibration. Irreversible UInt8 grayscale with explicit scalar quantization exposes reconstructed values with the irreversible metadata label and validated one-code agreement on named fixtures. Other lossy profiles, signed color/alpha, other color models, raw multichannel J2K, palettes, other packet metadata, progression changes, unknown part counts and tile coding overrides, boxes and exposed resolution levels, JPX and HTJ2K are separate profiles |
| I07 | GIF/APNG and animated WebP | [Native static GIF](../plugins/gif/README.md), [selected APNG frame](../plugins/apng/README.md) and [selected animated WebP frame](../plugins/webp-animation/README.md) delivered locally | GIF87a/89a full-canvas indexed stills preserve exact source indices and the rendered global/local palette, interlace and binary transparency. APNG and animated WebP open bounded composited frames with explicit blending/disposal and saved frame identity. Playback and additional sample or metadata profiles remain separate |
| I08 | TGA, PCX, QOI | [TGA](../plugins/tga/README.md), [PCX](../plugins/pcx/README.md) and [QOI](benchmarks/qoi-reader-plugin/README.md) delivered locally | TGA preserves exact plain/RLE RGB/RGBA, UInt8 gray and palette indices. PCX preserves version-5 RLE eight-bit planar RGB or VGA indices and version-2/5 four-bitplane indices with the RGB header palette. Other PCX layouts/versions/DCX and additional TGA alpha/metadata remain separate. QOI covers all six operations, exact RGBA8/alpha and separate linear display |
| I09 | JPEG XL | [Selected native integer still profile delivered locally](../plugins/jpegxl/README.md) | Original-profile 1–16-bit grayscale/RGB with same-depth straight alpha. Exact native codes, SDR structured sRGB or linear sRGB, bare codestream, complete codestream box or bounded indexed fragments in 1.1.0. Version 1.2.0 normalizes all eight orientations. Version 1.3.0 adds explicit opt-in for reconstructed sRGB codes from structured SDR sRGB XYB stills. ICC, other XYB color, HDR, float samples, additional boxes and animation remain separate |
| I10 | AVIF | [Selected SDR still profile delivered locally](../plugins/avif/README.md) | Plugin 1.0.0 preserves full-range 8/10/12-bit grayscale and identity-coded 4:4:4 RGB with optional straight alpha. An explicit `allow_yuv` option exposes reconstructed sRGB from selected matrices. The native project retains the profile and option identity. Sequence, HDR, ICC and geometric transforms require separate profiles |
| I11 | HEIF/HEIC | Unreviewed exact native stack and format/codec obligations | A libheif wrapper does not admit every codec or establish all commercial-use rights; no closed decoder fallback |
| I12 | OpenEXR, Radiance HDR | [Native OpenEXR RGB and RGBA scanline](../plugins/openexr/README.md) and [Radiance RGBE](../plugins/rgbe/README.md) profiles delivered locally | OpenEXR 1.1.0 preserves finite Float32 and exact HALF expansions for equal zero-origin windows with NONE, ZIPS or ZIP, including associated alpha and zero-alpha emission. RGBE 1.0.0 preserves linear Float32 bin centers for top-left flat or scanline-RLE input. Both define their preview separately. Extra OpenEXR channels/windows/parts and Radiance XYZE, legacy RLE or color/geometry overrides require separate profiles |
| I13 | TIFF/BigTIFF/OME-TIFF | Reuse the delivered quantitative profile. [Default `TiffData` mapping with trailing pages is locally verified](current-task.md#delivered-locally---read-default-ome-tiff-plane-mappings-with-extra-tiff-pages) | Inferred OME mappings ignore IFDs beyond the declared lattice. Explicit counts and incomplete mappings retain strict checks. Complete zero-origin Plane DeltaT and physically calibrated Plane PositionZ tables can supply regular intervals when Pixels does not declare them. Compound inference requires agreement among distinct resolved metadata documents. Plain TIFF normalizes all eight orientations into one raw/display frame with matching X/Y calibration and coherent SubIFDs. Lossless plain 4-, 8- and 16-bit palette TIFF preserves exact indices and uses each level's ColorMap for its default preview. The 4-bit path unpacks MSB-first rows to UInt8 values 0–15. Selected TIFF and BigTIFF directories reject a stored palette declaration whose ColorMap was lost during decoder recovery. Plain bilevel TIFF exposes exact UInt8 0/1 with both fill orders and honors display polarity. CCITT Groups 3 and 4 are locally verified. Plain lossless 4-bit grayscale exposes exact UInt8 codes 0–15 with most-significant-bit fill order and photometric display polarity. OME-TIFF requires top-left. Inventory other dtypes/layouts/axes/series/compressions before adding a missing profile |
| I14 | ImageJ/LSM/STK/QPTIFF and other TIFF metadata dialects | Reuse pixel decoder; each metadata/schema variant Unreviewed until inspected | Generic TIFF pixels do not qualify vendor channels, Z/T, registration, labels or calibration; apply open-format policy to the intended workflow |
| I15 | ICS/IDS v1/v2 | [Native raw and gzip profiles delivered locally](benchmarks/ics-reader-plugin/README.md#gzip-named-companions-in-150) | Guarded ICS1/IDS with a same-base .ids.gz fallback for declared gzip, inline ICS2 and a single same-directory ICS2 companion with explicit byte offset. Each explicitly named X/Y/Z/T/channel axis can appear in any storage order. Reads preserve calibration and exact UInt8/UInt16/Int16/Float32 samples in packed HWC at the requested Z and T. External raw selects the declared raster range. External gzip requires one exact member through EOF. Other axis meanings and encodings remain separate profiles |
| I16 | NRRD/NHDR | [Attached and detached scalar profiles delivered locally](benchmarks/nrrd-reader-plugin/README.md#spaced-single-and-list-companion-names-in-1140) | NRRD0003–0005 raw, one gzip or bzip2 stream per payload, ASCII or hex. Signed and unsigned 8/16/32/64-bit integers and Float32/Float64 in XY/XYZ and explicit XYT/XYZT. Version 3 supports one header-relative detached file. Versions 4 and 5 additionally support a positive diagonal local basis with zero or omitted origin and up to 63 guarded same-directory companions. Plugin 1.14.0 supports interior spaces and a single `./` prefix in single or LIST payload declarations. Earlier versions, other axis orders, filename conversions, concatenated compression and general affine/frame profiles remain open |
| I17 | NIfTI-1/2, Analyze 7.5 | [Native single-file and paired scalar NIfTI-1 and NIfTI-2 profiles delivered locally](benchmarks/nifti-reader-plugin/README.md#scalar-time-series-in-140) | `.nii`, `.nii.gz`, and independently plain or gzip `.hdr`/`.img` members preserve exact XY, XYZ and XYZT samples in both byte orders. The spatial contract is positive axis-aligned spacing and zero origin. Raw time selection preserves stored order and optional seconds calibration with zero time origin. NIfTI-2 preserves binary64 calibration and checks wide dimensions and offsets. Pairs guard the actual companion identity. Version 1.5.0 also accepts bounded code-6 ASCII comment extensions without changing voxel interpretation. Analyze, other extension codes, ranks above four, frequency axes, general affine frames and nonidentity intensity scaling remain separate profiles |
| I18 | FITS | [Native plain, externally gzip-compressed and selected GZIP_1/GZIP_2/RICE_1 tiled profiles delivered locally](benchmarks/fits-reader-plugin/README.md#lossless-tiled-volumes-in-190) | XY/XYZ primary and IMAGE HDUs preserve all six scalar BITPIX encodings, exact pseudo-unsigned offsets and raw validity. Bounded bundles retain physical HDU indices and verify canonical CHECKSUM or DATASUM. Version 1.9.0 reads XY/XYZ integer and lossless Float32/Float64 GZIP tiles plus 8/16/32-bit RICE_1 tiles, including partial Z tiles. Table import, general scaling, WCS and other compressed profiles remain separate |
| I19 | MRC/CCP4 | [Native plain, gzip and bzip2 scalar MRC2014 images, volumes and selected stacks delivered locally](benchmarks/mrc-reader-plugin/README.md#packed-mode-101-in-150) | Both byte orders, exact Int8/Int16/Float32/UInt16 modes, binary16 mode 12 widened to Float32, and even-column packed mode 101 decoded as exact UInt8. Single volumes support MAPC/MAPR/MAPS permutations. Image stacks and P1 volume stacks with sections along Z expose individually selected series. Nonzero origins, extended headers, other modes and legacy CCP4 variants remain separate |
| I20 | MetaImage MHA/MHD, VTK VTI, ENVI | [Native MetaImage](../plugins/metaimage/README.md), [native VTI](../plugins/vti/README.md) and [uncalibrated ENVI Standard](../plugins/envi/README.md) profiles delivered locally | MetaImage 1.7.0 reads binary XY/XYZ with ordered components, signed and unsigned 8/16/32/64-bit integers and Float32/64, identity geometry and ITK orientation tags. It reads attached and detached rasters and ordered XYZ LIST or numbered decimal patterns with up to 63 guarded XY slices, each plain or independently zlib-compressed. Same-directory companions can contain interior ASCII spaces. VTI 1.7.0 selects a named point array from each piece and preserves its ordered components. Bounded serial pieces support ASCII, inline base64 and appended base64/raw with optional zlib, LZ4 or CRC32 XZ. Selected coverage must be complete and overlapping tuples must agree. Nonunit VTI spacing requires explicit units retained in the project. VTI 1.8.0 accepts signed extent indices when the first stored point is physically at zero. ENVI 1.1.0 retains ordered native bands, BIP/BIL/BSQ, both byte orders and a guarded payload offset. It accepts same-stem `.img` by default, with explicit `.bin`, `.dat` or appended `.hdr` companion layouts saved with the project. Version 1.2.0 honors one finite typed data ignore value. Version 1.3.0 reads declared grayscale or RGB default bands without changing raw order or sample values. Version 1.4.0 retains exact Int64/UInt64 samples and compares integer ignore values at stored precision. Version 1.5.0 adds explicit same-stem RAW, BIL, BIP and BSQ companion selections. Georeferencing and spectral calibration remain separate profiles |
| I21 | Explicit raw binary plus metadata | Candidate only with a complete named sidecar schema | All dimensions/strides/endian/type/offset/units supplied and hashed; no suffix-based scientific guesses |
| I22 | OME-Zarr NGFF0.4/Zarr2 | Delivered local null/zlib/gzip subset; separate [OMERO-linked raw-block adapter](omero-integration.md) and [A08 integral-label provider](benchmarks/ngff-labels/README.md) | Each route keeps its own source/decoder contract. A08 validates integer/null label fills before opening. The separate [I22 native source candidate](benchmarks/ngff-fill/README.md) now rejects precision-losing image fills while preserving exact valid pixels/source/persistence identities; frozen delivery binaries remain unchanged. Other codecs/collections/versions need their own coverage |
| I23 | OME-Zarr NGFF0.5/Zarr3, shards and additional open codecs | [Regular and indexed-shard profiles delivered in local Windows plugin handoffs](benchmarks/ngff05/README.md) | Local UInt8, Int8, UInt16, Int16, UInt32, Int32, UInt64, Int64, Float32 and Float64 bytes with optional gzip, Zstandard or Blosc (LZ4, LZ4HC, BloscLZ, Zstandard, zlib and Snappy), composed transpose codecs, little- and big-endian shard indexes at either end with optional CRC32C, and CRC32C for regular chunks, inner chunks and whole serialized shards within the scan limits. The reader preserves calibrated axes and exact raw export and honors linear OMERO contrast windows and colors on the host plane. Other display models, dtypes, codecs, index encodings and layouts require their own evidence |
| I24 | N5 and BigDataViewer XML/HDF5/N5 | [TensorStore N5 datasets](benchmarks/n5/README.md), [aligned BDV N5 projects](benchmarks/bdv-n5/README.md) and [aligned BDV HDF5 projects](benchmarks/bdv-hdf5/README.md) delivered locally | Calibrated UInt16 raw/RGBA, sparse blocks, setup-ID channel order and equal integer XY pyramid scale with unchanged Z. Local HDF5 partitions use checked XML maps and master links. Shared acquisition roles retain zero origin and rotation. N5 1.3.0 and HDF5 1.4.0 read available channels in projects with explicit missing views. Distinct tiles or angles and general affine mapping remain open |
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
| W02 | DICOM whole-slide microscopy | Local Windows OpenSlide 4.0.1 evidence covers [sparse JPEG Baseline](current-task.md#verified-locally---sparse-dicom-whole-slide-viewing), [full-tile RGB8 and JPEG 2000](current-task.md#verified-locally---dicom-jpeg-2000-and-uncompressed-rgb-transfer-syntaxes), and [two-instance JPEG 2000 concatenation](current-task.md#verified-locally---dicom-concatenation-and-a-companion-identity-gap). The [optional DICOM WSI provider](../plugins/dicom_wsi/README.md) binds selected same-directory companions to saved projects | Keep each series together. The optional route refuses a same-geometry companion replacement until explicit review and adoption. Broader decoder resource bounds, DICOM profiles and SEG/SR remain open. See the [upstream WSI contract](https://openslide.org/formats/dicom/). |
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
| M02 | Matrix Market + QPX/10x TSV | Delivered declared native import, including an optional single-member gzip transport | Preserve exact UInt64 counts, sparse zeros, feature/observation IDs, explicit catalog/coordinate options and encoded file identities. Other Matrix Market variants require separate scope. |
| M03 | AnnData/H5AD | Delivered declared native profile | Reuse explicit dataset keys, dense/CSR/CSC and value/coordinate domain; additional encodings need a new profile |
| M04 | Dense CSV/TSV matrices and coordinate tables | [ABI1 counts TSV](format-family-plugins.md#counts-and-positions), [typed CSV](../plugins/csv_matrix/README.md) and [typed TSV](../plugins/tsv_matrix/README.md) delivered locally | The ABI2 providers share exact ordered IDs, UInt64 counts or finite binary64 continuous values and complete native position joins. Both use explicit column selection and accept an optional UTF-8 BOM. CSV supports quoted fields and TSV preserves literal quotes. Other dialects and multiline fields remain conditional |
| M05 | Loom HDF5 | [Selected native/CLI/SDK profile delivered locally](benchmarks/loom/README.md), including 284 admitted source/package/ZIP members and 12 installed/removal checks | Optional matrix ABI2, explicit Loom3 matrix/layer and row/column IDs, exact UInt64 counts or finite Float64 continuous values, complete native coordinate join and schema5 QMX/project ownership. Original axes/IDs, signed zero and exact integer conversion are preserved; graphs/unselected metadata/layers are explicitly omitted. Bounded streamed HDF5/filter checks; no general Loom/model/platform claim |
| M06 | 10x feature-barcode HDF5 | Deferred after schema-terms review. Use admitted M02 Matrix Market/TSV or M03 AnnData exports | The [published layout](https://www.10xgenomics.com/support/software/cell-ranger/latest/analysis/cr-outputs-h5-matrices) and [10x site terms](https://www.10xgenomics.com/legal/terms-of-use) do not provide the reusable specification terms required by this project. [DropletUtils](https://bioconductor.org/packages/release/bioc/html/DropletUtils.html) is an independently licensed producer. Its GPL-3 license covers that implementation. Reopen M06 when the exact schema has qualifying terms. |
| M07 | AnnData Zarr | [Native ABI4/schema 6 Windows workflow](benchmarks/anndata-zarr/README.md) delivered locally, including Blosc/LZ4, BloscLZ and sparse QMX in provider 1.2.0 | Local Zarr 2 dense/CSR/CSC, explicit catalog/domain/frame, separate observed and complete content identities, protected QMX and owned project workflow. CSR/CSC writes only stored entries within axis and workspace limits |
| M08 | Arrow/Feather/Parquet | [Arrow points](../plugins/arrow_points/README.md), [Arrow matrices](../plugins/arrow_matrix/README.md), [Parquet points](../plugins/parquet_points/README.md), [Parquet matrices](../plugins/parquet_matrix/README.md) and [GeoParquet annotations](../plugins/geoparquet/README.md) delivered locally | Both point routes use explicit level-zero X/Y columns and retain exact integer source IDs. Arrow Points 1.6.0 and Arrow Matrix 1.3.0 preserve exact integer IDs and support dictionary deltas for UTF-8, integer, Float32 and Float64 values through the shared decoder. Parquet points 1.1.0 shares its ID decoder with GeoParquet 1.3.1. GeoParquet 1.0/1.1 supports XY WKB and the six native separated GeoArrow encodings under 1.1, with explicit null CRS or a matching projected EPSG declaration and supplied affine. GeoParquet 1.3.0 also retains exact signed and unsigned integer feature IDs in both geometry routes. Arrow IPC V5/Feather v2 supports uncompressed, LZ4 and Zstandard buffers. Flat Parquet supports uncompressed, Snappy and Zstandard pages with checksum verification when present. Matrix routes preserve ordered axes, exact UInt64 or finite Float64, native position joins and owned QMX. Both matrix providers join integer observation IDs using their exact decimal spelling. Geographic reprojection and further table layouts require separate profiles |
| M09 | MuData H5MU, NPY/NPZ | Native NumPy NPZ [counts](../plugins/counts_npz/README.md), [continuous](../plugins/continuous_npz/README.md) and [selected H5MU modality](../plugins/mudata/README.md) profiles delivered locally | All three preserve explicitly selected axes and typed exact values through the native position join and QMX writer. H5MU 1.1.0 supports dense, CSR and CSC with ABI5 stored-entry publication for sparse matrices. The exact 144-million-cell example persists only three entries. Multimodal composition and additional NumPy schemas remain candidates |
| M10 | Visium, Xenium, CosMx, MERSCOPE and other platform bundles | Reuse admitted generic open exports only; proprietary schema/bundle compatibility is not admitted by MTX/CSV/HDF5/Parquet storage | Exact stable-ID association, units/coordinate frames and supplied registration; no invented spatial alignment or platform/biological acceptance |
| A01 | GeoJSON; measurements CSV | Delivered GeoJSON import/export, including [native project CLI exchange](format-family-plugins.md#native-geojson-project-exchange) and [native saved object CSV export](format-family-plugins.md#native-saved-object-measurements-export) | Preserve image-pixel frame and native loss/class/plane/property/undo behavior. CSV supports export only |
| A02 | ImageJ ROI and ROI ZIP | [Native selected profile delivered locally](benchmarks/imagej-roi/README.md#imagej-rotated-rectangle-import-in-110): optional ROI217–229 and stored/Deflate ZIP provider, ABI3 source facts/names/planes, bounded ZIP64 | Literal basic/subpixel geometry, linear even-odd composites and freehand subtype 5 rotated rectangles, names/styles/groups/properties, individual point counters/positions, shared native preview/apply/project/undo/CLI/SDK. Curves, splines, rounded rectangles, rotated ellipses, text/image/arrow subtypes and conflicting positive flat/CZT point constraints refuse; no Java runtime or general ImageJ mask/measurement equivalence |
| A03 | WKT/WKB | [Selected native image-pixel XY import delivered locally](benchmarks/wkt-wkb/README.md): optional ABI3 WKT/raw/hex WKB provider, seven basic2D types and retained collection membership | Explicit selected image-pixel units/native frame; both/mixed binary byte orders, strict closure, exact binary64 and source IDs/facts. No inferred geographic CRS/plane/class, dimension drop or repair; EMPTY/Z/M/SRID/extension profiles refuse. Shares producer/value support with the separately identified ImageJ candidate |
| A04 | OME ROI XML | [Selected vector and bit-mask profiles delivered locally](benchmarks/ome-roi/README.md). Provider 1.3.0 supports all eight grid-preserving XY orientations and integral translation. BinData supports none, zlib and bzip2 | Seven vector primitives, explicit multi-image/unbound selection, exact IDs/planes/binary64, source styles and structured annotations remain supported. Image-bound masks preserve exact coverage, holes and disconnected parts. Affines that require mask resampling, general ellipse transforms and unknown schemas remain separate profiles |
| A05 | ASAP/Aperio XML and vendor annotation dialects | [Selected native ASAP XML profile delivered locally](benchmarks/asap-xml/README.md); Aperio and other vendor dialects remain unreviewed for actual schema terms | ASAP Dot/PointSet/Polygon/canonical Rectangle/two-endpoint Measurement, explicit source float32/order/class/group/root-color semantics and retained metadata over the shared bounded XML/native import authorities. No inferred holes/planes/registration or spline flattening. Open ASAP source does not admit a closed vendor dialect or upstream restricted datasets |
| A06 | Explicit point/box/polygon CSV/TSV | [Point TSV](format-family-plugins.md) and [pixel geometry TSV](benchmarks/annotation-geometry/README.md) delivered; other CSV/TSV dialects remain conditional | Geometry profile adds rectangle/ellipse/points/polyline/polygon/multipolygon, holes, exact source/geometry/plane/class identity and guarded native preview/apply/undo/persistence. No heuristic columns, inferred frame, clamp, topology repair or silent row loss |
| A07 | COCO JSON polygons/RLE | [Selected native instance profile delivered locally](benchmarks/coco/README.md): optional ABI5 polygon unions and compressed/uncompressed RLE | Exact whole-pixel coverage, full image canvas/frame, category/instance IDs, crowd, holes/parts and source metadata through shared native preview/apply/undo/project/CLI/SDK. Explicit image/category mapping; bounded schema/empty/malformed/unsupported profiles refuse. BSD API code does not admit any COCO dataset/image/model assets |
| A08 | TIFF/PNG/NGFF label masks | [Native/CLI unsigned TIFF, grayscale PNG and explicitly selected indexed PNG labels](label-analysis.md#offline-label-masks), plus [optional NGFF0.4/Zarr2 integral-label plugin](benchmarks/ngff-labels/README.md), delivered locally | NGFF preserves all eight integral types, exact signed/unsigned source IDs, original property text/RGBA, explicit planes/reference/crop, original-raw measurements, publication/persistence/replay and QPLM1/CSV. Whole-store guards and exact decoder identity persist. No inferred resize/registration/classes or model execution. TIFF/PNG/QPL1 identities stay exact. Indexed PNG uses stored palette indices as label IDs, with zero as background; RGB and explicit alpha-channel PNG masks refuse |
| A09 | SVG subset, GeoPackage, NGFF tables/shapes | [SVG](../plugins/svg_annotations/README.md) and [GeoPackage](../plugins/geopackage/README.md) image-pixel profiles delivered locally. GeoPackage 1.1.0 adds projected EPSG layers with an explicitly supplied affine. NGFF tables/shapes remain unreviewed | SVG 1.1.0 preserves supported shapes, child transforms and even-odd rings with exact ABI6 canvas matching. GeoPackage uses the shared WKB parser, preserves original feature IDs and local SRS keys, checks exact optional source-space XY envelopes, and retains explicit registration coefficients. Full SVG rendering and geographic reprojection remain separate |
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
| X03 | Matrix/annotation exports | Preserve QMX, GeoJSON and measurements CSV. [Saved matrix exports](format-family-plugins.md) include dense CSV projections and a sparse Matrix Market ZIP for exact integer counts and axis catalogs, with new-only publication. H5AD/Loom/ROI/XML/label writers need exact mapping, declared losses and protected atomic publication checks |
| X04 | OMERO, DICOMweb, IIIF, HTTP/object stores/S3 | [Native OMERO metadata/previews, explicitly linked NGFF raw blocks and ROI/result files](omero-integration.md) are locally verified in a separate optional service. Saved raw bundles now support offline verification and exact NPY extraction. Raw Blosc bytes reuse an optional existing-ABI provider; native Zarr/geometry authorities retain interpretation. Public raw/point exchange pass; IDR live masks, private/write-back OMERO and other remote providers remain separate selections. These services do not extend the path-only file ABI |
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
| Loom / ImageJ ROI | [Loom layout](https://linnarssonlab.org/loompy/format/), [Loom exact source lock](../plugins/loom/source-lock.json), [ROI header reference](https://imagej.net/ij/developer/source/ij/io/RoiDecoder.java.html) | Selected Loom3 native profile has BSD-2-Clause reference provenance and exact frozen HDF5/zlib dependencies; the ImageJ profile has its separate NIH/public-domain source evidence. Neither admits reference runtimes, datasets or restricted contents. Each retains its own native/material/deployment handoff |
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
| FP05 | [Native geometry ABI2/TSV](benchmarks/annotation-geometry/README.md), [ImageJ ROI/ZIP](benchmarks/imagej-roi/README.md), [selected WKT/WKB](benchmarks/wkt-wkb/README.md), [OME ROI XML/ABI4](benchmarks/ome-roi/README.md), [ASAP XML/shared XML](benchmarks/asap-xml/README.md), [COCO pixel masks/ABI5](benchmarks/coco/README.md), [NGFF integral-label interoperability](benchmarks/ngff-labels/README.md) and [selected Loom matrix ABI2](benchmarks/loom/README.md) delivered locally | Each packet retains its own source/binary/profile/SDK/deployment receipt. Typed source facts, planes and native geometry/mask/import authorities are reused without transferring old identities. Other NGFF, N5/AnnData-Zarr, additional Loom, ROI and writer/application profiles still require actual terms/contracts/evidence under the full goal |
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
