# Native still-image WebP reader plugin

`org.qupathxx.webp` version 1.2.0 uses the `webp-static-uint8-samples/3`
contract and the native image ABI 1. The C++ adapter uses a private, pinned
generic C libwebp 1.6.0 decoder. Native options must be empty.

## Samples and supported container

The reader supports the following RIFF/WEBP still-image layouts.

- One `VP8L` chunk with a lossless version 0 bitstream.
- One `VP8 ` chunk with an opaque lossy frame.
- Exactly `VP8X`, `ALPH`, and `VP8 ` in that order. The extended header declares
  only the alpha feature and its canvas matches the single lossy frame.
- A `VP8X` header declaring EXIF, with the alpha flag permitted. The frame is
  opaque `VP8 `, `ALPH` followed by `VP8 `, or `VP8L`. One orientation-only
  `EXIF` chunk follows `VP8X` and may appear before or after the frame. The
  extended canvas matches the encoded frame dimensions.

Declared RIFF and chunk extents must match the input, including a zero byte
after an odd-sized chunk. The reader rejects extra chunks, trailing bytes,
concatenated images, other extended profiles, animation, ICCP, and XMP.
Malformed selected layouts and corrupt or truncated image data report native
I/O errors. Unsupported containers, features, and versions report unsupported.

Raw samples and display use the pinned decoder's straight UInt8 `Red`,
`Green`, `Blue`, `Alpha` in packed HWC order. Opaque VP8 yields alpha 255.
Libwebp reconstructs the lossy RGB values from the encoded frame, so they can
differ from the original pixels supplied to the encoder. For VP8L, the reader
preserves the exact decoded values, including hidden RGB under alpha zero.
The VP8L alpha-is-used field leaves alpha and channel count intact. Every
in-image pixel is valid, including alpha zero. Outside raw samples are zero
and invalid, and display padding is transparent black.

There is one level with `Z=0` and `T=0`. X/Y/Z/time calibration is unknown.
Libwebp determines acceptable bitstream padding within the checked container.

## EXIF orientation

The EXIF chunk is limited to 64 KiB. It contains classic TIFF in either byte
order, with an optional six-byte `Exif\0\0` prefix. Its first image file
directory contains exactly one entry, Orientation (tag 274), with TIFF type
SHORT, count 1, and a value from 1 through 8. The next-directory offset must be
zero. Other EXIF tags and additional directories are outside this profile.

Metadata, display and raw region reads use the same oriented pixel coordinates.
Orientations 5 through 8 swap the exposed dimensions. The reader maps each
requested pixel to its exact decoded RGBA sample, using one decoded raster and
preserving alpha and channel values across reflections and rotations. Stills
without EXIF use the encoded frame coordinates.

## Work, cancellation and build

Owned input/output, conservative path/owner allowances and every codec heap
allocation must fit the requested work budget, at most256MiB. Checked private
allocation interception includes allocation headers and decoder scratch. Resource
limits report invalid request; actual allocator failure reports internal error.
Codec calls run synchronously behind a native gate. Cancellation is checked
before/after each incremental decode call and at codec allocation boundaries;
raw reads check rows/bounded pixel spans. A compact image can finish in one codec
call, so no fixed latency or per-row in-codec cancellation guarantee is made.
No source, host-buffer or cancellation pointer survives the operation.

The selected build uses Windows x64, MSVC 1944 compiler 19.44.35225.0, and
Release `/MD`. The exact source selection and all 63 upstream member hashes are recorded in
[the source lock](libwebp-source-lock.json). No build fetches dependencies.
Use absolute paths in a matching Visual Studio developer shell.

```powershell
cmake -S plugins/webp -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_WEBP_SOURCE_DIR=LIBWEBP"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

In the source kit these paths are `source/plugins/webp`,
`source/reader-plugin-kit` and `source/deps/libwebp`. Tests default OFF;
`QUPATHXX_WEBP_BUILD_TESTS=ON` additionally needs the matching native SDK for the
test consumer. The module links no application core, Python, external codec DLL,
image library, encoder, mux/demux, SIMD or threading library. Configure before
catalog/source use and select the provider explicitly.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.webp INPUT.webp 0 0 0 512 512 OUTPUT.pam
```

Automatic routing and existing saved-source/currentness/implementation review
authorities retain their scope. Restart the host to change configured modules.
Close module users before removing verified package-owned files. Preserve user
images, projects and results.

## Licenses and evidence

Adapter, control/test sources and authored fixtures retain the repository's
[AGPL-3.0-only license](../../LICENSE). Unmodified selected libwebp sources at
commit4fa21912338357f89e4fd51cf2368325b59e9bd9 retain their own
[BSD-3-Clause terms](https://github.com/webmproject/libwebp/blob/4fa21912338357f89e4fd51cf2368325b59e9bd9/COPYING),
[PATENTS notice](https://github.com/webmproject/libwebp/blob/4fa21912338357f89e4fd51cf2368325b59e9bd9/PATENTS)
and AUTHORS. All are included with binary and corresponding-source packages.
No upstream image/test asset, restricted derivative or noncommercial component
is required. The open format reference is
[RFC9649](https://www.rfc-editor.org/rfc/rfc9649.html); its license is distinct
from the decoder/adapter licenses.

The [delivery receipt](../../docs/benchmarks/webp-reader-plugin/README.md)
binds the tested source, module and package identities to the selected
still-image profiles.
