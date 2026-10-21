# Class registry and shared colors

Each document owns a registry of `ObjectClass` definitions keyed by exact,
case-sensitive names. A definition stores its name and an RGB color packed as
`0xRRGGBB`. Names must be nonempty, at most 4096 bytes and contain no NUL.
The empty object class name means unclassified and has no registry entry.
Names are the class identity; hierarchical/derived classes are not implemented.

Existing string-based classification APIs remain available. Assigning a new
nonempty class creates its definition with the default mint color `0x41e1be`
in the same transaction. Undo removes that newly created definition together
with the assignment. Deleting or unclassifying objects retains an unused
definition so its configured color is not lost. `remove_class` explicitly removes
an unused definition; removing an assigned class fails without changing state.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
doc.set_class("Tumor", 0xe02030)
a = doc.add_rectangle(q.Rect(10, 10, 20, 20))
b = doc.add_detection(q.Roi.ellipse(q.Rect(50, 50, 10, 10)))
doc.classify_objects([a, b], "Tumor")  # One journal step; duplicate IDs are safe.
doc.rename_class("Tumor", "Positive")  # Definition and all assignments together.
doc.undo()
print(doc.object_class("Tumor").color, doc.class_count("Tumor"))
print([(c.name, c.color) for c in doc.classes])  # Copied, sorted snapshots.
```

`set_class` creates or changes a color. An identical request is a no-op and
preserves redo. `rename_class` rejects a missing source, empty/invalid destination
or existing destination; it never merges classes implicitly. It preserves kind,
geometry, nucleus, parent and configured color. Batch classification rejects a
missing object atomically and preserves IDs/revisions/history on failure. An
empty ID list does not create a class. Classes apply to all object kinds.

Registry changes use the same bounded journal as geometry and calibration.
Class patches, object patches, assignment counts and map nodes are prepared
before commit. History payload includes class keys/values and edited object
snapshots, excluding allocator capacity and tree overhead. A rename exceeding
the configured payload budget fails before changing anything. Undo and redo
restore definitions and assignments together; disabled recording still updates
counts and definitions normally.

Counts update only for touched class assignments; `class_count` is a logarithmic
lookup and returns zero for unknown/unclassified names. Color edits touch no
objects and do not advance object revisions or invalidate the bounds index.
The document revision advances so presentation refreshes. Rename scans the
object map and journals matching assignments. It is proportional to the document
scan plus affected objects, rather than a constant-time identity change. Spatial
queries reuse their tree across color, rename and classification changes.

The inspector's Classes section creates classes, changes colors, renames and
removes unused definitions. A selected object's Class picker uses the registry;
Apply publishes the assignment with the other inspector changes. Unselected
object outlines and annotation labels use the shared RGB color, selected objects
remain amber and nucleus outlines remain purple. Detection labels remain hidden
unless selected. The renderer uses const registry lookup without creating a
class snapshot per overlay.

Validation covers automatic registration, failure rollback, no-op/redo behavior,
rename collisions, assignment counts, duplicate batch IDs, repeated replacement
descriptors, cell/nucleus retention, history budgets and subtree restoration.
Framebuffer tests verify two colors and undo through actual draw output.
The current installed GUI wheel passes 91 Python tests; eight native suites run
with and without the optional GEOS/OpenSlide dependencies.

The [100,000-object class benchmark](benchmarks/classes-100000-windows.json)
records about 0.026 ms p95 shared color edits with 357 bytes of journal payload.
Renaming all 100000 assignments takes about 290 ms, undo 188 ms and redo 183 ms
on the development host. These native API timings include nanobind overhead and exclude
rendering and analysis. Color edits have no dependence on the number of assigned
objects; renames deliberately update each affected object. They are separate
workloads and do not establish an interactive-frame guarantee for large renames.

Multi-selection, vertex editing and Boolean clipping remain M3 work. Classifier
models, hierarchical/derived class semantics, persistence and the remaining
M0-M7 gates remain open.
