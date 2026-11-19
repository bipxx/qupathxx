# Native JPEG 2000 reader

Select `org.qupathxx.jpeg2000` from a compatible Windows x64 MSVC Release
reader-plugin host. Version 1.13.0 uses image ABI1 and the
`jpeg2000-native-samples/2` contract.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.jpeg2000 --raw INPUT.jp2 OUTPUT.npy
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.jpeg2000 INPUT.j2k 0 0 0 WIDTH HEIGHT OUTPUT.pam
```

The module reads baseline Part 1 grayscale J2K and JP2 with enumerated gray
or sRGB color. The JP2 sRGB profile has three same-precision, full-resolution
components in R, G, B order, with optional reversible component transformation.
Gray and sRGB JP2 may have one trailing straight-alpha component at the
same precision. A channel definition box must explicitly associate that
component with whole-image opacity and identify the preceding color channels
in canonical order. Premultiplied and per-color opacity are outside this profile.
The reversible profile accepts full-resolution samples with 8 to 16 bits of
precision and no quantization. Signed samples require grayscale without alpha.
The irreversible profile accepts unsigned 8-bit grayscale with explicit scalar
quantization, one step per subband and exponents from 1 to 16. Its raw UInt8
values are JasPer's reconstruction of the encoded image. The JP2/J2K vendor
metadata marks this profile as irreversible. The verified fixtures differ by at
most one code from OpenJPEG 2.5.4 and replay exactly with the same module.
Both profiles accept the five Part 1 packet orders (LRCP, RLCP, RPCL, PCRL
and CPRL) with 1 to 16 quality layers and up to six decomposition levels.
The main coding header may declare one precinct size per resolution. The
reader bounds explicit grids at 2,097,152 packets over all tiles, components
and quality layers. The module decodes all declared layers. The
private JasPer build checks that each tile's complete packet sequence is
present before accepting its final tile-part boundary.
The main coding header may declare SOP markers, EPH markers, or both.
The reader checks SOP segment extents, validates sequence numbers when SOP
markers appear, and requires EPH at each packet header when declared.
Tile headers may carry sequential PLT packet-length segments. The reader
validates their coding and aggregate byte count. The decoder independently
derives packet boundaries from the codestream. PLT entries are advisory
and are unavailable as a random-access index.
The module supports grids up to 4096 tiles with zero image and tile origins.
Each tile may have up to 255 nonempty parts with a consistent explicit count.
Parts can be interleaved across tiles, with each tile's numbering consecutive
from zero. Grid dimensions, part sequences, extents and the final marker are
checked before decode. Partial edge tiles preserve image coordinates.
JP2 framing and the codestream must agree on dimensions and precision.
Raw output preserves the decoded component codes in packed HWC, using UInt8
or Int8 for 8-bit data and UInt16 or Int16 for higher precisions. Fully transparent pixels
retain their original color samples and count as valid source pixels.
Display maps the declared minimum and maximum codes to 0 and 255 per channel,
including alpha. JP2 capture resolution supplies horizontal and vertical
pixel spacing in micrometers when declared. Default display resolution alone
leaves measurement calibration unknown, as does a raw J2K codestream.

The profile bounds each axis to 8192 and the complete image to 4,194,304
pixels. Encoded storage, decoded storage, row scratch and the private JasPer
allocator share the reader work allowance. Cancellation is checked during
file read, framing, waiting for the decoder, sample transfer and region reads.
An in-flight JasPer decode checks cancellation when it returns.

Raw multi-component J2K, other irreversible profiles, other component layouts,
progression changes, other packet metadata, ICC profiles, tile-specific coding overrides,
unknown part counts, additional JP2 boxes, JPX and HTJ2K require separate
profiles. The host exposes one full-resolution level.

## Offline build

Use the matching ReaderPluginKit from the source package and the exact
JasPer 4.2.9 subset in `source/jasper`. The build checks every file against
the source lock and verifies membership. Select the installed MSVC toolset
recorded by the kit. Build and install the Release module with CMake.

```text
cmake -S source/plugins/jpeg2000 -B local-build -A x64 -T version=14.44.35207 -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=ABS_SOURCE/reader-plugin-kit -DQUPATHXX_JPEG2000_JASPER_SOURCE=ABS_SOURCE/jasper
cmake --build local-build --config Release --target qpx_jpeg2000_plugin
cmake --install local-build --config Release --prefix local-install
```

The package carries the AGPL-3.0-only adapter and private static
JasPer-2.0 sources and notices. JasPer's private build enables only the JPC
codec and applies the packet-completeness check from the module CMake file.
The package ships the original pinned JasPer source files and the build
instructions for the private change. This build requires the matching
compiler/CMake and Windows CRT.
The independent fixture generator uses Pillow/OpenJPEG, NumPy, the
OpenJPEG 2.5.4 command-line tools and FFmpeg's native JPEG 2000 encoder.
The packaged native module links only its private JasPer codec.
