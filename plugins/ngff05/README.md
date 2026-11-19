# OME-Zarr 0.5 Windows reader

This optional native provider reads local UInt8, Int8, UInt16, Int16, UInt32, Int32,
UInt64, Int64, Float32 and Float64 OME-Zarr 0.5 images with regular
Zarr 3 chunks or indexed shards, bytes encoding with optional Zstandard,
gzip or Blosc compression, and the verified
writer pyramid geometry. It supports original raw samples and a default RGBA
projection. Indexed shards use little- or big-endian UInt64 pairs at either
index location. The reader validates CRC32C when declared, checks range
boundaries and overlaps, and fetches only the index and requested inner byte
ranges unless a whole-shard checksum requires a scan.
Use a separate output location for raw exports.

The Zarr 3 transpose codec can permute regular chunks, inner chunks, and the
axes entering indexed sharding. Exports still use the logical image axes.

CRC32C on regular chunks and inner chunks is supported on either side of
compression. An outer CRC32C after indexed sharding is verified over the
complete serialized shard before reading its index or inner chunks. Scans use
1 MiB slices and are capped at 128 MiB per shard and 512 MiB per raw region
request. Shard index and inner checksums still apply.

Blosc accepts LZ4, LZ4HC, BloscLZ, Zstandard, zlib and Snappy frames with the declared no-shuffle,
byte-shuffle or bit-shuffle mode and blocks through 4 MiB. The plugin checks
frame extent, decoded size, compressor family and shuffle settings before
single-threaded decompression. LZ4 and LZ4HC use the same native frame family.
Embedded Zstandard frames use the same bounded decoder as ordinary Zstandard
chunks. They must contain one frame per compressed split with no dictionary.

Integer exports use the declared byte order and preserve exact sample codes,
including Int64 and UInt64 endpoints and values beyond binary64 precision.
Multibyte integer profiles require an explicit byte order. Fills must be JSON
integers in the dtype range. Without producer display settings, integer
channels map their full dtype range to RGBA8.

Floating-point raw export retains original IEEE 754 words. Float64 sparse
fills support JSON numbers, named nonfinite values and exact hexadecimal
words. The default display uses the 0–1 range and makes nonfinite pixels
transparent.

The optional `ome.omero` linear color display uses declared channel windows,
RGB colors, activity and inversion. Contributions are added per color and
clamped to RGBA8. Display stays on the host's selected Z and time plane.
Malformed or unsupported rendering settings produce a display error while
raw reads remain available. Channel labels are retained in raw metadata.

Use the host `qupathxx_region.exe` with the module and manifest in the extracted
package. Version 1.17.0 ships the provider and source kit. Use it with a
store-aware Windows native host. For example, with the host executable on `PATH`:

```powershell
$manifest = (Resolve-Path .\bin\ngff05-reader-plugins.json).Path
qupathxx_region.exe --reader-plugins $manifest --readers
qupathxx_region.exe --reader-plugins $manifest --reader org.qupathxx.ngff05 --raw INPUT.ome.zarr OUTPUT.npy --width 64 --height 64 --channel 0 --z 0 --time 0
```

Keep the module, manifest and exact OpenSlide DLL together. If the host has
loaded OpenSlide from its own directory, place the module and manifest beside
the host and its matching OpenSlide DLL. The manifest records the DLL digest
and the host checks the loaded path. The package is for Windows x64, MSVC 19.44.35225.0, and the
Release dynamic CRT. Use shard index pipelines consisting of bytes with
optional CRC32C. Keep the original
module and manifest for saved bindings that require their exact identity.

## Build from the matching source

For the module, configure `source/plugins/ngff05` with the Visual Studio 2022
generator, `-A x64 -T version=14.44.35207`, and the following paths:

| Setting | Path |
| --- | --- |
| `qupathxx_DIR` | Matching FP04 SDK `lib/cmake/qupathxx` |
| `QUPATHXX_READER_PLUGIN_KIT_PREFIX` | `source/reader-plugin-kit` |
| `QUPATHXX_NGFF05_JSON_INCLUDE` | `source/third_party/native-overlay/json/include` |
| `QUPATHXX_NGFF05_ZLIB_INCLUDE` | `source/third_party/zlib/include` |
| `QUPATHXX_NGFF05_ZSTD_ARCHIVE` | `source/third_party/zstd-1.5.7.tar.gz` |
| `QUPATHXX_NGFF05_BLOSC_ARCHIVE` | `source/third_party/c-blosc-v1.21.6.tar.gz` |
| `QUPATHXX_NGFF05_SNAPPY_ARCHIVE` | `source/third_party/snappy-1.2.2.tar.gz` |

Build and install configuration `Release`. The 1.9.0 host source kit documents
its separate region CLI build. For packages that carry that host's source,
configure
`source/tools/ci/ngff05_host` with that same generator and SDK. Set
`QPX_SOURCE_ROOT` to the package's `source` directory, `QPX_JSON_INCLUDE`
to the same JSON headers, and `QPX_STORE_HOST` to the exact
`native/ngff_native.lib` from the separate NGFF label package. Put the rebuilt
CLI next to the module and its manifest. Rebuilds have their own binary digests.

`SOURCE.json` identifies these exact inputs. `FROZEN-SOURCE-AVAILABILITY.json`
identifies the separately retained SDK, corresponding first-party archive and
upstream dependency sources. For redistribution, provide those matching
sources and notices as required by each component. This package includes the
source changes, build recipes, pinned Zstandard, Snappy and C-Blosc sources and
component notices. The build applies the packaged C-Blosc decoder patch to the
verified archive and reuses the SDK zlib and existing Zstandard decoder.
Close the CLI before uninstalling. Remove the files this package installed
from `PACKAGE.json` and retain any existing shared OpenSlide DLL.
