# Vertex, hole and Boolean editing

Geometry edits rebuild immutable native ROIs, validate them, then publish through
the document command journal. There is no automatic topology repair. Original
snapshots survive an edit; invalid self-intersections, invalid holes, inadequate
vertex counts, out-of-image coordinates or hierarchy violations leave canonical
state and history unchanged. The existing history payload budget applies.

## Native and Python contracts

`with_vertex(path, vertex, point)`, `inserted_vertex` and `without_vertex` return
new shapes. Path and vertex indices are zero-based. Polygon paths are flattened
in part order: each part's shell followed by its holes. Rings omit the repeated
closing point. Insert accepts positions through the ring length (append); other
indices must already exist. Polygon rings require at least three vertices,
polylines two points and point collections one. Geometry factories enforce the
remaining validity rules. Multipart grouping and holes are preserved.

Rectangles and ellipses require explicit `as_polygon()` conversion for vertex
editing. Rectangle conversion is exact; ellipse conversion uses 512 edges.
`with_hole(part, ring)` and `without_hole(part, hole)` support polygon parts and
also convert an analytic area when required. Holes must form a valid polygon
with the existing shell and holes; nesting or overlap is rejected.

The document methods `edit_vertex`, `insert_vertex`, `remove_vertex`, `add_hole`
and `remove_hole` use these same native operations and journal one operation.
They preserve object ID, name, class, kind, nucleus and parent. Parent geometry
changes revalidate existing children, including same-bounds hole edits. Use
immutable nucleus edits with `update_nucleus` to change a cell nucleus separately.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
obj = doc.add_roi(q.Roi.rectangle(q.Rect(10, 10, 100, 100)).as_polygon())
doc.edit_vertex(obj, 0, 0, q.Point(5, 10))
doc.add_hole(obj, 0, [q.Point(30, 30), q.Point(50, 30), q.Point(40, 50)])
doc.undo()  # Removes only the hole; the earlier vertex edit remains.
```

## Area Boolean operations

`Roi.boolean_operation(other, BooleanOp)` supports UNION, INTERSECTION,
DIFFERENCE and SYMMETRIC_DIFFERENCE through the
[GEOS reentrant C API](https://libgeos.org/doxygen/geos__c_8h.html).
Inputs must be area ROIs. Each operation owns its context and parses immutable
input WKT there, avoiding shared-backend locks and cross-context ownership.
Output shells/holes are extracted with checked coordinate counts and rebuilt as
validated polygon or multipolygon ROIs under the one-million-vertex limit.
Borrowed GEOS rings remain owned by their result geometry. GEOS allocations use
context-aware RAII cleanup. Immutable Python Boolean calls release the GIL.

The result is area-only. Disconnected polygon components and holes are preserved;
line/point components from touching boundaries are excluded. A result with no
area returns `None`. Analytic ellipses are polygonized at 512 edges for overlay,
so Boolean output is a polygon approximation, not an exact curved ROI. Existing
ellipse measurements remain analytic. Independent fixtures bound the ellipse
conversion's relative area error below 3e-5; this is not a QuPath parity claim.

`Document.apply_boolean(target, operands, operation)` folds operands in the
provided order, replacing only the target's primary geometry in one journal
step. Inputs remain in the document. Duplicate IDs, the target among operands,
missing IDs and empty operand lists are rejected. Empty intermediate XOR results
can become nonempty with later operands; only an empty final result is rejected
by the document. Remove an object explicitly when deletion is intended. Failure
preserves IDs, metadata, descendants, class counts, revisions and redo state.

```python
other = doc.add_rectangle(q.Rect(80, 10, 100, 100))
doc.apply_boolean(obj, [other], q.BooleanOp.UNION)
assert doc.object_count == 2  # Operand retained; target ID unchanged.
```

## GUI workflow

Select an annotation, then choose Vertices or press V. Drag a handle to move it;
press I near an edge to insert a vertex, or Delete to remove the active vertex.
Point collections insert at the pointer. Escape cancels a drag. Rectangle/ellipse
annotations expose an explicit Convert to polygon button. Detection/cell geometry
remains read-only in this tool; programmatic native edits remain available.

Vertex coordinates are cached by object revision. A drag draws its draft without
GEOS validation on each motion. Release validates and commits once. An invalid
draft is rejected and the original shape remains. Object changes during a drag
cancel the stale edit. Only the primary object's handles are edited; selection
and other objects are retained. Off-canvas handles are omitted from drawing.

Choose Hole or press H after selecting an area annotation. Click the interior
ring, then Enter/double-click to finish; Escape cancels. The tool locates one
containing multipart component and rejects a ring crossing its exterior or
existing holes. Changing the primary selection rejects a stale hole draft. A hole becomes part
of the original object's ROI, not a new
annotation. API calls can remove a hole, and its individual vertices can be
edited with the vertex tool.

For multiple selected objects, choose the Boolean operation and Apply Boolean,
or press B. The primary selection is the target; other selected IDs are operands
in ascending order. The UI labels the target/operand behavior and ellipse
approximation. Inputs are retained and the operation is undoable.

Replay supports image-coordinate pointer targets in addition to normalized
canvas/list/overview targets, enabling checks against exact editable vertices.

## Evidence and remaining work

The installed GUI wheel passes 119 Python tests. New checks compare Boolean areas
and pixel-center masks with independently computed rectangles, verify holes and
multipart splits, reject invalid topology and parent containment, preserve cell
state, test empty XOR folds and enforce journal budgets. GUI replay exercises
drag/cancel, insertion/deletion, hole creation, Boolean application and history.
Ten native suites pass with GEOS/OpenSlide enabled and disabled.

The [geometry benchmark](benchmarks/geometry-4096-windows.json) measures Boolean
union and validated vertex edits on overlapping 4096-vertex radial polygons,
plus a 2560 x 1440 view with vertex handles. On the recorded host, p95 union
latency is about 11.9 ms, validated edits 8.2 ms and CPU frame work 17.2 ms.
Input fixture construction is excluded; validated Boolean output construction is
included. The workload
does not establish vendor, million-vertex, simultaneous-many-polygon or QuPath
workflow performance. Numerical upstream comparisons, feature cache contracts,
broader geometry stress and the remaining M0-M7 gates remain open. Persistence
and interchange can now build on these stable shape/object command contracts.
