# Native scalar NRRD plugin: FP02 local Windows delivery

## Spaced single and LIST companion names in 1.14.0

`org.qupathxx.nrrd` 1.14.0 uses contract `nrrd-scalar-window/15`. It reads
interior ASCII spaces in same-directory single and LIST companion names and
accepts a single `./` prefix in those declarations. A pynrrd-written gzip pair
established the prior refusal. The direct ABI checks verify exact stored and
display samples, ordered source paths, duplicate names after prefix removal,
Windows device names with internal whitespace and traversal refusals.
The same pinned zlib and bzip2 sources and notices apply to this package.
[Validation report](../../../build/nrrd-reader/spaced-companion/validation/validation.json)

## Version 3 and 4 scalar headers in 1.13.0

`org.qupathxx.nrrd` 1.13.0 uses contract `nrrd-scalar-window/14`. The native
reader accepts NRRD0003, NRRD0004 and NRRD0005 for the declared scalar profile.
Version 3 can address one detached file with `./basename`. The reader
resolves that basename beside the header using the existing guarded file-set
resolver. Version 4 also supports multiple payloads and the selected
space-direction profile. [NRRD format and version rules](https://teem.sourceforge.net/nrrd/format.html)

Validation covers 166 valid profiles, 175 refusals and 64 native commands.
Version 3 attached and detached reads agree with pynrrd. Version 4 preserves
the exact calibrated XYZ raster and opens a detached XYZT project with all
companion identities and exact Z/T samples on a fresh process. Both detached
formats export the same Z1/T2 values. Earlier versions, nonlocal companion
paths and format extensions remain outside this profile.

The validated module SHA-256 is
`b87afb07b43931d6e6c7361d193c95d2cddec5d2a0b832ac01b4c7f013678c21`.
[Baseline](../../../build/nrrd-reader/legacy-version-candidate/baseline.json) ·
[Validation](../../../build/nrrd-reader/legacy-version-candidate/validation/validation.json)

## Axis-aligned space directions in 1.12.0

`org.qupathxx.nrrd` 1.12.0 uses contract `nrrd5-scalar-window/13`. The
reader maps a positive diagonal basis in an unnamed local space into native
dimensions and micrometer calibration. It accepts a zero or omitted origin
and uses the NRRD `none` direction for an optional independent time axis.
The original array order is preserved. [NRRD orientation fields](https://teem.sourceforge.net/nrrd/format.html#spacedirections)

The attached producer fixture uses pynrrd 1.1.3 with mixed length units.
The native checks include two-dimensional length conversion, a detached
XYZT gzip LIST project, Z1/T2 CLI export, and refusal of nonzero origin, shear,
reflection, conflicting per-axis fields and malformed vectors.
The retained and new corpus totals 162 profiles, 168 refusals and 63 native
commands. The module SHA-256 is
`193b659a436fb62e3ea65a8e80df9bf823305265777b57d29becc24bcdab8b9f`.
The exact source package keeps the existing AGPL, Zlib and bzip2 notices.
[Baseline](../../../build/nrrd-reader/geometry-candidate/baseline.json) ·
[Validation](../../../build/nrrd-reader/geometry-candidate/validation/validation.json)

## Scalar time series in 1.11.0

`org.qupathxx.nrrd` 1.11.0 uses contract `nrrd5-scalar-window/12`. In the
[NRRD format definition](https://teem.sourceforge.net/nrrd/format.html#kinds),
`time` identifies a temporal domain. The native profile accepts that kind
as the final axis of XYT or XYZT with optional calibrated time in seconds.
Raw requests preserve the selected Z/T samples. Display uses Z0/T0.

Validation covers 159 valid profiles, 159 refusals and 63 native commands.
The added checks include big-endian attached XYZT, gzip LIST frames, XYT text
with unknown time spacing, and numbered bzip2 time slabs. They verify exact
raw data, axes and calibration in an SDK project opened in separate processes,
plus host NPY export at Z1/T2. The NRRD, zlib and libbzip2 materials carry
the same applicable terms as 1.10.0.

The validated module SHA-256 is
`e633322d4ee5e1b7cc3a33db230b9add2f2d6e39498486e90933985de00629a4`.
[Validation](../../../build/nrrd-reader/time-series-candidate/validation-corrected/validation.json)

## Bzip2 scalar payloads in 1.10.0

`org.qupathxx.nrrd` 1.10.0 uses contract `nrrd5-scalar-window/11`. The
`bzip2` and `bz2` spellings read a single checked stream per payload. Line
skips precede decompression, byte offsets apply to decoded data, and the raw
endian and source guards are shared with the other encodings.

The linked libbzip2 1.0.8 library comes from the checksum- and signature-verified
[Sourceware archive](https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz).
The unmodified archive and its original notice are in the source package.
The library uses the `bzip2-1.0.6` license text, and the plugin compiles only
the seven library units. The existing statically linked Zlib dependency keeps
its own notice. The adapter and authored fixtures remain AGPL-3.0-only.

Local checks cover 155 valid profiles, 153 refusals and 60 native commands.
They include multi-block decoding, bounded workspace, exact UInt64 and Float64
bits, indicated file sets, corrupt, short, excess and concatenated streams,
plus exact installed project reopen and replay.
[Baseline](../../../build/nrrd-reader/bzip2-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/bzip2-candidate/results.json)

## Hexadecimal scalar payloads in 1.9.0

`org.qupathxx.nrrd` 1.9.0 uses contract `nrrd5-scalar-window/10`. The hex
decoder converts case-insensitive ASCII digit pairs in the owned encoded
buffer and shares native endian conversion. The same rules apply to attached,
detached, listed and numbered data. Line and byte skips precede hex decoding.

Local checks cover 149 valid profiles, 144 rejections and 54 native commands.
The cases include all six whitespace characters, malformed or excess digits,
Float64 signed zero and NaN bits, and exact installed SDK project replay.
Independent hex decoding validates the same samples as the pinned raw producer
control. The existing AGPL-3.0-only and private static Zlib terms apply.
[Baseline](../../../build/nrrd-reader/hex-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/hex-candidate/results.json)

## Complete native scalar types in 1.8.0

`org.qupathxx.nrrd` 1.8.0 uses contract `nrrd5-scalar-window/9`. The six added
sample types complete the standard scalar widths supported by the existing
ABI. Raw and gzip retain full signed and unsigned integer codes and original
IEEE binary64 bits in either byte order. Text conversion checks the declared
type and normalizes text NaN to quiet NaN. Display uses binary64 windowing.

Local checks cover 141 valid profiles, 135 refusals and 48 native commands.
They include integer limits, values above 2^53, Float64 signed zero,
subnormals, finite extrema and signaling NaN bit preservation. The installed
SDK project preserves UInt64 and Float64 sample bytes and companion sets.
The existing AGPL-3.0-only adapter and private static Zlib terms apply.
[Baseline](../../../build/nrrd-reader/scalar-types-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/scalar-types-candidate/results.json)

## Numbered detached file series in 1.7.0

`org.qupathxx.nrrd` 1.7.0 uses contract `nrrd5-scalar-window/8`. The detached
numbered profile expands one signed decimal `%d` or `%i` conversion with
optional zero padding. The plugin derives the ordered series from the signed
range and uses the same extent, encoding and guarded source file rules as
`LIST`. It checks each generated basename and the existing 63-member limit.

The 92 exact profiles, 129 rejections and 44 native commands cover numbered
rows, slices and slabs, positive and negative steps, signed endpoints,
nonlanding bounds, and malformed ranges. The installed SDK reopens and
replays numbered sources and detects byte-identical companion replacement.
The AGPL-3.0-only adapter and private static Zlib terms carry forward.
[Baseline](../../../build/nrrd-reader/pattern-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/pattern-candidate/results.json)

## Ordered detached file lists in 1.6.0

`org.qupathxx.nrrd` 1.6.0 uses contract `nrrd5-scalar-window/7`. The detached
`LIST` profile reads up to 63 declared same-directory companions in header
order. Default and explicit subdimensions partition the raster into rows,
slices or equal slabs. Native raw, text and gzip interpretation and the
same per-file offsets apply to all members. The existing source-file ABI
guards the header and every companion.

Local checks cover 86 valid cases, 120 refusals and 39 native commands. They
include reverse lexical order, both byte orders, the 63-member cap, malformed
counts/extents and exact installed project replay after a fresh reopen. A
byte-identical replacement of one member invalidates the retained binding.
The existing AGPL-3.0-only and Zlib obligations apply.
[Baseline](../../../build/nrrd-reader/list-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/list-candidate/results.json)

## Scalar payload line preambles in 1.5.0

`org.qupathxx.nrrd` 1.5.0 uses contract `nrrd5-scalar-window/6`. The retained
1.4.0 reader refused four exact pynrrd controls, including a detached NRRD
that described a PGM raster. The native parser now skips complete lines before
decoding or applying a byte offset. Raw EOF-relative selection takes precedence.
Detached scans use a fixed-size buffer and a 256 MiB scan limit.

The 79 exact cases and 113 refusals cover attached and detached line preambles,
LF and CRLF across a block boundary, gzip and combined offsets, and exact
installed SDK replay. The existing AGPL-3.0-only adapter and private static
Zlib terms apply.
[Baseline](../../../build/nrrd-reader/line-skip-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/line-skip-candidate/results.json)

## Scalar payload byte offsets in 1.4.0

`org.qupathxx.nrrd` 1.4.0 uses contract `nrrd5-scalar-window/5`. The retained
1.3.0 provider refused byte-skip controls. The selected profile applies the
declared offset to raw and text payloads or to decoded gzip data. Raw `-1`
selects the declared raster from the end of the file. A detached raw read
seeks directly to the raster and retains the full companion file identity.
Earlier raw files with no byte-skip field still require an exact extent.

The 72 exact cases and 107 rejections cover both byte orders, placement,
calibration, typed integer and float data, malformed and short offsets,
and strict gzip framing and budget checks. Installed project consumers
check byte-skip replay and changed companion identity. The existing
AGPL-3.0-only adapter and private static Zlib terms apply.
[Baseline](../../../build/nrrd-reader/byte-skip-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/byte-skip-candidate/results.json)

## Attached and detached ASCII in 1.3.0

`org.qupathxx.nrrd` 1.3.0 uses contract `nrrd5-scalar-window/4`. The retained
1.2.0 package refused two ASCII producer files that pynrrd 1.1.3 reopened
exactly. The same native reader now accepts `ascii`, `text` and `txt`. Checks
cover all four scalar types, six ASCII whitespace characters, Float32 signed
zero, signed infinities, canonical text NaN, subnormals and finite range
limits. Binary NaN payloads retain their exact bits. Malformed tokens and
missing or extra samples refuse.

The 66 positive and 95 rejection profiles retain the earlier raw and gzip
coverage. Installed consumers check the detached ASCII file set, exact raw
and analysis replay, and refusal after changes to the payload or its
declaration. A native CLI export checks exact calibrated Int16 Z1 samples
from a pynrrd-produced detached `.nhdr` and `.txt` pair. The matching source
and notices retain the existing AGPL-3.0-only and private static Zlib terms.
[Baseline](../../../build/nrrd-reader/ascii-candidate/baseline.json) ·
[Handoff](../../../build/nrrd-reader/ascii-candidate/results.json)

## Detached scalar pairs in 1.2.0

`org.qupathxx.nrrd` 1.2.0 uses contract `nrrd5-raw-gzip-scalar-window/3`.
The same scalar decoder reads NRRD0005 with one same-directory raw or gzip
companion. The native ABI1 file-set extension declares the one payload path
before opening. The existing host guards track both header and payload
through native reads and saved projects. Attached sources declare zero
companions. The exact statically linked zlib 1.3.2 source and notices carry
forward. See the [profile guide](../../../plugins/nrrd/README.md) for supported
basenames, separator forms and refused inputs.

An independent pynrrd 1.1.3 producer emitted both calibrated big-endian Int16
pairs. The prior release refused them. All four current Z exports match the
producer's stored samples exactly. Direct ABI checks cover the attached
profile and four new detached profiles. File-set patterns, absolute or
directory-relative paths, aliases, missing payloads, size mismatches and
damaged gzip refuse before a usable handle is returned.

## Attached gzip in 1.1.0

`org.qupathxx.nrrd` 1.1.0 uses contract `nrrd5-raw-gzip-scalar-window/2`.
The attached NRRD0005 profile adds a single gzip member behind the existing
scalar axis, endian, window and calibration path. Declared decoded length and
complete encoded consumption are checked alongside the gzip CRC32 and ISIZE.
The work budget covers both payloads and 64 KiB of codec workspace.

Python's gzip writer produced the fixtures, and pynrrd 1.1.3 decoded them for
the independent raw oracle. The suite checks 55 supported profiles and 65
refusals. It includes calibrated big-endian Int16, exact Float32 bit patterns,
multi-slice UInt8 inflation, cancellation, compressed/decoded budget limits,
corrupt trailers, truncated streams, inflated-size mismatches and trailing or
concatenated members. Fourteen native commands cover the ABI, CLI, and project
save, reopen and replay through the existing native host and SDK.

The exact offline inputs include the original
[zlib 1.3.2 source archive](https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz)
and its Zlib notice. The adapter and kit retain AGPL-3.0-only. Detached headers,
arbitrary world frames and vector/color/time volumes stay outside this profile.

## Original attached raw handoff in 1.0.0

`org.qupathxx.nrrd` version1.0.0, contract `nrrd5-raw-scalar-window/1`, delivers
the selected scientific scalar image/volume workflow. Its
[profile/build/use guide](../../../plugins/nrrd/README.md) declares the exact
axis, sample, physical metadata, display and refusal contracts. ICS and external
matrix/annotation workflows remain selected next; the full roadmap is unfinished.

## Candidate and source identity

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The four NRRD compiler inputs have product context
`8951e234c3939e964d218ebb7ff2e0250c37d8915bc1c8b15a1b4a826ad857c3`:
decoder/ABI source, CMake, platform profile and manifest recipe. No host, binding,
GUI, kernel or external codec/library was rebuilt or added.

Final outside-checkout module SHA-256:
`141a85c4a616a666177ce099b6326442f38c6d4d3fa811f8ce55140cef365b2e`.
Those installed bytes received their own full native validation after a relocated
offline source-kit build with tests OFF, no SDK/core discovery/link and no Python
or network dependency. The runtime manifest binds this actual module hash.

Workspace evidence: `build/nrrd-reader/oracle.json`, `before.json`, initial build
and source snapshots, `relocation.json`, `final-source.json`,
`validation-final-μ-图/validation.json`, package admission and `results.json`.
The final report maps original outside paths into the retired task evidence.

## Scientific interpretation and decisive checks

The profile accepts attached raw NRRD0005 scalar XY/XYZ with explicit domain/space
kinds and UInt8/UInt16/Int16/Float32 samples. Axis0 is fastest X by this declared
index contract; NRRD itself does not name axes XYZ. Both file byte orders become
exact native-endian raw bits by byte reversal. Signaling/quiet NaN payloads,
infinities, signed zero and every Z plane remain intact and valid.

Display is Z0 grayscale with explicit persisted numeric window, default0..255.
Half-up quantization applies only to display; nonfinite display is transparent
black without changing raw validity. Positive recognized spacings/units calibrate
axes independently. Unknown remains unknown, and unsupported signed directions,
world geometry/origins/frames/other metadata refuse rather than being repaired.

| Decisive check | Result |
| --- | --- |
| Typed samples/axes |50 exact supported profiles cover four dtypes, byte orders, XY/XYZ, both Z planes, aliases, line endings, case spellings, spacing/unit/unknown combinations and display windows |
| Independent raw oracle | Authored integer/Float32 bitwords plus pynrrd1.1.3 in C index order; native-endian comparison uses byte swapping/views without float casts, preserving signaling NaNs and signed zero |
| Independent display oracle | Decimal190 reconstructs finite Float32 values from literal bits and applies the declared window/half-up rule; nonfinite values remain separate from raw validity |
| Reference limitations | pynrrd rejects case-equivalent keys/enums and type-word tab spelling accepted by the primary format rules/profile. Canonical same-payload seeds provide reference bytes; limitations are recorded instead of weakening native assertions |
| Refusals |56 exact statuses cover malformed/duplicate/missing/per-axis-order/ASCII fields, unsupported kinds/types/dimensions/encoding/metadata/detached/skip forms, endian/spacing errors, overflow/header/work limits and exact raster extents |
| Region/ABI | Exact Z/raw/display/crop/padding/channel/validity, buffer canaries/strides/sizes, text/structure sizes, coordinate bounds and raw bits before/after display |
| Memory/cancellation | A65,684-byte inline input first opens normally, then refuses a32KiB work budget. This checks one owned input, not nonexistent decompression/RGBA-cache expansion. Cancellation covers source/header/endian/raw checkpoints and Unicode paths |
| Native consumers | Nine ABI/SDK/CLI routes: typed UInt16/Float32 Z0/Z1 reads, display analysis keep/save/reopen/replay/resave, missing provider, changed implementation/source/options and host catalog/region |
| Exact persistence | Equal samples from changed module bytes still require review and a new result-input digest. A valid comment-only source change retains all raw codes but refuses currentness; historical IDs/values/digests stay exact |
| Window identity | Saved explicit0..255 survives a provider-default change. A fresh0..65535 attachment keeps raw bits, changes display and has distinct native input identity |

The decoder owns one input array and normalizes endian in place; display uses no
volume duplicate or expanded RGBA cache. A fixed string-view field table avoids
header-sized temporary copies. Input/bookkeeping and conservative path/options/
owner allowance fit the declared256MiB work bound. Header and option bounds are
64KiB and512 bytes. Nonfinite display classification inspects exponent bits
before evaluating even a local signaling NaN.

Missing-provider tests submit valid Z1 requests and require the actual native
provider-unavailable diagnostic. Source-change tests first prove the same reads
work, then require `WorkflowInputChanged`. Unsupported views/recipes remain absent.

## Package, licenses and preservation

The Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package contains20
admitted members with the complete offline adapter/test/control sources,
ReaderPluginKit, AGPL text and exact build provenance. Parser and authored inputs
retain AGPL-3.0-only. Published NRRD definitions and the MIT pynrrd reference have
their own terms; neither upstream implementation nor image is copied into the
shipped parser. The independently pinned pynrrd wheel is development-only;
NumPy2.5.4 and typing_extensions4.16.0 prerequisites were unchanged. No Python
runtime, NC/research-only material or closed codec enters this module/package.

Stage/ZIP share20 exact members under the existing material guard. Handoff checks
cover six-plugin NRRD/WebP/BMP/PNM/PNG/JPEG coexistence, old catalogs/P6 behavior,
bad hashes and cross-location project replay. Private removal deletes only
byte-verified package files, preserves user images/projects/results/notes and
checks offline reopening. Earlier inventory groups and all64 frozen native/
earlier-plugin inputs remain exact. The named outside comparison is retired only
after absolute path/worktree/reparse and fail-closed process-use checks.

## Corrections and remaining scope

All native compilation,50 profiles,56 refusals and nine routes pass on the first
combined build. Source-only review settled axis-kind/order, calibration range,
bounded field storage and literal IEEE handling before compilation. One reference
fixture probe was corrected to catch pynrrd's documented exception class only
when using explicitly authored equivalent-header seeds; no product assertion was
relaxed and no product rebuild was needed.

General affine/world registration, vector/color/time axes, detached/compressed
NRRD and other versions remain outside this profile. Raw access does not grant
TIFF/Zarr-specific recipes or new UI plane controls. ICS, external family plugins
and later conditional roadmap packets remain unfinished. No new host candidate,
platform, biological/clinical/speed acceptance, commit or publication is claimed.
