# Native PCX reader

Select `org.qupathxx.pcx` from a compatible Windows x64 MSVC Release
reader-plugin host. Version 1.1.0 uses image ABI1 and the
`pcx-native-samples/1` contract. A project linked with version 1.0.0
requires source review before it uses version 1.1.0.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.pcx --raw INPUT.pcx OUTPUT.npy --channels 3
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.pcx INPUT.pcx 0 0 0 WIDTH HEIGHT OUTPUT.pam
```

The reader accepts version-5 RLE with eight bits per plane. Three-plane
rasters retain exact UInt8 RGB. Single-plane rasters retain exact UInt8
indices and use the required VGA palette for display, including gray
palettes. Use `--channels 1` for a single-plane image. Equal palette colors
remain distinct raw indices.

Version 1.1.0 also reads version-2 and version-5 RLE with four one-bit
planes and an RGB header palette. Raw exports contain one UInt8 index
per pixel in the range 0–15. Display uses the corresponding header color.
Each plane is unpacked most significant bit first. Footers and palette
trailers are outside the four-plane profile.

Inclusive PCX window coordinates map to a top-left image lattice. The
reader honors the declared even per-plane stride, skips padding and
accepts runs crossing planes within a scanline. It rejects runs crossing
scanlines and unexplained data between the final scanline and the palette
or end of file. Device-resolution hints do not establish physical calibration.
The module bounds the full padded raster and shares the work allowance
between encoded input, decoded output and row scratch.

Other bit layouts and versions, extra bottom scanlines and DCX containers
are outside these profiles.

## Offline build

Build with the matching ReaderPluginKit, MSVC 19.44, and the Release dynamic CRT.

```text
cmake -S source/plugins/pcx -B local-build -A x64 -T version=14.44.35207 -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=ABS_SOURCE/reader-plugin-kit
cmake --build local-build --config Release --target qpx_pcx_plugin
cmake --install local-build --config Release --prefix local-install
```

The independently authored adapter and matching source use AGPL-3.0-only.
The [ZSoft technical reference](https://msxcomputermagazine.nl/mccm/millennium/milc/formats/topic_0.htm)
is referenced for format semantics. The package includes the adapter and its
kit without external codecs or upstream reference materials. Pillow and
NumPy are development-only checks.
