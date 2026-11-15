# Native lossless WebP reader plugin

`org.qupathxx.webp` version1.0.0, contract
`webp-static-lossless-uint8-samples/1`, reads a declared static lossless profile
through the native image C ABI. The C++ adapter uses a private, pinned generic-C
libwebp1.6.0 decoder. Python is optional development control, never a runtime
decoder. Native options must be empty.

## Samples and supported container

The initial profile is SIMPLE RIFF/WEBP with exactly one VP8L chunk, lossless
version0. Declared RIFF/chunk extents must match the input exactly, including a
zero byte after an odd-sized chunk. Extra chunks, unexplained gaps/trailing bytes
and concatenated images refuse. Unsupported VP8, VP8X, ALPH, animation, ICCP,
EXIF, XMP and unknown chunks are refused rather than silently ignored. Invalid
selected headers or corrupt/truncated lossless data report native I/O errors;
unsupported container/encoding/version reports unsupported.

Raw samples are always original decoded UInt8 `Red`, `Green`, `Blue`, `Alpha`
in packed HWC order. Display is the same straight RGBA8, without premultiplication,
color conversion or quantization. Hidden RGB under alpha0 is preserved. The
VP8L alpha-is-used field is only a hint; it does not erase decoded alpha or change
raw channel count. Every in-image pixel is valid, including alpha0; outside raw
samples are zero/invalid and display padding is transparent black.

Lossless samples can differ from pixels that existed before an encoder discarded
hidden RGB or applied near-lossless preprocessing. The decoder preserves the
values actually encoded, without claiming recovery of discarded originals.

There is one level, `Z=0`, `T=0`; X/Y/Z/time calibration stays unknown. Metadata
and orientation are not inferred. Typed raw reads do not grant TIFF/Zarr views
or admit this reader into an otherwise restricted scientific recipe. The codec's
accepted padding within a VP8L bitstream is not an exact compressed-bitstream
consumption contract; the outer container's exact extents remain checked.

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

Selected profile: Windows x64 MSVC1944/compiler19.44.35225.0, Release `/MD`.
The exact source selection and all63 upstream member hashes are recorded in
[the source lock](libwebp-source-lock.json). No build fetches dependencies.
In a matching Visual Studio developer shell, with absolute paths:

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
catalog/source use and select the provider explicitly:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.webp INPUT.webp 0 0 0 512 512 OUTPUT.pam
```

Automatic routing and existing saved-source/currentness/implementation review
authorities retain their scope. Restart the host to change configured modules.
Close module users before removing verified package-owned files; preserve user
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
binds tested source/module/package identities and limits. No broader WebP variant,
platform, host/GUI/binding build, clinical/biological or speed qualification follows.
