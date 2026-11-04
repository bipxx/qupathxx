# Annotation clipboard

`copy_objects` and `paste_objects` share the native GeoJSON authority used by
file interchange. Python and Dear ImGui use the same validation and atomic
object append; no Python object model or separate clipboard parser is involved.

```python
import qupathxx

snapshot = qupathxx.copy_objects(source, selected_ids, include_descendants=False)
result = qupathxx.paste_objects(target, snapshot.text)
new_ids = result.imported_ids
# Explicitly move the copied objects to the target's displayed plane:
result = qupathxx.paste_objects(target, snapshot.text, plane=target.active_plane)
```

Copy requires a nonempty selection. Repeated IDs are deduplicated; descendants
are included only when requested. Selected parent links survive, while an
unselected parent is detached with a `parent_not_exported` notice. The copied
text is independent of subsequent document changes or closure. Paste assigns
fresh IDs in the target document and remaps parent links in one **Paste objects**
undo/redo transaction. Undo prunes the removed IDs from selection; redo restores
objects without automatically reselecting them, matching ObjectSelection policy.
IDs are document-local, not globally unique identifiers.

Coordinates stay in level-zero image pixels with a top-left origin. Different
source and target image dimensions are allowed when the actual geometry fits
the target. Paste never rescales using physical calibration or changes target
calibration. Ordinary paste preserves object planes; the explicit plane option
replaces them on every pasted object. Invalid target geometry, hierarchy,
class-color conflicts or unavailable planes refuse before document mutation.
File `import_geojson` keeps its existing image-dimension matching policy.

The existing format retains supported geometry, cell nuclei, names, classes,
metadata, measurements, locks, object color and native reported probability.
Unknown fields and omitted TMA metadata/cell-run links retain the existing
loss-aware refusal. Use `allow_losses=True` only after inspecting the reported
omissions. Ellipse export also requires that permission because foreign readers
may use its polygon approximation; native analytic hints retain the exact ROI.
This transport does not copy image data, retained analysis runs or TMA grids.

In ImGui, **Ctrl+C** copies the selection, **Ctrl+V** pastes with its original
planes, and **Ctrl+Shift+V** pastes to the displayed plane. These shortcuts do not
run while entering text or while edits are blocked. Equivalent controls,
**Copy descendants**, loss permission and operation notices are in **Import /
export**. Paste selects the new objects visible on the current view; objects on
other planes remain stored and a notice explains how to find them. ROI locks
are retained and do not prevent copying.

Clipboard text defaults to 16 MiB with at most 100,000 requested IDs/objects.
Python/C++ may set `max_bytes` from 1 byte through 256 MiB; GUI uses 16 MiB.
Nested export supports at most 32 object levels, and existing JSON depth/vertex
limits apply. These admission limits are not a whole-process peak-memory claim.
Malformed JSON, duplicate keys/IDs, NUL bytes and unsupported CRS refuse; text
is never evaluated as a script. Copy validates before replacing system clipboard
text. Only explicitly copied selections are read from the native document.
