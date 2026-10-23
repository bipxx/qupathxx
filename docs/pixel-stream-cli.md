# Headless pixel stream commands

The installed `qupathxx_batch` executable exports a target from a saved project
using the same bounded native producer and background export job as Python/F5.
It can also verify an existing QPS1 file without opening a project or slide.

```text
qupathxx_batch --export-pixels project.qpxproj 2 17 model.qpc mask.qps --block-size 128 --work-bytes 8388608
qupathxx_batch --verify-pixels mask.qps
```

The positional IDs are the project image ID and its target object ID. The model
is an existing resolution-bound `.qpc` pixel classifier. Its saved feature recipe
and sampling resolution govern prediction; compatible natural pyramid levels
are selected using the existing model-resolution contract. The selected object
must satisfy the same area/ROI requirements as Python prediction. The project
and model are read without saving or modifying them. No complete output mask is
assembled; each prediction block is released after encoding.

Export options:

| Option | Meaning |
| --- | --- |
| `--block-size N` | Output block size, 16–512; default 256 |
| `--work-bytes N` | Producer/kernel work allowance; metadata/header storage is separate |
| `--tile-size N` | Kernel read tile size under the existing prediction contract |
| `--max-pixels N`, `--max-blocks N` | Producer quotas, checked before source pixel reads |
| `--file-bytes N` | Output-file limit; default 1 TiB, at most 64 TiB |
| `--min-vote NUMBER` | Minimum winning tree-vote fraction |
| `--window X,Y,WIDTH,HEIGHT` | Requested level-zero window intersected with the captured area |
| `--replace-existing` | Explicit replacement of a fully verified prior QPS1 file |

Verification accepts only `--file-bytes N`, using the same default/max limits.
It validates all blocks, numeric values, aggregate counts and content commitment.
It needs neither a training backend nor a live image; the minimal native build
can verify supported rectangle files. Polygon topology still requires GEOS.

Stdout is UTF-8 JSON Lines. Export emits `qpx.pixel-export/1` snapshots and one
terminal line. Progress is emitted only when completed blocks change, at most
once per 100 ms. Completed-block counters describe prediction, not committed
disk records. A successful `saved` line includes `summary` with bytes, blocks,
valid/classified pixels, class counts and `content_sha256`. Other terminal
states have no summary. The content hash is the QPS1 framed commitment, not a
raw file SHA-256. Records also include image/object IDs, model identity, native
source fingerprint and the estimated producer work allowance.

Verification emits one `qpx.pixel-verify/1` JSON object. Only `verified` results
include a trusted summary. Argument/setup errors emit the existing
`qpx.batch-error/1` JSON object to stderr. Exit codes are 0 for a saved/verified
result, 1 for a worker/file failure, 2 for argument or capture/setup errors and
130 for cancellation. The existing project-wide OD/stain/tissue/cell batch and
CSV checkpoint commands retain their syntax and behavior.

Ctrl-C/SIGINT and SIGTERM request cancellation on Unix; Ctrl-C/Ctrl-Break do so
on Windows. The controller cancels both prediction and writer checkpoints and
joins the worker. An uninterruptible OS/decoder call can finish first. Model
loading supports cancellation; project loading uses the existing synchronous
project loader before export capture. A completed atomic commit wins over late
cancellation and is reported as `saved`. Cancellation cannot retract that file.
Temporary cleanup, unrelated-file protection and replacement rules follow the
[QPS1 contract](pixel-stream-file.md). Abrupt process termination is outside the
cooperative cancellation guarantee.

This command exports one image/area per invocation. Shell/Python orchestration
can invoke it for multiple targets. Project-wide resumable pixel-classifier
batches, project-linked stream records, source reconciliation and stitched
whole-stream contour publication remain open. This addition does not mark those
requirements complete.


The [local verification record](benchmarks/pixel-stream-cli-local-windows-linux.json)
links the installed executable/wheel, targeted regression suites, native
configurations and both synthetic export probes. Reproduce a probe with
`python tools/benchmark_pixel_stream_cli.py --size 2048 --output cli.json`.
It retains the generated project, model and stream under a unique
`build/pixel-stream-cli` directory. This synthetic fixture compares every saved
label/validity/vote with an independent window and verifies original input
bytes; it establishes no RSS, vendor-slide throughput or biological accuracy.
