# Headless project processing

The native batch API runs archived ROI OD/stain means, tissue detection or cell
detection over a frozen project snapshot. Each completed image publishes a
separate `.qpxproj` checkpoint with its source objects, existing archives, new
results and an embedded measurement CSV. Open that checkpoint in the existing
viewer to inspect results. The source project is never modified by a batch.

## Native executable

`qupathxx_batch` builds even with Python and the GUI disabled. Windows wheels
install it beside `_native` and the reader/geometry runtime libraries.

```powershell
qupathxx_batch study.qpxproj results --kind tissue --continue-on-error
qupathxx_batch study.qpxproj results --kind cells --image 4 --object 9
qupathxx_batch --export-csv results/image-.../....qpxproj measurements.csv
```

`--help` lists selection, stain/white, threshold, existing pyramid level, tile,
publication and byte-limit flags. Default selections are all annotation area
ROIs, ordered by image and object ID; explicitly selected object IDs can have
other object kinds. A selection with no object IDs selects all matching areas
in that image. It cannot express an intentionally empty object set. `--object`
requires exactly one `--image`. Default OD measurement does not depend on the
stain basis; stains/tissue/cells use the supplied model. The CLI default is an
explicit approximate H&E basis, printed by `--help`; supply appropriate vectors
for the acquisition. `--normalized` requires all nine already normalized basis
components and preserves their exact values.

Stdout is JSONL: one `qpx.batch-image/1` event per attempted image, followed by
`qpx.project-batch/1`. Setup/argument exceptions use `qpx.batch-error/1` on stderr.
Exit codes: 0 success, 1 failed or partially successful batch, 2 setup/argument
error, 130 cancellation. Ctrl+C cancels at native checkpoints; a decoder call
already in progress must return before cancellation can finish. Initial CLI
project opening and snapshot capture precede the batch cancellation loop.

## Python and C++

```python
import qupathxx as q

project = q.Project.open("study.qpxproj")
transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
options = q.ProjectBatchOptions()
options.kind = q.ProjectBatchKind.TISSUE
options.failure = q.ProjectBatchFailure.CONTINUE
request = q.ProjectBatchRequest(project, transform, options=options)

job = q.ProjectBatchJob()
job.request(request, "results")
# Call poll() on the model owner thread while other work continues.
status = job.wait()
report = job.result
for image in report.images:
    if image.state in ("saved", "reused"):
        q.save_batch_checkpoint_csv(image.path, image.path + ".csv")
```

`run_project_batch(request, directory)` is synchronous and releases the Python
GIL. `ProjectBatchJob` uses a native worker; request capture, polling, result
access and controller destruction belong on the owner thread. Its request is
retained there until the worker ends. `wait()` holds the GIL while waiting so
owner-side release of captured model data remains safe; use polling to allow
other Python threads to continue. No Python callback executes on the worker.
The C++ synchronous API additionally accepts an atomic cancellation flag and
native per-image callback. An async controller's `cancel()` is nonblocking;
its destructor cancels and joins. Reports retain strings/counts, not documents.

`ProjectBatchSelection` provides `image_id` and `object_ids`. Default capture
limits are 10,000 images, 100,000 source objects and 256 MiB logical snapshot
payload. Duplicate image selections reject; duplicate explicit object IDs
collapse. Requests preserve geometry/classes/calibration/archives even if the
live source is later edited or removed. Linked image pixels are read when the
worker runs; the request does not freeze external files.

## Transactions and restart

Only one writer uses an output directory at a time, enforced by a native
exclusive `.qupathxx-batch.lock`. Image work occurs in exclusively created,
marked private work directories. A saved image is a SQLite transaction containing
canonical data, CSV and checkpoint metadata. It publishes to a new versioned
filename without replacing prior artifacts. Cancellation or per-image failure
publishes no partial image. Previously completed images remain available.
Stop-on-error is the default; continue-on-error produces a `partial` report.
A separate uniquely named terminal JSON report records completed/failed images.
An audit write failure makes the overall report failed even if images saved.

Restart is enabled by default. The request key includes the frozen input
project serialization, model, all algorithm parameters and native build/runtime
environment. For candidate checkpoints, a digest-only tile pass recomputes the
exact requested RGBA input streams before reuse. Changed linked pixels create
a new checkpoint. Reuse also checks metadata/CSV and canonical project data
SHA-256 checksums and configured result limits. Missing/corrupt/edited matching
checkpoints fail explicitly rather than being silently replaced. `--no-resume`
recomputes and publishes a separate artifact. Ordinary project saves remove the
completion marker transactionally; edited results are no longer reusable.

