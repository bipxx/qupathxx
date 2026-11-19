# GeoPackage annotations and affine registration

Use the native `org.qupathxx.geopackage` provider to import a selected
GeoPackage feature layer. Select `image-pixels` for undefined Cartesian SRS −1
coordinates already expressed in level-zero image pixels. Version 1.1.0 adds
`projected-xy` for an explicitly registered EPSG layer.
[GeoPackage specification](https://www.geopackage.org/spec/)

```text
qupathxx_format_import --format-plugins ABS_FORMAT_MANIFEST annotations-import org.qupathxx.geopackage INPUT_PROJECT IMAGE_ID INPUT.gpkg NEW_PROJECT --plugin-options-json '{"layer":"rois","name_column":"name","coordinate_space":"image-pixels"}'
```

Supply the image reader manifest if the saved image uses a reader plugin.
The optional `name_column` accepts text or SQL null. Native stable IDs derive
from the exact integer feature IDs. Collection paths identify children
where a native ROI cannot represent the entire collection as one object.
The selected layer, original feature ID and SRS ID remain in source facts.

For projected input pass a canonical `source_crs` such as `EPSG:3857` and
`source_to_pixels` with six row-major coefficients `[a,b,tx,c,d,ty]`. For
stored source XY, the map is `x_pixel=a*x+b*y+tx`, `y_pixel=c*x+d*y+ty`.
The importer checks the layer's SRS references, EPSG authority row, projected
WKT declaration and geometry header before applying the supplied affine.
The local SRS key may differ from the EPSG code. Both the declared source CRS
and exact coefficients remain in the saved import receipt.

The profile accepts GeoPackage 1.2–1.4 StandardGeoPackageBinary geometry
with basic XY WKB, independently declared byte order and an optional exact
XY envelope. Empty, extended, geographic, Z/M and out-of-frame
geometries are rejected. Other layers and attribute columns are outside the
selection. The native importer opens a bounded read-only snapshot of a
closed rollback-mode file. Close writers and checkpoint any WAL before
import.

The AGPL-3.0-only plugin uses a private static copy of the admitted SQLite
3.54.1 amalgamation. Build in Release with the matching
`QUPATHXX_FORMAT_PLUGIN_KIT_PREFIX` and
`QUPATHXX_GPKG_SQLITE_ARCHIVE`. The package includes SQLite source and notice.
GDAL via pyogrio and Shapely are development-only fixture tools.
[SQLite public-domain terms](https://www.sqlite.org/copyright.html)
