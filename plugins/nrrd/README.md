# Native scalar NRRD reader plugin

`org.qupathxx.nrrd` version 1.14.0, contract `nrrd-scalar-window/15`, is an
independently authored C++ image C ABI reader. The selected workflow opens small
scientific scalar images/volumes while keeping stored values separate from
display. The module links zlib 1.3.2 and libbzip2 1.0.8 statically and uses no Teem/NrrdIO,
application core or Python.

## Selected file and axis contract

Accept attached `NRRD0003`, `NRRD0004` and `NRRD0005`, or a matching detached
header, of dimension 2, 3 or 4 with `encoding: raw`, `gz`, `gzip`, `bz2`, `bzip2`, `ascii`,
`text`, `txt` or `hex`.
Raw payloads with no positive line skip or explicit byte skip must match the
declared byte count.
Compressed payloads must consist of one complete gzip or bzip2 stream with
valid codec checksums and exactly the declared decoded byte count, including
any skipped prefix. Gzip also validates its stored size field.
Trailing compressed bytes, concatenated streams, other encodings,
arbitrary additional fields and custom
key/value metadata refuse. Header content is ASCII. LF, CRLF and mixed line
endings are supported. Field names and the
selected enumerated values are case insensitive. Quoted unit strings are case
sensitive. `dimension` precedes per-axis fields, as required by NRRD.
Duplicate/malformed/missing required fields refuse.

Text payloads contain exactly the declared scalar count, separated by ASCII
whitespace. Leading and trailing ASCII whitespace is allowed. Tokens have a
128-byte limit. The decoder accepts decimal integers and Float32 or Float64 decimal or
scientific notation, with explicit `inf`, `infinity` and `nan` spellings.
Text NaN maps to one quiet NaN. Finite values must fit their declared type.
Each float type preserves rounded finite values at its declared width, signed zero and signed infinity.

Hexadecimal payloads contain exactly two case-insensitive digits per binary
byte. The reader skips six ASCII whitespace characters, including between digits.
Hex preserves the declared endian order and exact raw bits through the same
native conversion as raw input. Half-bytes, non-hex markers and additional
digits are rejected. The decoder reuses the owned encoded buffer.

`byte skip` (alias `byteskip`) accepts a nonnegative signed-32-bit count
relative to the payload start. Gzip and bzip2 apply the offset after decompression.
`line skip` (alias `lineskip`) accepts a nonnegative signed-32-bit count of
LF- or CRLF-terminated lines. It applies before byte skipping and before
decompression, with a 256 MiB detached preamble scan limit. Raw `byte skip: -1`
uses the raster at EOF and ignores `line skip`. A positive line skip or explicit
byte skip permits taking the declared raw range from a larger file. The detached
reader scans preambles in fixed-size blocks and reads only the selected raw
range after scanning, under the observed companion identity. Attached input and
the retained encoded data for other encodings stay within the reader work budget.

A version 3 detached header declares one `./basename` relative to its own
directory. Version 4 and 5 detached headers declare a basename with an optional `./`,
`LIST [subdim]`, or a numbered series using `data file` or `datafile`.
With `LIST`, enter one basename per line in data
order through EOF. For a numbered series, give the format, signed 32-bit first
index, inclusive last bound, nonzero step, and optional subdimension. For
example, `data file: slice-%03d.raw 1 8 3` refers to indices 1, 4 and 7 in that
order. Use one `%d` or `%i` conversion, optionally with zero-padded width.
For both series forms, the default subdimension is one less than the image rank.
A lower subdimension splits along the slower axes. A full-rank subdimension
allows equal slabs along the slowest axis. Each file uses the declared byte
order, encoding and offsets independently. Each series allows up to 63 unique
companions and must fit within the 64 KiB header limit.

The basename uses ASCII letters, digits, underscores, hyphens, interior dots
and interior spaces. A LIST entry can start with `./`. Numbered patterns remain
single tokens. Keep the original basenames and place all files in the same
directory. Absolute paths, other path components, other format conversions, Windows device names,
reparse points, directories and aliases of the header are outside this profile.
A single-companion or numbered header can end at the last field or with a blank separator
and no following bytes. Attached headers require the blank separator before the payload.

The first, fastest-varying axis is local X and the next is Y. A third spatial
axis is Z. Spatial axes require `domain` or `space`. An explicit final
`time` kind adds T after XY or XYZ. This profile rejects missing kinds,
misplaced or duplicate time axes, and non-scalar kinds such as color and list.

| NRRD type family | Native raw samples |
| --- | --- |
| unsigned 8-, 16-, 32- and 64-bit aliases | UInt8, UInt16, UInt32, UInt64 |
| signed 8-, 16-, 32- and 64-bit aliases | Int8, Int16, Int32, Int64 |
| `float` | Float32, original IEEE bits for binary input |
| `double` | Float64, original IEEE bits for binary input |

Multibyte binary input must declare `endian: little` or `big`. Byte reversal creates
native-endian values without floating-point conversion. Binary NaN payloads, signaling
NaNs, infinities and signed zero remain exact. Text input is byte-order independent.
Raw reads expose one `Scalar`
channel and every declared Z/T plane. All source pixels are valid, including
nonfinite values. Out-of-image samples are zero and invalid. Metadata records
one level and the dimension order `XY`, `XYZ`, `XYT` or `XYZT`.

## Calibration and display

