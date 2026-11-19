# Native MetaImage reader

`org.qupathxx.metaimage` 1.7.0 provides the `metaimage-binary-samples/2` profile.
It is an optional Windows x64 MSVC reader built against the matching
ReaderPluginKit. The parser and authored tests are AGPL-3.0-only. The
[ITK MetaImage documentation](https://docs.itk.org/en/latest/learn/metaio.html)
describes the file layout. The plugin uses the native ABI and the Windows C++
runtime. Version 1.2.0 statically links the admitted zlib 1.3.2 source under
the Zlib license and ships the original source archive and notice.

- The profile accepts binary XY and XYZ with one or more components. Attached MHA
  has `ElementDataFile = LOCAL` followed immediately by the raster. Detached
  MHD names one same-directory file. `HeaderSize` may select a nonnegative
  offset or locate a raster of exactly the declared size at EOF using `-1`.
- An XYZ MHD header can use `ElementDataFile = LIST` (also `LIST 2D` or
  `LIST 2`) with one plain same-directory filename per XY slice in stored Z
  order, up to 63 slices. Every filename must identify a distinct regular
  file. Each slice uses the declared byte order, component count and offset.
  Uncompressed slices accept EOF-relative offsets.
- An XYZ MHD header can also specify numbered slices, for example
  `ElementDataFile = slice%03d.raw 1 5 2` for three slices. One `%d` or
  zero-padded decimal placeholder with a field width of 1–11 expands signed
  32-bit indices in ascending order. The step defaults to one. The inclusive
  range and positive step must select exactly the declared Z size. The same
  63-file limit and same-directory guard apply.
- Detached names can contain interior ASCII spaces. LIST uses the full
  filename on each line. Numbered patterns with spaces require all three
  trailing arguments (start, end and step).
- `CompressedData = True` selects one zlib stream with its checksum, decoded
  raster extent and end of stream validated. `CompressedDataSize`, when
  present, must match the stored stream size. An EOF-relative offset needs
  this field to locate the stream. Other offsets allow the stream extent to
  be determined through EOF. A compressed slice series uses one zlib stream per
  slice and a nonnegative offset. A supplied `CompressedDataSize` applies
  to each slice.
- Exact samples retain signed and unsigned 8-, 16-, 32- and 64-bit integers
  and Float32 and Float64. The 64-bit integer tags are `MET_LONG_LONG` and
  `MET_ULONG_LONG`. Multibyte files identify their byte order explicitly.
  Raw requests select Z and a contiguous component range with a single
  series and timepoint. `ElementNumberOfChannels` defaults to one and is
  bounded at 65,536 by the native reader contract and raster work budget.
  Components are interleaved within each voxel and exposed in source order.
  Multi-component axes are CXY or CXYZ, with labels `Component 1` onward.
- Positive axis spacing is converted from millimetres to micrometres.
  `ElementSize` supplies the scale when spacing is absent. Omitted scale
  remains unknown. The accepted geometry has an identity direction matrix
  and zero origin and rotation center.
- Version 1.1.0 accepts the default orientation tags from ITK's identity
  writer, `??` for XY and `RAI` for XYZ, with an explicit identity direction
  matrix. Samples remain in source order.
- The default Z0 RGBA view displays component 1 in grayscale using the full integer range or a fixed
  `[-1, 1]` floating-point window. Nonfinite samples are transparent in
  this view and retain their stored bits in raw output.

The reader bounds headers to 64 KiB and loads the raster within the native
reader workspace. It checks exact extent, rejects detached primary aliases
and reparse-point companions, and reports every companion to the host
for project identity. ASCII data, other filename patterns, general affine
geometry, and other anatomical frames require separate profiles.

Configure this directory with `QUPATHXX_READER_PLUGIN_KIT_PREFIX` set to the
matching kit and `QUPATHXX_METAIMAGE_ZLIB_ARCHIVE` set to the pinned offline
archive. Build and install the Release configuration. Load the
resulting `reader-plugins.json` and select `org.qupathxx.metaimage` in the
native host. The separate `tests` directory builds the project probe against
a native SDK. `tools/ci/check_metaimage_reader_plugin.py` exercises the
installed manifest and probe. Generate its producer inputs using
`tests/write_simpleitk_fixtures.py` with SimpleITK 2.5.3 and NumPy in a
development environment. Run the generator for separate raw and
`--compressed` fixture directories. Include `--vector` for component tests and
`--wide-integers` for Int64 and UInt64 fixtures. Use `--slice-list` in
separate runs with and without `--compressed` for ordered LIST fixtures.
Use `--numbered-slices` for the equivalent numbered fixture runs.
Use `--spaced-filenames` to generate detached, LIST and numbered examples.
SimpleITK serves as an independent producer and
round-trip oracle. [SimpleITK 2.5.3 license](https://github.com/SimpleITK/SimpleITK/blob/v2.5.3/LICENSE)
