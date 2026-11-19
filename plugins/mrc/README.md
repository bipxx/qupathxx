# Native scalar MRC2014 reader plugin

`org.qupathxx.mrc` version 1.5.0, contract `mrc2014-scalar-window/3`, is an
independently authored C++ reader for calibrated scalar images and volumes.
It uses image reader ABI 1 and builds as an optional Windows module.

## Selected file and axis contract

The reader accepts a 1024-byte MRC2014 header followed by one raster of
exactly the declared size. It requires the `MAP ` identifier, version
20140 or 20141, and a supported machine stamp. Both IEEE little-endian
stamps (`44 44 00 00` and `44 41 00 00`) and the IEEE big-endian stamp
(`11 11 00 00`) are supported.

Files with a case-insensitive `.gz` or `.bz2` suffix and matching magic may
wrap the same MRC bytes in one complete compressed stream. The reader verifies
the checksum, decoded length, and end of the compressed stream. The project
links and fingerprints the original compressed file directly.

| Mode | Native raw samples | Default display window |
| --- | --- | --- |
| 0 | Int8 | −128 to 127 |
| 1 | Int16 | −32768 to 32767 |
| 2 | Float32, original IEEE bits | −1 to 1 |
| 6 | UInt16 | 0 to 65535 |
| 12 | IEEE binary16 widened exactly to Float32 | −1 to 1 |
| 101 | Packed 4-bit unsigned samples exposed as exact UInt8 values | 0 to 15 |

Columns vary fastest in storage, followed by rows and sections.
`MAPC`, `MAPR`, and `MAPS` place those dimensions on the X, Y, and Z axes.
The reader exposes the named axes in X/Y/Z order and preserves increasing
indices along each axis. MRC producers may use different handedness
conventions, so the reader makes no world registration or handedness inference.

`ISPG=0` is accepted for an XY image or an image stack with `MAPS=3` and
`MZ=1`. Each section is one XY series. `ISPG=1` is accepted for one orthogonal
XYZ volume. `ISPG=401` with `MAPS=3` groups sections into P1 volume series,
each with `MZ` Z planes. The total section count `NZ` must divide evenly
into those volumes. The selected profile requires 90-degree cell angles,
zero starts and origin, positive `MX`, `MY`, `MZ` and no extended header.
Other space groups, nonzero origins, oblique cells, complex and other sample
modes, and older variants remain outside this profile.

## Calibration, raw reads, and display

Finite, positive `CELLA` in angstroms is divided by the corresponding
`MX`, `MY`, or `MZ` sampling count and converted to micrometers. Each
axis is independent. A zero cell length leaves its calibration unknown.
XY images leave Z spacing unknown. Negative or nonfinite cell lengths
are malformed.

Raw reads expose one scalar channel at each Z plane of the selected series,
with one resolution level and T0. Series selection follows the stored stack
order and defaults to zero. Native byte ordering preserves stored integer
codes and Float32 representations, including quiet and signaling NaNs,
infinities, and signed zero. All in-image raw samples are valid. Samples
outside the image are zero and invalid.

Mode 12 uses the CCP-EM binary16 extension. Widening preserves finite
values including subnormals, signed zero, infinities and the sign and
signaling or quiet payload bits of NaNs. The underlying file remains a
two-byte-per-sample raster. [CCP-EM format proposals](https://www-test2.ccpem.ac.uk/mrc-format/mrc-format-proposals/)

Mode 101 supports even storage-column counts. Each byte contains the earlier
column in its low nibble and the next column in its high nibble. Header byte
order leaves nibble order unchanged. The reader preserves the exact 0–15 codes
and the original packed file identity. [CCP-EM format update](https://www.ccpem.ac.uk/mrc-format/mrc-format-proposals/) · [EMAN2 decoder reference](https://blake.bcm.edu/doxygen/mrcio_8cpp_source.html)

Display uses Z0 of the selected series and a grayscale window. Select a
zero-based series and optionally override the mode-specific window in
`--reader-options` or the provider's manifest `options` object:

```json
{"series":1,"window":[-2,2]}
```

The endpoints and their positive difference must be finite. Display uses
`floor(clamp((value-low)/(high-low),0,1)*255+0.5)` for finite values, with
alpha 255. Nonfinite pixels and display padding are transparent black.
Windowing changes display only. Header min, max, mean, and RMS are
descriptive and do not set the window.

## Build and use

Use Windows x64 and the MSVC compiler and Release `/MD` profile recorded in
the matching ReaderPluginKit. Build with absolute paths in a Visual Studio
developer shell:

```powershell
cmake -S plugins/mrc -B BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release `
  "-DQUPATHXX_READER_PLUGIN_KIT_PREFIX=KIT" `
  "-DQUPATHXX_MRC_ZLIB_ARCHIVE=ZLIB_ARCHIVE" `
  "-DQUPATHXX_MRC_BZIP2_ARCHIVE=BZIP2_ARCHIVE"
cmake --build BUILD --config Release
cmake --install BUILD --config Release --prefix INSTALL
```

The offline package includes `source/plugins/mrc`, `source/plugins/image-common`,
`source/reader-plugin-kit`, and the pinned archives `zlib-1.3.2.tar.gz` and
`bzip2-1.0.8.tar.gz` under `source/third_party`. Use the absolute paths to
the archives in the build command. Native source builds use the default
`QUPATHXX_MRC_BUILD_TESTS=OFF`. For test builds, enable that option and
provide a source-matched native SDK for the project consumer.

Configure the generated hash-bound manifest and select the provider explicitly:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.mrc INPUT.mrc 0 0 0 512 512 OUTPUT.pam
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.mrc --raw INPUT.mrc OUTPUT.npy --width 512 --height 512 --z 0
```

Work is bounded at 256 MiB, including compressed and decoded allocations,
codec workspace and reader overhead.
The reader checks cancellation during opening and raw reads within the
existing ABI. Close active module users before removing package-owned files.

## Rights and verification

The adapter and its authored tests use [AGPL-3.0-only](../../LICENSE).
The module links private static builds of unmodified zlib 1.3.2 under
the [Zlib license](https://zlib.net/zlib_license.html) and libbzip2 1.0.8
under the bzip2-1.0.6 license. Both source archives and original notices
ship with the module.
The format is documented by [CCP-EM](https://www.ccpem.ac.uk/mrc-format/mrc2014/)
and the [open-access MRC2014 paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC4642651/).
The validation fixtures use [BSD-3-Clause mrcfile 1.5.4](https://github.com/ccpem/mrcfile/tree/v1.5.4)
as a development oracle. The pinned wheel has SHA-256
`195370a13db5ce19600499f9bdf90fd9888979bf39de19a17c19024436c6f4c2`.
The standalone module and corresponding source build require neither
mrcfile nor Python.

The [delivery receipt](../../docs/benchmarks/mrc-reader-plugin/README.md)
records the independent inputs, native verification, and package provenance.
