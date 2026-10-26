# Saved raw prediction contours and fresh replay

A complete QWS1 prediction file can be converted to geometry without opening its
original image or restoring a project. The stored model, ROI, exact global
lattice axes, region, labels and class counts are historical inputs. The shared
boundary/GEOS path preserves cropped phase and clipping. Partial prefixes cannot
produce complete contours and reject; indexed block inspection remains available.

`RawPixelFileContours` is immutable historical output, separate from live
`RawPixelStreamContours`. It exposes `contours`, `link`, `file`, `class_names`,
`options`, `completed_samples`, `estimated_work_bytes` and `payload_bytes`.
Its common geometry has object ID zero and no live input identity. Passing it
to `publish_raw_pixel_stream_contours` is a type error.

```python
import qupathxx as q

limits = q.RawPixelStreamFileLimits()
limits.work_bytes = 16 * 1024**2
file = q.RawPixelStreamFile("prediction.qws", limits)
history = q.trace_raw_pixel_stream_file(file)
for component in history.contours.components:
    print(history.class_names[component.label - 1], component.geometry.wkt)
```

The converter verifies header/framing, every block, complete chains and census
in the same pass that feeds the boundary builder. It releases each block before
reading the next and checks the file version before result delivery. No whole
label raster is allocated. The result retains its verified file metadata and
geometry; a later file/source change does not mutate that historical snapshot.

`RawPixelStreamFileReadJob.trace(file, options)` and
`trace_path(path, limits, options, expected=None)` provide background execution.
`trace_path` opens and converts in one verified pass; a supplied retained link
must match completely. Wait/poll on the owner, then call `take_contours()`.
Wrong result-transfer methods return None without consuming the pending result.
Only `cancel()` is cross-thread; shutdown cancels and joins before releasing files.

## Reverify before live apply

A file does not restore document identity, a current ROI or publication authority.
Reopen the real project and use `capture_raw_pixel_stream_replay(doc, file)` after
full file verification. This creates a new owner-bound request from the current
image/ROI/calibration and stored model/lattice/options. Its request commitment
must match history. `trace_raw_pixel_stream_replay(request, file, options)` then
re-executes prediction and compares complete input/prediction commitments and
census. It returns a live result for that exact fresh request.

```python
project = q.Project.open("source.qpx")
doc = project.document(project.images[0].id)
file.verify()
request = q.capture_raw_pixel_stream_replay(doc, file)
options = q.RawPixelStreamContourOptions()
job = q.RawPixelStreamContourJob()
job.request_replay(request, file, options)
job.wait(doc)
result = job.result
if result is not None:
    ids = q.publish_raw_pixel_stream_contours(doc, request, result, "Replayed")
    project.save_new("replayed.qpx")
```

Changed source/model/ROI/lattice/calibration rejects matching or replay. An
identical separately captured request cannot publish another capture's result.
The live result retains the file version check through final publication; late
history changes, stale document/revision, cancellation and quotas prevent apply.
Publication remains one admitted undoable command with no partial edit.

## Application entry points

The desktop **Preview and files** tab provides **Inspect saved contours**,
**Reverify saved contours for apply** and **Apply reverified contours**. Inspection
needs no active live model. Preview colors use the stored classes. Only successful
fresh replay enables apply; the source project can then save ordinary annotations.
The Retained files tab provides the same inspect/reverify/apply actions for a
saved project record. Opening another history retires the previous block preview.
Contour and stream limits remain in the existing Contours tab.

`ViewerSession.contour_raw_pixel_file(image_id, path, limits={}, options={},
record_id=0, replay=False, apply=False, name_prefix="Raw pixel",
expected_revision=None)` queues the same shared frontend. Choose a path or one
retained record. `apply=True` requires `replay=True`. Historical events expose
`raw_pixel_file_contours`; live replay events expose `raw_pixel_stream_contours`.
Result payload and anticipated IDs are reserved before mutation. Optional revision
checks and owner capture prevent applying after edits during file verification.

```text
qupathxx_batch --inspect-raw-contours prediction.qws --file-work-bytes 16777216
qupathxx_batch --replay-raw-contours source.qpx 1 prediction.qws replayed.qpx --file-work-bytes 16777216
```

Inspection emits bounded JSONL `qpx.raw-pixel-file-contours/1`, terminal state
`historical`, with class/sample labels and WKT geometry. `--json-bytes` bounds
serialization. It opens no original image. Replay emits the existing contour
saved-project report and requires a new output path; all project, reader, model/
history and sidecar collision/version protections remain. Request overrides such
as axes, object, region or vote are rejected: those semantics come from the file.
Both commands accept file/contour resource limits; replay also accepts project/
publication limits and annotation prefix. Ctrl+C/Ctrl+Break cancels and joins.

Work admission includes the retained file budget plus the larger boundary/GEOS
phase; replay also admits its recorded prediction producer budget. Increase the
explicit total limit or choose a smaller admitted file-reader budget when needed.
These conservative estimates are separate from measured process RSS, hardware
budgets and scientific/platform/release acceptance.
