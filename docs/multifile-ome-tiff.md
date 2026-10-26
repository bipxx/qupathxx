# Multi-file OME-TIFF implementation slice

Status: native multi-file opening and the synthetic installed workflow are
locally verified in the [workflow receipt](benchmarks/multifile-ome-local.json).
The [frozen platform campaign](benchmarks/multifile-ome-210-qualification.json)
is engineering accepted in its declared synthetic scope. Companion fault/cancellation
and decoder/work-budget adversarial controls remain open. This slice
is part of M1/M6 and does not replace broader vendor, scientific, resource or
supported-release requirements.

The [OME specification](https://ome-model.readthedocs.io/en/stable/ome-tiff/specification.html#multi-file-ome-tiff)
maps logical planes to file UUIDs and physical IFDs through `TiffData`. A
filename provides a direct lookup; each referenced file has its own root UUID.
The implementation must verify that mapping rather than concatenate files in
directory or filename order.

Implementation uses the existing open-source libtiff/zlib/tinyxml2 stack under
the [reader licensing policy](reader-license-policy.md). Closed SDK adapters
are excluded from this slice and from required reader parity.

## Independent corpus

Run `tools/reference_multifile_ome.py OUTPUT_DIRECTORY` with the existing
NumPy/tifffile development dependencies. No qupathxx code supplies its pixels
or expected mapping. `manifest.json` records file SHA-256 and canonical
little-endian sample commitments for every physical IFD and SubIFD.

The fixture contains eight uint16 planes with two channels, two Z positions,
two timepoints, and a 19-by-17 base image. The stored pixel equation is
`10000*T + 1000*Z + 100*C + 19*y + x`. Physical IFD order is shuffled across
two files, one little-endian/Deflate and one big-endian/uncompressed. A Unicode
companion filename and relative references exercise opening from either member.
Every plane also has a factor-two SubIFD with independently specified samples
at level-zero coordinates `(2*x, 2*y)`.

Four fixture checks validate all physical samples against a scalar equation,
root/reference UUIDs, relative paths, file/sample commitments, base C/Z/T
assembly from either member and output non-overwrite. The pinned
tifffile2024.9.20 reader explicitly cannot assemble a multi-file pyramid as an
OME series. Its base series is checked independently; every SubIFD is checked
through direct IFD access. That limitation does not remove native pyramid
acceptance from this slice. The original failing fixture-check JUnit is retained
alongside the corrected, scoped checks.

## Shortest complete implementation route

1. Extend the existing source identity with a bounded, owning dependency set.
   This foundation is implemented: `ReaderSource.companions` and versioned
   JSON/QPV/QRA retain dependency observations. Preserve existing
   single-file encodings. Use an explicitly versioned encoding for compound
   sources and reject attempts to serialize one through an older encoding.
   An individual primary-file fingerprint must not claim to hash the fileset.
2. Keep `open_tiff`, `TiffView`, typed raw requests and the current controller
   authority. Resolve explicit OME references relative to their containing file;
   verify UUIDs, mapping completeness, unique logical slots, IFD bounds and
   consistent sample/axis/pyramid geometry before publishing a reader. Support
   opening either fixture member. Keep missing-filename UUID search and partial
   metadata/BinaryOnly support explicit in the capability map until implemented;
   do not accidentally scan the working directory.
3. Reuse the native directory/block reader with a file identity in each IFD and
   cache key. Offsets and block numbers can collide between files. Share a
   single declared decoder/cache budget across the fileset, retain RAII and
   bound file count, metadata bytes, directories and logical plane mappings.
   Do not allocate one full cache budget for every companion.
4. Validate every dependency before serving cached or decoded data and verify
   it again before publication. Retain guards for a complete operation instead
   of reopening every companion for each inner 256-square display conversion.
   A changed/missing companion must reject even when the primary is unchanged.
   Failed open/read, cancellation, logical view close and retained array
   ownership must preserve existing behavior.
5. Route the same reader through project add/save/reopen, raw scientific
   capture/QRA replay, Python, CLI, batch, ImGui and managed-viewer paths.
   Persist the reader selection and complete captured source observations.
   Project relocation and an explicit relink must resolve the fileset again;
   no frontend-specific OME parser or alternate analysis implementation.

## Delivery checkpoints

These checkpoints detail MFO1 in the single ordered API queue; they are not
separate release milestones. The integration owner implements them in order.
A-D have local evidence and E has engineering platform acceptance in the declared
synthetic scope. The remaining fault/resource controls below are not waived.

| Checkpoint | Smallest implementation path | Exit / dependency |
| --- | --- | --- |
| A: source foundation | Reuse the implemented owning companion list, QPV3/QRA3/QRA4, stale-source comparison and typed bindings | Locally verified at native `4d59c46`, binding/stub follow-up `916d2fc`; retain the existing receipt and legacy byte fixtures |
| B: file dispatch and indexing (locally verified) | Keep changes private to `cpp/tiff_server.cpp`. Catalog each resolved file with its UUID and file-local IFD range; add file ordinal to private IFDs and map logical planes to the catalog | Both synthetic entry points resolve the same C/Z/T pixels and SubIFDs; reject missing/wrong UUID, duplicate physical files, conflicting metadata, incomplete/duplicate slots and invalid IFD bounds |
| C: resources and operation guards (locally verified scope) | Key the existing shared LRU by file ordinal, directory offset and block number. Use one active bounded libtiff decoder, switching files under the existing reader lock. Guard all dependencies once per complete raw/display operation, including cached and out-of-bounds reads | Colliding offsets return correct pixels; companion-only changes reject before publication; cancellation/close/lifetime behavior remains correct. No multiplied per-file decoder/cache allowance |
| D: installed workflow closure (locally verified) | Reuse `open_tiff`, `TiffView`, raw requests, project save/reopen and existing frontend controllers. Preserve compound observations when deriving logical views | Installed Python exercises nonempty companion ownership; project restart/relink, QRA capture/replay, CLI, batch, ImGui and managed viewer use the real two-file corpus, with legacy single-file regressions |
| E: coherent qualification | After B-D pass focused checks, freeze compiler inputs and register exact new cohorts in producer and auditors together; use the existing required platform campaign | Source-bound native/installed/frontend/platform receipts retain failures, skips, identities and raw evidence. Local foundation results do not qualify the completed reader |

For B, resolve each explicit filename relative to the containing TIFF, normalize
the absolute path and reuse an exact existing catalog entry. Legitimate `..`
references must work from the companion entry point. Validate the referenced
root UUID and selected-series dimensions, type, dimension order, channel groups,
calibration and pyramid geometry semantically; XML formatting and per-file UUIDs
need not be identical. A UUID without a filename may name an already cataloged
file; otherwise refuse explicitly. Directory scanning and partial/BinaryOnly
metadata remain unsupported until separately implemented and verified.

For C, retain the source descriptor's existing limit of 63 companions plus the
primary. Apply directory limits across the complete fileset: at most 65,536
primary IFDs and 131,072 total IFD/SubIFD entries. The implemented 1 MiB XML
limit per file and 4 MiB aggregate XML limit have refusal checks. These are
admission limits, not measured RSS or latency guarantees. Hold
opening guards through indexing and operation guards through final publication;
do not reopen every companion for each inner display-conversion chunk. Preserve
the compound scope and companions when creating another `TiffView` descriptor.

Avoid retaining vector-element references while companion discovery grows the
catalog. Use stable file records and integer directory indices. Copy TIFF-owned
XML before switching the sole decoder handle. These private changes reuse the
public source foundation instead of expanding public headers for dispatch.

Run focused semantic checks after coherent edits, then freeze once for native
builds. Compare the independent scalar equation and stored sample commitments,
not only agreement between two qupathxx frontends. Reuse unchanged foundation
and legacy evidence with explicit applicability; rebuild when compiler inputs
change. Do not restart a live build on an observation timeout.

## Required acceptance

- Exact dtype, validity, C/Z/T mapping and pixel commitments at both levels,
  including crossing tile edges and out-of-bounds padding, from either member.
- Exact calibration and preview channel/view selection through installed Python
  and project restart; native source-disabled builds retain their named refusal.
- Refusal for missing/wrong-UUID/changed companions, conflicting or duplicate
  logical mappings, malformed references, mismatched type/pyramid geometry,
  exhausted file/metadata/decode/cache/work budgets and interrupted publication.
- Colliding file-local offsets/block numbers cannot return another file's
  cached samples. A cached read still rejects a changed companion.
- Complete captured dependency provenance survives the relevant archives and
  project restart; older single-file records remain byte-compatible. A codec
  unable to retain dependencies must reject instead of dropping them.
- Installed Python/CLI/batch and desktop/managed smoke workflows use the same
  native source and retain exact resource/source proofs. Run focused checks
  during development, then freeze the complete slice for the required platform
  qualification. Preserve the original frozen TMA candidate and failed auditor evidence unchanged.

Implementation owner: active integration worktree, MFO1 in the
[single ordered queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
E1/E2 have completed their engineering/local checks. The
[compound source foundation](benchmarks/compound-source-local.json) now supplies
owning companion observations, primary-only fingerprint status, QPV3/QRA3/QRA4,
raw stale-status comparison and typed Python ownership. Seven selected native
suites pass on each Windows/Linux host; 200 installed cases and 15 desktop
regressions pass, with two OpenSlide capability skips retained separately.
The [reader/workflow follow-up](benchmarks/multifile-ome-local.json) now verifies
explicit FileName/UUID dispatch, file-aware directory/cache keys, one active
decoder, shared budgets and all-dependency operation guards. Seven native
suites pass on each Windows/Linux host; the installed regression passes 304
cases with 14 declared skips. A 17-case desktop follow-up covers the 12 desktop
skips, leaving two OpenSlide capability gaps. The final real multi-file cohort
passes 39 cases including project restart/QRA3/QRA4, relocation/relink and
Python/CLI/batch/ImGui/managed paths. These cohorts overlap.
Exact native, 44 installed, two desktop and typing cohorts are now registered
in the producer and all three auditors. Frozen source `9e7d0be` passes 562
clean-checkout admission controls. The [combined receipt](benchmarks/multifile-ome-210-qualification.json) now
accepts all original engine/Mac-native/portable runs and all 11 verified archives.
Next add direct companion fault/cancellation and decoder/work-budget controls;
normal synthetic success does not prove these cases. Keep broader cancellation/fault stress,
whole-slide/scientific/resource and supported-release acceptance open. The
original foundation receipt remains unchanged and retains its narrower scope.
