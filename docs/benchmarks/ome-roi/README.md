# A04 OME ROI XML native interoperability

## Uncompressed image-bound masks in 1.1.0

The existing ABI5 mask authority preserves exact pixel coverage, including one
hole and a disconnected component in the authored five-by-five crop. The
source selects a 16-by-16 quantitative volume and its explicit Z/T plane.
The same fixture keeps a vector point in the mixed ROI. The unpacking control
uses the MSB-first bit packing demonstrated in OME's OMERO mask example.
[OME Mask and BinData schema](https://www.openmicroscopy.org/Schemas/Documentation/Generated/OME-2016-06/ome_xsd.html#Mask)
[OME mask packing example](https://github.com/ome/omero-documentation/blob/develop/omero/developers/Python.rst)

The independent SDK checks every covered cell after reopening the saved
project. The [relocated module](../../../build/gui-aggregation/catalogue-closure/ome-roi/mask-zip-validation/validation.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/ome-roi/mask-offline-validation/validation.json)
each passed two imports, two offline reopens, 15 refusal controls and
five vector regressions. The [package inventory](../../../build/gui-aggregation/catalogue-closure/ome-roi/package-mask-1.1.0-admission.json)
binds 90 files to matching source and notices.

## Vector coverage in 1.0.0

Selected provider `org.qupathxx.ome-roi1.0.0`, contract
`ome-2016-06-vector-roi/1`, uses the existing bounded annotation producer and
native Roi/frame/class/capture/apply/undo/persistence authorities. Separate ABI4
negotiates real native Locked state without changing ABI1–3 layouts/identities.
Generic native plugin options are forwarded by the thin CLI/public SDK consumer.
The [format contract](../../../plugins/ome_roi/README.md) defines selection,
binary64 coordinates, transforms, planes, metadata and explicit refused profiles.

Local authored fixtures independently specify exact geometry bytes and XML DOM
infoset facts.39 positives/64 refusals and17 native/CLI process checks pass:
seven vector primitives, binary64 precision/subnormal/signed zero, UTF-8/16,
namespace aliases, inherited/rebound/undeclared namespace contexts for QName
metadata, multi-image and unbound selection, affine paths/rectangles,
axis-exchanged ellipses, RGBA/units/structured-annotation metadata, locks, native
scripted edits, exact undo/redo/reopen, budgets/cancellation, DTD/entity refusal,
staleness and atomic failure without outputs. Existing R5-EDITSTATE1 defines the
lock as an interactive geometry hint; native scripted edits remain available.
Unchanged desktop lock policy/evidence is reused, with no GUI rebuild.

Fifty affected native compatibility checks retain ABI1's exact original preview,
ABI2 geometry/persistence and28 ABI3 metadata/plane/malformed-provider controls.
Frozen WKT/ImageJ modules retain exact preview, receipt and saved-object identities
on the new host. Controlled providers prove flag2 is invalid in ABI3, becomes a
lock in ABI4, and unknown flag8 fails in both. Changed shared-producer candidates
also pass WKT48/79 and ImageJ26/37 profiles under their separate source/binary
identities; those module candidates do not replace prior frozen deliveries.

The207-member exact source/module/kit/native package passes directory/ZIP material
admission. Its standalone module builds offline and passes11 positive/eight
refused profiles. Fifteen offline/SDK/coexistence process checks include matching
public SDK imports and three actually admitted providers without diagnostics.
Owned-file removal preserves user data and provider-free project reopening;
the exact counts and all immutable proof hashes are recorded under
build/gui-aggregation/catalogue-closure/ome-roi/results.json.
Exact matching native overlay/CLI/headers/source
are kept separate from the frozen FP04 SDK/core/runtime, wheels and prior modules.
No full platform/corpus, biological, speed, GUI or publication acceptance is
claimed, and no commit/push/publication is selected by this local packet.

A final correctness check added bounded in-scope namespace retention (QOX2) for
QName-valued structured metadata. Candidate01 sources/modules/packages/receipts
remain frozen; the corrected candidate has its own source/binary identity and
format/offline/package proof. Native service/CLI/SDK/test code did not change, so
passing host compatibility evidence is reused.

The first configure added two required upstream MIT build files and corrected
parent-scope SDK discovery. Expat's UTF-8 comments required an explicit /utf-8
compile option on this codepage936 machine; upstream sources and entropy controls
were unchanged. Test corrections honored the existing interactive-lock contract,
made a purported overflow fixture actually overflow, and fixed compatibility
harness output paths. Failed logs/products remain; no numeric/persisted-identity
assertion was weakened and only affected test/parser inputs were recompiled.

This is one delivered format profile. Other eligible catalogue formats and
application/integration gaps remain active under the existing roadmap/R queue;
it is not full QuPath parity. Masks, arbitrary ellipse transforms, unknown
schemas and acquisition/pixel-payload import remain outside this profile.

## Compressed bit masks in 1.2.0

The optional Windows x64 ABI5 module reads one zlib or bzip2 OME BinData
stream per selected mask. The package pins the codec archives and carries
their original notices. The reader validates canonical base64, exact decoded
crop extent and unused bits before using the native mask contour and project
services. Integral translation remains the supported mask transform.

The cached module, relocated ZIP and offline rebuilt DLL each passed four
imports, four offline reopens, 29 refusal controls and five vector
regressions against the retained host. The independent mask probe checks every
cell on the target canvas and verifies holes, parts, plane, lock and source
facts. Removing the 96 admitted package members preserved both compressed-mask
projects byte for byte and they reopened successfully.

[Delivery evidence](../../current-task.md#delivered-locally---import-compressed-ome-xml-bit-masks)

## Whole-pixel mask transforms in 1.3.0

The optional Windows x64 module supports quarter-turn rotations, reflections,
axis swaps and integral translations under the existing ABI5 mask workflow.
It checks the complete transformed crop against the image and preserves each
covered unit cell. Fractional placement, scale and shear require a separate
resampling contract.

The eight authored fixtures passed the pinned OME 2016-06 schema. Their
expected binary maps come from independently transformed source cell centers.
The released 1.2.0 profile refused the compressed 90 degree case. The cached
1.3.0 DLL, relocated ZIP and offline rebuild each passed twelve imports,
twelve offline reopens, 35 refusal controls and five vector regressions.
Removing all 96 admitted package members preserved both checked projects.

[Delivery evidence](../../current-task.md#delivered-locally---preserve-whole-pixel-ome-mask-transforms)
