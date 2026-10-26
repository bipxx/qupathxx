# Multi-file OME-TIFF implementation slice

Status: independent fixture prepared; native multi-file opening is still
unsupported. This slice remains part of M1/M6 and does not replace the broader
vendor, scientific or supported-release requirements.

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
   `ReaderSource` currently retains only `primary`; its JSON and binary
   provenance codecs cannot describe companion observations. Preserve existing
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
  qualification. Preserve the independently running TMA candidate unchanged.

Implementation owner: active integration worktree. Next action: implement the
compound source descriptor and its codec compatibility before external-file
dispatch. The fixture is preparation, not proof of native support.
