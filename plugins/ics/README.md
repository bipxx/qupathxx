# Native uncompressed ICS reader plugin

`org.qupathxx.ics` version1.0.0, contract `ics-raw-channel-window/1`, is an
independently authored C++ fluorescence-array reader using the native image and
source-file-set C ABI. The module needs no libics, codec, core or Python runtime.

## Files and samples

The `.ics` primary uses ICS1.0 with adjacent same-base `.ids` data at offset0,
or ICS2.0 with data immediately following its `end` line. Uppercase `.ICS` derives
`.IDS`. Header `filename` is descriptive, never a path source. External source
paths/offsets, compressed data and `.ids.gz`/`.ids.Z` auto-discovery refuse.
Primary/companion extents are exact, without unexplained gaps or trailers.

Headers declare TAB fields and LF/CRLF line endings; canonical keys/enums are case
sensitive. Required fields declare version/filename, layout parameter count/order/
sizes/video coordinates/full-width significant bits, representation format/sign/
byte order/uncompressed encoding and parameter origin/scale/units for every slot.
Supported orders, fastest axis first after the scalar bits slot:

```text
bits x y
bits x y z
bits ch x y
bits ch x y z
```

Only explicit `ch` identifies channels; probe/wavelength/sensor axes are not
guessed. Channels are limited to1–256 within the frozen host's metadata budget.
UInt8, UInt16, Int16 and Float32 storage is supported. Each byte-order rank must
form a valid sample-byte permutation, applied without numeric conversion.

Raw arrays are native-endian HWC across all Z planes, with ordinal names `C0`,
`C1`, etc. Scalar images also use `C0`. NaN payloads, signaling NaNs, infinities
and signed zero stay exact. Every source pixel is valid, including nonfinite
values; outside raw samples are zero/invalid. There is one level and T0;
metadata order is `XY`, `XYZ`, `CXY` or `CXYZ`.

## Interpretation and display

Video coordinates select the upper-left origin. Spatial origins must be0.
Positive finite scales with `nm`, `um`, `mm`, `cm` or `m` supply micrometers per
pixel/plane; empty/`undefined` spatial units leave calibration unknown. Conversion
must remain finite and positive. Unrepresented directions/origins/units refuse;
no registration, rotation or resampling is inferred. Scalar origin/scale/unit
must be0/1/`relative`; channel origin/scale/unit0/1/empty-or-`undefined`.

Optional axis labels may only repeat canonical `intensity`, `ch`, `x`, `y`, `z`
meanings. They are not fluorophore names. Other sensor, microscope, SCIL, source
or unknown semantic fields refuse. Bounded descriptive history never implies
an orientation change. This is an array profile, not general acquisition metadata.

Native options select Z0 grayscale display independently of raw samples:

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

Discovery reads only a bounded primary prefix, returning one derived IDS path for
ICS1 and none for ICS2. It never reads companion pixels. The host observes the
primary and declared companion identities/versions before open and around reads/
replay. Discovery and open use the same interpretation/path rule.

Header/options limits are64KiB/512 bytes. Open owns the complete primary and,
for ICS1, the IDS array. Byte permutation is in place; display has no expanded
cache. All owned arrays/bookkeeping and conservative path/options/context costs
fit work≤256MiB. Larger inputs refuse; this is a bounded full-input profile.
Source/header/permutation/raw operations honor synchronous cancellation at bounded
spans, retaining no caller pointers.

Unsupported formats/semantics report unsupported; malformed selected fields or
missing/truncated data report I/O error. Request/size/work limits report invalid
request, actual allocation failure internal error and cancellation cancelled.
Raw access does not grant TIFF/Zarr-specific recipes or new UI plane controls.

## Build and use

Windows x64 MSVC1944/compiler19.44.35225.0, Release `/MD`, matching ReaderPluginKit:

```powershell
cmake -S plugins/ics -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Kit paths are `source/plugins/ics` and `source/reader-plugin-kit`. Default tests
OFF need only C++ and the kit, with no network/C-library/Python discovery.
`QUPATHXX_ICS_BUILD_TESTS=ON` additionally requires the matching SDK and exact
libics development sources at `QUPATHXX_ICS_REFERENCE_SOURCE_DIR`. The
[reference lock](tests/libics-reference-lock.json) records every source hash;
the library is neither bundled nor linked into the product module.

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
No upstream implementation/image or closed format branch enters this parser.

Development-only libics1.7.2 is pinned at
`c1a1ae6f46ed1167001c0a14d9ef767c37ff4833`. Its
[per-file grant](https://github.com/svi-opensource/libics/blob/c1a1ae6f46ed1167001c0a14d9ef767c37ff4833/libics.h)
is Library GPL2-or-later; bundled
[GNU_LICENSE](https://github.com/svi-opensource/libics/blob/c1a1ae6f46ed1167001c0a14d9ef767c37ff4833/GNU_LICENSE)
contains Lesser GPL2.1, the successor. Those notices stay distinct from the
adapter's license. No reference images/archive, zlib or NC component is required.

The [delivery receipt](../../docs/benchmarks/ics-reader-plugin/README.md) records
actual source/file-set/package qualification. Broader encodings, platforms,
host/binding builds and biological/clinical/speed claims remain separate.
