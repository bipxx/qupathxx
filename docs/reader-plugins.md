# Native reader plugins and saved sources

The optional Windows x64 pilot loads explicitly configured native image readers
through a versioned [C ABI](../cpp/include/qupathxx/reader_plugin.h). The installed
P6 example decodes 8-bit binary PPM inside its own DLL, using the same extracted
decoder component as the built-in PPM reader. It links no application core library
and provides display RGBA only. The frozen
[phase 3 receipt](benchmarks/reader-plugin-pilot/README.md) covers that standalone
pilot, including its then-current Project refusal. The
[phase 4 receipt](benchmarks/reader-plugin-project/README.md) records source-scoped
native tests and local deployment validation for the saved-source implementation.
It has no new wheel, publication or release qualification claim.

## Additional image readers

The [native PNG plugin](../plugins/png/README.md) adds static PNG through the
same image ABI, with separate original typed samples and display RGBA8.
Its [delivery receipt](benchmarks/png-reader-plugin/README.md) covers the exact
profile, independent source-kit build, saved-source replay and package licenses.
Configure its manifest and select `org.qupathxx.png` explicitly. This does not
change automatic reader selection or the existing TIFF/Zarr recipe requirements.

The [native JPEG plugin](../plugins/jpeg/README.md) uses the same ABI for the
declared8-bit Huffman DCT Gray/RGB/YCbCr profile. Raw samples are canonical decoded
values from lossy input. EXIF orientation applies to raw/display and calibration
axes; unsupported color/encoding metadata refuses. Its
[receipt](benchmarks/jpeg-reader-plugin/README.md) records exact validation,
saved-source replay, PNG coexistence and the admitted offline source/notice kit.
Configure its manifest and select `org.qupathxx.jpeg` explicitly.

The [PNM/PAM plugin](../plugins/pnm/README.md) adds the declared P1–P7 source-code
profile, including original UInt16 samples and PAM alpha. Native explicit transfer
options affect display without changing raw codes; saved bindings retain those
options when provider defaults change. Its [receipt](benchmarks/pnm-reader-plugin/README.md)
records exact tests, offline source build and PNG/JPEG coexistence.

The [BMP plugin](../plugins/bmp/README.md) adds the selected uncompressed
40-byte-header profile. Raw palette indices/RGB stay exact; canonical rows and
per-axis pixel density are explicit, and32-bit X is unused rather than alpha.
Its [receipt](benchmarks/bmp-reader-plugin/README.md) covers native replay,
four-plugin coexistence and the admitted offline source/package handoff.

The [lossless WebP plugin](../plugins/webp/README.md) adds simple single-VP8L
images with exact raw/display RGBA, hidden RGB and hint-independent alpha.
Its private libwebp decoder retains its own BSD/PATENTS/AUTHORS notices alongside
the AGPL adapter sources. The [receipt](benchmarks/webp-reader-plugin/README.md)
covers bounded codec scratch, saved-source replay and five-plugin coexistence.
Lossy/extended/animated/metadata WebP remains outside this initial profile.

The [scalar NRRD plugin](../plugins/nrrd/README.md) adds the selected attached
raw XY/XYZ scientific profile. Typed values and all Z planes retain exact bits;
explicit persisted windowing affects Z0 display separately. Its
[receipt](benchmarks/nrrd-reader-plugin/README.md) covers calibration/refusals,
native saved-window identity and six-plugin coexistence without a host rebuild.

The [ICS plugin](../plugins/ics/README.md) adds the declared uncompressed scalar/
channel-first fluorescence profile. ICS1 derives a guarded adjacent IDS companion;
ICS2 holds inline data. Exact typed C/Z samples, byte permutations and native
channel/window choices use the existing file-set extension. Its
[receipt](benchmarks/ics-reader-plugin/README.md) records the independent native
reference, seven-plugin coexistence and admitted source/package handoff.

The [QOI plugin](../plugins/qoi/README.md) adds all six operations from the open
specification1.0, exact canonical RGBA8 including hidden RGB and alpha, and
separate linear-to-sRGB display conversion. It has no external codec dependency.
Its [receipt](benchmarks/qoi-reader-plugin/README.md) covers40 exact profiles,
26 refusals, saved-source identity, eight-reader coexistence and an admitted
offline source kit. Select `org.qupathxx.qoi` explicitly.

