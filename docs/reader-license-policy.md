# Reader licensing policy

## Proprietary and noncommercial material separation

Owner update2026-11-08: [the eight-gap expansion plan](feature-expansion-plan.md)
keeps proprietary and noncommercial (NC) material separate from the open product
and from one another. [Restricted references](licensing/restricted-materials.md)
contain metadata only; any later explicitly authorized local artifacts belong in
different external pools, not the source/build/fixture/package tree. Unreviewed
terms do not grant admission. No restricted SDK, codec, weights, evaluation data
or derivative asset becomes an open dependency because it is optional or converted
to an open container. This does not authorize proprietary-format integration.

Extend existing source/notice/fixture inventories and release staging checks at
L01. Record code, codec, model and dataset rights independently. Preserve historical
archives while classifying future release inputs; no cleanup/relicensing is implied.
Private user research inputs remain local under their permissions and are not
automatically project fixtures or distributable material.

## Additional open-format rule for fluorescence and spatial omics

Owner update2026-11-08: new fluorescence/spatial-omics work must use open formats
as well as openly licensed readers and codecs. The legacy allowance for a vendor
format through an open implementation is insufficient for this new phase.
See the selected formats, source evidence and excluded/deferred scope in the
[fluorescence/omics plan](fluorescence-omics-plan.md#format-admission).
OME-TIFF and the documented Matrix Market/TSV profile are selected; AnnData/H5AD
is an open candidate requiring a separate native dependency/encoding admission.
No existing reader is removed and no new reader is declared implemented here.

The format/schema must have published reusable terms; the exact reader, required
codec/filter stack and fixture rights must also be established. Closed containers,
NDA-only schemas, closed required decoders and research/noncommercial restrictions
are excluded. Exporting data into an eligible open format is a separate supported
path only when that format's declared schema and coordinate contract are met.

## Existing reader admission policy

Owner decision: 2026-10-25. Only open-source-licensed image readers and their
required decoder dependencies are eligible for qupathxx integration, optional
adapters, runtime download or distribution. This requirement applies to all
remaining M0-M7 reader work and supersedes broader reader-parity ambitions.

Exclude proprietary SDKs, binary-only readers without an open-source license,
NDA-only implementations, noncommercial/research-only licenses and
source-available licenses that restrict use or redistribution. A free download
does not make a reader eligible. Unverified licensing blocks admission until
the exact implementation and dependency licenses are established.

Open-source includes copyleft as well as permissive licenses. GPL/LGPL code is
eligible under this policy; integration and distribution must still meet its
actual terms and the eventual project license. This decision does not select
the project's own license or establish release compliance.

The restriction concerns the reader implementation and required decoder stack.
A vendor-specific file format may remain supported through an eligible
open-source implementation. File-format naming or proprietary origin alone
does not exclude it. Fixture redistribution rights remain a separate check.

## Current reader inventory

| Reader/component | Evidence | Decision |
| --- | --- | --- |
| OpenSlide native reader | [Upstream licensing](https://openslide.org/) and [LGPL 2.1 text](https://github.com/openslide/openslide/blob/v4.0.1/COPYING.LESSER); current CI requests OpenSlide 4.0.1 | Retain. Inventory the matching SDK's complete dependency licenses and notices for each distributed artifact |
| Quantitative TIFF/OME reader: libtiff 4.7.1 | [Pinned license](https://github.com/libsdl-org/libtiff/blob/v4.7.1/LICENSE.md); `cmake/TiffReader.cmake` pins the archive and installs its license | Retain; preserve the additional bundled-code notices in that license file |
| TIFF Deflate: zlib 1.3.2 | [Upstream license](https://zlib.net/zlib_license.html); pinned archive and installed license in `cmake/PinnedZlib.cmake` (shared by TIFF/H5AD) | Retain |
| TIFF JPEG: libjpeg-turbo 3.1.3 | [Pinned upstream terms](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/3.1.3/LICENSE.md), README.ijg and attribution are installed byte-for-byte; cmake/TiffJpeg.cmake pins archive SHA256 3a13a5ba767dc8264bc40b185e41368a80d5d5f945944d1dbaa4b2fb0099f4e5 | Retain. Static/PIC libjpeg API only; separate upstream CMake build, no TurboJPEG/Java/tools runtime or closed dependency. [Local admission](benchmarks/tiff-jpeg-r2/README.md) records exact notices and coexistence |
| OME XML: tinyxml2 11.0.0 | [Pinned license](https://github.com/leethomason/tinyxml2/blob/11.0.0/LICENSE.txt); pinned archive and installed license in `cmake/PinnedZlib.cmake` (shared by TIFF/H5AD) | Retain |
| AnnData/H5AD: optional HDF5 C2.2.0 | [Exact full license](https://github.com/HDFGroup/hdf5/blob/2.2.0/LICENSE), BSD-3-Clause plus retained notices; cmake/H5adReader.cmake pins source SHA5b8d75125ae7b4fef55d3b39e8d3dbfd238cdf63b6518afd67fa69ac80f06542 and installs LICENSE | O01 native decoding in progress. Static C/local native connector, shared admitted prefixed zlib, shuffle/Fletcher32 only. Plugins, SZIP, network, high-level/C++/Java/Fortran/tools excluded. Windows static calls serialize through the native cancellation-aware gate; no unsupported threadsafe-static flag. This dependency admission is not full O01 delivery. |
| Dense/PPM reader | Repository-native implementation; no separate proprietary reader SDK | Retain; repository license decision remains open |
| Future Bio-Formats adapters | [Upstream GPL license](https://github.com/ome/bioformats/blob/v8.5.0/LICENSE.txt); BSD/GPL components and their complete transitive dependencies must be checked individually | Open-source components remain candidates; no Java runtime adapter is added by this policy, and the existing first-release Java deferral remains |
| Any closed vendor reader/codec SDK | Ineligible even when optional, free to download or invoked through another process | Excluded from implementation and required parity targets |

The inventory checks the configured reader families and pinned TIFF/HDF5 components;
it is not a certification of every binary in every SDK. Existing artifact
notice/runtime inventories remain required. No proprietary reader adapter was
identified in the current CMake reader configuration, so no implemented reader
needs removal for this decision.

## Admission and delivery

Before adding a reader or codec, record its upstream source, exact revision or
archive digest, license text and all required decoder/runtime dependencies.
Check the actual selected binary/source bundle, including optional modules;
an open-source wrapper around a required closed SDK is ineligible. Preserve
the license and notice inventory with the qualifying artifact.

Use the existing native OpenSlide and TIFF paths first. Extend multi-file OME
through the existing open-source TIFF/XML stack. For formats with no eligible
implementation, record `Excluded: requires non-open reader` in the capability
inventory and do not count proprietary parity as unfinished required work.
When an eligible implementation exists, its remaining functionality and
scientific/platform validation stay in scope.

Historic source-bound receipts and frozen campaigns remain immutable. Apply
this policy to the active plan and future dependency admission; do not rewrite
past evidence or relabel its licensing/coverage claims.
