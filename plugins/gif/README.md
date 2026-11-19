# Native static GIF reader

`org.qupathxx.gif` 1.0.0 reads one full-canvas GIF87a or GIF89a image on
Windows x64. It preserves the source's 8-bit palette indices and emits
straight RGBA8 from the active color table. A transparent index sets alpha
to zero and retains the palette RGB and quantitative index. Unspecified or
square pixel aspect is accepted without inferring physical calibration.
[GIF89a format specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt)

The reader accepts global and local color tables, row interlace, bounded
comment blocks and optional untimed transparency. LZW decoding checks
sub-block framing, exact output extent, palette domain and the final
trailer. The dictionary supports clear codes, width growth and deferred
clears after saturation.

Multiple images, application and plain-text extensions, partial-canvas
images, non-square aspect, display timing and restore disposal require
separate profiles. The plugin uses the native reader ABI and the Windows
C++ runtime, with no bundled codec. Parser, tests and reader kit are
AGPL-3.0-only. Pillow is used for independently authored fixtures and
RGBA comparison during development.

Configure with the matching `QUPATHXX_READER_PLUGIN_KIT_PREFIX`, build
Release and install. Load the resulting `reader-plugins.json` and choose
`org.qupathxx.gif`. The separate `tests` directory builds a native SDK
consumer, and `tools/ci/check_gif_reader_plugin.py` drives validation.