These checksums detect accidental changes, not malicious tampering. Input
digests describe the algorithm's decoded tile streams, including bounding-box
pixels outside the ROI; they do not hash an entire slide or establish that all
decoder dependencies remained unchanged during a read. Reuse is conservative:
changing capture revision, build or input archive timing can require recompute.

Each checkpoint CSV contains only records generated by that batch image, with
the existing native ROI/tissue/cell table columns plus five `batch_*` context
columns for source project identity/generation/revision and image ID/name.
Empty selections produce a header with no invented measurements. CSV is inside
the committed checkpoint; extraction verifies checksums and protects the
checkpoint and captured primary image path/file identity from overwrite.

## Resource and failure boundaries

The worker materializes one image at a time from a bounded save snapshot. Defaults
are 512 MiB conservative materialization charge, 256 MiB generated archive
payload, 64 MiB CSV and a 1 GiB checkpoint file. Tissue/cell work buffers retain
their own validated limits. Optional cell object publication additionally checks
available materialization space. `checkpoint_limit` limits SQLite page count;
journals, private input files and terminal audits consume additional disk.
Up to 64 MiB is allowed for the terminal report.

These are payload/work limits, not a process RSS cap. The already-open source
model, immutable snapshot blocks, one writable image, table snapshots, encoded
archives, SQLite page cache and buffers can coexist. The CLI initially loads
the complete input project model; it does not stream a catalog from storage.
Snapshot capture scans selected objects and must run on the owner thread.
Restart still reads the selected tile streams and verifies checkpoint content;
it skips expensive analysis, not all I/O. A corrupted matching checkpoint is a
failure requiring explicit recovery/recompute policy from the caller.

Normal completion cleans only known files in its own marked work directory.
Abrupt termination can leave private work directories; restart ignores them.
Automatic abandoned-work cleanup, portable/distributed workers, a GUI batch
launcher, live scripting and complete M6 workflow parity remain unfinished.
Windows publication uses write-through move; no cross-platform power-loss
durability claim is made. The GUI, Python and original project remain the
authorities for their own live mutable state.


## Validation and measured scope

The installed Windows GUI wheel passes 22 focused batch cases: all three
workflows, direct-kernel/independent CSV comparisons, preserved old archives,
empty headers, selected IDs, frozen model lifetime, owner collection, changed
linked pixels, checkpoint/CSV/data corruption, ordinary-save invalidation,
stop/continue, budget failures, concurrent-output rejection, native Unicode CLI
and repeated publication, coarse-grid independent hashes, optional cell object
publication and viewer reopening. Native tests additionally cancel from an
image-completion callback and verify restart reuses that image while processing
the remainder. Existing algorithm tests establish the numerical kernels; these
batch tests establish composition and storage behavior.

The synthetic benchmark uses 16 embedded 512x512 images, one tissue ROI each,
and 100000 background detections in the first image. The frozen capture charges
51343612 logical bytes; p95 capture is 3.129 ms over 20 captures. Native submit
is 0.0458 ms and maximum sampled poll 0.0182 ms. Processing saves 16 checkpoints
(total 24281088 bytes) in 1377.46 ms; verified restart takes 938.20 ms. During a
separate recomputation with all 100001 objects selected/visible, viewer p95 CPU
frame work is 16.85 ms, maximum 17.25 ms, and the concurrent batch takes 2289.69
ms. Overlay payload is 3675040 bytes and tile GPU payload 1048576 bytes.

These measurements cover one warm synthetic workload on Windows 11 with an
RTX 4090. CPU frame work excludes presentation wait and does not establish a
refresh rate. Capture includes the object scan; the metric is not an O(1)
capture claim. Logical charges are not process RSS. Restart retains substantial
serialization, materialization and verification work. Real-slide throughput,
long-running RSS stability and broader failure/power-loss campaigns remain
unverified. [Raw benchmark](benchmarks/project-batch-view-windows.json) records
exact native and source hashes and the complete workload.


The final installed GUI wheel regression passes 1081 Python cases with two
existing Windows file-symlink permission skips in 247.04 s. Native full/core/
minimal suites pass 32/31/31; a separate storage-disabled build compiles the CLI
and passes the focused batch/digest/storage-rejection check. Source fingerprint
verification is included in the full installed-wheel run. The benchmark above
uses that same native artifact. The full M0-M7 goal remains active.
