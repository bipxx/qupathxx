# Native scalar FITS reader plugin

`org.qupathxx.fits` version 1.9.0 implements `fits-image-hdu-scalar-window/2`
for scalar images in primary, IMAGE extension or selected tile-compressed HDUs. The
optional Windows x64 module uses image reader ABI 1.

## File and coordinate profile

The first HDU has `SIMPLE=T`, followed by the ordered `BITPIX` and
`NAXIS` cards. A primary array and each IMAGE extension have two or
three positive dimensions. An empty primary with `NAXIS=0` may precede
IMAGE extensions. Each extension begins with `XTENSION='IMAGE'` and has
`PCOUNT=0` and `GCOUNT=1` after its dimension cards. The reader checks
the complete file, up to 64 HDUs, including the structure and
padding of unselected images.

Ordinary `BINTABLE` and ASCII `TABLE` HDUs may appear before or after a selected image. The
reader checks the table dimensions, column declarations used for structural
admission, binary heap extent, required padding and any declared checksums.
ASCII tables require a starting column for every field, use space padding
and include those padding bytes in the declared checksums. Binary tables
and images retain zero padding. The reader skips table contents and
preserves the physical HDU indices. A table index cannot be opened as an image.

Each header uses 80-byte printable ASCII cards and an `END` card
followed by space padding to a 2880-byte block boundary. Each array is
big-endian and is followed by zero padding to the next block boundary.
The reader bounds each header to 64 blocks. The primary header may
contain `EXTEND`. `COMMENT`, `HISTORY`, and blank cards are descriptive.
An IMAGE extension may also carry `EXTNAME` and `EXTVER`. Table import,
world-coordinate transformations and unrecognized image metadata remain outside
this profile.

## Lossless GZIP_1, GZIP_2 and RICE_1 image tiles

