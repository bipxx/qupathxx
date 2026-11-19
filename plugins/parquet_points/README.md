# Parquet point annotations

The optional Windows x64 native module `org.qupathxx.parquet-points` 1.1.0 imports
one point per row from a flat Parquet table. Select the unique ID column and the
two level-zero pixel coordinates explicitly. The native annotation import
workflow validates the target image frame and saves to a new project.

```powershell
qupathxx_format_import --format-plugins C:/plugins/parquet-points/format-plugins.json annotations-import org.qupathxx.parquet-points study.qpxproj 1 points.parquet with-points.qpxproj --plugin-options-json '{"coordinate_space":"image-pixels","id":"source_id","x":"x_px","y":"y_px","name":"title"}'
```

Pass the saved image reader's absolute `--reader-plugins` manifest when needed.
IDs can be nonempty UTF-8 or signed or unsigned 8-, 16-, 32-, or 64-bit
integers. Integer IDs are stored as exact decimal text and retain their
declared type in `parquet.source_id_type`. Coordinates must be finite,
nonnegative and exactly representable as binary64. Null or empty names fall
back to the ID. A selected
`class` column can supply source labels for an explicit `--class-map-json`.
Null or empty class labels leave the points unclassified.

The profile supports dictionary or plain pages with uncompressed, Snappy or
Zstandard buffers. Page checksums are verified when present. The provider
uses local unencrypted flat columns and enforces the native work budget.
GeoParquet geometries, physical units, coordinate reference systems and
affine transforms require different profiles.

## Offline build

From the package root, use PowerShell 7, CMake and the Windows x64 MSVC1944
FormatPluginKit supplied with the module source.

```powershell
pwsh -File source/tools/ci/build_parquet_decoder.ps1 -Archives source/third_party -BuildRoot decoder-build
cmake -S source/plugins/parquet_points -B plugin-build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit -DCMAKE_PREFIX_PATH=decoder-build/decoder-sdk
cmake --build plugin-build --config Release --target qpx_parquet_points_plugin
```

The plugin uses annotation ABI3 and the same statically linked Parquet 26.0.1
decoder as the matrix plugin. QuPathxx code is AGPL-3.0-only. The decoder's
source and codec licenses, notices and hashes are included in the package.
[Apache Parquet file format](https://parquet.apache.org/docs/file-format/)
