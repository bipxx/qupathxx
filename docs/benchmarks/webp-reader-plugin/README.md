# Native WebP plugin: FP01 local Windows delivery

## EXIF orientation in 1.2.0

`org.qupathxx.webp` 1.2.0 uses contract `webp-static-uint8-samples/3`.
It adds a bounded EXIF orientation profile for an extended still with VP8,
ALPH plus VP8, or VP8L. The primary classic TIFF IFD carries exactly one
orientation tag. Its eight possible values define the pixel coordinate
mapping for metadata, display and raw reads. The decoded RGBA raster is
retained in its original layout and accessed without interpolation or a
second raster. The [format and metadata limits](../../../plugins/webp/README.md)
define the admitted scope.

Pillow with libwebp 1.6.0 generated all three extended layouts with orientation
6. WebP 1.1.0 refused them. Pillow's independent orientation transform supplies
the expected coordinates and channel bytes for the authored fixtures.

## Still lossy WebP in 1.1.0

`org.qupathxx.webp` 1.1.0 uses contract `webp-static-uint8-samples/2`.
It accepts an opaque simple VP8 image and an alpha-only extended still with
exactly VP8X, ALPH and VP8 in that order. The adapter checks both RIFF extents
and agreement between the canvas and VP8 frame before decoding. The existing
simple VP8L profile retains its exact lossless sample contract.

Pillow with libwebp 1.6.0 produced the opaque and alpha fixtures that the
previous module refused. The reference bytes for VP8 are the output of the
pinned decoder, including its YUV conversion and upsampling. Opaque output
has alpha 255, and extended output retains decoded alpha without
premultiplication. See [the selected profile](../../../plugins/webp/README.md)
for limits and format exclusions.

## Original lossless handoff in 1.0.0

`org.qupathxx.webp` version1.0.0, contract
`webp-static-lossless-uint8-samples/1`, addresses FP00's demonstrated static
lossless WebP gap. Its [profile/build/use guide](../../../plugins/webp/README.md)
defines the exact container, samples and resource limits. This completes the
three ranked ordinary-input gaps; later format packets remain outstanding.

## Candidate and source identity

Application baseline is main `7c981bf9`; retained engine/SDK source identity is
`bc8ca7384be33c6308130affeec66e8d84cf8b40f2bfaf91dc244651f57990eb`.
The seven BMP-independent WebP compiler inputs have product context
`ab365b115fcdc50a6791d1306ca6670de9e5c01f1f9d46a39b11c874352574ee`:
adapter, memory backend C/header, CMake, platform profile, manifest recipe and
source lock. The lock binds every selected upstream source byte.

Final outside-checkout module SHA-256:
`2926cf41573ec607e31d471dee393ac649d1db446e9a5ae1d9402994f76a60aa`.
It was built from the offline source kit with tests OFF, no SDK/core discovery
or link and no dependency download. Those installed bytes received their own
complete native validation, and their actual hash binds the runtime manifest.

Private libwebp1.6.0 is pinned at
`4fa21912338357f89e4fd51cf2368325b59e9bd9`. Only63 unmodified code/header/build-
list/notice members were retrieved individually;29 generic C sources compile.
Each selected C/header has the upstream BSD license reference. No upstream
archive, image/test asset, encoder, mux/demux, SIMD, thread library or external
image decoder is needed. Codec allocation interception is a source-local forced
include with an explicit compilation macro; locked upstream bytes stay exact.

Workspace evidence: `build/webp-reader/source-admission.json`, source lock,
`before.json`, initial build/source snapshots, `relocation.json`,
`final-source.json`, `validation-final-μ-图/validation.json`, package admission
and `results.json`. Results map the retired outside comparison into retained
task evidence; original execution paths stay in logs.

## Scientific interpretation and checks

The initial profile accepts SIMPLE RIFF/WEBP with exactly one VP8L chunk,
lossless version0 and exact outer extents/zero odd padding. Raw is always UInt8
RGBA in HWC order; display is the identical straight RGBA. Hidden RGB and alpha0
are preserved, and alpha0 stays valid. Alpha-is-used is only a hint, never an
instruction to erase alpha or change channel count. All calibration is unknown.

