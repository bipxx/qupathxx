# Native TGA reader

Select `org.qupathxx.tga` from a compatible Windows x64 MSVC Release
reader-plugin host. Version 1.1.0 uses the image ABI1
`tga-native-samples/1` contract. Projects linked with version 1.0.0 need
source review when switching to version 1.1.0.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.tga --raw INPUT.tga OUTPUT.npy --channels 3
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.tga INPUT.tga 0 0 0 WIDTH HEIGHT OUTPUT.pam
```

The optional reader accepts the public Truevision type 2 and type 10 true-color
images with 24-bit BGR or 32-bit BGRA and eight declared attribute bits.
Use `--channels 4` for BGRA. Raw data retains exact UInt8 RGB or RGBA samples,
including RGB under zero alpha. Display uses straight RGBA8 and opaque alpha
for RGB. The four descriptor origins map to a top-left image lattice.
Physical calibration is unknown.

Version 1.1.0 also reads 8-bit grayscale (types 3 and 11) and 8-bit
indexed images with a 24-bit BGR palette (types 1 and 9). For these profiles,
use `--channels 1`. Raw values are the gray codes or exact palette indices.
Display maps gray to opaque RGB or looks up the stored palette color.
Partial palettes honor the declared first index and reject pixels outside
the palette range. Equal display colors can have distinct raw indices.

Legacy files may omit the Truevision 2.0 footer. The selected footer accepts
an empty extension or one contiguous 495-byte extension with no raster
settings and an explicit matching alpha type. Textual metadata and image
IDs are skipped. Version 2 repeat packets stay within a scanline. The reader
checks packet and raster extents, rejects trailing bytes, and bounds encoded
and decoded storage under the existing reader work allowance.

Other palette and gray depths, undefined/premultiplied alpha, interleaving,
nonzero origin coordinates, developer blocks and raster-setting extensions
require separate profiles.

## Offline build

Build the source package with its matching ReaderPluginKit, Windows x64
MSVC 19.44, and Release dynamic CRT.

```text
cmake -S source/plugins/tga -B local-build -A x64 -T version=14.44.35207 -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=ABS_SOURCE/reader-plugin-kit
cmake --build local-build --config Release --target qpx_tga_plugin
cmake --install local-build --config Release --prefix local-install
```

The independently authored native reader and matching source use
AGPL-3.0-only. The [Truevision specification](https://raw.githubusercontent.com/DEAKSoftware/Truevision-TGA/master/truevision-tga.pdf)
is referenced as a schema and stays outside the package. The module links
no external codecs. Pillow and NumPy are development-only verification tools.
