# Detection and cell objects

Objects now have `ObjectKind.ANNOTATION`, `DETECTION` or `CELL`. Cells are also
detections (`is_detection` is true). For compatibility, the existing `Annotation`
snapshot type, `annotations` list and annotation mutation method names cover all
three kinds. IDs, class names, hierarchy, revisions and history are shared.

Cells store a primary area ROI and an optional immutable nucleus area ROI.
This follows the conceptual split in
[QuPath's cell model](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/objects/PathCellObject.java).
No upstream code was copied. The nucleus need not be contained by the cell
boundary; segmentation imports can retain independent contours. Both shapes
must lie inside the image. Parent containment applies to the primary shape.
Only cells may store nuclei. Cells and nuclei require area ROIs; annotations
and generic detections may also represent lines or point collections.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
cell = doc.add_cell(
    q.Roi.ellipse(q.Rect(100, 100, 30, 40)),
    nucleus=q.Roi.ellipse(q.Rect(108, 110, 10, 15)),
    class_name="Tumor",
)
doc.update_nucleus(cell, None)  # One undoable edit, preserving the cell boundary.
doc.undo()
doc.set_calibration(.25, .25)
print(doc.area_um2(cell), doc.nucleus_area_um2(cell))
print(doc.mean_intensity(cell), doc.nucleus_mean_intensity(cell))
```

`add_detection` and `add_cell` support parent ID, name and class. General
`ObjectEdit.create_geometry` / `replace_geometry` accept `kind` and `nucleus`.
These factories specify a complete record: omitted kind defaults to annotation
and omitted nucleus clears it. Conversions are explicit full replacements.
Convenience rename/classify/reparent/update-ROI methods preserve kind and nucleus.
Updating the body leaves the nucleus in its original image coordinates; use a
single replacement descriptor to move both as one operation.

`geometry` and legacy `roi` describe the primary shape; `bounds` encloses both
body and nucleus. Spatial queries use this combined envelope and picking refines
against either shape, excluding gaps between them. Nucleus-only changes update
index deltas even when the primary shape is unchanged. Metadata changes preserve
the index. History counts both geometry payloads and restores them together.
Failed validation preserves IDs, revisions, spatial state and redo history.

Primary area/mean methods retain their existing behavior. Separate nucleus
methods reject a missing nucleus, rather than substituting the body. Both area
methods require known calibration. Intensity samples each shape independently
at pixel centers. Cytoplasm subtraction and stain/cell features remain M5 work.

The GUI shows object kind and nucleus presence, draws nucleus contours in purple,
and reports calibrated nucleus area. Detection/cell geometry fields are read-only
in the inspector, while names/classes/parents can be edited without losing the
nucleus. Native/Python commands can replace detection geometry. Drawing tools
create annotations; automated detection and interactive conversion UI remain.
Unselected detection labels are omitted to limit visual clutter and text work.

Validation includes optional nuclei, independent boundary/nucleus masks, type
conversion, snapshot lifetime, compound failure, redo preservation, geometry
wrappers, subtree restoration and spatial delta/undo behavior. A framebuffer
test verifies a nucleus remains visible when its body is offscreen. The full
installed GUI wheel passes 119 Python tests; ten native suites pass both with
GEOS/OpenSlide enabled and with them disabled.

The [100,000-cell benchmark](benchmarks/cells-100000-windows.json) imports small
rectangular cell/nucleus pairs in about 268 ms, undoes in 150 ms and redoes in
164 ms on the recorded host. A rename records 1087 bytes of payload. This excludes
descriptor construction, rendering and allocator/cache overhead; it does not
establish complex polygon, million-cell or full analysis performance. Immutable
shape storage is shared across snapshots and metadata edits.

Class registries/colors are implemented; see [classes.md](classes.md).
Multi-selection is implemented; see [selection.md](selection.md).
Vertex editing and Boolean clipping are implemented; see geometry-editing.md. Persistence, analysis and the remaining M0-M7 gates remain open.
