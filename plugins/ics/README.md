# Native ICS reader plugin

`org.qupathxx.ics` version 1.5.0 uses the `ics-raw-gzip-channel-window/3`
contract and the native image and source-file-set ABI 1. The C++
fluorescence-array reader links a private, prefixed build of zlib 1.3.2.

## Files and samples

ICS1.0 stores data in an adjacent, same-base `.ids` file at offset 0. The
reader derives `.IDS` for an uppercase `.ICS` header. If that derived name
is missing and the header declares gzip, the reader also tries the derived
name with `.gz` appended. The canonical companion takes precedence when both
names exist. Projects capture the path and identity of the selected companion.
ICS2.0 supports data
immediately following its `end` line or a single adjacent data file declared
by the paired, TAB-separated `source file` and `source offset` fields.

An external ICS2.0 header ends at EOF or at an `end` line followed immediately
by EOF. Its source offset is an unsigned decimal byte offset, including 0.
The source filename is one regular same-directory basename using ASCII
letters, digits, underscores, hyphens and dots, with no leading or trailing
dot. The reader refuses absolute paths, directory components, Windows device
names, aliases of the header, hard links, and reparse points.

The header declares `uncompressed` or `gzip` in the representation compression
field. For an uncompressed external ICS2.0 payload, the reader reads the exact
raster range declared by the offset and layout. Bytes outside that range are
left untouched. An external gzip payload spans the declared offset through EOF.
Gzip data consists of one complete member with a valid CRC32 and stored size
and exactly the decoded bytes declared by the layout. The reader refuses
truncated data, concatenated gzip members, bytes following a gzip member, other
compression schemes including Unix compress (`.ids.Z`).

Headers declare TAB fields and LF/CRLF line endings; canonical keys/enums are case
sensitive. Required fields declare version/filename, layout parameter count/order/
sizes/video coordinates/full-width significant bits, representation format/sign/
byte order/compression and parameter origin/scale/units for every slot.
The scalar `bits` slot comes first. Storage axes follow from fastest to slowest.
The reader accepts explicit `x`, `y` and optional `z`, `t` and `ch` in any order.
Examples:

```text
bits x y
bits x y z
bits ch x y
bits ch x y z
bits x y z ch
bits t z y x ch
```

Only explicit `ch` identifies channels; probe/wavelength/sensor axes are not
guessed. Channels are limited to1–256 within the frozen host's metadata budget.
UInt8, UInt16, Int16 and Float32 storage is supported. Each byte-order rank must
form a valid sample-byte permutation, applied without numeric conversion.

Raw arrays are native-endian HWC at every Z and T index, with ordinal names `C0`,
`C1`, etc. Scalar images also use `C0`. NaN payloads, signaling NaNs, infinities
and signed zero stay exact. Every source pixel is valid, including nonfinite
values; outside raw samples are zero/invalid. There is one resolution level.
Metadata reports the declared storage order, independently of the packed HWC
layout of each raw result.

## Interpretation and display

Video coordinates select the upper-left origin. Spatial and temporal origins must be zero.
Positive finite scales with `nm`, `um`, `mm`, `cm` or `m` supply micrometers per
pixel/plane; empty/`undefined` spatial units leave calibration unknown. Conversion
must remain finite and positive. Unrepresented directions/origins/units refuse;
no registration, rotation or resampling is inferred. Scalar origin/scale/unit
must be0/1/`relative`; channel origin/scale/unit0/1/empty-or-`undefined`.

For time, a positive finite scale with `s`, `ms` or `us` supplies an interval
in seconds. Empty or `undefined` units leave the interval unspecified.

Optional labels may match their axis token or use libics' default spatial
labels `x-position`, `y-position`, `z-position` and the label `time` for `t`.
Sensor, microscope, SCIL, and
unrecognized semantic fields refuse. Bounded descriptive history never implies
an orientation change. This is an array profile, not general acquisition metadata.

Native options select Z0/T0 grayscale display independently of raw samples:

```json
{"channel":0,"window":[0,255]}
```