| Decisive check | Result |
| --- | --- |
| Literal pixels |19 supported profiles cover opaque RGB, partial/all transparency, hidden RGB, odd/asymmetric sizes, few-color/gradient patterns, encoder variations and alpha-hint mutations |
| Oracle boundaries | Authored literal RGBA is authoritative; Pillow13/libwebp1.6 verifies unmutated seeds. This is a separate adapter check, not an independent codec algorithm. Pillow chooses RGB from a cleared hint and drops alpha; that limitation is explicitly recorded rather than adopted as expected scientific data |
| Container/refusals |28 exact statuses cover malformed/truncated RIFF/chunk/payload, size/padding/trailer/duplicate-image errors, unsupported versions, lossy/extended/animation/metadata/unknown chunks and decoded size bounds |
| Region/ABI | Exact raw/display/crop/channel slice/OOB validity, alpha0 validity, packed buffers/strides/canaries, text/structure sizes, coordinate overflow and invalid requests |
| Decoded work | Normal success precedes a32KiB refusal for a44-byte256² image; decoded RGBA alone is262,144 bytes |
| Codec scratch | Normal success precedes a96KiB refusal for a44-byte128² image; encoded input,65,536-byte external RGBA and conservative wrapper/path allowance fit, proving codec scratch is also bounded |
| Cancellation/Unicode | Successful callback trace supplies a reached during-open cancellation checkpoint; raw cancellation is separate. Paths/directories contain Unicode; no fixed latency or per-row cancellation inside a compact codec call is claimed |
| Native consumers | Eight ABI/SDK/CLI routes cover raw RGBA/alpha attach/read, analysis keep/save/reopen/replay/resave, missing provider, changed implementation/source and host catalog/region |
| Persisted identity | Changed module bytes with equal pixels require review and a different result-input identity; source mutation raises the native currentness exception while historical IDs/values/digests stay exact |

Every codec allocation charges payload, aligned tracking header and conservative
CRT bookkeeping against the remaining work allowance. Context/output descriptors
are also charged. The synchronous codec gate serializes upstream static DSP
initialization and decoding. Cleanup releases the decoder and tracked allocations
before clearing thread-local cancellation/context. This is an owned-work bound,
not a process-RSS claim. Actual allocation failure remains internal error.

The upstream decoder's accepted padding inside the VP8L payload is distinct from
the strict outer-container contract; no exact bitstream-consumption claim follows.
Offline provider checks use valid requests and the actual unavailable-provider
diagnostic. The same requests pass before a source mutation, then require
`WorkflowInputChanged` afterward.

## Package and preservation

The Windows x64 MSVC1944/compiler19.44.35225.0 Release `/MD` package contains90
admitted exact members, including the complete offline selected source kit.
Adapter/test/control sources and authored fixtures retain AGPL-3.0-only;
libwebp retains BSD-3-Clause and its full COPYING/PATENTS/AUTHORS notices in source
and binary-license locations. Format specifications retain their own terms.
No NC, research-only or proprietary material is admitted.

The stage and ZIP use the same90 members under the existing material guard.
Handoff checks cover five-plugin WebP/BMP/PNM/PNG/JPEG coexistence, old built-in
image/matrix/annotation catalogs, P6 behavior, bad hashes and cross-location
project replay. Removal deletes only byte-verified package files from a private
deployment and preserves images/projects/results/notes before offline reopen.
Earlier inventory groups and all62 frozen native/earlier-plugin inputs stay exact.
The named outside comparison is retired only after path/worktree/reparse and
fail-closed process-use checks. No engine, binding, GUI or wheel build occurs.

## Corrections and remaining scope

The first combined compilation and all native pixel/refusal/SDK/CLI commands
passed. The runner's final PAM comparison used a stale BMP fixture basename;
only that Python development path was corrected, then the same module was
validated without a rebuild. Initial failure evidence is retained. Source-only
review settled allocation bookkeeping and the narrower cancellation contract
before compilation; no scientific or identity assertion was relaxed.

Lossy/extended/animated/metadata WebP profiles, JPEG2000 and scientific/store/
family/writer/remote packets remain unselected or unfinished. Their actual input
workflow and applicable native boundary must be selected before expansion.
Automatic routing, existing recipe restrictions and separate UI work keep their
scope. No biological/clinical/speed acceptance, new platform, commit or public
release is claimed; the full format-roadmap goal is not complete.
