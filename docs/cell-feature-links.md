# Published cell features

Detected cell objects retain a typed `(record_id, label)` reference to immutable
[archived cell features](cell-archive.md). One run stores the masks, parameters,
transform, input digest and feature rows; each object adds only an optional pair
of integers. Features are not copied into every object or recalculated on edits.

```python
request = q.CellRequest(doc, source_id, transform, parameters)
result = q.detect_cells(request)
cell_ids = doc.publish_cells(request, result, class_name="Detected")
cell = cell_ids[0]
link = doc.annotation(cell).cell_feature_link
feature = doc.cell_object_feature(cell)
print(link.record_id, link.label, doc.cell_object_status(cell))
print(feature.nucleus_rgb, feature.cell_rgb, feature.cytoplasm_rgb)
```

`CellJob.keep_result` uses the same publication path. It retains the run
implicitly if needed and reuses an existing current archive of the exact same
native result. A job that successfully rechecks an archive publishes against
that verified archive directly. All created objects are one Undo/Redo step. Archives remain
independent of object history: Undo removes the objects while preserving the
run, and Redo restores their original feature references. A new implicit archive
increments the document revision separately before the object edit; both changes
are rolled back if publication fails, including quotas, history limits, invalid
names or revision exhaustion. Empty results publish no objects or implicit run.

`doc.publish_cell_record(record_id)` publishes a saved run after its status is
`current`. Reopened archives require an explicit pixel recheck before publication.
The GUI offers **Add saved cells** below a saved run. Each invocation deliberately
creates a new set of cell objects referring to that run; it does not replace an
earlier set. Live jobs keep their existing one-publication guard.

## Edits and status

`Annotation.cell_feature_link` is read-only in Python. Manual cells have no link.
Renaming, classification, reparenting and replacement of an existing cell preserve
its reference. Changing an object to a different kind clears the reference.
Undo/Redo carries the reference with the object. Copying/importing objects through
`append_objects` clears document-local references, preventing accidental binding
to an unrelated run with the same numeric ID in another document.

`cell_object_feature(id)` returns a copy of the historical feature row. It does
not imply that the current object or image still matches the run. Inspect
`cell_object_status(id)` alongside it:

| Status | Meaning |
| --- | --- |
| `no_features` | Manually created object or no run reference |
| `archive_missing` | The referenced run was explicitly removed; lookup fails |
| `invalid_label` | Reference cannot resolve to a row; invalid persisted links are rejected on load |
| `geometry_unavailable` | This build or the archived contour limits cannot reconstruct the saved contours |
| `geometry_changed` | Current body or nucleus differs from captured contours |
| `current`, `unverified`, `object_changed`, `source_changed`, `source_closed`, `verification_failed` | Geometry agrees; the status comes from the run's source authority and pixel recheck |

Geometry comparison uses exact native contour representation. Name/class/parent
changes on the published cell do not invalidate its captured numeric values.
Changes to the source annotation still change the archived run status under the
existing revision contract. Geometry edits never rewrite historical RGB/channel
means or sample counts. The ImGui measurement inspector shows the run/label,
status, captured sample counts and RGB means, explicitly identified as historical.

Removing a run is permitted and leaves an explicit `archive_missing` reference.
This survives project/recovery storage and Undo/Redo; it prevents accidental reuse
of an ID from presenting different features. No feature values are silently
reconstructed from remaining geometry. Keep the archive when measurements are
needed. Measurement lookup performs no image I/O and holds no new reader owner.

## Persistence and interchange

Project schema 6 adds the strict `cell_feature_links` table, keyed by image and
object ID. It stores unsigned run IDs as canonical decimal text and labels as
integers. Save/load and recovery preserve links in immutable object blocks, so
background snapshots keep the captured mapping after live edits/removal.
The native loader rebuilds derived contours from validated historical labels,
sharing the detector's bounded normalization. It does not read today's pixels.
Runs that cannot fit their archived contour limits retain readable numeric data
with `geometry_unavailable`; cancellation still aborts loading.
Schemas 1-5 remain readable and upgrade transactionally on save. Failure rolls
back both the new table and canonical data. Validation rejects orphan rows,
non-cell references, zero/future IDs and invalid labels for present runs. Removed
archives are a supported explicit state and do not require a dangling SQL foreign
key to a run row.

GeoJSON transports geometry and classes but not complete run archives. Export of
linked cells reports `cell_features_dropped` and requires `allow_losses=True`;
imported objects have no run links. Generic object CSV retains its existing
geometry-oriented schema. [Cell CSV](cell-tables.md) remains keyed by archived
run labels; use the typed object reference to join it to published cell IDs.
[Project-wide archived cell CSV](project-cell-tables.md) adds image context to
the historical rows. Capture-time [source/build provenance](analysis-capture-provenance.md) is stored
with new runs. Joined current-object tables, full-file/binary/runtime manifests and numerical
QuPath feature comparisons remain open work.

## Validation

`tests/test_cell_feature_links.py` checks live and saved publication, exact
compartment features, reuse, edits, Undo/Redo, missing archives, snapshot survival,
project/recovery round trips, malformed records, transactional schema-5 migration,
explicit GeoJSON loss and failed-publication rollback. Native archive tests cover
publication/lookup/history and invalid future links, including GEOS-disabled
build behavior. Existing desktop cell publication tests exercise the shared path.

```powershell
python tools/benchmark_cell_links.py --output build/cell-links.json
python tools/benchmark_cell_links.py --objects 100000 --gui --output build/cell-links-view.json
```

The synthetic fixture contains 4096 isolated five-sample cells. The benchmark
validates every link, sample count and saved feature row before recording native
publication, lookup, save/load and optional selected/all-visible viewer timings.


The [4096-cell recording](benchmarks/cell-links-4096-windows.json) publishes all
objects and retains one run in 5.15 ms. Looking up all statuses takes 0.488 ms;
copying every feature row through Python takes 0.576 ms. Native save capture is
0.012 ms, charging 11284585 logical snapshot bytes; worker save is 43.14 ms and
validated load, including derived contour reconstruction, is 687.97 ms.

The [104097-object recording](benchmarks/cell-links-view-100000-windows.json)
adds 100000 unrelated objects and publishes the same 4096 cells in 6.64 ms. Save
capture is 0.0225 ms, worker write 265.30 ms and validated load 814.15 ms. All
104097 objects remain selected and visible with the final cell as primary, so
the captured-feature inspector is active. Across 180 frames the unchanged viewer
records 16.93 ms p95 and 17.48 ms maximum CPU frame work, using 3675040 GPU canvas
payload bytes on an RTX 4090. These are single warm synthetic observations on
one Windows machine. Publication uses disabled history for this throughput
workload; history atomicity/rollback is checked separately. Other slides/devices,
invalidated canvases, initial rendering and prolonged pressure remain unverified.

The final installed-wheel regression passes 902 Python cases with two Windows
symlink cases skipped. All 26 native full-GUI suites and 25 suites in each
core/minimal configuration pass. The 123 focused cell/archive/desktop cases
include exact saved-run reuse after rechecking, numeric-only contour-limit
fallback and geometry/source status. A native desktop capture verifies that the
inspector's run/label, currentness, sample counts, RGB means and historical-value
explanation are readable.
