# Annotation interchange

Native `export_geojson`, `import_geojson`, `save_geojson`, `load_geojson`,
`measurements_csv` and `save_measurements_csv` are shared by Python and ImGui.
Open **Import / export**, enter a path, then export/import (F6/F7) or write CSV.
Selection export rejects an empty selection. Import selects the created objects.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
doc.add_rectangle(q.Rect(10, 20, 100, 50), "Region")
q.save_geojson(doc, "annotations.geojson")
target = q.Document(q.Image.demo())
report = q.load_geojson(target, "annotations.geojson")
q.save_measurements_csv(target, "measurements.csv")
```

Coordinates are full-resolution image pixels, origin top left. Native collections
tag this convention and image dimensions; dimensions must match on import.
Untagged files require `assume_pixels=True`. Geographic/projected CRS and
non-default Z/T/channel planes reject. These are image-coordinate GeoJSON
workflows, not RFC 7946 geographic coordinates.

| Data | Supported contract |
| --- | --- |
| Geometry | 2D points, multipoints, lines, polygons with holes and multipolygons; polygon topology requires GEOS |
| Analytic shapes | Native foreign hints preserve exact rectangles/ellipses; hints must agree with coordinates |
| Ellipse export | Generic readers receive 512 polygon vertices; requires `allow_losses=True`, reports `ellipse_polygonized` |
| Objects | Annotation/detection/cell, independent nucleus, names, flat classes and RGB colors |
| Identity | Source IDs reported alongside new monotonically allocated native IDs; existing objects remain |
| Hierarchy | Native parent IDs and nested `childObjects`/`children`; missing parents reject; subset export needs loss permission to omit parents |
| History | One atomic undo step including class definitions; invalid batch leaves objects, IDs and history unchanged |
| CSV | Stable ID order, quoted UTF-8 text, CRLF, explicit pixel/physical units; unknown calibration and absent compartments blank |
| Intensity | Opt-in `include_intensity=True`; scalar mean of RGB bytes, not stain/channel-specific measurements |

Exports sort/deduplicate requested IDs. Empty ID lists mean all objects; full
collections retain unused class definitions. Color conflicts with an existing
class reject. Unsupported properties (including measurements, metadata, locks
and object colors) reject by default; `allow_losses=True` drops them with counted
issues. Geometry collections, multiline strings, derived classes and RGBA colors
are unsupported. Do not use interchange as a replacement for native project saves.

Input/output is capped at 256 MiB, JSON nesting at 128, imported objects and
vertices per geometry at one million. Duplicate JSON keys, invalid numbers,
unclosed rings and conflicting IDs reject. Import parses/stages before publishing.
File exports generate and validate text first, write/flush a unique sibling,
then replace the destination; failed conversion leaves the destination intact.
This does not establish power-loss durability on every filesystem.

## QuPath bridge and evidence

The schema was checked against [QuPath v0.7.0 adapters](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/io/QuPathTypeAdapters.java)
and [ROI adapters](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/io/ROITypeAdapters.java).
[QuPath's export documentation](https://qupath.readthedocs.io/en/stable/docs/advanced/exporting_annotations.html)
defines image-pixel coordinates. A starting Groovy export recipe is:

```groovy
exportObjectsToGeoJson(getAnnotationObjects(), '/path/annotations.geojson',
    'FEATURE_COLLECTION', 'EXCLUDE_MEASUREMENTS')
```

Load this untagged output with `assume_pixels=True`; inspect issue reports.
Ordinary flat QuPath export does not retain hierarchy. Exported native numeric
IDs are not QuPath UUID identities; QuPath can regenerate them. Our own analytic
hints/parent extensions are not interpreted by QuPath. QuPath ellipse envelopes
are reconstructed when `isEllipse` is present, with an `ellipse_hint` notice.
The committed protocol fixture is hand authored from the public schema; it is
not an upstream-generated fixture. A separate pinned runtime corpus now checks
flat shape/class/cell/nucleus round trips and measurements; see
[QuPath comparisons](qupath-comparisons.md). UUID remapping and upstream
quantization are recorded. The explicit QuPath ellipse precision option resolves the captured thin-ellipse
decoder case with reported coordinate loss; hierarchy and broader workflows
remain open. Default import preserves raw analytic envelopes.

`tests/test_interchange.py`, the native interchange suite and GUI F6/F7 replay
cover the stated contracts. `tools/benchmark_interchange.py` measures a bounded
100000-rectangle workload; complex shapes and intensity export need separate
performance evidence. GeoJSON uses pinned nlohmann/json 3.12.0 (MIT), enabled by
default with `QUPATHXX_BUILD_INTERCHANGE`; CSV remains available when disabled.

On the recorded Windows host, 100000 classified unit rectangles exported to
35 MB GeoJSON in about 804 ms, imported in 1655 ms, and exported to 7.3 MB CSV
in 329 ms. This excludes filesystem IO and intensity reads, with history disabled.
A streaming duplicate-key/depth validation pass avoids the library callback
parser's growing-array rescans (about 23 seconds before this change). See the
[recorded workload](benchmarks/interchange-100000-windows.json).


`import_geojson` and `load_geojson` accept `qupath_ellipse_precision=False`.
Opting in matches pinned QuPath v0.7.0's precision-reduced float envelope for
foreign `isEllipse` hints and requires GEOS. Any changed bounds require
`allow_losses=True` and produce `qupath_ellipse_precision_changed`. Collapsed
or unrepresentable envelopes reject before publication. Native analytic hints
remain exact. The GUI exposes the same choice as Match QuPath ellipse bounds,
with independent Assume image pixels and Allow lossy conversion controls.

The [actual hierarchy/loss migration corpus](annotation-migration.md) now tests
nested upstream import through native project restart and ImGui, and retains
raw duplicate-metadata rejection. Native-to-QuPath flat export loses parent links. The additive
`nested_hierarchy=True` export/file option and ImGui QuPath hierarchy checkbox
now preserve the tested parent links in actual QuPath imports. The broader
integration gate retains a Windows recovery-path failure; see the linked receipt.
