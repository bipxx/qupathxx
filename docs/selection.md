# Multi-selection and batch actions

`ObjectSelection` is native presentation state bound to a document. It holds a
sorted set of object IDs and one primary ID. Python keeps the document alive for
the selection's lifetime; native callers must ensure the document outlives it.
Selection changes do not change document revisions, create history or restore
selection through undo. Each GUI session owns its own selection instance.

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
a = doc.add_rectangle(q.Rect(10, 10, 20, 20))
b = doc.add_rectangle(q.Rect(50, 10, 20, 20))
selection = q.ObjectSelection(doc)
selection.replace([a, b], primary=a)
doc.classify_objects(selection.ids, "Tumor")  # One journal step.
doc.remove_objects(selection.ids)            # One journal step.
assert selection.size == 0                   # Prunes deleted IDs.
doc.undo()                                  # Restores objects, not selection.
```

`replace` validates all IDs and the optional primary before publishing. With no
explicit primary it uses the last input ID; duplicate IDs do not duplicate
membership. `toggle` makes an added ID primary. Removing the primary falls back
to the largest remaining ID, or zero for an empty selection. `ids` returns a copy.
`select_all` preserves the current primary when possible. `select_range` follows
the document's ascending ID order, matching the unfiltered object list.

`select_region` uses spatial candidates and selects objects whose combined
body/nucleus envelope is fully enclosed by the window. This is a window selection,
not polygon intersection or centroid selection. Zero-size windows can select
point objects. Range/window methods replace selection by default and support an
additive option. Invalid inputs leave live selection unchanged.

A document membership revision changes only when object IDs are added or removed,
including undo/redo. Selection access prunes missing IDs once after such a change.
Color, calibration, geometry, classification and name changes do not trigger a
selection-wide pruning pass. Deleted IDs observed and pruned from selection are
not re-added when undo restores their objects. IDs restored before selection is
observed retain their original identity. Membership checks use a balanced set;
cached size/primary reads are constant time.

## GUI gestures

- Click selects the topmost picked object; click empty canvas clears selection.
- Ctrl-click toggles an object; Ctrl-click empty canvas retains selection.
- Shift-click in the object list selects a range from the last click anchor.
  Ctrl+Shift-click adds the range to existing selection.
- Shift-drag in the canvas selects fully enclosed objects; Ctrl+Shift-drag adds.
  A Shift-click without a drag toggles the picked object.
- Ctrl+A selects every stored object, including offscreen objects. Escape clears
  selection when no drawing/window gesture is active; otherwise it cancels that
  gesture. Switching tools cancels a selection window.

All selected primary-shape outlines use amber; the primary object drives the
inspector and individual measurements. The inspector explicitly labels primary
editing when more than one object is selected. Batch class and Classify selected
apply one classification command to all selected IDs. Delete selected and Delete
remove all selected subtrees in one command. Clear selection changes presentation
only. Labels remain limited for detections to avoid rendering every selected name.

`Document.remove_objects` validates all requested IDs before editing. Duplicate
IDs and selected ancestor/descendant pairs are safe in either creation order.
An empty batch is a no-op. Undo restores hierarchy, kinds, nuclei, class counts
and IDs. A rejected batch preserves document, selection, history and allocation
state. Single-object GUI delete retains the existing Delete subtree history label;
multiple-object deletion uses Delete objects.

Replay modifiers enter ImGui's event queue, with canvas, overview and object-list
targets. List rows activate on mouse release, so replay keeps Ctrl/Shift set
through that release. `GuiReport.selected_ids` and `primary_selection` report
final state without changing it. These checks exercise ImGui input; OS input and
monitor DPI transitions remain separate validation gates.

## Evidence and remaining scope

The installed GUI wheel passes 119 Python tests, including native selection
lifetimes, primary/range/toggle rules, invalid requests, cell envelope windows,
membership pruning and batch delete restoration. GUI replay verifies list ranges,
canvas toggles/windows, selection outlines, Ctrl+A/Escape and group delete/undo.
Ten native suites pass with optional GEOS/OpenSlide enabled and disabled.

[Selection timings](benchmarks/selection-100000-windows.json) cover 100000 stored
objects, cached membership reads after a color edit, a window selecting 100 roots,
batch classification/deletion and undo. The window includes the initial index
build. [Selected-view timings](benchmarks/selected-view-100000-windows.json) select
all 100000 objects through actual Ctrl+A replay while rendering a narrow viewport.
On the recorded host, selecting all takes about 24 ms; cached selection access
(size, primary and one membership check together) measures 0.0004 ms at p95.
The 2560 x 1440 view with 828 visible overlays measures 16.9 ms p95 CPU frame
work and 3 MiB GPU tile payload. Only visible overlays are drawn; this does not
establish performance when fitting
all overlays, complex polygon stress, or large analysis jobs.

Vertex editing, hole drawing and Boolean clipping are implemented; see
[geometry-editing.md](geometry-editing.md). Independent QuPath numerical
comparisons and feature-cache contracts remain M3 work. Group geometry transforms are not implemented. All
unfinished M0-M7 persistence, analysis, scripting and delivery gates remain active.
