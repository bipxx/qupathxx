# Bounded pixel prediction blocks

`PixelPredictionStream` consumes a target's natural sampling grid without
allocating its full label/validity/vote mask. Construction captures the original
ROI, model, source/provenance and exact document authority. The producer then
computes one bounded rectangular block at a time through the existing source
prediction kernel. It owns no queue of completed blocks and creates no worker
threads itself.

```python
options = q.PixelStreamOptions()
options.block_size = 128
prediction = q.PixelPredictionOptions()
prediction.work_bytes = 8 * 1024 * 1024
options.prediction = prediction

stream = q.PixelPredictionStream(document, target_id, model, options)
for block in stream:
    consume(block.level_bounds, block.labels_numpy(), block.valid_numpy())
    # Consume or write each block here; retaining every block consumes caller RAM.
assert stream.state == "complete"
counts = stream.class_counts
```

C++ uses `qupathxx/pixel_stream.hpp`. `next()` returns a shared immutable
`PixelPredictionResult`, or an empty pointer at successful exhaustion. Python
`next()` returns `None` at exhaustion, while the iterator protocol raises
`StopIteration`. Cancellation and failures raise exceptions rather than
pretending the target was completely processed. Neither terminal condition
can be resumed; construct a new producer for a new run.

## Grid, masks and memory

Blocks are emitted in row-major order, with no gaps or overlaps. `level_bounds`
is the complete emitted grid. The block's own `level_bounds`, `bounds`, level
and downsample locate its arrays. An optional level-zero `window` restricts the
overall stream using the existing [window semantics](pixel-prediction-windows.md).
`stream.windowed` records that overall choice. Every returned block is a window
of the original target and has `block.windowed == True`, including a single
block that happens to cover the entire target.

ROI membership, holes, opacity, feature recipe, resolution and vote thresholds
follow source prediction. Feature halos extend beyond block edges to source
image boundaries. Moving block boundaries does not change the feature context
of a sampled center. Original geometry/provenance remains on each block; the
target is never rewritten into temporary annotation objects.

The default block size is 256 grid pixels per side, with supported values
16..512. `options.prediction.tile_size` independently controls kernel read/work
tiles within a block. The producer reserves its bookkeeping and second immutable
snapshot before calculating the remaining kernel work allowance. Therefore a
block's saved `options.work_bytes` can be smaller than the producer's requested
budget. `estimated_work_bytes` conservatively includes those producer allocations
and the worst-sized block; smaller edge blocks can report that same upper bound.

The work budget is an allocation estimate, not a process RSS limit. Source pixels,
the immutable input model, reader caches, other producers and caller-retained
results are outside that per-operation allowance. Keeping a NumPy view retains
its native block allocation. Consume/release blocks to keep caller storage
bounded. The producer itself does not retain previously returned masks.

`max_pixels` (default 10¹², maximum 10¹⁵) and `max_blocks` (default 10⁷, maximum
10⁸) reject oversized scans before reading pixels. These quotas bound sampled
centers and block count, not wall time or geometry complexity. A too-large block
or feature recipe can still exceed the work budget: choose smaller blocks or a
larger explicit allowance. Planning stores no per-block list.

## Ownership, cancellation and snapshots

Construct the producer on the document owner. `next()` is synchronous and may
run on a consumer worker; Python releases the GIL during native production.
Only one consumer may call it at a time. An overlapping call fails immediately
without changing the active consumer's result or counters. `cancel()` and atomic
progress queries may run concurrently. Keep a native producer alive until its
consumer call returns.

A decoder read already in progress may finish after cancellation. Its unfinished
block is discarded at the cancellation checkpoint. Final counter/result adoption
is serialized with cancellation; cancellation after complete exhaustion leaves
the completed state unchanged. Progress counts committed complete blocks only.
Individual counter reads during production are approximate concurrent observations;
final counts are exact after successful exhaustion.

`matches(document)` is an owner-side input-identity query. A document edit makes
it false, but does not silently rewrite or stop frozen numeric production. The
caller can cancel obsolete work or continue consuming the original ROI snapshot.
Check that identity when deciding whether a result is current. Closing the
source makes further production fail. Source ownership survives outstanding reads;
returned arrays remain readable after the producer/document has been destroyed.

The producer returns numeric snapshots, not live `PixelPredictionRequest`
capabilities. Tracing a block is allowed, but an independently captured request
cannot grant it publication authority. Explicit whole-stream publication needs
a separate reconciled operation; it is not implemented here. Individual blocks
can use existing QPA2 artifacts, retaining their original target and cropped grid.
For compact complete disk output, use [QPS1 stream files](pixel-stream-file.md),
which retain the model once and provide indexed historical block access.

The [background export job](pixel-stream-export.md) powers native/Python and F5
streamed disk output. Restart/resume and stitched contour publication remain
open. This native/Python producer supplies their
bounded source/mask processing primitive; it does not complete those workflows.

## Validation and measured allocation bounds

The installed [1,024-square](benchmarks/pixel-stream-1024-windows.json) and
[2,048-square](benchmarks/pixel-stream-2048-windows.json) probes use the same
18-feature/16-tree recipe, 128-square blocks and an 8 MiB work limit. Both full
mask requests exceed that limit. Every streamed label, validity bit and vote is
compared with a separately captured window request; completed blocks are then
released, leaving no full output array or retained list.

| Target | Blocks | Estimated producer work | Maximum block arrays | Producer time |
| --- | ---: | ---: | ---: | ---: |
| 1,024×1,024 | 64 | 4,406,028 bytes | 147,456 bytes | 0.780 s |
| 2,048×2,048 | 256 | 4,406,028 bytes | 147,456 bytes | 3.757 s |

Block p95/max times were 14.79/15.27 ms and 18.28/20.21 ms respectively.
Timings exclude oracle checks and are single shared-workstation observations.
They measure synchronous producer calls, not GUI frame cost. Input images/models
are caller-owned; these are not RSS, file-backed WSI, disk throughput, hardware
or biological acceptance results. Reproduce with
`python tools/benchmark_pixel_stream.py --size 2048 --output report.json`.

Native tests additionally plan 62,500 blocks over a 32,000-square virtual slide
without allocating full source pixels or reading during capture. They produce
two bounded blocks and exercise cancellation during an in-flight reader call;
they do not scan all billion pixels. Other checks compare complete mosaics with
full masks at natural levels 1, 1.5 and 2, validate block artifacts, and reject
overlapping consumers without disturbing the active consumer.

The [local validation record](benchmarks/pixel-stream-local-windows-linux.json)
verifies 1,556 installed Python passes with six expected skips, all 143 focused
API cases and 16 real-window cases, 39 GUI native and 38 in every other native
configuration, exact stubs, strict typing and installed software-GL launch.
It checks source/wheel/native and benchmark identities. It also preserves the
initial desktop-wheel failure and controlled Win32 reproduction; final camera
assertions remain intact after scripted-wheel isolation.
