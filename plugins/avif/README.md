# Native AVIF still reader

`org.qupathxx.avif` 1.0.0 opens bounded SDR sRGB AVIF stills. The exact-sample
profile reads full-range grayscale and full-range identity-coded 4:4:4 RGB.
Both profiles accept optional straight alpha. Eight-bit samples use UInt8.
Ten- and twelve-bit samples use UInt16 with their original code values.
The RGBA preview scales each channel to eight bits. Physical calibration is
unspecified.

Set `{"allow_yuv":true}` in the reader manifest or pass it to the native CLI
with `--reader-options` to enable reconstructed RGB. The plugin uses libavif's
scalar bilinear chroma reconstruction for SDR sRGB YUV with BT.601, BT.709 or
BT.470BG matrix coefficients. Subsampled input requires the unspecified AV1
chroma sample position. Raw exports in this mode contain reconstructed sRGB
codes and carry the `AVIF (decoded sRGB)` metadata label. The project retains
the selected options and module digest for reopen.

The profile limits each dimension to 2,048 pixels and the encoded input to
32 MiB, with an additional pixel limit based on the available reader
workspace. Animated or progressive images, ICC and HDR metadata, Exif,
geometric transforms and premultiplied alpha require separate profiles.

Configure Release with `QUPATHXX_READER_PLUGIN_KIT_PREFIX` set to the matching
kit and `QUPATHXX_AVIF_ARCHIVES` set to the directory of the two pinned
decoder-source bundles. `tools/ci/prepare_avif_reader_sources.py` derives the
bundles from verified upstream archives. The package includes the exact
selected sources and original notices. The build uses libavif 1.4.2 and the
generic single-threaded libaom 3.14.1 decoder. Libavif includes its bundled
libyuv scaling fallback. RGB reconstruction uses libavif's scalar path.
The source selections exclude upstream image and test assets.
[libavif license](https://github.com/AOMediaCodec/libavif/blob/v1.4.2/LICENSE)
[libaom license](https://aomedia.googlesource.com/aom/+/refs/tags/v3.14.1/LICENSE)
[libaom patents](https://aomedia.googlesource.com/aom/+/refs/tags/v3.14.1/PATENTS)

Use the installed `reader-plugins.json` with the native host. The separate
`tests` CMake project builds the project reopen probe against the native SDK.
`tools/ci/check_avif_reader_plugin.py` checks authored samples and a Pillow
4:2:0 still, using `avifenc` and dav1d-backed `avifdec` from the official
libavif 1.4.2 Windows archive as development tools.
[Official Windows binaries](https://github.com/AOMediaCodec/libavif/releases/tag/v1.4.2)
