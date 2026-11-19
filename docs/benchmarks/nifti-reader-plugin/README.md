# Native single-file scalar NIfTI-1 plugin

`org.qupathxx.nifti` version 1.0.0 uses contract
`nifti1-scalar-window/1`. It reads a single 2D or 3D scalar NIfTI-1 image
stored directly in a file or inside one complete gzip member. The native
reader retains exact typed bytes in X-fast voxel order. The selected positive
diagonal qform and sform cases provide physical voxel spacing without
resampling. The [profile guide](../../../plugins/nifti/README.md) specifies
the supported units, transforms, sample types and display windows.

## Independent fixture and rights

NiBabel 5.3.2 generated both layouts with Int16 codes and an anisotropic
affine. Both files reopened with the authored values and affine. The
retained host refused both before raw export, as recorded in
`build/nifti-reader/baseline.json`.

The adapter, authored fixtures and reader kit retain AGPL-3.0-only.
The [NIMH NIfTI-1 format definition](https://nifti.nimh.nih.gov/nifti-1/)
is the public reference. The optional module includes private static zlib
1.3.2 under the Zlib license. Matching source and its original notice are
in the package. The NiBabel wheel and its MIT core and third-party notices
are pinned separately in `build/nifti-reader/oracle-download.json` for
development fixture verification.

## Paired header and payload in 1.1.0

The 1.1.0 provider accepts plain `.hdr`/`.img` pairs and pairs with `.gz` on both
members. It preserves the exact scalar bytes after the declared integral offset
and uses the same calibration and display interpretation as the single-file
reader. The host's file-set ABI protects both source identities in display,
raw export, saved projects and result replay. The independently pinned NiBabel
5.3.2 producer and fixtures cover the admitted layouts without new dependencies.
[NIfTI-1 paired file rules](https://nifti.nimh.nih.gov/nifti-1/documentation/faq/index.html)

## NIfTI-2 scalar images and volumes in 1.2.0

The same native reader accepts selected NIfTI-2 single-file and paired layouts
in both byte orders. Checked 64-bit dimensions and offsets feed the existing
scalar raster path. Binary64 transform fields feed the same positive axis-aligned
calibration contract, and reader metadata identifies the on-disk format version.
NiBabel 5.3.2 verifies the four producer layouts, wider dimensions and exact
sample bytes. The independently authored profile checks reject corrupt headers,
unsupported transforms and extents beyond the bounded reader contract.
[NIfTI-2 specification](https://nifti.nimh.nih.gov/nifti-2/)

## Mixed-compression image pairs in 1.3.0

The Windows native provider reads a plain header with a gzip image or a
gzip header with a plain image for both NIfTI versions. When both image
suffixes exist, the matching compression keeps precedence. Corrupt preferred
files refuse, and saved projects observe the actual companion path and identity.
The independent NiBabel 5.3.2 oracle uses explicit header and image paths.
It verifies exact raw samples and offsets. The hosted checks also cover
project attach, reopen, missing provider, adding a preferred companion and
replacing the observed companion with byte-identical content.
[Reference reader companion discovery](https://github.com/NIFTI-Imaging/nifti_clib/blob/master/niftilib/nifti1_io.c#L2647)

## Scalar time series in 1.4.0

Single-file and paired NIfTI-1 and NIfTI-2 XYZT profiles expose exact scalar
samples at each raw Z/T index. Display stays on Z0/T0. Declared seconds,
milliseconds and microseconds normalize to seconds, independently from
spatial units. Unknown time units leave the interval unspecified. Nonzero
time origin, frequency units and invalid step or raster extent refuse.
NiBabel 5.3.2 checks stored axes, samples and timing. The hosted checks
cover later-frame export, reopened and offline project metadata, and display.
[NIfTI time and unit fields](https://nifti.nimh.nih.gov/nifti-1/documentation/nifti1fields/nifti1fields_pages/pixdim.html/document_view.html)

## Standard comment records in 1.5.0

The Windows plugin accepts standard code-6 ASCII comments in single-file
and paired NIfTI-1 and NIfTI-2. Native parsing checks the extender, each
record's byte order and 16-byte alignment, the bounded extension area and
zero padding. The checked records do not affect interpretation of the raw
voxel payload, calibration or display. NiBabel 5.3.2 independently serializes
and reads the extension fixtures. The native validator checks both byte orders,
multiple records, gzip, pairs with independent raster offsets and malformed
extension framing.
[NIfTI extension format](https://nifti.nimh.nih.gov/nifti-1/documentation/faq.html)