The [JPEG 2000 plugin](../plugins/jpeg2000/README.md) reads baseline grayscale
JP2 and J2K with exact UInt8/UInt16 raw codes and a separate RGBA8 display.
Version 1.1.0 adds bounded tile grids through the private static JasPer JPC
codec and the standard file reader ABI. Select `org.qupathxx.jpeg2000` explicitly.

The [TGA plugin](../plugins/tga/README.md) reads plain and RLE true-color,
8-bit grayscale and indexed rasters. It retains exact UInt8 RGB/RGBA,
grayscale codes and palette indices. Display uses the stored palette for
indexed images. The reader normalizes the four descriptor origins and uses
the standard file reader ABI.
Select `org.qupathxx.tga` explicitly.

The [PCX plugin](../plugins/pcx/README.md) reads eight-bit version-5 RLE
and version-2/5 four-bitplane RLE. It preserves planar RGB or indexed raw
samples, with separate VGA or header palette display and declared scanline
padding. Select `org.qupathxx.pcx` explicitly.

### Native raw and preview file exports

The shared C++ [region export service](../cpp/include/qupathxx/region_export.hpp)
owns raw-file output and display PAM serialization. The source-matched native
frontend accepts an explicit reader with `--raw`:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.qoi --raw INPUT.qoi OUTPUT.npy --channels 4
```

Place `--reader-options JSON_OBJECT` immediately after an explicit plugin
ID to select options for that invocation. The JSON is one argument. The
object replaces the provider's manifest defaults for this open, and
`{}` selects empty options. The same option works for raw and display
modes. The raw snapshot records the canonical effective options. This
example selects FITS HDU 2 and exports a crop:

```powershell
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.fits --reader-options '{"hdu":2}' --raw INPUT.fits OUTPUT.npy --width 3 --height 1
```

Plain plugin NPY output retains exact native sample types/codes and requires
complete valid samples. Its schema2 stdout report includes the native reader snapshot,
source/module/dependency identities, options and metadata. Primary/companion
files stay guarded through export. Directory-store raw export still requires a
complete snapshot. The source-matched CLI can export from identified plugin
stores using their retained directory observation, with a new output outside
the source root. The default TIFF route retains schema1 and its exact
NPY behavior.

Use `--reader ome-zarr` for built-in NGFF 0.4/Zarr 2 raw export. The command
uses the same native directory verifier as raw analysis. Its JSON report
includes `reader_source` with the directory identity and any available
physical calibration. `level_downsample` describes the selected grid relative
to level zero. `pixel_width_um` and `pixel_height_um` describe the spacing
of the exported samples. By default, the request's `x` and `y` are level-zero
coordinates. Pass `--level-coordinates` to address the selected array directly.
The report identifies the coordinate convention in `coordinate_space`.
`source_grid` identifies the selected dataset and gives the signed array index
of output row 0, column 0 as `level_x` and `level_y`, including any padding.
Add the output column and row to these indices to locate a sample. The `axes`,
`units`, `scale` and `translation` arrays describe the selected dataset in
declared axis order. A source-array index maps to its coordinate as
`index * scale + translation` for each axis. Use the validity field to exclude
padded positions from quantitative analysis.
Use a new output path outside the store.

```powershell
qupathxx_region --reader ome-zarr --raw IMAGE.ome.zarr CROP.npy --z 1 --time 0 --width 64 --height 64 --with-validity
```

To export a complete translated level, select it with `--level-coordinates`
and leave the origin and dimensions at their defaults.

```powershell
qupathxx_region --reader ome-zarr --raw IMAGE.ome.zarr LEVEL.npy --level 1 --level-coordinates
```

Add `--with-validity` to a raw export to retain missing or padded pixels
in one structured NPY file. NumPy loads an H×W array with fields
`values` (native sample type, H×W×C) and `valid` (Boolean, H×W).
Use the validity field to distinguish missing pixels from valid zeros.
The schema3 report identifies the layout and counts valid pixels. TIFF,
built-in OME-Zarr, and native plugins support this mode. Zarr fill values for
absent in-bounds chunks are valid. Crop padding is invalid.

Use `--bundle` to keep a raw array and source receipt in one ZIP file. This
works with either NumPy layout. The archive contains `raw.npy` and
`receipt.json`. The versioned receipt records the region report, observed
source and available calibration. The command still prints the region report.
The archive is published atomically at a new output path.

Use `--raw-project INPUT.qpx IMAGE_ID OUTPUT.npy` to export through a saved
reader view. Supply `--reader-plugins ABS_MANIFEST` before the command for
a plugin-backed image. Its saved reader contract and options select how the
source opens. Validity and bundle options work with project exports.
Project bundles use receipt schema version 3. The digest covers the saved
project identity, generation, file SHA-256 and native image ID in
`region.project`. Compare that SHA-256 with a relocated project copy to
identify the exact saved input.
When the project has a saved source verification, the export checks it
before reading the crop. An identical single-file replacement can pass by
checksum. Changed bytes, companion observations or directory observations
require source review and adoption before export.

```powershell
qupathxx_region --reader ome-zarr --raw IMAGE.ome.zarr CROP.zip --level 1 --level-coordinates --width 64 --height 64 --with-validity --bundle
```

Run `qupathxx_region --verify-raw-bundle CROP.zip` to verify an archive and
print its receipt offline. The verifier checks the stored ZIP framing, both
CRCs, the NPY dtype and shape, and any structured validity count. Version 2
also checks `array_sha256` against the full NPY and `receipt_sha256` against
the compact receipt JSON with that field removed and no trailing newline.
Version 1 uses CRC and structure checks. Version 3 adds validation of the
project fields and uses the same digest checks as version 2. The verifier
uses only the archive.

Run `qupathxx_region --extract-raw-bundle CROP.zip CROP.npy` to recover the
exact `raw.npy` member and print its receipt. Extraction uses the same offline
checks and publishes the array atomically at a new output path after they pass.

PAM exports owned display RGBA, with atomic new-only publication
and source/companion/directory alias protection. The [receipt](benchmarks/qoi-reader-plugin/README.md)
binds these changed frontend/service bytes separately from the frozen SDK,
GUI, bindings and wheels; installing a module does not replace an older frontend.

The [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
records future candidates and missing native contracts. The separately delivered
[matrix/annotation plugins](format-family-plugins.md) use their own typed C ABI,
native import authority and matching host/SDK candidate. Directory stores, writers
and remote sources still require their selected contracts; none extends image v1
merely by appearing in the plan.

## Build and use the installed kit

`QUPATHXX_BUILD_READER_PLUGINS` defaults to `OFF`. Enable it in an existing native
Windows x64 MSVC Release `/MD` build; Python remains optional and defaults OFF.
Build `qupathxx_region`, then install the development components:

```text
cmake --install BUILD --config Release --prefix INSTALL --component ReaderPluginKit
cmake --install BUILD --config Release --prefix INSTALL --component ReaderPluginHost
```

The kit installs `include/qupathxx/reader_plugin.h`, the dependency-free decoder,
the recorded compiler profile, license and example under
`share/qupathxx/reader-plugin`. Copy the example outside the application checkout.
Its [instructions](../examples/reader_plugin/README.md) build a DLL and a
hash-bound `reader-plugins.json` using only the relocated installed kit. Compiler,
architecture, Release and dynamic-CRT guards apply to that independent build.

Configure the CLI with an explicit absolute manifest path before the existing
listing or display command:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.p6 IMAGE.ppm 0 0 0 512 512 OUTPUT.pam
```

