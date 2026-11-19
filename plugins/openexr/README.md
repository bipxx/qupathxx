# Native OpenEXR linear RGB scanline reader

`org.qupathxx.openexr` 1.1.0 opens single-part version 2 scanline images with
`R`, `G`, and `B` channels and optional associated `A`, square pixels, and
identical zero-origin data and display windows. It reads increasing and decreasing scanline order
with NONE, ZIPS or ZIP compression, including verbatim blocks in ZIP files.
The profile checks exact offset and block extents and is subject to the shared
reader work budget. Extra channels or attributes, including custom primaries,
non-finite samples, other windows, tiles, deep and multipart images require
separate profiles. [OpenEXR file layout](https://openexr.com/en/latest/OpenEXRFileLayout.html)

Raw reads preserve finite Float32 values and widen HALF losslessly to Float32,
including negative color values and signed zero. Alpha must be finite in
`[0, 1]`. Associated RGBA retains the original color values, including
emission at zero alpha, and uses the `OpenEXR (linear associated RGBA)`
metadata label. Physical calibration is unspecified.

The straight RGBA preview divides associated color by positive alpha, then
clamps negatives to zero, divides each channel by one plus the peak channel
in the pixel, applies the sRGB transfer curve and rounds to eight bits.
Alpha is rounded independently. Opaque RGB uses the same tone mapping.
Zero-alpha pixels render transparent black. Their emission remains in the
raw export. The preview applies no gamut transform.
[OpenEXR alpha convention](https://openexr.com/en/latest/TechnicalIntroduction.html#premultiplied-vs-un-premultiplied-color-channels)

Configure Release with `QUPATHXX_READER_PLUGIN_KIT_PREFIX` and
`QUPATHXX_OPENEXR_ZLIB_ARCHIVE` pointing to the matching kit and pinned zlib
1.3.2 source archive. The plugin links zlib statically with private prefixed
symbols. The package retains the exact zlib archive and notice.

Use the installed `reader-plugins.json` with the native host. Build the
project probe in `tests` against the native SDK. Run
`tools/ci/check_openexr_reader_plugin.py` with the retained RGB and RGBA ZIP fixtures and
upstream OpenEXR 3.5.4 installed in the development oracle environment.
The plugin and project probe run without Python or the upstream OpenEXR library.