Select the physical HDU index of an XY or XYZ scalar `ZIMAGE` extension with one
`COMPRESSED_DATA` column of `PB` or `QB` descriptors. The reader checks the
grid, partial tile sizes, descriptor and heap bounds, and declared checksums
on the stored table HDU. Table rows order tiles with X varying fastest, then
Y, then Z. Each axis can have a partial edge tile. The selected Z plane is
available for raw reads. Display uses the usual Z0 view. GZIP_1 and GZIP_2
check exact member extent and CRC.
GZIP_2 reverses the byte-plane shuffle before sample conversion. Both GZIP
profiles support integer, Float32 and Float64 storage. Floating tiles use one
lossless data column with absent quantization or `ZQUANTIZ=NONE`. The Astropy
`NO_DITHER` form requires explicit `NOISEBIT=0`. Signed zero and subnormals
retain their exact bits, and NaN uses the existing raw validity policy.
RICE_1 reads 8-, 16- and 32-bit storage, checks the seed and block codes,
and requires exact extent with zero terminal padding. It defaults to 32
pixels per block and four bytes per pixel when those parameters are absent.
The declared or default `BYTEPIX` must match integer `BITPIX`. The existing
integer scaling, raw validity and display policies apply to the decoded raster.
Quantized or dithered tiles, other compressors, and additional columns require
separate profiles.
[FITS tiled image convention](https://fits.gsfc.nasa.gov/registry/tilecompression.html)

Externally gzip-compressed FITS files such as `.fits.gz` use one complete
gzip member containing the same FITS layout. The reader verifies the gzip
extent and CRC before parsing HDUs and applies the FITS checksum rules
inside the decoded file. Concatenated members and trailing bytes refuse.
The work budget accounts for encoded and decoded bytes, decoder workspace
and the selected raster as they are needed. No temporary file is written.

`DATASUM` verifies the original stored data and `CHECKSUM` verifies the
complete HDU. The reader validates either declaration independently, including
for unselected HDUs. It accepts quoted unsigned decimal DATASUM values and
canonical 16-character CHECKSUM values. Blank strings indicate unknown sums.
Duplicate, malformed and mismatched declarations refuse.
[FITS checksum convention](https://fits.gsfc.nasa.gov/registry/checksum.html)

For a file with multiple HDUs, set a zero-based numeric HDU index in
the provider's `options` object in the configured manifest. The primary
HDU is index 0. Select an image with the compact form

```json
{"hdu":1}
```

Valid indices range from 0 through 63. The index must identify an
existing image array. A file containing a single primary array opens
with empty options or a window override alone. Saved reader options
record the HDU index for project replay.

The region CLI also accepts `--reader-options '{"hdu":1}'` immediately after
`--reader org.qupathxx.fits`. This selects the HDU per invocation for both
raw and display modes. Include `window` in the same object when needed.

FITS axis 1 varies fastest in storage. The reader exposes axes 1, 2,
and, when present, 3 as uncalibrated X, Y, and Z index coordinates, in
increasing stored-index order. Z0 is the first plane. The reader exposes
one scalar channel, one resolution level, and one timepoint.

## Scalar samples and validity

| FITS representation | Native raw samples |
| --- | --- |
| `BITPIX=8` | UInt8 |
| `BITPIX=16`, `32`, or `64`, with zero offset | Int16, Int32, or Int64 |
| `BITPIX=16`, `32`, or `64`, with the corresponding unsigned offset | UInt16, UInt32, or UInt64 |
| `BITPIX=-32` or `-64` | Float32 or Float64, with exact IEEE bits |

`BSCALE` defaults to 1 and `BZERO` defaults to 0. The supported unsigned
offsets are exactly 32768, 2147483648, and 9223372036854775808 for 16, 32,
and 64 bits respectively. The reader validates these numbers exactly and
applies the corresponding integer mapping. Other scale or offset values
are unsupported.

An integer sample matching `BLANK` in the stored integer domain or a
floating-point NaN returns zero bytes and validity 0 in the raw region.
Their original codes remain in the source file. Infinities remain valid
raw samples and retain their exact IEEE bits. Out-of-image samples also
return zero bytes and validity 0.

Use `--with-validity` on a region CLI raw export to retain the validity
field alongside the sample values in a single structured NPY file.

Display uses the selected image's first plane and a grayscale window.
Integer defaults span the native type range. Floating-point defaults
span −1 through 1. To select an HDU and override its window together,
set the provider's `options` object in the configured manifest:

```json
{"hdu":1,"window":[-2,2]}
```

For a standalone primary array, the window alone is
`{"window":[-2,2]}`. The endpoints and their positive difference must
be finite. Finite, valid samples map to grayscale using
`floor(clamp((value-low)/(high-low),0,1)*255+0.5)` and have alpha 255.
Undefined samples, nonfinite samples, and display padding are transparent
black. The window affects display only.

## Build and use

Use Windows x64, the compiler recorded in the matching ReaderPluginKit,
and Release `/MD`. In a Visual Studio developer shell with absolute paths:

```powershell
cmake -S plugins/fits -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_FITS_ZLIB_ARCHIVE=ZLIB_1_3_2_ARCHIVE"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

The offline source kit includes `source/plugins/fits`,
`source/plugins/image-common`, and `source/reader-plugin-kit`. Set
`QUPATHXX_FITS_BUILD_TESTS=ON` and provide a source-matched native SDK
to build the project consumer tests. The default module-only build uses
the ReaderPluginKit, platform CRT and bundled zlib source.

Configure the generated hash-bound manifest and select the provider:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.fits INPUT.fits 0 0 0 512 512 OUTPUT.pam
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.fits --raw INPUT.fits OUTPUT.npy --width 512 --height 512 --z 0
```

The reader uses the ABI's work budget up to 256 MiB and checks
cancellation while opening files and reading raw regions. Close active
module users before removing package-owned files.

## Rights and verification

The independently authored reader and its tests use
[AGPL-3.0-only](../../LICENSE). The
[IAU FITS Standard 4.0](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa.pdf)
documents the format. The C++ module uses the ReaderPluginKit and the
platform CRT. It links private zlib 1.3.2 from the exact shipped source archive.
Redistribute the archive and its original Zlib license in the package.

The validation fixtures use [Astropy 7.2.0](https://github.com/astropy/astropy/tree/v7.2.0)
under its [BSD-3-Clause license](https://raw.githubusercontent.com/astropy/astropy/v7.2.0/LICENSE.rst)
as a development oracle. The pinned Astropy wheel has SHA-256
`dc7c340ba1713e55c93071b32033f3153470a0f663a4d539c03a7c9b44020790`.
The local oracle download receipt records the exact wheels used for
validation. The package includes the native module and independently
authored product and test sources.

The [delivery receipt](../../docs/benchmarks/fits-reader-plugin/README.md)
records the independent inputs, native verification, and package provenance.
