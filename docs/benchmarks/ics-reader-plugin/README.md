# Native ICS fluorescence plugin: FP02 local Windows delivery

## Gzip-named companions in 1.5.0

The Windows optional module discovers a same-base `.ids.gz` file when the
ICS1 header declares gzip and the canonical `.ids` file is missing. Both the
source-file ABI and the decoder resolve the same path. The checks cover
exact samples, reopening saved projects, canonical-name precedence, and
refusal to use cached raw or display data after the selected file changes.
[Pinned libics companion lookup](https://raw.githubusercontent.com/svi-opensource/libics/c1a1ae6f46ed1167001c0a14d9ef767c37ff4833/libics_binary.c#L358-L390)

## Planar and reordered axes in 1.4.0

The C++ reader maps declared X/Y and optional Z/T/channel axes through
bounded storage strides. Raw data is returned in packed HWC at the
requested Z and T. Calibrations stay associated with named axes and
metadata reports their original order. The libics oracle checks physical
storage order, and separately authored raw and display expectations check
planar, middle-channel and reordered time-series reads after project reopen.
[libics dimension metadata](https://svi-opensource.github.io/libics/Ics_Header.html)

## Microscopy time series in 1.3.0

The reader accepts a terminal time axis for scalar or channel-first XY and
XYZ arrays. Exact native raw samples can be exported from any stored Z/T
index. Declared seconds, milliseconds and microseconds normalize to seconds.
Unknown units leave time calibration unspecified. Display stays on Z0/T0.
The pinned libics oracle verifies authored ICS1/IDS, inline ICS2 and offset
ICS2 time series, including gzip data and byte permutations. Native tests
verify later frames, project reopen, offline time metadata and malformed input.
[libics axis documentation](https://svi-opensource.github.io/libics/Ics_Header.html)

## External ICS2 payloads in 1.2.0

`org.qupathxx.ics` 1.2.0 uses contract `ics-raw-gzip-channel-window/3`.
An ICS2 header may explicitly name one regular sibling source and its byte
offset. Raw reads take exactly the declared raster from that range. Gzip reads
the one declared member from the offset through EOF. Both routes apply the
existing calibration, channel, endian and display semantics. The project
retains identities for the header and full companion file, including bytes
outside a selected raw range.

The pinned libics oracle recovers the original calibrated channel volume
from both authored offset-24 examples. ICS 1.1.0 refused both. Scope
and refusal rules are in the [profile guide](../../../plugins/ics/README.md).

## Gzip channel volumes in 1.1.0

`org.qupathxx.ics` 1.1.0 uses contract `ics-raw-gzip-channel-window/2`.
A declared gzip member occupies the existing adjacent ICS1 `.ids` file
or the existing ICS2 inline payload position. The decoder checks CRC32,
ISIZE, exact compressed and decoded extents, and the combined input,
decoded raster and codec-workspace budget. Byte permutation and the
channel, calibration and display rules are shared with the raw path.
The existing compound-source guards track the ICS1 header and IDS file.

ICS 1.0.0 refused the authored gzip ICS1 and ICS2 baselines. The pinned
libics 1.7.2 reader with gzip enabled reopens both with the same sample
bytes as the raw control. All four candidate Z exports matched the
reference channel-interleaved values byte for byte. The source kit includes
the admitted original zlib 1.3.2 archive and license. Libics remains a
development oracle.

## Original raw-only handoff in 1.0.0

`org.qupathxx.ics` version1.0.0, contract `ics-raw-channel-window/1`, delivers
the selected uncompressed fluorescence file/file-set workflow. Its
[profile/build/use guide](../../../plugins/ics/README.md) defines exact samples,
axes, physical interpretation, source identity and limits. External matrix and
annotation workflows remain selected next; the complete roadmap is unfinished.

## Candidate and source identity

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The four product compiler inputs have source context
`f6f7ccd35768593df356b74c0d2572b5df69473279b8a7506c91673141c3e391`:
independent decoder/ABI source, CMake, compiler profile and manifest recipe.

Final outside-checkout module SHA-256:
`df9af405fa515ea62f2017de564e311e8167e0e9d9306287b7374dae3f45f9c2`.
It was built from the offline source kit with tests OFF, no SDK/core/C-library/
Python discovery or link, and no dependency download. Those installed bytes
received complete native validation; the manifest binds their actual hash.

The independent development reference is libics1.7.2 at
`c1a1ae6f46ed1167001c0a14d9ef767c37ff4833`,19 exact source/header/config/notice
members and12 standard C sources. It is never linked into the product module.
Reference dump executable SHA-256:
`e8c884d6704e35550084dca8e150dd1857dcc20580a1534ca0e2a3571bfea737`.
Every reference source hash was checked before/after compilation; the executable
is hash-bound before/after fixture/reference/native validation.

Workspace evidence: `build/ics-reader/oracle-source.json`, initial reference-source
and product snapshots/build logs, `before.json`, `relocation.json`,
`final-source.json`, `validation-final-μ-图/validation.json`, package admission
and `results.json`. Results map original outside execution paths to retained
task evidence after comparison retirement.

## Scientific interpretation and decisive checks

The profile reads ICS1.0 plus a derived adjacent IDS file or inline ICS2.0.
Explicit scalar XY/XYZ or channel-first CXY/CXYZ arrays preserve UInt8/UInt16/
Int16/Float32 stored bits across all C/Z. Byte permutations never use float
conversion. Generic ordinal names do not imply fluorophores; channel count≤256
fits the frozen host metadata budget. Positive represented spatial calibration
and zero origins are explicit. Unsupported acquisition/scalar-scaling/source/
compressed semantics refuse.

Display uses one selected channel at Z0 with a persisted explicit finite window,
default channel0/window0..255. Half-up grayscale is separate from raw values;
nonfinite display is transparent black while raw validity remains1.

| Decisive check | Result |
| --- | --- |
| Supported profiles |66 exact profiles cover ICS1/IDS and inlineICS2, all four dtypes, scalar/channel XY/XYZ, C256, permutations, calibration/unknown axes, windows and line/end variants |
| Independent data oracle | Authored literal integer/Float32 bitwords plus public native libics read/layout/position/order/significant-bit APIs; all24 Float32 permutations include non-involutive2,3,1,4 |
| Independent display oracle | Decimal reconstruction and half-up window anchors; all channels/Z and raw NaN payloads/signaling NaNs/±Inf/±0 remain exact before/after display |
| Header/data boundary | Canonical end-TAB and bare-end controls, LF/CRLF and leading raster09/0A/0D/space/# confirm that no payload byte is silently skipped |
| Refusals |60 exact statuses cover malformed/missing/duplicate layout/representation/parameter fields, unsupported axes/coordinates/precision/scaling/metadata/compression, byte permutations, extents and header/work limits |
| Source-file extension | Count/path callbacks reparse only bounded primary bytes, return the derived IDS despite missing data, never read alternate/header-named companions, and change from one companion to zero after a primary ICS1→ICS2 replacement |
| Region/ABI | Packed typed C/Z slices, crop/padding/validity, source callback buffers/indices/canaries, text/structure sizes, selector/coordinate/layout errors, Unicode and cancellation |
| Native routes |11 ABI/SDK/CLI commands: base keep/save/reopen/replay, missing provider, changed implementation, primary, companion, file set and options, plus host catalog/region |
| Source/implementation identity | A comment-only primary change with equal pixels, an equal-size IDS mutation and a source-set change each refuse native currentness. Equal pixels under changed module bytes require implementation review and a different result-input identity |
| Persisted display choices | Saved channel/window remain stable when manifest defaults change; a fresh choice keeps raw bits but changes selected display and native source/result identity |

Mutable SDK cases seed isolated project/source pairs. Restoring bytes does not
restore saved filesystem version timestamps; isolation ensures each control
passes before its intended mutation. The relocation base stays pristine.
Historical IDs, means and digests remain exact. Missing-provider reads use valid
nondefault C/Z requests and require the actual native unavailable-provider error.

Open owns primary plus IDS, or one inline primary, with in-place byte permutation
and no expanded display cache. Array headers/bookkeeping and conservative path/
options/owner allowance are charged against work≤256MiB. Primary-only discovery
allocates at most64KiB and observes actual primary size for inline extent checks.
Metadata names fit the existing1MiB host contract. Cancellation polls bounded
source/header/permutation spans and at most4096 selected raw samples.

## Package, rights and preservation

The Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package contains22
admitted members: exact offline product/test/control sources, reference-source
lock/helper source, ReaderPluginKit, AGPL text and provenance. The independent
parser and authored fixtures retain AGPL-3.0-only. The maintainer identifies the
ICS/ICS2 format as open; format/reference terms remain distinct from adapter terms.

Development-only libics per-file grants are GNU Library GPL2-or-later, while its
bundled GNU_LICENSE contains Lesser GPL2.1, the successor. Both are retained
accurately in the separate reference cache. The unmodified historical compression
source retains its nested public-domain provenance; no zlib is enabled. None of
that reference implementation/binary or any upstream image/archive enters the
product package. No NC, research-only or closed component is required.

Stage/ZIP share22 exact members through the existing material guard. Handoff
checks cover seven-plugin ICS/NRRD/WebP/BMP/PNM/PNG/JPEG coexistence, old catalogs/
P6 behavior, bad hashes and cross-location project replay. Removal deletes only
verified package-owned files from a private deployment and preserves user primary/
companion images, projects, results and notes before offline reopen. Earlier
inventory groups and all66 frozen native/earlier-plugin inputs stay exact. The
outside proof is retired after path/worktree/reparse and fail-closed process checks.

## Corrections and remaining scope

The first combined build and all independent reference/native checks pass.
Source-only review fixed a local declaration, bounded selected-sample cancellation
and canonical end-TAB syntax before compilation. The reference closure check
initially treated disabled ICS_ZLIB inclusion as active; its conditional was
corrected before any build. A Windows missing-file fixture was changed from a
case-only spelling to an actually absent derived path with existing alternatives,
retaining its I/O4 assertion. A malformed documentation patch was corrected before
packaging. No scientific/status/identity assertion was relaxed and no native
repair build was needed.

Other ICS axes/encodings/acquisition semantics, private source paths and nonfull
precision are outside this profile. No new UI controls, TIFF/Zarr recipe access,
host/SDK/binding build, platform, biological/clinical/speed acceptance, source
commit or publication is claimed. External matrix/annotation plugins require
their own typed native boundaries and distinct host/SDK candidate next.
