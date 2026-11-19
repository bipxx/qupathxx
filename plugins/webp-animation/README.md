# Native animated WebP frame reader

`org.qupathxx.webp-animation` 1.0.0 opens one rendered WebP animation canvas.
Set `{"frame":1}` in the reader catalogue or pass it with `--reader-options`
after `--reader org.qupathxx.webp-animation`. Indices start at zero. The
project retains the selected frame and module identity. Durations and playback
are outside this frame-selection profile.

The reader uses the `ANIM` background for initial fill and rectangle disposal.
Frame replacement preserves straight RGBA8. Frame blending uses linear sRGB
and rounds each resulting canvas to RGBA8. Fully transparent blend results
are transparent black. Both raw and display reads expose the rendered canvas,
labeled `WebP (rendered animation frame)`. Lossy frame colors come from the
pinned decoder. [WebP container specification](https://developers.google.com/speed/webp/docs/riff_container#animation)

The profile accepts VP8L, VP8, and ALPH/VP8 frame payloads. It bounds the canvas
to 2,048 pixels per axis, the frame count to 256, and decoded rectangles to
64 Mi pixels in total. It checks RIFF extents, geometry and frame data through
the end of the animation, including frames after the selected one. ICC, EXIF,
XMP, and additional chunks require separate profiles.

Configure Release with the matching `QUPATHXX_READER_PLUGIN_KIT_PREFIX` and
`QUPATHXX_WEBP_SOURCE_DIR` pointing to the admitted libwebp selection. The
module reuses the static WebP codec and its guarded allocator. Build the native
SDK probe in `tests`, then run `tools/ci/check_webp_animation_reader_plugin.py`
with the retained writer fixture. The runtime does not require Python.
