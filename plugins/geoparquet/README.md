# GeoParquet image-pixel annotations

The optional Windows x64 native module `org.qupathxx.geoparquet` version 1.3.1
imports one selected WKB geometry column from GeoParquet 1.0.0 or 1.1.0. It also
imports the six native separated XY encodings in GeoParquet 1.1.0. Choose the
source ID and coordinate interpretation explicitly. The native annotation
workflow imports into the saved target image and writes a new project.

```powershell
qupathxx_format_import --format-plugins C:/plugins/geoparquet/format-plugins.json annotations-import org.qupathxx.geoparquet study.qpxproj 1 regions.parquet with-regions.qpxproj --plugin-options-json '{"coordinate_space":"image-pixels","geometry":"geometry","id":"source_id","name":"title"}'
```

Pass `--reader-plugins` with the saved image reader's absolute manifest if it
uses a reader plugin. IDs must be unique and non-null. Select either nonempty
UTF-8 text or a signed or unsigned 8-, 16-, 32-, or 64-bit integer ID column.
Integer IDs are retained as exact decimal text, with their declared type in
`geoparquet.source_id_type`. Name and class columns use UTF-8 and are optional.
Pass `--class-map-json` to map nonempty source class
labels to existing native classes. Source facts retain the selected geometry
column, schema version, root feature ID and admitted source CRS.
[Parquet logical integer types](https://parquet.apache.org/docs/file-format/types/logicaltypes/)

The `image-pixels` option requires explicit `crs: null` and planar edges.
Use `projected-xy` for a declared two-axis Cartesian projected CRS when you
have an affine from the stored source XY to the image's level-zero pixels.
Supply its matching EPSG ID and coefficients in row-major order
`[a,b,tx,c,d,ty]`, where the equations are
`pixel_x = a*x + b*y + tx` and `pixel_y = c*x + d*y + ty`.

```json
{"coordinate_space":"projected-xy","source_crs":"EPSG:3857","source_to_pixels":[0.5,0,500,0,-0.25,500],"geometry":"geometry","id":"source_id","name":"title"}
```

The EPSG ID is compared to the selected column's declaration. Use coefficients
computed for that file and target image. The plugin validates the source
bounding box in stored coordinates before applying the affine and retains
the registration in the project receipt. The native host checks the mapped
geometry against the image. Omitted CRS means CRS84 under the GeoParquet
specification. Geographic coordinates, reprojection and Z/M require separate
profiles. The plugin admits
the seven basic XY WKB types with exact polygon holes and collections. Native
GeoArrow accepts Point, LineString, Polygon, MultiPoint, MultiLineString and
MultiPolygon with separated Float64 `x` and `y` fields.

The module shares the bounded native Parquet and WKB readers. It supports
uncompressed, Snappy and Zstandard pages and verifies checksums when present.
The module source and the pinned Apache Parquet decoder sources, codec
licenses and notices are included.

## Offline build

From the package root, use PowerShell 7, CMake and the Windows x64 MSVC1944
FormatPluginKit supplied with the source.

```powershell
pwsh -File source/tools/ci/build_parquet_decoder.ps1 -Archives source/third_party -BuildRoot decoder-build
cmake -S source/plugins/geoparquet -B plugin-build -A x64 -DQUPATHXX_FORMAT_PLUGIN_KIT_PREFIX=source/format-plugin-kit -DCMAKE_PREFIX_PATH=decoder-build/decoder-sdk
cmake --build plugin-build --config Release --target qpx_geoparquet_plugin
```

QuPathxx source is AGPL-3.0-only. The statically linked decoder uses the
component licenses in `source/plugins/parquet_matrix/decoder-sources.json`.
GeoPandas and Shapely are development fixture producers.
[GeoParquet 1.1.0 specification](https://geoparquet.org/releases/v1.1.0/)
