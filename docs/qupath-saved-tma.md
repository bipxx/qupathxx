# Native saved-data TMA grids

Pinned QuPath 0.7 version 3 saved-data import retains recognized nonempty rectangular
`DefaultTMAGrid` layouts and analytic-ellipse `TMACoreObject` objects. Native
project import uses the existing TMA model, commands, inspector, metadata exports
and store. No new public API or project schema is needed.

```python
import qupathxx as q

preview = q.preview_qupath_project("project.qpproj")
project = preview.import_new("native.qpxproj")
image_id = project.images[0].id
doc = project.document(image_id)
grid = doc.tma_grid()
if grid is not None:
    print(grid.info.rows, grid.info.columns)
    print(q.tma_metadata_csv(grid))
    print(q.tma_case_summary_csv(grid))
```

Each grid-list entry defines a row/column position independently of hierarchy
traversal order or core name. Duplicate names are valid; duplicate node references
or source identities refuse. Foreign UUIDs map to the same newly assigned native
object IDs used by the imported hierarchy. Children, detections, cells/nuclei,
names, classes, planes, historical facts and supported edit state remain on those
objects. Core ellipse bounds retain their native analytic parameters; this route
does not polygonize them through QPTMA text exchange.

The decoder validates positive dimensions, at most 10,000 cores, exact rectangular
count, recognized core types and matching hierarchy/grid membership. Each
hierarchy core must be listed, and every listed core must occur in the hierarchy.
Non-ellipse, empty/ragged, duplicate, unlisted/orphaned or absent-member grids and
unknown representations refuse before publication. These refusals cannot be
bypassed by allowing losses, and do not produce annotation-only/image-only imports.

Missing flags and case IDs use existing `TmaCoreInfo`. The legacy `Unique ID`
metadata fallback matches pinned read behavior when `Case ID` is absent. Native
case IDs remain bounded to 256 UTF-8 bytes; values are never silently truncated.
Historical imported metadata stays historical when the live TMA case assignment
is edited. Grid names/axis labels are not stored by this QuPath class; native
defaults are `TMA`, alphabetical rows and numbered columns. Core names are kept
verbatim, without guessing axis labels from names.

Geometry and identities are imported into the private preview first. One staged
metadata command attaches the validated grid to those objects. Failures leave
no published destination. Preview source guards still bind the original saved
data, source and manifest before import publication. Import-construction history
is cleared before returning the imported project.

After import, existing missing/case/translation edits and undo/redo apply.
Translation can move descendants; unrelated root objects stay independent.
Immutable grid snapshots and asynchronous project saves retain captured state.
Existing TMA persistence tables preserve IDs, slots and metadata across reopen.
Multiple images keep separate object namespaces, and existing cohort summaries
pool explicit matching case IDs across those image namespaces.

The desktop TMA panel recognizes the imported grid immediately. Existing metadata
CSV export, case inventory and save/reopen workflows consume that native state.
See [local evidence](benchmarks/saved-tma-r5/README.md). Broader binary versions,
grid representations, assisted-state exchange, biological dearraying acceptance
and resource/platform/release claims remain in the delivery queue. Existing
[QPTMA text exchange](qupath-tma-interchange.md) is a separate format contract.
