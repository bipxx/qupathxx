# Native PNG reader plugin

`org.qupathxx.png` version `1.0.0`, contract `png-static-native-samples/1`, is an
independently built image reader for the existing native C ABI. It links private
static libpng1.6.59 and prefixed zlib1.3.2, with no application-core library link.
Python is not a product or decoder dependency. The source-scoped
[delivery receipt](../../docs/benchmarks/png-reader-plugin/README.md) owns tested
artifacts, commands and limitations; this page does not qualify another build.

## Supported profile

Static PNG, one image level, `Z=0`, `T=0`, including Adam7 interlacing:

| PNG color type | Sample depths | Native raw channels |
| --- | --- | --- |
| Grayscale | 1, 2, 4, 8, 16 | Gray |
| Truecolor | 8, 16 | Red, Green, Blue |
| Indexed palette | 1, 2, 4, 8 | Palette index |
| Grayscale with alpha | 8, 16 | Gray, Alpha |
| Truecolor with alpha | 8, 16 | Red, Green, Blue, Alpha |

Raw reads preserve original sample values in packed native-endian HWC storage.
Sub-byte gray and palette samples unpack into UInt8 without scaling; palette
raw values remain indices. Sixteen-bit samples remain exact UInt16 values.
Transparency does not erase samples: every in-bounds raw pixel has validity1,
including pixels with alpha0. Out-of-bounds samples are zero with validity0.
Channel-subset and cropped/padded reads use the same source values.

Display is separate straight-alpha RGBA8. Palette and `tRNS` transparency affect
display only. Sub-byte gray scales to0–255; sixteen-bit display takes each
sample's high byte. In-bounds hidden RGB values are preserved at alpha0;
out-of-bounds display pixels are transparent black. Display bytes are not the
quantitative source samples and do not establish raw scientific-recipe admission.

`pHYs` with meter units and a positive per-axis pixels-per-meter value establishes
that axis's calibration as `1,000,000 / pixels_per_meter` µm/pixel. Unknown units
or a zero axis value do not establish physical calibration. No scale, rotation
or orientation is guessed.

The profile permits untagged encoded samples and standard sRGB metadata:
`gAMA=45455` and standard sRGB `cHRM` coordinates. It explicitly refuses:

- APNG chunks `acTL`, `fcTL` and `fdAT`.
- ICC profiles (`iCCP`), Exif (`eXIf`) and nonstandard gamma/chromaticities.
- Extended color interpretation (`cICP`, `mDCV`, `cLLI`).
- Alternate calibration/placement (`sCAL`, `pCAL`, `oFFs`, `sTER`).

Malformed/truncated input, invalid CRCs, unknown critical chunks, libpng warning
recovery and trailing data after IEND also refuse. Unused ancillary payloads
are CRC-checked and discarded rather than retained or interpreted.

Opening requires an absolute UTF-8 path, empty options and a work allowance no
larger than256MiB. Encoded size, decoder buffers, temporary codec allocations
and checked raster sizes must fit their bounds. The custom allocator rejects
decoded expansion exceeding the allowance even when the encoded file is small;
the checks include a valid compressed raster with a32KiB decoded-work allowance.
Open/decode and raw reads support cancellation. Reads validate dimensions,
coordinate overflow, channel ranges and the host's packed output-buffer layout.

## Offline build

Use the compiler recorded by the relocated ReaderPluginKit: Windows x64,
MSVC1944 / compiler19.44.35225.0, Release, dynamic CRT (`/MD`). CMake checks the
C and C++ compiler/profile and every locked decoder source file. Other platforms,
toolchains and configurations are not claimed.

From a source checkout, prepare the locked code-only dependencies with
[prepare_png_reader_sources.py](../../tools/ci/prepare_png_reader_sources.py):

```powershell
python tools/ci/prepare_png_reader_sources.py --output "DEPS" --libpng-cache "PNG_CACHE" --zlib-source "ZLIB_SOURCE"
```

Preparation is offline by default. An explicit `--download` permits only locked
libpng code-member downloads; zlib must already be supplied. No upstream images,
models, contrib implementations or automatic runtime downloads are required.
The source package supplies `source/deps/libpng`, `source/deps/zlib` and the
relocated `source/reader-plugin-kit`; these can be used directly without Python.

Build in a matching Visual Studio developer shell, substituting absolute paths:

```powershell
cmake -S plugins/png -B "BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_PNG_SOURCE_DIR=LIBPNG_SOURCE" `
  "-DQUPATHXX_PNG_ZLIB_SOURCE_DIR=ZLIB_SOURCE"
cmake --build "BUILD" --config Release
cmake --install "BUILD" --config Release --prefix "INSTALL"
```

For an unpacked source package, use `source/plugins/png` as the source directory.
`QUPATHXX_PNG_BUILD_TESTS` defaults OFF. Enabling it additionally requires the
matching native SDK for the project test consumer; that test's core-library link
does not enter the plugin. No host/core or Python-binding build is required.

## Use and saved sources

Keep `qpx_png_plugin.dll` beside its generated, hash-bound `reader-plugins.json`
and installed license directory. Configure the manifest before catalog/source use:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.png INPUT.png 0 0 0 512 512 OUTPUT.pam
```

`--reader org.qupathxx.png` is explicit: automatic `.png` selection is unchanged.
The module exposes typed raw access, but existing TIFF/Zarr-only recipes keep
their native reader/view requirements. The plugin does not add a `raw_view()` or
permit an unsupported scientific operation.

Project attach/save/reopen, source currentness, offline missing-provider reports
and changed-implementation refusal use the existing host authorities. Relocating
the unchanged module/package preserves implementation identity; replacing bytes
requires a matching manifest and the native source-review rules. Restart the host
to change configured modules. Remove only manifest-owned plugin package members;
preserve the host, original images, saved projects and user outputs.

## Licenses and scope

Repository-authored adapter code remains AGPL-3.0-only under the repository
[LICENSE](../../LICENSE). libpng retains its PNG Reference Library License2 and
zlib retains the Zlib license. Installed texts are under `licenses/qupathxx`,
`licenses/libpng` and `licenses/zlib`. Exact selected source identities are in
[libpng-source-lock.json](libpng-source-lock.json) and
[zlib-source-lock.json](zlib-source-lock.json). Plugin separation does not relabel
upstream code or waive attribution, source or distribution obligations.

This delivery uses native readers and admitted code-only dependencies. It adds no
Java/Groovy compatibility, noncommercial/research-only material, UI implementation,
broader platform acceptance, biological qualification or speed claim.