Thumbnail, resampling and RGB-nominal display modes accept the same prefixes.
Existing positional commands and automatic built-in selection keep their legacy
policies. The quantitative raw CLI retains its explicit TIFF contract.

Native application and batch processes accept the same bootstrap prefix before
opening a project or running an existing command:

```text
qupathxx_app --reader-plugins ABS_MANIFEST --project SAVED_PROJECT
qupathxx_app --reader-plugins ABS_MANIFEST IMAGE.ppm --reader org.qupathxx.p6
qupathxx_batch --reader-plugins ABS_MANIFEST EXISTING_BATCH_COMMAND ...
qupathxx_labels --reader-plugins ABS_MANIFEST EXISTING_LABEL_COMMAND ...
```

Configured providers append to the existing desktop reader choices. Automatic
selection and the original built-in positions keep their existing behavior.
The manifest must be configured before the first catalog query or source open.
For independently installed packs, repeat `--reader-plugins ABS_MANIFEST` in
`qupathxx_region`, `qupathxx_app`, `qupathxx_format_import`, `qupathxx_batch`
or `qupathxx_labels`, or call
`qpx::configure_reader_plugin_manifests` once with up to 32 absolute paths.
Module and dependency paths resolve within each manifest's own package. An ID
claimed by multiple manifests is refused in every claiming package before
any of those modules load. An unrelated package can still be admitted.
The single-path C++ function and one-flag invocations retain their behavior.

