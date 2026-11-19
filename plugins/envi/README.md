# ENVI Standard raster reader

`org.qupathxx.envi` opens an uncalibrated ENVI Standard `.hdr` with one
raster in the same directory. The default is a same-stem `.img`. Choose the provider explicitly since
the `.hdr` extension also names other formats. The native reader records both
files in the project source identity.

```text
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.envi --raw IMAGE.hdr OUTPUT.npy --channel 0 --channels 3
```

Version 1.1.0 adds `--reader-options '{"companion_layout":"dat"}'` before
`--raw` for `IMAGE.hdr` plus `IMAGE.dat`. Use `bin` for `IMAGE.bin` and
`appended` for `IMAGE.bin.hdr` plus `IMAGE.bin`. `img` explicitly selects the
default layout. Version 1.5.0 also accepts `raw`, `bil`, `bip`, and `bsq` for
same-stem companions with those suffixes. The option selects the filename.
The header's `interleave` field determines how raster samples are stored.
Project creation accepts the same options and saves the selection and source
facts. A missing selected companion stops the read.
[GDAL ENVI naming conventions](https://gdal.org/en/stable/drivers/raster/envi.html)

Set `--channels` to the number of bands you want to export, starting from
`--channel` (zero-based). The profile supports BSQ, BIL and BIP storage, both
byte orders, and an explicit payload offset. It retains band order and supplied
ASCII band names. The supported ENVI type codes are 1, 2, 3, 4, 5, 12, 13,
14 and 15 (UInt8, Int16, Int32, Float32, Float64, UInt16, UInt32, Int64 and
UInt64). Version 1.4.0 adds the two 64-bit integer types.
[ENVI raster layout reference](https://gdal.org/en/stable/drivers/raster/envi.html)

The RGBA preview uses the integer type range or the floating-point interval
[0, 1]. Version 1.3.0 reads `default bands` from the header. Use one one-based
band number in braces for grayscale or three in red, green, blue order. For
example, `default bands = {3, 1, 4}` selects bands 3, 1 and 4 for RGB.
An omitted field uses the first band in grayscale. A nonfinite sample in any
displayed band makes the pixel transparent. Raw reads use the stored band order
and explicit channel selection.

Version 1.2.0 reads one scalar `data ignore value`.
An integer raster requires a decimal integer within its sample range. A
floating-point raster accepts a finite number parsed at the raster's stored
precision. An ignore value in any displayed band makes the pixel transparent.
Integer equality uses the stored precision, including adjacent Int64 and
UInt64 values above the exact range of binary64.
In raw reads, a tuple is invalid when any requested band equals the sentinel.
`--with-validity` writes an invalid flag and a zero-filled tuple, as it does
for pixels outside the raster. Valid tuples preserve the original sample bits.
Plain NPY export requires all requested tuples to be valid.
[ENVI header reference](https://www.nv5geospatialsoftware.com/docs/enviheaderfiles.html)

Inputs with georeferencing or wavelength metadata, complex samples, or other
fields outside the declared profile are refused.

Build the optional AGPL-3.0-only module in Release against its matching
`QUPATHXX_READER_PLUGIN_KIT_PREFIX`. The module has no codec dependencies.
Spectral Python 0.24 (MIT) served as the independent development writer and
oracle for authored data. It is outside the native build and package.
[Spectral Python ENVI documentation](https://www.spectralpython.net/fileio.html)
