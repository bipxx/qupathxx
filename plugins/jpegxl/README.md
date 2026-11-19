# Native JPEG XL integer still reader

`org.qupathxx.jpegxl` 1.3.0 is an optional Windows x64 reader for original-profile
integer grayscale and RGB JPEG XL stills with optional straight alpha at the
same bit depth. It preserves 1–8-bit codes in UInt8 and 9–16-bit codes in
UInt16. All eight codestream orientations map to one canonical XY raster
shared by the raw and display routes. The source must specify sRGB primaries
and D65, with either the sRGB
transfer function or linear sRGB. The RGBA preview converts linear color to
sRGB and scales alpha independently. Raw exports retain the decoded codes.

Set `{"allow_xyb":true}` in the reader manifest or pass it through the CLI's
`--reader-options` to admit structured SDR sRGB XYB stills. These exports
contain libjxl's reconstructed sRGB integer codes, and the metadata labels
them `JPEG XL (decoded sRGB)`. The project retains the opt-in in its exact
reader implementation identity. The default profile continues to require
original-profile inputs.

The reader accepts bare codestreams and containers with the JPEG XL signature,
file type, and optional level box. The container may hold one complete
`jxlc` box or up to 4,096 nonempty `jxlp` fragments. Version 0 file types
require ascending indices. Version 1 accepts out-of-order fragments and
assembles them by index. Every index is unique and the highest index carries
the single final marker. The reader checks exact box extents and rejects
trailing data. Animation, float and HDR samples,
profile-managed ICC color, XYB outside structured SDR sRGB, and additional boxes
require separate profiles. The native ABI records the exact provider and
source identity. No physical calibration is inferred.

Configure Release with `QUPATHXX_READER_PLUGIN_KIT_PREFIX` set to the matching
kit and `QUPATHXX_JPEGXL_ARCHIVES` set to the directory of offline archives
named in the plugin CMake file. The package includes the exact archives and
original notices, including the libjxl patent notice. The product compiles
the pinned libjxl 0.13.0 decoder and Highway. Brotli and skcms source pins
fulfil the upstream configuration inputs for the selected build. The
encoder, utilities and external testdata are outside this build.
[libjxl release and license](https://github.com/libjxl/libjxl/tree/v0.13.0)

Install the module and use its `reader-plugins.json` with the native host.
The separate `tests` CMake project builds the SDK probe. Set
`QUPATHXX_JPEGXL_TEST_DECODER_CACHE` to the matching native build to
also build the direct container probe. Run
`tools/ci/check_jpegxl_reader_plugin.py` with its recorded baseline and the
imagecodecs 2025.8.2 development oracle. The oracle uses libjxl 0.11.1 as a
separate writer and compares the native decoder with authored sample arrays.
The direct container probe verifies the ordered and reordered fixtures
with libjxl 0.13.0's own box parser.
Orientation tests use `cjxl` and `djxl` from the official Windows x64
libjxl 0.13.0 archive with SHA-256
`61f382a964f4669095cd460f0b588ed8fb55500b33f7a16bc03c6ef7dce689d9`.
The producer tools are development inputs. [Official Windows binaries](https://github.com/libjxl/libjxl/releases/tag/v0.13.0)
