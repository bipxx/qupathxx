# Native BMP reader plugin

`org.qupathxx.bmp` version1.0.0, contract `bmp-infoheader-uint8-samples/1`,
is an independently authored C++ image C ABI reader. The module uses the matching
ReaderPluginKit and platform runtime, with no external codec, application-core,
Python, Windows WIC or GDI image-decoding dependency. Native options must be empty.

## Samples, display and calibration

The selected profile is a `BM` file header followed by exactly the40-byte
`BITMAPINFOHEADER`, one plane and uncompressed `BI_RGB` pixels.

| File encoding | Raw UInt8 channels | Display RGBA8 |
| --- | --- | --- |
|1/4/8-bit indexed | Original unpacked `Palette index` | RGB from the original BGR0 palette, alpha255 |
|24-bit BGR | `Red`, `Green`, `Blue`, exact original codes | Same RGB, alpha255 |
|32-bit BGRX | `Red`, `Green`, `Blue`, exact original codes | Same RGB, alpha255; X is unused |

The32-bit fourth byte is never interpreted as alpha, including X=0. Indexed raw
values preserve original indices even when palette colors repeat. No gamma,
color-profile conversion or source alpha is inferred. Raw arrays are packed
native HWC. Every in-image raw pixel is valid; out-of-image samples are zero and
invalid. Display padding is transparent black.

Positive file heights are bottom-up; negative heights are top-down. Both produce
the same upper-left canonical coordinates. DWORD row padding and unused packed
bits/nibbles do not become pixels. Palette entries are RGBQUAD records whose
reserved fourth byte must be zero. A positive `biClrUsed` selects a compact
palette up to the depth's maximum; zero requires the full palette. Out-of-range
pixel indices refuse. `biClrImportant` is zero or at most the indexed palette size.

There is one level, `Z=0`, `T=0`. Each positive pixels-per-meter field independently
supplies micrometers per pixel as `1000000 / ppm`; zero means unknown, represented
by an absent native SDK optional. Negative density refuses. No Z/time calibration
is inferred. Typed raw access does not grant TIFF/Zarr views or admit this
provider into an otherwise reader-restricted scientific recipe.

## Strict input profile and resource bounds

The declared file size must equal the actual file size, file reserved fields must
be zero, and pixel data starts immediately after the header and declared palette.
`biSizeImage` is zero or the exact padded raster size. Truncation, header/palette
overlap, unexplained gaps, trailing bytes and concatenated images refuse.

Other DIB header sizes,16-bit pixels, compression/RLE/bitfields, embedded JPEG/PNG,
V4/V5/ICC semantics and truecolor optimization palettes are unsupported. Their
semantics are not silently approximated. Invalid headers/data report native I/O
errors; unsupported encodings report unsupported; request/size/work limits report
invalid request. Allocation failure reports internal error.

Owned encoded input, raw raster, display raster and conservative array/path/owner
allowances must fit a requested work budget of at most256MiB. Large full-decode
rasters refuse. Open/decode and raw reads honor synchronous cancellation, including
chunk, row and bounded pixel checkpoints. No input, cancellation or host-buffer
pointer is retained. Source paths are absolute UTF-8.

## Build and use

Selected profile: Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`.
CMake verifies the matching kit/compiler and performs no dependency fetch.
In a matching Visual Studio developer shell, using absolute paths:

```powershell
cmake -S plugins/bmp -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

The complete source kit uses `source/plugins/bmp` and `source/reader-plugin-kit`.
Tests default OFF. `QUPATHXX_BMP_BUILD_TESTS=ON` additionally requires the matched
native SDK for the project test consumer; the module itself remains independent.
Keep the DLL, hash-bound manifest and licenses together and configure before
catalog/source use:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.bmp INPUT.bmp 0 0 0 512 512 OUTPUT.pam
```

Automatic routing follows its existing native policy. Saved-source currentness,
implementation review and exact result replay use existing host authorities.
Restart the host to change configured modules. Close module users before removing
byte-verified package-owned files; preserve images, projects and outputs.

## Licenses and reference definitions

This independent decoder, authored fixtures and source kit retain the repository's
[AGPL-3.0-only license](../../LICENSE). The published Microsoft
[bitmap storage](https://learn.microsoft.com/en-us/windows/win32/gdi/bitmap-storage),
[BITMAPFILEHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader)
and [BITMAPINFOHEADER](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader)
definitions are references. No Microsoft image codec, sample implementation or
sample image is copied or packaged; the adapter's license is not assigned to
the format specifications.

The [source-scoped receipt](../../docs/benchmarks/bmp-reader-plugin/README.md)
records tested artifacts and remaining scope. No further encoding, platform,
host/GUI/binding build, Java compatibility, biological/clinical or speed
qualification follows from this reader profile.
