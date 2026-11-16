# Optional Blosc decoded byte-array provider

`org.qupathxx.blosc-bytes`1.0.0 implements the existing native image-reader ABI,
contract `blosc1-lossless-flat-bytes/1`. Its purpose is lossless Blosc1 frame
expansion for the [native OMERO NGFF adapter](../../docs/omero-integration.md).
It exposes one UInt8 byte array (width = decoded bytes, height1, channel1), no
calibration, Z/T or biological channel interpretation. The outer native container
owns dtype/endian, shape, axes, transforms and source/result identity. Automatic
file selection and the frozen local Zarr reader are unchanged.

The selected codecs are BloscLZ and LZ4/LZ4HC, with generic shuffle/bitshuffle.
Encoded and decoded frames are each bounded to64MiB; header lengths, block/type
sizes, payload integrity and work are validated before a reader is published.
Single-thread context decoding accounts input, output, block scratch and opening
overhead within the ABI's maximum256MiB budget. Reads preserve every decoded byte;
OOB is zero with invalidity, while valid zero bytes remain valid. Display is a
separate grayscale rendering of bytes. Cancellation is checked around decoding
and within bounded read loops. This does not establish general Blosc2/Zarr3,
additional codec, whole-store or image semantics.

## Build without downloads

Use Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`, and the matching
ReaderPluginKit. Both compiler pins and all34 selected dependency members are
checked by CMake. The module statically links only the selected C-Blosc/LZ4 sources;
no application core, Python/Java, additional codec or runtime download is required.

```powershell
cmake -S plugins/blosc-bytes -B BUILD -G "Visual Studio 17 2022" -A x64 -T "v143,version=14.44.35207" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT -DQUPATHXX_BLOSC_SOURCE_DIR=BLOSC_SOURCE
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

`BLOSC_SOURCE` is an extracted copy of the exact C-Blosc1.21.6 archive recorded
in [source-lock.json](source-lock.json), archive SHA256
`9fcd60301aae28f97f1301b735f966cc19e7c49b6b4321b839b4579a0c156f38`.
No preparation script or Python is needed when those sources and the kit exist.
Keep the hash-bound generated `reader-plugins.json` with its module. A combined
manifest can also include the existing PNG provider for OMERO raster masks.

## Licenses and evidence

Adapter/kit: repository AGPL-3.0-only. C-Blosc1.21.6: BSD-3-Clause, including its
FastLZ-derived BloscLZ MIT terms and Windows pthread shim MIT terms. Bundled
LZ4 1.9.4: BSD-2-Clause. C-Blosc fastcopy derives from zlib-ng under Zlib terms.
Bitshuffle MIT and legacy MSVC header BSD-3-Clause notices are also retained for
locked source members, although their optional code paths are not compiled. Full applicable attribution/terms are retained in
[THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt); the source lock identifies
selected compile/header/license members. Distribution must retain those notices
and provide matching adapter/dependency source as required by their actual terms.
Plugin separation does not remove these obligations. No NC/research-only material
or upstream image/model fixture is used. See the
[reader inventory](../../docs/reader-license-policy.md) and
[native/public evidence](../../docs/benchmarks/omero/README.md) for exact binaries,
source identity and limitations, including the later cleanup module identity.
This is local component evidence, not a new
published app/SDK/wheel or a platform-wide qualification.
