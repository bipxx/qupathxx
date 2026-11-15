# Native JPEG reader plugin

`org.qupathxx.jpeg` version1.0.0, contract `jpeg8-oriented-decoded-samples/1`,
uses the existing image C ABI with a private static libjpeg-turbo3.1.3 codec.
It does not link the application core. C/C++ owns all runtime interpretation;
Python is optional development fixture/build/package control only.

## Supported interpretation

Single-image, 8-bit Huffman DCT JPEG, grayscale, RGB or YCbCr, baseline or
progressive. One level, `Z=0`, `T=0`. Decoding uses the pinned codec's integer
`JDCT_ISLOW`, fancy upsampling, no block smoothing, scaling or quantization.
Grayscale returns one Gray channel; RGB and YCbCr return Red/Green/Blue.

Raw reads return canonical **decoded** UInt8 samples in packed HWC order.
Lossy JPEG cannot recover precompression samples. Display is the same oriented
Gray/RGB expanded to straight RGBA8 with alpha255. In-image raw validity is1;
out-of-image raw/display samples are zero with validity0/alpha0. Channel subsets
and padded/cropped reads preserve those values. This capability does not admit
JPEG into existing TIFF/Zarr-specific scientific recipes.

EXIF orientation1–8 transforms both raw and display pixels. Orientations5–8 swap
dimensions and physical calibration axes. Bounded TIFF IFD0 parsing supports
both byte orders, orientation, X/YResolution and ResolutionUnit. A bounded one-hop
Exif IFD supports ColorSpace1 (sRGB); unsupported color overrides refuse.
Explicit inch/cm JFIF densities or EXIF rational resolutions yield µm/pixel:
`25400 / pixels_per_inch` or `10000 / pixels_per_cm`. Unknown units do not
establish calibration. Conflicting physical metadata, malformed/duplicate tags,
invalid offsets or denominators refuse. No physical scale is inferred.

The strict profile refuses CMYK/YCCK, ICC, unsupported Adobe/XMP metadata,
arithmetic, lossless and non8-bit encodings, MPO/MPF, concatenated/trailing data,
truncation and codec warning recovery. It does not synthesize a missing EOI or
silently color-manage samples. Untagged supported samples retain the declared
decoder interpretation. This is a research image input, with no clinical claim.
Other APP interpretations (including Photoshop APP13), competing EXIF gamma/
chromaticities/transfer functions and APP metadata after the first scan refuse.
One bounded thumbnail IFD may be structurally inspected; its image payload and
orientation/calibration do not participate in primary-image decoding.

Open requires an absolute UTF-8 path, empty options and at most256MiB of work.
Checked raster sizes and actual tracked codec/image/temporary allocations must
fit the work allowance. No disk-backed allocation is available. Open/decode and
raw reads support cancellation. Regions validate dimensions, coordinate overflow,
channels and host buffer layout before use.

## Offline build and use

Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD` are the selected kit
profile. CMake verifies both compilers and every locked upstream member.
Other platforms/configurations are not qualified by this local delivery.

The source kit supplies `source/deps/libjpeg-turbo` and `source/reader-plugin-kit`.
From a repository checkout an optional development helper copies only exact
locked members from an existing admitted cache:

```powershell
python tools/ci/prepare_jpeg_reader_sources.py --source JPEG_SOURCE --output DEPS
```

Build in the matching Visual Studio developer shell, using absolute paths:

```powershell
cmake -S plugins/jpeg -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" "-DQUPATHXX_JPEG_SOURCE_DIR=DEPS"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

For the source kit use `source/plugins/jpeg` as source. Upstream builds as a
standalone static external project with SIMD, arithmetic, TurboJPEG, Java,
tools, tests and fuzzing disabled. No downloads or upstream test images enter
the build. Default tests are OFF; enabling `QUPATHXX_JPEG_BUILD_TESTS` requires
the matched native SDK only for the test consumer. No Python or host/core build
is needed for the default module.

Keep the DLL, generated hash-bound manifest and licenses together. Configure
before catalog/source use and select the provider explicitly:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.jpeg INPUT.jpg 0 0 0 512 512 OUTPUT.pam
```

Automatic suffix routing is unchanged. The host's existing saved-source,
currentness, missing-provider and implementation-identity authorities apply.
Relocating unchanged module bytes preserves implementation identity; replacing
them requires a matching manifest and native review rules. Restart the host
to change configured modules. Close users of the module before removal, remove
only package-owned members and preserve images, projects and outputs.

## Component licenses

Adapter code remains AGPL-3.0-only under the repository [LICENSE](../../LICENSE).
libjpeg-turbo retains its applicable IJG, BSD-3-Clause and Zlib terms and per-file
notices; the exact unmodified selection is in
[libjpeg-source-lock.json](libjpeg-source-lock.json). Full `LICENSE.md` and
`README.ijg` texts are installed under `licenses/libjpeg-turbo`. No optional
upstream asset is admitted by the codec's license.

This software is based in part on the work of the Independent JPEG Group.

The [delivery receipt](../../docs/benchmarks/jpeg-reader-plugin/README.md)
records exact tested artifacts and limits. No UI, Java/Groovy compatibility,
wheel, publication, broader parity or biological/speed acceptance is implied.