Desktop startup accepts `--reader-options JSON_OBJECT` when opening an image
with an explicit native plugin. The object replaces that plugin's manifest
defaults for this open. A project saved from the new image retains the chosen
reader and options. A project reopen uses its saved selection.

```powershell
qupathxx_app --reader-plugins ABS_MANIFEST --reader org.qupathxx.vti --reader-options '{"spacing_unit":"mm","point_array":"Auxiliary"}' INPUT.vti
```

## Admission and lifetime

The manifest schema is `qpx.reader-plugins/1`. Each provider declares a unique
namespaced ID, display name, version, semantic contract, ABI/profile, display/raw
upper bounds, one module path/hash and an explicit dependency path/hash list.
An optional `options` JSON object supplies defaults for newly opened sources;
the host normalizes it before passing it to the module. Options are bounded to
4096 UTF-8 bytes; an empty object has the canonical empty representation.
The example generates this manifest after compilation. Module/dependency paths
are relative to the manifest directory and must resolve within that package;
discovery does not search CWD or PATH. At most 32 providers, 63 dependencies per
provider, 1 MiB manifest text and 256 MiB package image bytes are admitted.

The host locks and hashes actual module/dependency files before loading, checks
x64 PE imports, refuses undeclared non-system dependencies, and checks actual
resolved import targets. Loads use package/System32 search flags without changing
the process-wide search path. See Microsoft's [LoadLibraryExW contract](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw)
and [PE import format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
The pilot rejects delay-load and cyclic package dependencies. Its manifest profile
must match the host's recorded MSVC series; the supplied kit additionally checks
the exact compiler version and `/MD` build configuration.

Returned ABI tables, callback origins and provider descriptors must match the
admitted package. The catalog freezes before its first open/listing and cannot
reload. Rejected modules produce owned `plugin_diagnostics` and cannot replace
built-ins. Loaded modules are pinned until process exit; restart to change them.
These are admitted in-process native modules. Reuse the existing
[material/reader admission policy](reader-license-policy.md) for code, formats,
dependencies and notices; a runtime manifest does not establish licensing rights.

## Data contract and current limits

The host owns output buffers and copies bounded UTF-8 metadata. It validates
shapes, strides, exact fill lengths, sample types, calibration and validity before
returning results. Raw transfer is packed native-endian HWC with one validity byte
per pixel; valid integer bits, NaN/Inf and signed zero remain unchanged. Per-handle
calls and close serialize, and module-owned handles are destroyed exactly once.
Cancellation is a synchronous host callback; modules retain no host pointers.

Providers that declare `source_files` also export the optional
`qpx_reader_files_entry_v1` table. The host discovers up to 63 absolute companion
paths before opening the decoder, under the primary file guard, and repeats
discovery around reads. The host observes versions and identities for the whole
set and rejects duplicate paths, file identities or primary aliases. This
extension leaves the base v1 function table unchanged at 72 bytes on the supported
x64 profile, so single-file v1 modules do not need the extra entry point.

Directory providers declare `source_store` (capability8), mutually exclusive with
`source_files`, and export `qpx_reader_store_entry_v1`. Its separate16-byte table
returns one normalized store-relative primary metadata key. The host snapshots
the bounded recursive regular-file membership and native file versions, refusing
links/reparse ancestors, oversized traversal, mutation and cancellation. This
observed digest is not a content hash or a hash of empty-directory timestamps.
The unchanged72-byte base table and24-byte file-set extension retain old callers.

The [selected NGFF label-mask provider](benchmarks/ngff-labels/README.md) also
declares `label_description` (capability16), requiring raw and store capabilities,
and exports `qpx_reader_label_entry_v1`. Its separate16-byte table writes at most
65536 bytes of owned neutral UTF-8 source/frame/properties metadata. Core services
validate that neutral description and explicit native target frame; NGFF file
interpretation remains in the optional provider. Host-owned buffers, currentness,
cancel/error status and whole-store output guards apply through publication and
export. No label or quantitative recipe is admitted by a capability bit alone.

The P6 module admits single-file sources, conservatively bounds encoded files to
the host's 256 MiB work allowance, and refuses raw access. A separate test-only
module proves typed raw transfer and is never installed. Source file guards check
currentness around native work. Provider capability flags do not admit a source
into an existing scientific recipe. The bridge exposes no legacy TIFF/Zarr
view/options objects and rejects view mutation. Cached raw metadata and typed raw
reads do not provide a `raw_view()`; existing raw-analysis, raw-classifier,
dark/flat and unmixing reader/view requirements remain in force. The P6 module
exposes no raw metadata.

## Saved projects and implementation review

Project schema 30 stores a bounded `qpx.reader-plugin-link/1` payload for each
linked plugin image. The private canonical record contains display metadata,
optional raw axes/channel metadata, host-observed single/compound file facts and
an implementation snapshot. That snapshot binds the provider, version, semantic
contract, ABI, compiler/CRT profile, capabilities, module SHA-256, sorted dependency
basenames and SHA-256 values, and normalized explicit options. Installed module
paths are excluded from implementation identity so relocation preserves it.
The link is bounded to 2 MiB, raw metadata to 1 MiB, and source observations to
128 KiB with at most 63 companions. The later host-owned directory route uses
`qpx.reader-plugin-link/2` and source identity version3; its separate captured
root/primary key/observed membership never changes old file/compound link/1
identities. Saved directory-source review requires an explicit replacement root.
The built-in OME-Zarr source retains its separate existing contract.

The implementation digest appears in the plugin's observed reader identity as
`PROVIDER:plugin/1:SHA256`; `ImageServer::reader_id()` and `images.reader` retain
the provider ID. Built-in reader IDs, source JSON and verification/provenance
encodings keep their existing representations.

`Project::open` validates saved links without loading modules or freezing the
catalog. With a missing provider or a loader-OFF build, the image, saved metadata
and self-contained retained reports remain readable and can be re-saved. Source
pixel reads refuse without falling back to another reader. A successful lazy open
requires the saved implementation, metadata and observed file set to match.
Older hosts reject schema 30 rather than treating plugin links as built-ins.

Saved options remain explicit: changing manifest defaults does not replace them
on reopen. A new source opened with different explicit options gets a different
binding. Different module or dependency bytes under the same version string also
change implementation identity. Native source review reports
`changed_implementation`, and `Project::apply_source_review` requires
`accept_changed=true` to adopt that candidate. Existing stale-review and file
guards remain the publication boundary; historical records stay unchanged.

The retained RegionAnalysis digest binds plugin implementation identity as well
as its RGBA input. Tissue and Cell pixel-content hashes retain their existing
formats, while their native verification authority refuses to make an archived
run current under a different plugin implementation, even when masks and values
are identical. Recompute and retain a new result after adoption. No quantitative
recipe gate is expanded by this saved-source support.

## Native SDK and local development scope

Installed native consumers include
[`qupathxx/reader_plugins.hpp`](../cpp/include/qupathxx/reader_plugins.hpp) and call
`qpx::configure_reader_plugins(absolute_manifest)` before a catalog query or source
pixel read. `qpx::reader_catalog_json()` returns an owned report. Configuration
may follow `Project::open`, because parsing a saved project needs no module.
The [external SDK example](../examples/reader_plugin_project/main.cpp) performs
native add, RegionAnalysis, keep, source review, save, reopen and replay through
these same services. Python remains optional and supplies no implementation or
fallback for this workflow.

The installed kit and SDK handoff are local development artifacts. The
[phase 4 receipt](benchmarks/reader-plugin-project/README.md) records source-scoped
native tests, relocated SDK/module use and replay, and removal preserving research
files.
Distributable membership and publication
remain a separate owner-selected checkpoint in the
[plan](format-reader-plugin-plan.md); the frozen phase 3 evidence and candidate8
qualification do not transfer to this source.
