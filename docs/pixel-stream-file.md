# Durable pixel stream files

`save_pixel_stream` consumes a fresh [native block producer](pixel-stream.md)
and publishes a compact QPS1 file. It stores the model, capture provenance and
original target geometry once, followed by row-major label/validity/vote blocks.
No complete output mask or per-block index list is assembled in RAM.

```python
producer = q.PixelPredictionStream(document, target_id, model, options)
summary = q.save_pixel_stream(producer, "slide-mask.qps")
saved = q.PixelStreamFile("slide-mask.qps")
saved.verify()
assert saved.verified
block = saved.read_block(7)
labels = block.labels_numpy()  # Native, read-only ownership.
```

Opening validates the bounded header, sampled grid, file length and completion
footer. Individual `read_block(index)` calls validate that block's checksum,
numeric hash, ROI membership, class/vote rules and counters. `verify()` reads
every block sequentially and recomputes the ordered commitment and aggregate
totals. A corrupt unrequested block can therefore be detected later rather
than during opening. `declared_summary` deliberately names footer declarations;
they are fully corroborated only after successful complete verification.
Each indexed access also rechecks prefix/header commitment and footer framing.
Block-zero reads and complete verification rehash the stored header, rather
than trusting its cached decoded value. This detects framing/header changes
even when filesystem timestamps do not distinguish rapid writes.

The reader retains decoded metadata and the first block, not an open decoder
handle or a full mask. Each access checks the original file version against a
fresh guarded path. Changed/replaced inputs fail; previously returned native
arrays remain usable. Block reads and verification serialize their file work
and release the Python GIL. `cancel()` is thread-safe and terminal for that reader, interrupts
long verification at bounded checkpoints, and clears verified status. Create
a new reader to continue after cancellation. `verified` describes the last
successful scan, not continuous filesystem monitoring.

All loaded blocks are historical values. Original annotation identity, geometry,
sampling resolution and build/source provenance are retained, but no live
`PixelPredictionRequest` capability is restored. A newly captured request cannot
grant a loaded block publication authority.

## Publication and resource limits

The default output and reader limit is 1 TiB, configurable up to 64 TiB. Size
arithmetic, grid coverage, block count and expected exact file length are checked
before large allocations or arbitrary seeks. A metadata header inherits the
QPA2 artifact limits: at most 128 MiB encoded and 256 MiB charged decoded payload.
That header allowance is separate from the producer work budget. Subsequent
records use one block plus a roughly 64 KiB I/O buffer; model metadata is shared.
These are bounded allocation/encoding contracts, not process RSS limits.

New output is written to an exclusively created temporary sibling, flushed,
then atomically committed. Cancellation, source failure, byte-limit failure and
validation errors remove that temporary and leave the destination untouched.
The producer may have been consumed partially; use a fresh producer to retry.

Existing paths require `replace_existing=True`. Replacement fully verifies the
old stream and retains its version guard through generation. Unrelated or
corrupt files are rejected before consuming the new producer. If the destination
was absent initially, even a replacement-enabled call uses non-overwriting
publication, protecting a file that appears while generation is in progress.
Final replacement follows the repository's guarded atomic-export policy. OS
flush/rename behavior is not a cross-platform power-loss durability guarantee.

Do not use the same producer concurrently from another consumer during saving.
Coverage and progress checks reject interference rather than publishing a
missing or reordered stream. Python producer cancellation stops unfinished
prediction. The native writer additionally accepts an I/O cancellation token;
the [background export job](pixel-stream-export.md) exposes both producer and
writer cancellation to Python and the desktop.

## QPS1 layout

All integers are unsigned little-endian; floating values use IEEE-754 bits.

| Component | Contents |
| --- | --- |
| 68-byte prefix | `QPS` + version byte 1; global x/y/width/height, effective block size, block count, window flag and header length as eight u64 values |
| Header | One bounded QPA2 prediction archive for block zero, with the complete model, provenance and original target |
| Header checksum | 64 lowercase hexadecimal SHA-256 bytes over prefix and header |
| Remaining blocks | 64-byte numeric hash, 64-byte input hash, f64 elapsed milliseconds, 9 bytes per pixel, then a 64-byte record checksum |
| Completion footer | `QPE` + version byte 1; valid/classified totals, one u64 per model class, ordered content commitment and footer checksum |

Each pixel is u32 class label, u8 validity and f32 winning vote fraction. Label
zero means unclassified. Invalid fractions use canonical `0x7fc00000` NaN bits.
The record checksum hashes domain `qpx.pixel-stream-block/1`, its u64 block
index, metadata and packed arrays. The ordered commitment hashes domain
`qpx.pixel-stream-content/1`, the header checksum, block-zero archive checksum
and subsequent record checksums. The footer checksum includes the header
checksum and footer fields. `summary.sha256` is this framed content commitment,
not the SHA-256 of all raw file bytes. Checksums do not authenticate authors.

Block offsets are calculated from the grid and edge dimensions; no disk or RAM
offset table is required. The effective block size can be smaller than the
configured producer size when a single block covers the entire target.

Restart/resume, project-linked stream records, stitched whole-stream contours
and an interactive historical file browser remain open. This format provides atomic complete
numeric output and bounded historical access for those later workflows.


The [local validation record](benchmarks/pixel-stream-file-local-windows-linux.json)
ties the installed wheel, source fingerprint, full suite, native configurations,
portable fixtures and both file benchmarks together. Reproduce a synthetic file
probe with `python tools/benchmark_pixel_stream_file.py --size 2048 --output report.json`;
its default unique output is retained under `build/pixel-stream-files`.