Empty options use those defaults; the manifest records them explicitly. Nonempty
options require both keys in canonical compact form, an existing channel and
finite increasing window bounds with a finite positive span. Finite samples use
half-up quantization of `clamp((value-low)/(high-low),0,1)*255`, alpha255.
Nonfinite display is transparent black while raw validity remains1. Raw bits do
not change with display choices. Saved explicit options survive default changes.

## Source and resource bounds

Discovery reads only a bounded primary prefix. It returns the derived IDS path
for ICS1, the declared sibling path for external ICS2, and zero companions for
inline ICS2. It never reads companion pixels. Discovery and open use the same
path rule. The host tracks the identity and version of the primary and entire
companion before open and around reads and replay, including bytes outside an
external raw raster range.

The header limit is 64 KiB and the options limit is 512 bytes. The 256 MiB work
limit covers the primary, the bytes read from any companion, array bookkeeping,
and path/options/context costs. External reads seek to the declared offset
before allocating the payload buffer. For gzip the work limit also covers the
decoded array and 64 KiB of codec workspace. Byte permutation happens in
place. Source reads, header parsing, inflation, permutation and raw reads check
cancellation at bounded intervals.

Unsupported formats/semantics report unsupported; malformed selected fields or
missing/truncated data report I/O error. Request/size/work limits report invalid
request, actual allocation failure internal error and cancellation cancelled.
Raw access does not grant TIFF/Zarr-specific recipes or new UI plane controls.

## Build and use

Windows x64 MSVC1944/compiler19.44.35225.0, Release `/MD`, matching ReaderPluginKit:

```powershell
cmake -S plugins/ics -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_ICS_ZLIB_ARCHIVE=ZLIB_ARCHIVE"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

For a packaged source build, use `source/plugins/ics` as the source directory,
`source/reader-plugin-kit` as the kit prefix, and
`source/third_party/zlib-1.3.2.tar.gz` as the zlib archive. Keep the shared
headers at `source/plugins/image-common`. The default build uses those inputs
offline with the matching MSVC C and C++ compilers.
`QUPATHXX_ICS_BUILD_TESTS=ON` additionally requires the matching SDK and exact
libics development sources at `QUPATHXX_ICS_REFERENCE_SOURCE_DIR`. The
development build enables gzip for the libics oracle. The
[reference lock](tests/libics-reference-lock.json) records every source hash.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.ics INPUT.ics 0 0 0 512 512 OUTPUT.pam
```

Configure before catalog/source use. Restart module users to change configured
code; close them before removing verified package files, preserving images,
companions, projects and results. Automatic routing keeps its existing policy.

## Rights and evidence

Independent sources and authored fixtures retain the repository's
[AGPL-3.0-only license](../../LICENSE). The maintainer identifies
[ICS/ICS2 as open](https://svi.nl/ICSfileFormat); its published
[axis](https://svi-opensource.github.io/libics/Ics_DataRepresentation.html) and
[scalar](https://svi-opensource.github.io/libics/Ics_ImelRepresentation.html)
definitions and public reference sources establish the selected reusable layout.
The package includes the exact zlib 1.3.2 source archive and its original
Zlib notice at `licenses/zlib/LICENSE`.

The separately sourced development oracle uses libics 1.7.2 pinned at
`c1a1ae6f46ed1167001c0a14d9ef767c37ff4833`. Its
[per-file grant](https://github.com/svi-opensource/libics/blob/c1a1ae6f46ed1167001c0a14d9ef767c37ff4833/libics.h)
is Library GPL2-or-later. Its
[GNU_LICENSE](https://github.com/svi-opensource/libics/blob/c1a1ae6f46ed1167001c0a14d9ef767c37ff4833/GNU_LICENSE)
contains Lesser GPL2.1, the successor. These notices apply to the oracle's
development sources. The shipped module links only the admitted codec and
platform runtime.

The [delivery receipt](../../docs/benchmarks/ics-reader-plugin/README.md) records
actual source/file-set/package qualification. Broader encodings, platforms,
host/binding builds and biological/clinical/speed claims remain separate.
