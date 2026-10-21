# Native ROI analysis requests and jobs

`qupathxx/analysis.hpp` provides one native path for synchronous C++, Python and
background ImGui analysis. The first operations summarize RGB optical density
or three supplied-stain concentrations over a body or nucleus ROI. They use the
[fixed stain kernels](stain-transforms.md), independently of display settings,
physical calibration and the geometry-only shape cache.

## Captured inputs and publication

Create an immutable `AnalysisRequest(document, object_id, transform, kind,
nucleus=False, tile_size=256)` on the document's owner thread. `kind` is
`AnalysisKind.OPTICAL_DENSITY_MEANS` or `AnalysisKind.STAIN_MEANS` (default).
The request captures the selected immutable ROI, object/document revisions,
normalized stain directions, per-channel whites, operation and tile size.
Nucleus requests require a nucleus. Non-area ROIs, closed sources, ROIs outside
the image and bounds containing no pixel centers are rejected before starting.
Tile sizes must be in `[16,1024]`.

The private publication key combines a retained document instance token, the
retained image-server instance, object ID and object revision. Requests do not
retain a Document or Project. Copying, moving, assigning or restoring a Document
creates a fresh authority token; an old request cannot publish to a new graph
whose IDs/revisions happen to match. Relinking changes the source instance and
invalidates old requests. Closing a source invalidates completed results too.
The retained server prevents pointer-address reuse from matching a different
source. Object edits, deletion, Undo/Redo and primary-selection changes reject
the old result. Unrelated object edits and calibration edits preserve these
dimensionless measurements. A future physically calibrated operation must
include calibration in its own dependency contract.

These are **runtime identity checks**, not an image-content fingerprint. A file
modified externally while an existing reader is open is not detected here.
Retained records now add decoded ROI input digests and explicit rechecks after
reopening; see [analysis provenance](analysis-provenance.md). Whole-source
fingerprints, environment manifests and full reproducibility remain required work.
The captured document revision is
provenance, not a dependency that invalidates on every unrelated edit.

`request.matches(document, selected_id)` tests applicability without computing.
`analyze(request)` returns a value directly and does not publish or modify live
objects. Its caller must check applicability before attaching the value to a
live graph. `AnalysisJob.poll/wait` enforce that check before exposing a result.

## Sampling and numerical values

The versioned algorithm is `qpx.roi-channel-means/1`. It reads level-zero tiles
in row order and visits each candidate pixel center once. Rectangle candidates
use half-open bounds; other area ROIs use the same immutable geometry containment
predicate as native measurements, including holes and multipart areas. Alpha
must equal 255. Partial/transparent pixels are excluded, never counted as black
tissue. No valid sampled centers is an error, not a zero-valued measurement.

Each pixel first becomes float32 OD or concentration through the shared kernel.
Three double-precision compensated sums then accumulate those values, and the
result divides by the valid pixel count. This is the mean of pixel ODs, not the
OD of the mean raw intensity. Signed concentrations remain signed. Changing
tile partitions may change the last floating-point bits of a summary; independent
fixtures use numerical tolerances rather than requiring partition-independent
bitwise sums. The per-pixel transform itself remains exactly partition invariant.
There is no spatial halo for these pointwise operations. This does not establish
border reconciliation for future connected-component/detection algorithms.

`AnalysisResult` is an independent numeric value: algorithm version, operation,
body/nucleus flag, object ID/revision, captured document revision, normalized
stains/whites, three means, valid sample count, tile count and elapsed milliseconds.
It owns no image, ROI, document, request or Python references. The request exposes
its captured ROI and transform for inspection. Results are not automatically
written to objects or CSV measurement tables. Explicit `keep_analysis` /
`AnalysisJob.keep_result` now retain them in schema-3 project/recovery storage;
see [analysis provenance](analysis-provenance.md).

## Background controller and ownership

```python
import qupathxx as q

doc = q.Document(q.Image.demo())
obj = doc.add_rectangle(q.Rect(100, 100, 200, 150))
transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
request = q.AnalysisRequest(doc, obj, transform)
# Use q.analyze(request) instead for synchronous execution.
job = q.AnalysisJob()
assert job.request(request)
status = job.wait(doc, obj)  # Interactive callers use nonblocking poll instead.
if status.state == "ready":
    print(job.result.means, job.result.samples)
```

