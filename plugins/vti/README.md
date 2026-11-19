# Native VTK ImageData reader

`org.qupathxx.vti` 1.8.0 provides the `vtk-imagedata-point-array/3` profile
on Windows x64. It reads up to 64 pieces in a serial VTI dataset with a
first stored point at physical zero and identity direction. Every piece
has the same selected point array and optional empty CellData.
Their union must cover the entire volume. Overlap is allowed when complete
voxel tuples agree. It uses the native reader ABI and preserves component
order and the X-fastest spatial order.
[VTK XML ImageData format](https://docs.vtk.org/en/latest/vtk_file_formats/vtkxml_file_format.html#imagedata)

Whole and piece extent indices can be signed. The declared origin plus the
whole extent's starting index times spacing must equal zero on each axis.
The reader subtracts the whole extent start when addressing samples. This
keeps spacing, voxel order and physical positions exact.
[VTK 9.4 extent and origin definitions](https://vtk.org/doc/nightly/release/9.4/html/classvtkImageData.html)

- Accepts XY and XYZ with a selected named point array and signed or unsigned 8-, 16-, 32- and 64-bit
  integers, Float32 and Float64. It reads ASCII, inline base64 and a single
  selected appended array per piece in base64 or raw-byte form. Appended offsets
  identify positions in the original file after the underscore. Binary arrays
  support both byte orders and UInt32 or UInt64 header words.
- Multi-component scalars and three-component active vectors expose contiguous
  raw component ranges. The reader preserves explicit `ComponentNameN` labels
  and labels unnamed components with the array name and their one-based index.
  Multi-component axes are CXY or CXYZ.
- Use `{"point_array":"Auxiliary","spacing_unit":"mm"}` to select an exact
  array name, including an inactive array. With no name the reader uses the sole
  active scalar or vector. Different active scalar and vector names require
  explicit selection. Each piece can carry up to 64 named arrays. All appended
  offsets delimit byte ranges, including those of unselected arrays.
- Binary arrays accept uncompressed values and VTK's zlib, LZ4 and LZMA
  layouts. The LZMA writer profile uses one CRC32-checked XZ stream per VTK
  block. The reader checks header geometry, block counts, input extents and
  decoded sizes. Zlib and XZ checksums are verified. Up to 65,536 blocks fit
  within the reader work budget.
- Raw-appended files use VTK writer framing. The reader separates the binary
  body before parsing the XML metadata and consumes exactly the declared
  body bytes. LF and CRLF metadata are supported.
- Configure `{"spacing_unit":"mm"}` or `{"spacing_unit":"um"}` in a reader
  catalog entry or pass it using the CLI's `--reader-options`. The host
  retains the selected unit in the project's reader identity. Spacing of
  one on every axis may be read without a unit and remains uncalibrated.
- The Z0 grayscale preview uses the first component and its integer type range or a fixed floating-point
  window of `[-1, 1]`. Raw export supports any stored Z plane. Nonfinite
  floating values retain their raw binary representation and render transparent.

This profile bounds file input to 32 MiB and uses no entity references,
DTDs or additional XML metadata. The decoded piece data and assembly
workspace remain bounded. Other compressors,
parallel VTI layouts, cell scalars, nonidentity directions and
grids whose first sample is physically translated require separate profiles.

Build the Release configuration with the matching
`QUPATHXX_READER_PLUGIN_KIT_PREFIX` and the admitted offline
`QUPATHXX_VTI_TINYXML2_ARCHIVE`, `QUPATHXX_VTI_ZLIB_ARCHIVE`,
`QUPATHXX_VTI_LZ4_ARCHIVE` and `QUPATHXX_VTI_XZ_ARCHIVE`. The AGPL-3.0-only
plugin statically compiles tinyxml2 11.0.0 and zlib 1.3.2 under Zlib,
liblz4 1.10.0 under BSD-2-Clause and XZ Embedded revision
`ae63ae3a36ed01724674e8f3d750dc47bf125410` under 0BSD.
The source archives and original notices ship with the plugin. VTK 9.4.2
serves as a development oracle through `tests/write_vtk_fixtures.py`.
Use `--lz4` or `--lzma` for those codecs and `--streamed` to reproduce
multi-piece controls. Use `--shifted-extent` for a signed-index volume.
Build the optional `tests` SDK consumer and use
`tools/ci/check_vti_reader_plugin.py` to verify the installed manifest.
[tinyxml2 license](https://github.com/leethomason/tinyxml2/blob/11.0.0/LICENSE.txt) · [VTK license](https://github.com/Kitware/VTK/blob/v9.4.2/Copyright.txt)
