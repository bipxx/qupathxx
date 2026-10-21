# Native document commands and edit history

The document now owns an indexed map of rectangular annotations, a child index,
monotonic IDs and revisions, and a journal of changed objects. Python and ImGui
submit the same native operations. Python annotation results are snapshots; C++
borrowed object-map references must not be retained across mutation.

## Atomic edits and hierarchy

Add, replace, rename, classify, reparent and subtree deletion use one transaction
path. An annotation has an ID, name, rectangle, optional parent ID (0 means root),
free-text class name and revision. Parent geometry must contain child geometry.
Cycles, missing parents, depth over 256, nonfinite/out-of-image rectangles and
embedded NUL or text longer than 4096 bytes are rejected.

ObjectEdit factories describe create, full-record replacement and subtree removal.
Document.execute stages descriptors in sequence and validates final geometry and
hierarchy before publishing. This permits a compound parent/child move even when
its intermediate rectangles would not contain each other. Descriptors after a
removal cannot access the removed object. Explicit reparenting before deleting a
parent can preserve a child. Deleting a parent otherwise deletes its descendants.

Rejected edits preserve objects, child links, calibration, revisions, ID
allocation, undo/redo records and labels. All allocating map nodes and journal
capacity are prepared before canonical state changes. Commit transfers map nodes
with integral keys. A no-op does not add history or advance the revision.

IDs are never reused after a successful creation, even after undo or clearing
history. Undo/redo restores IDs, geometry, classes and parent links, but assigns
fresh revisions to restored/replaced objects. Global document revisions never
decrease. Pending mean measurements are invalidated on document revision changes
so editing, deletion or undo cannot publish stale values.

## Undo and retention

Every successful transaction is one undo step. Calibration setting/clearing uses
the same journal. Initial reader calibration is document initialization, not an
edit. New mutations after undo discard the redo branch; failed or no-op requests
preserve it. Selection is presentation state and is not recorded in the journal.

Defaults retain 128 steps and at most 64 MiB of recorded payload. A record counts
fixed record/optional-object values and name/class/label bytes; tree nodes, spare
vector capacity and allocator overhead are not included. Limits apply jointly
to undo and redo records. Old records are evicted when needed. An individual edit
larger than the payload budget fails before commit; callers may explicitly raise
the budget or disable recording. Either zero limit disables recording and clears
history. clear_history leaves IDs, objects and revisions unchanged.

The journal stores only touched objects, not a copy of the document on every
edit. Object lookup is logarithmic; ordinary edits do not traverse unrelated root
objects. Reparenting may copy affected sibling sets and validate descendant depth.
Parent geometry changes must validate affected children. Subtree deletion is
proportional to the affected subtree. This is not a spatial index.

## Python example

```python
import qupathxx as qpx

doc = qpx.Document(qpx.Image.demo())
parent = doc.add_rectangle(qpx.Rect(0, 0, 300, 300), "Tissue")
child = doc.execute([
    qpx.ObjectEdit.create(qpx.Rect(10, 10, 20, 20), "Region", parent, "Tumor")
], "Add classified child")[0]
doc.execute([
    qpx.ObjectEdit.replace(parent, qpx.Rect(100, 100, 300, 300), "Tissue"),
    qpx.ObjectEdit.replace(child, qpx.Rect(110, 110, 20, 20), "Region", parent, "Tumor"),
], "Move tissue and child")
doc.undo()  # Restores both objects in one step.
doc.redo()
print(doc.annotation(child).parent_id, doc.children(parent), doc.revision)
```

## GUI and validation

The inspector edits name, class, rectangle and parent ID as one Apply operation.
Calibration has its own explicit Apply/Clear actions. Toolbar Undo/Redo and
Ctrl+Z/Ctrl+Y use the native journal. Delete removes the selected subtree. R
toggles rectangle drawing. Invalid drafts show an error and remain editable.
Selection clears when undo/delete removes the selected ID. The object list is
virtualized and caches its IDs by revision; overlays cull off-screen rectangles
but still scan the map. The 100,000-object rendering gate remains open.

The tests cover atomic failure, no-ops, branching redo, stable IDs, monotonic
revisions, subtree restore, containment/cycles/depth, child-index consistency,
calibration history, payload rejection/eviction, disabled recording and ImGui
keyboard delete/undo/redo. Native and Python suites exercise the same API.

The [100,000-object benchmark](benchmarks/document-100000-windows.json) reports
approximately 203 ms batch creation, 117 ms undo and 120 ms redo on the development
host. A rename adds 447 bytes of recorded payload and measured 0.0016 ms at p95.
The timed API includes nanobind call/return overhead and excludes descriptor
construction, GUI rendering and allocator overhead accounting. These are host
observations, not cross-machine guarantees or full M3 acceptance.

## Remaining M3 work

Probe and integrate robust polygon topology before adding polygons, holes,
ellipses, polylines, points and multipart ROIs. Add detection/cell object kinds,
class registries/colors, multi-selection and spatial indexing. Verify general
geometry against independent fixtures and extend drawing/edit/undo tests to
those shapes. Project persistence and interchange remain M4 work.