Positive `spacings` with corresponding quoted `nm`, `um`, `mm`, `cm` or `m`
units become micrometers per pixel/plane independently. Absent/NaN spacing or
an empty unit leaves that axis calibration unknown. A unit alone never implies
spacing1. Unknown nonempty units and negative finite spacings are unsupported;
the latter are valid NRRD directions that this positive-spacing ABI cannot
represent. Zero/infinite spacings are malformed. Values are never made positive
with an absolute-value operation. Unit conversion must remain finite and positive;
unrepresentable calibration refuses.

A positive time spacing with unit `s`, `ms`, `us` or `ns` gives the interval
in seconds. Absent spacing, NaN or an empty time unit leaves that interval
unknown.

For an unnamed local spatial basis in version 4 or 5, `space dimension` must be 2 or 3 and match
the number of spatial axes. Declare it before `space directions`, `space units`
and any `space origin`. Each spatial direction must be a positive diagonal
vector in X, Y, Z order, with explicit length units per basis component. Omit
the origin or declare all zeros. A final time axis uses direction `none` and
can retain its own per-axis spacing and time unit. Spatial per-axis spacings
must be absent or NaN and spatial per-axis units absent or empty when direction
vectors supply the calibration.

Named world spaces, nonzero origins, permutations, reflections, oblique basis
vectors, measurement frames, axis minima/maxima, centers, thickness, scalar
sample units and labels are outside this profile. `content` and comments are
descriptive. Admitted calibration leaves the stored array order unchanged.

Display is a grayscale view of Z0 at T0. Its native option is:

```json
{"window":[0,255]}
```

Empty options use that same default for every sample type, and the generated
manifest records it explicitly. Low/high must be finite, increasing, and have a
finite positive difference. For a finite stored value, display uses
`floor(clamp((value-low)/(high-low),0,1)*255+0.5)` with alpha255. Display converts
integers to binary64 before windowing, so adjacent 64-bit integers can share a
display value. Native raw reads preserve their exact integer codes. Nonfinite display
pixels are transparent black while raw validity remains1. No gamma, automatic
extrema or data-dependent normalization applies. Window changes affect display
and source/result interpretation identity, without changing any raw bits.
Saved explicit options remain stable when provider defaults change.

## Resource and native boundary

Headers, including magic and any blank separator, are bounded to 64 KiB. Native
options are bounded to 512 bytes. The work budget is at most 256 MiB and covers
the header, encoded data, array bookkeeping and a path/options/owner allowance.
Gzip and text decoding additionally charge the declared decoded raster to the
same budget. Gzip adds 64 KiB of codec workspace. Bzip2 uses the small decoder
under a 3 MiB workspace allowance. For a list, the full output
raster and one temporary member are charged together. The detached preamble
scan limit applies across the file set. Endian conversion
occurs in place, and display is computed on demand. Opening, decoding, endian
conversion and raw reads check synchronous cancellation at bounded intervals.
No host/cancel pointer is retained.

The plugin reports all companion paths through the existing reader file-set
extension. The host captures and verifies the header and every payload identity
together for detached sources. Changing a payload invalidates reads and replay
against the original observed source.

Malformed selected inputs report I/O error; unsupported versions/types/axes/
encodings/metadata report unsupported. Size/request/work limits report invalid
request, actual allocation failure internal error and cancellation cancelled.
Typed reads do not grant TIFF/Zarr views, reader-specific recipes or UI plane
switching. Those interfaces keep their own contracts.

## Build, use and rights

Windows x64, MSVC1944/compiler19.44.35225.0, Release `/MD`, matching ReaderPluginKit:

```powershell
cmake -S plugins/nrrd -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_NRRD_ZLIB_ARCHIVE=ZLIB_ARCHIVE" `
  "-DQUPATHXX_NRRD_BZIP2_ARCHIVE=BZIP2_ARCHIVE"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

Source-kit paths are `source/plugins/nrrd`, `source/reader-plugin-kit` and
`source/third_party/zlib-1.3.2.tar.gz` and `source/third_party/bzip2-1.0.8.tar.gz`.
Tests default OFF; `QUPATHXX_NRRD_BUILD_TESTS=ON` additionally requires the matched
SDK for the project consumer. Builds perform no dependency fetch or Python
discovery. Configure the hash-bound manifest before opening/catalog use:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.nrrd INPUT.nrrd 0 0 0 512 512 OUTPUT.pam
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.nrrd --raw INPUT.nhdr OUTPUT.npy --width 512 --height 512 --z 0
```

The independent parser and authored test inputs retain the repository's
[AGPL-3.0-only license](../../LICENSE). The published
[NRRD definition](https://teem.sourceforge.net/nrrd/format.html) and MIT-licensed
[pynrrd reference](https://github.com/mhe/pynrrd/tree/v1.1.3) establish the public,
reusable layout/profile; their actual licenses are distinct from the adapter.
The package includes the exact zlib 1.3.2 and libbzip2 1.0.8 source archives and
their original notices in `licenses/zlib/LICENSE` and `licenses/bzip2/LICENSE`.
pynrrd1.1.3 is an independent development oracle only, outside runtime and package
dependencies. Native code remains the application's parsing authority.

The [source-scoped receipt](../../docs/benchmarks/nrrd-reader-plugin/README.md)
records actual qualification and remaining scope. Automatic routing, the frozen
host/SDK/bindings and separate UI work retain their existing scope. Close module
users before removing verified package files, preserving user data.
