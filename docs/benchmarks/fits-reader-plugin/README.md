# Native primary scalar FITS plugin

## Lossless tiled volumes in 1.9.0

The native plugin opens the retained Astropy 7.2.0 Float32 GZIP_2 volume
with five Z planes and partial tiles on all axes. All planes and raw crops
match the independent writer input. GZIP_1 and RICE_1 volume controls
exercise the shared grid and raster path. A singleton Z keeps the XYZ
axis contract. The tests reject inconsistent or missing Z declarations,
a mismatched grid, wrong edge depth and an oversized raster. The saved
HDU and all five planes reopen under a different catalogue default.
[Writer baseline](../../../build/fits-reader/tile-volume-baseline/baseline.json)
[Validation](../../../build/fits-reader/tile-volume-baseline/packaged-validation/validation.json)
[Package provenance](../../../build/fits-reader/tile-volume-baseline/package-1.9.0/SOURCE.json)

## Lossless floating GZIP tiles in 1.8.0

The native plugin reads the retained Astropy 7.2.0 GZIP_2 Float32 image,
including signed zero, a subnormal and partial tiles. It restores byte planes
before applying the existing endian and scalar validity rules. Controls cover
Float64 under both GZIP codecs, unsigned integer blanks and one-byte samples.
It rejects nonzero quantization, unsupported dithering, invalid member extent
and bad CRC. The same saved HDU reopens under a different catalogue default.
[Writer baseline](../../../build/fits-reader/tile-gzip2-baseline/baseline.json)
[Validation](../../../build/fits-reader/tile-gzip2-baseline/packaged-validation/validation.json)
[Package provenance](../../../build/fits-reader/tile-gzip2-baseline/package-1.8.0/SOURCE.json)

## Default RICE_1 image tiles in 1.7.0

The native plugin opens Astropy 7.2.0's default signed Int16 tile output,
including the retained nine-tile image with partial edges. Coverage exercises
8-, 16- and 32-bit storage, exact unsigned offsets and blank validity,
constant and high-entropy blocks, default parameters, and a seven-sample
coding block with reordered parameters in a wide descriptor column. The
reader rejects invalid parameters, a seed mismatch, reserved block codes,
truncated streams and nonzero padding. The test suite checks the retained
source under exact raw crop, display and fresh-process HDU replay.
[Writer baseline](../../../build/fits-reader/tile-rice-baseline/baseline.json)
[Validation](../../../build/fits-reader/tile-rice-baseline/packaged-validation/validation.json)
[Package provenance](../../../build/fits-reader/tile-rice-baseline/package-1.7.0/SOURCE.json)

## Integer GZIP_1 image tiles in 1.6.0

Astropy 7.2.0 wrote a signed Int16 extension with a nine-tile grid and
partial right and bottom tiles that 1.5.0 refused. The local 1.6.0 plugin
opens the retained file and preserves the native scalar read and display
contracts. The test suite checks a raw crop across tile boundaries, an
externally wrapped unsigned image with integer blanks, an explicit heap gap
with a `QB` descriptor, and eight invalid tile profiles. A fresh process
reopens the saved tiled image with HDU 0 as the catalogue default. The
original Astropy fixture is verified by hash and independently regenerated
with only the gzip member timestamps varying.
[Writer baseline](../../../build/fits-reader/tile-gzip-baseline/baseline.json)
[Validation](../../../build/fits-reader/tile-gzip-baseline/packaged-validation/validation.json)
[Package provenance](../../../build/fits-reader/tile-gzip-baseline/package-1.6.0/SOURCE.json)

## Ancillary ASCII tables in 1.5.0

The reader can select an image in a FITS bundle containing ASCII and binary
tables. It traverses ASCII table records using the declared dimensions and
checks column starts, space padding and any declared sums over the full
padded records. Astropy verifies the stored table fixtures independently.
Tests cover plain and gzip images, multiple table blocks, preserved image
indices, and refusal of changed table data or padding.
[FITS ASCII table structure](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)

## Ancillary binary tables in 1.4.0

The provider opens an existing IMAGE HDU at its original file index even
when ordinary BINTABLE extensions occur before or after it. The table is
skipped using checked row and heap extents. Defined checksums cover the
stored table and heap, including when a different HDU is selected.
Astropy verifies the fixed rows and a variable-length heap with a gap.
Tests cover plain and gzip source reopen, data corruption and refusal
of table selection. Table data and tile compression remain unsupported.
[FITS binary-table structure](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)

## External gzip in 1.3.0

The reader accepts a single external gzip member around an admitted FITS file.
The installed plugin verifies gzip integrity and exact extent, then uses the
same FITS parser for primary arrays and explicit HDU selection. The validation
covers exact raw samples, saved compressed source identity, uncompressed
display equivalence and refusal of invalid gzip framing or an invalid inner
checksum. The Windows package includes the exact zlib source and notice.
[CFITSIO compression terminology](https://heasarc.gsfc.nasa.gov/docs/software/fitsio/compression.html)

## Declared checksums in 1.2.0

The native reader verifies canonical CHECKSUM and DATASUM cards on original
stored bytes before returning the selected image. It checks declarations in
each image-only HDU, including empty primaries and unselected extensions.
Astropy and an independent word-sum fixture verify complete, data-only and
header-plus-data declarations. Tests cover multiblock headers, partial words,
corruption and external export refusal. No new runtime decoder is needed.
[FITS 4.0 checksum convention](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)

## Image selection in 1.1.0

`org.qupathxx.fits` 1.1.0 uses contract `fits-image-hdu-scalar-window/2`.
A bounded bundle can begin with an empty primary HDU and contain further
IMAGE extensions. Each bundle read requires the zero-based HDU index in
the manifest options. The reader checks all header and data extents and
padding, and decodes samples only for the chosen image. Each HDU uses the
existing scalar, display and validity rules. Saved projects retain the HDU
option as part of the native source identity.

Astropy 7.2.0 wrote the empty-primary file with a UInt16 SCI volume and
Float32 AUX image. FITS 1.0.0 refused this layout. The selected profile and
option forms are in the [reader guide](../../../plugins/fits/README.md).

## Original standalone primary handoff in 1.0.0

`org.qupathxx.fits` 1.0.0 uses contract `fits-primary-scalar-window/1`.
The optional reader exposes one primary XY or XYZ array in FITS index order.
It preserves signed integers and floating-point bits, and applies the exact
standard pseudo-unsigned offsets for UInt16, UInt32 and UInt64. Integer
`BLANK` and floating NaN remain intact in the source. Raw reads yield zero
with an invalid pixel flag at those positions. Display applies the declared
grayscale window to Z0.
See the [profile guide](../../../plugins/fits/README.md) for the bounds.

## Independent fixture and rights

Astropy 7.2.0 wrote a UInt16 volume with the standard offset and a Float32
image. It reopened both with the expected samples. The retained host attempted
TIFF and refused both before export. The baseline is
`build/fits-reader/baseline.json`.

The independently written adapter and authored fixtures retain AGPL-3.0-only.
The [IAU FITS 4.0 specification](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa.pdf)
is the layout reference. The separately pinned BSD-3-Clause Astropy wheel and
its development import dependencies are recorded in
`build/fits-reader/oracle-download.json`. The source kit includes the native
adapter and reader kit. The Astropy oracle reads only the authored fixtures.
