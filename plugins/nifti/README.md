# Native scalar NIfTI reader plugin

`org.qupathxx.nifti` version 1.5.0 implements `nifti-scalar-window/2` through
image reader ABI 1. The optional module opens scalar XY, XYZ and XYZT NIfTI-1 and
NIfTI-2 images from `.nii`, `.nii.gz`, and paired `.hdr` or `.hdr.gz`
with `.img` or `.img.gz`.

## File and coordinate profile

Both formats support either byte order. NIfTI-1 uses a 348-byte header and
NIfTI-2 uses a 540-byte header. Single files require `n+1` or `n+2` magic,
a four-byte extender and an exact voxel payload through end of file. Without
extensions, the voxel payload starts at byte 352 or 544 respectively. Pairs
require `ni1` or `ni2` magic and may use a bare header. The NIfTI-2 eight-byte signature includes
its exact end-of-line check. Open
the header and keep its same-stem image file in the same directory. Both members
must be regular files with one link. The image payload starts at the declared
nonnegative integral `vox_offset` and ends exactly after the last voxel.
If both image candidates exist, the one matching the header's compression
is selected. The other candidate is considered only when the preferred path
is absent. Gzip members each require one complete stream. The host tracks the
header and selected companion together. The first
two, three or four dimensions must be positive and fit the reader's axis and work
limits. NIfTI-2 reads 64-bit dimensions and voxel offsets before checking their
representability, and its binary64 calibration retains the source precision.

Standard code-6 ASCII comment records can follow the extender up to the
single-file voxel offset or the end of a pair header. The decoded header area
is limited to 1 MiB. Each record uses its header byte order, a length divisible
by 16 and optional trailing zero padding. Comments remain in the linked
source and are skipped during voxel interpretation. Other extension codes
and unframed padding are refused.
[NIfTI extension format](https://nifti.nimh.nih.gov/nifti-1/documentation/faq.html)
X varies fastest, followed by Y, Z and T for the axes present. The reader
exposes one scalar channel and one resolution level, and displays Z0/T0.
Select other Z/T positions through the raw region API or the region CLI's
`--z` and `--time` options. The profile requires zero intent, slice timing
and time origin. Analyze and ranks above four remain outside it.

The reader exposes local voxel indices and the following axis spacings.
Spatial unit codes 1, 2, and 3 indicate meters, millimeters, and micrometers.
Known spacings convert to micrometers. Unit code 0 leaves physical
calibration unspecified. A 2D image has X and Y spacing only.
XYZT files require finite positive `pixdim[4]`. Seconds, milliseconds and
microseconds convert to a positive seconds interval. Unknown temporal units
leave the interval unspecified. Frequency units and nonzero `toffset` refuse.

| Active transform | Accepted geometry and spacing source |
| --- | --- |
| Neither | Finite positive spatial `pixdim` values for the present axes. |
| qform | Zero rotation quaternion components and zero translation, with `qfac` zero or +1. Spacing comes from finite positive spatial `pixdim`. |
| sform | Finite positive diagonal spatial scales, with zero off-diagonal and translation terms. Spacing comes from the diagonal. |
| Both | Both transform profiles apply and their spatial scales must agree exactly. |

Supported qform and sform codes are 0 (inactive), 1, and 2. The reader ignores
inactive transform fields. It does not apply registration or reorient
the voxel array. The [NIMH NIfTI-1 field documentation](https://nifti.nimh.nih.gov/nifti-1/documentation/nifti1fields/index_html.html)
defines the storage, unit, and transform fields.
The [NIfTI-2 header specification](https://github.com/NIFTI-Imaging/nifti_clib/blob/master/nifti2/nifti2.h)
defines its corresponding wider fields.

## Scalar samples and display

| `datatype` | `bitpix` | Raw sample |
| --- | --- | --- |
| 2 | 8 | UInt8 |
| 4 | 16 | Int16 |
| 8 | 32 | Int32 |
| 16 | 32 | Float32 |
| 64 | 64 | Float64 |
| 256 | 8 | Int8 |
| 512 | 16 | UInt16 |
| 768 | 32 | UInt32 |
| 1024 | 64 | Int64 |
| 1280 | 64 | UInt64 |

The reader accepts `scl_slope=0` with an inactive intercept or
`scl_slope=1` with a zero intercept. It translates to the native byte order
while preserving every stored sample bit, including floating-point NaNs
and infinities. Every in-bounds raw sample has validity 1. Out-of-image
samples return zero bytes and validity 0.

Display uses grayscale on the first stored plane. The default integer
window spans the scalar type range. The floating-point default is −1 to 1.
Set the provider's manifest options to override the window.

```json
{"window":[-2,2]}
```

The endpoints and their positive difference must be finite. Finite samples
map to grayscale with `floor(clamp((value-low)/(high-low),0,1)*255+0.5)`
and alpha 255. Nonfinite samples and out-of-image padding are transparent
black. The window affects display only.

Each gzip file must contain one complete member with validated checksum and
size and decode to the exact NIfTI raster extent. Compressed pair headers are
limited to 1 MiB decoded. The reader
uses the ABI work budget up to 256 MiB and charges gzip input, decoded
payload and inflate workspace against that budget. The reader observes
cancellation while opening files and reading regions.

## Build and use

Use Windows x64, the compiler recorded in the matching ReaderPluginKit,
and Release `/MD`. In a Visual Studio developer shell with absolute paths,
point the build at the kit and pinned zlib archive.

```powershell
cmake -S plugins/nifti -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_NIFTI_ZLIB_ARCHIVE=ZLIB_ARCHIVE"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

The staged offline build uses `source/plugins/nifti`,
`source/plugins/image-common`, `source/reader-plugin-kit`, and
`source/third_party/zlib-1.3.2.tar.gz`. The zlib archive SHA-256 is
`bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`.
Set `QUPATHXX_NIFTI_BUILD_TESTS=ON` and provide a source-matched native
SDK for the independent ABI tests and installed project consumer.

Configure the generated manifest and select the provider.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.nifti INPUT.nii.gz 0 0 0 512 512 OUTPUT.pam
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.nifti --raw INPUT.nii OUTPUT.npy --width 512 --height 512 --z 0
```

Close active module users before removing package-owned files.

## Rights and verification

The independently authored reader and tests use
[AGPL-3.0-only](../../LICENSE). The native module links a private static
zlib 1.3.2 under the Zlib license and uses the kit's platform CRT. The
package includes the exact zlib archive and original notice at
`licenses/zlib/LICENSE`. It includes the authored build inputs and
does not incorporate third-party NIfTI reader implementations.

[NiBabel 5.3.2](https://github.com/nipy/nibabel/tree/5.3.2) supplies a
development-only producer and oracle for repository-authored arrays. Its
core is MIT and its [COPYING file](https://raw.githubusercontent.com/nipy/nibabel/5.3.2/COPYING)
records third-party declarations. The pinned wheel SHA-256 is
`52970a5a8a53b1b55249cba4d9bcfaa8cc57e3e5af35a29d7352237e8680a6f8`.
The local `build/nifti-reader/oracle-download.json` records the download and
notice identities. The wheel is outside the runtime and source package.

The [delivery receipt](../../docs/benchmarks/nifti-reader-plugin/README.md)
records the independent inputs, verification, and package provenance.
