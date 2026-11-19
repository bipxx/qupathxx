# Arrow IPC point annotations

The optional Windows x64 native module `org.qupathxx.arrow-points` imports
point annotations from little-endian Arrow IPC V5 files, including Feather v2
files written with default PyArrow LZ4 frame compression. Version 1.6.0 supports
uncompressed, LZ4-frame and Zstandard buffers. Zstandard buffers use one
self-contained frame. The target is an existing project
image. Each row creates one point on Z0/T0 through the native annotation
preview and save-new project workflow.

Pass an absolute path to the installed `format-plugins.json`. Use
`--reader-plugins` with the image's existing reader manifest when applicable.
For example:

```powershell
qupathxx_format_import --format-plugins C:/plugins/arrow/format-plugins.json annotations-import org.qupathxx.arrow-points study.qpxproj 1 points.feather with-points.qpxproj --plugin-options-json '{"coordinate_space":"image-pixels","id":"source_id","x":"x_px","y":"y_px","name":"title"}'
```

Column options use exact, distinct names. Each ID must be unique. Use nonempty
UTF-8 text of at most 256 bytes or a signed or unsigned
8-, 16-, 32-, or 64-bit integer column. Integer IDs use exact decimal text and
retain the declared type in `arrow.source_id_type`. Both coordinates are required, measured in
level-zero image pixels from the top-left origin. Float32 values widen exactly
and Float64 values retain their bits. Integer coordinates are accepted when
exactly representable in binary64. The existing host checks the image bounds.
Optional `name` and `class` columns use UTF-8. Null or empty names fall back to
the ID. Null or empty class values leave the object unclassified. Pass an
explicit `--class-map-json` for source labels and existing native class keys.
UTF-8, large UTF-8, integer, Float32 and Float64 dictionaries use integer
indices and one base per dictionary ID. Delta batches append values in file
index order, preserving earlier indices. Each selected ID, label and coordinate
is resolved before validation. IDs retain the text or integer requirement.
Projection, affine transforms, physical-unit interpretation and plane selection
require a different input profile.

The module checks the redundant schemas, indexed record batches, coordinates,
text and private decoder allocation limit before publication. The current
profile covers flat files with up to 64 columns, 4,096 record batches, 64
dictionary batches, 100,000 entries per dictionary and 100,000 rows, subject
to the host import budget. Arrow streams, Feather v1, nested arrays, selected
extension types and Parquet require separate profiles.

## Offline native build

Use the matching Windows x64 MSVC1944 Release FormatPluginKit, CMake and the
three exact library source archives in the package. From the package root:

```powershell
cmake -S source/plugins/arrow_points -B build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit -DQUPATHXX_NANOARROW_ARCHIVE=source/third_party/nanoarrow-0.9.0-library.zip -DQUPATHXX_ARROW_LZ4_ARCHIVE=source/third_party/lz4-1.10.0-library.zip -DQUPATHXX_ARROW_ZSTD_ARCHIVE=source/third_party/zstd-1.5.7.tar.gz
cmake --build build --config Release --target qpx_arrow_points_plugin
```

This module uses the shared annotation ABI3. QuPathxx source is AGPL-3.0-only.
Apache Arrow nanoarrow 0.9.0 and its bundled FlatCC runtime are Apache-2.0.
The build applies the included patch to interpret dictionary batch compression,
append deltas and enforce complete LZ4 and Zstandard frames. Modified translation
units carry a notice.
LZ4 library 1.10.0, including its bundled xxHash, is BSD-2-Clause. The
Zstandard 1.5.7 decoder uses its BSD-3-Clause option. Exact
corresponding source, dependency notices, checksums and module identity travel
with the package. PyArrow is only used to independently author validation
inputs. [Arrow IPC file specification](https://arrow.apache.org/docs/format/Columnar.html#ipc-file-format)
