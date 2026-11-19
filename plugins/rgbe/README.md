# Native Radiance RGBE reader

`org.qupathxx.rgbe` 1.0.0 opens Radiance RGBE images with the `#?RADIANCE` or
`#?RGBE` signature, `32-bit_rle_rgbe` format and top-left `-Y height +X width`
order. Each dimension is 1–32,767, subject to the shared reader work budget.
It reads flat RGBE tuples and component-wise scanline RLE. Legacy repeat
markers, XYZE, alternate orientations and headers that prescribe exposure,
color correction, custom primaries or pixel aspect require separate profiles.
[Radiance format reference](https://radsite.lbl.gov/radiance/refer/Notes/picture_format.html)

Raw reads expose three linear Float32 channels in the source color basis.
Nonzero shared exponents use the midpoint of each RGBE quantization bin.
A zero exponent maps to exact black. Physical calibration is unspecified.
The opaque RGBA preview divides each channel by one plus the peak channel
in that pixel, then applies the sRGB transfer curve and rounds to eight bits.
This fixed preview curve leaves raw values intact and applies no gamut transform.

Build Release with the matching `QUPATHXX_READER_PLUGIN_KIT_PREFIX`. The
plugin uses the shared native reader ABI and has no third-party decoder.
Use the installed `reader-plugins.json` with the native host. Build the
project probe in `tests` against the native SDK and run
`tools/ci/check_rgbe_reader_plugin.py` with the retained exposure sweep.
The harness uses imagecodecs and NumPy for development fixtures and reference
values. The plugin and its SDK probe use C++ and run without Python.
