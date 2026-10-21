# Persistent image views and schema migration

Schema 2 adds a native `ProjectView` for each image that has a stored view.
It contains a full-resolution pixel center, zoom (screen units per image pixel)
and RGB display mode/window/gamma. Recording the image center instead of the
window origin preserves the viewed location across different window sizes.
Views do not alter pixel data, calibration, geometry or quantitative measurements.

```python
import qupathxx as q

project = q.Project.open("study.qpxproj")
image = project.active_image
view = q.ProjectView()
view.center, view.zoom = q.Point(500, 300), 2
project.set_view(image, view)
project.save(project.path)
```

`Project.view(id)` returns a snapshot or `None`. Mutating a returned view does
not change the project. Use `set_view` to publish it, or `clear_view` to remove
it. Identical assignments and clearing an absent view do not dirty the project.
Changed views dirty project metadata without changing the document revision or
undo history. Removing an image removes its view in the same native operation.
View and display values must be finite and drawable within the float screen
coordinate range for the associated image. Invalid state rejects before mutation.

## Viewer behavior

The GUI restores stored views on startup and image switches. An explicit
`GuiOptions.start_view` overrides the active image's saved camera and uses
`GuiOptions.display`; otherwise a saved display takes precedence over the default.
Images without a view fit on first display. The current image's view is published
to the native project when camera/display/canvas inputs change. Unchanged frames
perform a small tuple comparison, with no selection copying, filesystem checks
or serialization. Restoring a view establishes an unchanged baseline so floating
point center/origin conversion does not itself mark a saved project dirty.

Ctrl+S executes after the frame's camera/display edits are published. It saves
the latest navigation and all inactive images' recorded views as well as the
canonical document state. Views remain available in memory when `show` returns;
they reach disk only through a successful project save. Selection, editing
history, unfinished tools and docking/layout state are not persisted by this
schema. [Autosave/recovery](project-recovery.md) stores this canonical state and
the native GUI supports dirty-close decisions. [In-window project opening](project-opening.md)
loads ordinary projects on a worker. [Empty desktop workspaces and image removal](project-lifecycle.md)
are implemented. [Reviewed checkpoint deletion and background recovery loading](recovery-maintenance.md)
are implemented, as is [moved-primary association](recovery-association.md).
Automatic retention and abandoned work cleanup remain M4 work.

## Schema 1 to 2

`Project.open` accepts schemas 1, 2 and 3. Schema 1 opens read-only without creating
views or changing its bytes. Missing view state means normal fit/default display.
New saves use schema 3, which also retains [analysis provenance](analysis-provenance.md).
Saving back to schema 1 checks project identity and save generation, then creates
the `views` and `analysis_records` tables and advances `user_version` in the
same SQLite transaction as the canonical rows. Failure rolls back both schema
and data. Schema 2 adds only the missing analysis table in that same transaction.
Save-as writes schema 3 to the new destination and leaves the source file
unchanged. Older schema-1/2-only applications reject schema 3.
An interrupted writer's hot journal is an exception to read-only opening: SQLite
requires write access to roll back its transaction before the read-only loader
continues. This recovery performs no schema migration; see [project saving](project-save.md).

The native loader validates every view, checks its image reference, rejects
duplicate/orphan records and invalid display modes, and rejects nonfinite or
out-of-range values. Unknown schema versions continue to reject; no speculative
forward migration or downgrade is performed. The ROI BLOB encoding remains QRO1.

The [schema-1 fixture](../tests/fixtures/project-v1.qpxproj) was produced by the
actual writer at commit `39711ba27c82eb2940b0e7c05a592ab23adb6b3a`, before schema 2
was implemented. Its SHA256, producer and contents are recorded in the
[fixture manifest](../tests/fixtures/manifest.json). It contains two embedded
images, a rectangle parent, analytic ellipse cell/nucleus, class color and
anisotropic calibration. Tests inspect read-only opening, stable IDs/geometry,
successful migration, save-as preservation and rollback after injected SQL
failure. GUI replay additionally checks different window sizes, inactive views,
explicit overrides and a wheel event in the same frame as Ctrl+S.

## Measured scope

The [schema-2 100000-object run](benchmarks/project-v2-100000-windows.json) records
217 ms initial save, 358 ms full resave and 188 ms validated load on the development
Windows host. It includes one camera/display view and an embedded dense image;
input construction, cold-storage and autosave work are excluded. The
[browser run with live view publication](benchmarks/project-views-browser-1000-windows.json)
retains 100000 selected objects, draws 828 visible overlays and completes 20
switches in a 1000-image catalog. It records 1.08 ms maximum switch time, 17.3 ms
p95 CPU frame work and 3 MiB GPU tile payload. These remain dense-image workload
measurements; no vendor-latency or crash-recovery performance claim is made.
