# Hash-only pixel input scans

Input scans let callers compare captured source pixels with previously verified
pixel output without extracting feature planes or running the forest. They are
a prerequisite for future project-wide resumable pixel batches.

```python
import qupathxx as q

request = q.PixelPredictionRequest(document, target_id, model, prediction_options)
current_input = q.input_sha256(request)
assert current_input == q.predict_pixel_region(request).input_sha256

stream = q.PixelPredictionStream(document, target_id, model, stream_options)
current_stream_input = stream.input_sha256()  # Does not consume blocks.
assert stream.completed_blocks == 0

saved = q.PixelStreamFile("mask.qps")
saved.verify()  # Complete scan, bounded to one decoded block at a time.
historical_input = saved.input_sha256()  # Cached aggregate from that scan.
```

The low-level `pixel_feature_input_sha256(pixels, options, transform, window)`
matches `extract_pixel_features(...).input_sha256`, including schema, requested
window, required halo and every RGBA byte in that context. Bytes outside the
required context are excluded. Alpha bytes are included even for invalid pixels.
It applies the same option/window/work-budget validation as feature extraction.

`input_sha256(request)` follows the inference tile grid and hashes each feature
input plus the captured ROI membership of output pixel centers. It reads one
tile and its halo at a time, allocates no feature planes or output mask, releases
the Python GIL, and preserves the document and captured request. Source reads
and polygon membership checks still cost time. Native callers can supply a
cancellation token; source read calls must return before cancellation is observed.

`stream.input_sha256()` scans the entire captured grid, including after partial
or completed production, without advancing counters. It shares the single
consumer guard with `next()`, reports `busy` and state `hashing` while active,
and rejects concurrent consumers. `cancel()` stops a fresh or partially produced
stream's scan and production. As with production, cancelling an already complete
stream is a no-op; native callers can pass a separate scan cancellation token.
Failed, cancelled or closed-source streams cannot be scanned.

The stream aggregate uses SHA-256 over the ASCII domain
`qpx.pixel-stream-input/1`, the model's 64-character hash, the little-endian
unsigned 64-bit block count, then each block's unsigned 64-bit index and
64-character input hash in order. Full file verification computes this aggregate
during its existing scan. The QPS1 format is unchanged. The file accessor requires
successful verification and returns the last verified scan's value; it performs
no new filesystem check. Observed read/verification failures or cancellation
invalidate access to the cached value.

Equality means equal hashed input and framing. It does not establish equivalent
inference parameters, source ownership, biological validity or publication
authority. In particular, a resume implementation must also compare prediction
parameters (including minimum vote), grid, model and captured target metadata.
Tile and block layouts affect the digest even when numeric masks happen to match.
Current-source reconciliation and project-wide pixel batch resume remain open.


## Local performance probe

The installed Windows release scans synthetic 1024² and 2048² embedded RGB
ellipses with the portable fixture model, 128-pixel blocks and 64-pixel inference
tiles. Hash-only time is 27.6/127.2 ms; inference is 432.9/1725.5 ms, an observed
15.7×/13.6× ratio. Every inferred block's input commitment independently frames
to the same aggregate. No stream counters advance during the hash-only scan.
The conservative inference work estimate stays 1,352,748 bytes at both sizes;
one output block contains at most 147,456 array bytes. These are single runs on
a shared workstation, with no measured RSS or vendor-slide/hardware acceptance.
See the [1024](benchmarks/pixel-input-digest-1024-windows.json) and
[2048](benchmarks/pixel-input-digest-2048-windows.json) records and
`tools/benchmark_pixel_input_digest.py` for the reproducible probe. CI runs the
same probe without imposing a workstation-specific speed threshold.