`AnalysisJob` is an owner-thread controller with one worker and no queue.
`request` returns false while a future is pending, including finished work not
yet polled. No request replacement or unbounded backlog is hidden inside it.
`poll(document, selected_id)` is nonblocking; `wait` joins then reconciles.
Stats report state, generation, completed/total tiles and worker elapsed time.
States are `idle`, `running`, `cancelling`, `ready`, `stale`, `cancelled`, `failed`.
An observed stale key latches: returning to the old selection cannot revive it.
An explicit cancellation wins even when computation just finished. Start a new
request after cancellation or failure. A ready value remains available until
invalidated/cancelled/replaced; already copied numeric results remain usable.
Exceptions become failed-job errors, while synchronous `analyze` throws.

The worker reads only the immutable request owned by the controller; it captures
no shared source/Document/Python ownership. The owner thread retains the source
until completion. Stale/cancelled/failed requests release there after joining;
a ready result retains its request key for later invalidation. Destruction
cancels and joins before destroying request ownership. This also handles a
Python-owned server proxy without releasing its last reference on a worker
while the main thread holds the GIL. Native/Python teardown is regression tested.
Nonblocking polling refers to worker/I/O readiness; releasing a terminal
request's last source owner can still add owner-thread destruction latency.

Document/request capture, job control and result publication are owner-thread
APIs. Python `analyze` and `wait` release the GIL. Other Python threads must not
mutate a Document concurrently with `wait` reconciling it. During background job
execution the owner can edit normally; the worker never reads canonical maps.
Built-in dense, linked and OpenSlide readers expose `closed()` atomically so
owner-thread polling does not wait behind an in-progress region read. Native
custom readers should make their closed-state query nonblocking as well.

Cancellation is checked before reads, inside transforms, each mask row and before
returning a result. An individual synchronous reader call is not interruptible;
vendor/filesystem stalls can delay cancellation and destruction. No partial
summary is published. Progress is tile-level and may remain unchanged during
one such call.

For tile side `T`, the kernel holds one `4*T*T` RGBA input and one `12*T*T`
float result, plus the captured ROI, fixed lookup tables, reader-specific scratch
and constant-size summary. Default payload is at most 1 MiB per job; the maximum
1024 tile uses 16 MiB. There is no full-image output allocation or result cache.
Source storage and reader caches are additional. Multiple caller-created jobs
still require caller-level concurrency limits; a global analysis executor and
batch scheduling remain open.

## ImGui integration and evidence

The selected object's inspector contains **Region analysis**, OD/stain selection,
body/nucleus choice, white values and two supplied RGB OD directions. The third
stain channel is the signed cross-product residual. Example directions are not
an automatic stain estimate. **Analyze region / Ctrl+M** submits one request;
**Cancel analysis / Ctrl+Shift+M** cancels busy work. Parameter edits clear the
previous result, and source/object/selection changes clear stale results. Three
channel means appear on separate rows with sample count and worker duration.
Display transforms do not alter inputs. The UI never automatically recomputes
these features on every frame.

`tests/test_analysis.py` independently checks pixel formulas/matrix solves,
ellipse and hole/nucleus masks, multiple partitions, synchronous/job equality,
unrelated edits, invalid inputs, cancellation/reuse, relink/reopen identity and
external-source teardown. `tests/test_gui_analysis.py` replays OD/body/nucleus
requests, checks exact GUI/native summary equality and rejects deleted-object
results. Native tests additionally cover authority copy/assignment/move, a
controlled blocked reader with responsive polling and stale cancellation, and
failure when a reader returns only transparent pixels. A rendered inspector
capture is visually checked for all three result channels.

`tools/benchmark_analysis.py` records capture, worker start, progress/poll latency,
completion and numerical equality; its optional 100001-object selected viewer
keeps the same full-image job on the native worker. The committed synthetic
[headless record](benchmarks/analysis-job-4194304-windows.json) measures 4194304
pixels in 64 tiles with 0.0076 ms capture, 0.085 ms submission, 43.7 ms worker
time and 0.012 ms maximum poll. The separate
[viewer record](benchmarks/analysis-job-view-100001-windows.json) reports 100001
selected objects, 876 visible, 16 MiB GPU tile payload and 25.4 ms analysis work,
with 16.88 ms sampled frame p95 and 17.17 ms maximum sampled frame work.
These are individual dense-image recordings, not a uniform speedup claim.
Viewer timings exclude its first ten frames and swap wait; early rendering
stalls, wider views, vendor readers and cross-platform performance remain open.

This is the first shared analysis request/publication path. M5 still requires
complete source/environment provenance, tissue detection, connected
components, stain estimation, watershed cell detection, feature publication and
QuPath runtime workflow comparisons. M6 still requires batch restart/failure
policies, a bounded shared executor, NumPy ownership exchange and stable stubs.
