# Native scalar NRRD reader plugin

`org.qupathxx.nrrd` version1.0.0, contract `nrrd5-raw-scalar-window/1`, is an
independently authored C++ image C ABI reader. The selected workflow opens small
scientific scalar images/volumes while keeping stored values separate from
display. There is no Teem/NrrdIO, application-core, codec or Python dependency.

## Selected file and axis contract

Accept attached `NRRD0005`, `encoding: raw`, dimension2 or3, with exactly the
declared payload bytes. Detached data, compression, skips, arbitrary additional
fields and custom key/value metadata refuse. Magic/fields/comments/blank separator
are ASCII; LF, CRLF and mixed line endings are supported. Field names and the
selected enumerated values are case insensitive. Quoted unit strings are case
sensitive. `dimension` precedes per-axis fields, as required by NRRD.
Duplicate/malformed/missing required fields refuse.

The first, fastest-varying axis is explicitly selected as local X, the next Y
and optional third Z. NRRD itself does not name these axes XYZ. This profile
requires `kinds` to declare every axis as `domain` or `space`; missing kinds,
color/vector/time/list axes and other dimensions are unsupported. It does not
guess that an unlabelled axis is a spatial volume or a color channel.

| NRRD type family | Native raw samples |
| --- | --- |
| unsigned8-bit aliases | UInt8 |
| unsigned16-bit aliases | UInt16 |
| signed16-bit aliases | Int16 |
| `float` | Float32, original IEEE bits |

Multibyte raw input must declare `endian: little` or `big`. Byte reversal creates
native-endian values without floating-point conversion. NaN payloads, signaling
NaNs, infinities and signed zero remain exact. Raw reads expose one `Scalar`
channel and every declared Z plane; all source pixels are valid, including
nonfinite values. Out-of-image samples are zero and invalid. One level and T0
are supported; the metadata dimension order is `XY` or `XYZ`.

## Calibration and display

Positive `spacings` with corresponding quoted `nm`, `um`, `mm`, `cm` or `m`
units become micrometers per pixel/plane independently. Absent/NaN spacing or
an empty unit leaves that axis calibration unknown. A unit alone never implies
spacing1. Unknown nonempty units and negative finite spacings are unsupported;
the latter are valid NRRD directions that this positive-spacing ABI cannot
represent. Zero/infinite spacings are malformed. Values are never made positive
with an absolute-value operation. Unit conversion must remain finite and positive;
unrepresentable calibration refuses.

World-space names, origins, directions, measurement frames, axis minima/maxima,
centers, thickness, scalar sample units and labels are not represented by this
ABI profile and refuse. `content` and comments are descriptive only. No geometry,
registration, physical origin or resampling is inferred or silently discarded.

Display is a grayscale view of Z0. Its native option is:

```json
{"window":[0,255]}
```

Empty options use that same default for every sample type, and the generated
manifest records it explicitly. Low/high must be finite, increasing, and have a
finite positive difference. For a finite stored value, display uses
`floor(clamp((value-low)/(high-low),0,1)*255+0.5)` with alpha255. Nonfinite display
pixels are transparent black while raw validity remains1. No gamma, automatic
extrema or data-dependent normalization applies. Window changes affect display
and source/result interpretation identity, without changing any raw bits.
Saved explicit options remain stable when provider defaults change.

## Resource and native boundary

Headers, including magic and the blank separator, are bounded to64KiB. Native
options are bounded to512 bytes. Exactly one owned input array holds the header
and raster; endian conversion occurs in place, and display is computed on demand
without a second volume or expanded RGBA cache. The input, array bookkeeping and
conservative path/options/owner allowance must fit a work budget≤256MiB.
Larger inputs refuse; this is a bounded full-input profile, not scalable arbitrary
volume streaming. Open/endian conversion and raw reads check synchronous
cancellation at bounded chunks/sample spans. No host/cancel pointer is retained.

Malformed selected inputs report I/O error; unsupported versions/types/axes/
encodings/metadata report unsupported. Size/request/work limits report invalid
request, actual allocation failure internal error and cancellation cancelled.
Typed reads do not grant TIFF/Zarr views, reader-specific recipes or UI plane
switching. Those interfaces keep their own contracts.

## Build, use and rights

Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`, matching ReaderPluginKit:

```powershell
cmake -S plugins/nrrd -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Source-kit paths are `source/plugins/nrrd` and `source/reader-plugin-kit`.
Tests default OFF; `QUPATHXX_NRRD_BUILD_TESTS=ON` additionally requires the matched
SDK for the project consumer. Builds perform no dependency fetch or Python
discovery. Configure the hash-bound manifest before opening/catalog use:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.nrrd INPUT.nrrd 0 0 0 512 512 OUTPUT.pam
```

The independent parser and authored test inputs retain the repository's
[AGPL-3.0-only license](../../LICENSE). The published
[NRRD definition](https://teem.sourceforge.net/nrrd/format.html) and MIT-licensed
[pynrrd reference](https://github.com/mhe/pynrrd/tree/v1.1.3) establish the public,
reusable layout/profile; their actual licenses are distinct from the adapter.
No upstream implementation or image is copied into this parser/package.
pynrrd1.1.3 is an independent development oracle only, outside runtime and package
dependencies. Native code remains the application's parsing authority.

The [source-scoped receipt](../../docs/benchmarks/nrrd-reader-plugin/README.md)
records actual qualification and remaining scope. Automatic routing, the frozen
host/SDK/bindings and separate UI work retain their existing scope. Close module
users before removing verified package files, preserving user data.
