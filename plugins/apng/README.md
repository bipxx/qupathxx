# Native APNG frame reader

`org.qupathxx.apng` opens one composited canvas from a bounded RGB8 or RGBA8
APNG. Use `--reader org.qupathxx.apng --reader-options '{"frame":1}'` or set
`{"frame":1}` in the reader catalogue. Frame indices start at zero. A
separate fallback PNG contributes no pixels or index to the animation.

SOURCE replaces the rectangle, including hidden RGB. OVER combines straight
alpha in linear sRGB and rounds the resulting canvas to RGBA8 after each
frame. BACKGROUND clears the rectangle and PREVIOUS restores its old
contents. Raw and display reads both expose the selected rendered RGBA8
canvas. Metadata labels it `APNG (rendered frame)`, and a project captures
the exact frame option. Delays and playback are outside this frame-selection
profile. [PNG animation specification](https://www.w3.org/TR/png-3/#apng-frame-based-animation)

The reader admits non-interlaced RGB/RGBA8 with optional `sRGB`, up to 256
frames, 2,048 pixels on each canvas axis and 64 Mi decoded pixels. Color
profiles, other PNG sample layouts and additional chunks require separate
profiles. Frame count, CRCs, chunk sequence and decoded frame data are checked
through IEND, including for frames beyond the selected one.

Configure Release against the matching `QUPATHXX_READER_PLUGIN_KIT_PREFIX`
and the admitted `QUPATHXX_PNG_SOURCE_DIR` and `QUPATHXX_PNG_ZLIB_SOURCE_DIR`.
The plugin reuses the locked libpng 1.6.59 and zlib 1.3.2 sources and original
notices from the static PNG reader. The `tests` project builds against the
native SDK. `tools/ci/check_apng_reader_plugin.py` checks writer and authored
frames, rejection paths and project replay.
