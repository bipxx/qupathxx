# Native scalar NRRD plugin: FP02 local Windows delivery

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
