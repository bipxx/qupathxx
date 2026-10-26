# Resumable project pixel batches

For quantitative channel inputs, use the separate [raw-classifier batch kind](project-raw-pixel-batches.md).
This page describes the existing RGB pixel workflow.

The project batch engine can export a pinned pixel classifier across selected
area objects and images. Each image produces a completed checkpoint project,
one QPS1 file per target, and a class-count CSV. Results remain independent of
the open source project.

```python
import qupathxx as q

options = q.ProjectBatchOptions()
options.kind = q.ProjectBatchKind.PIXELS
options.pixel_model = q.load_pixel_classifier("model.qpc")
stream = q.PixelStreamOptions()
stream.block_size = 128
prediction = q.PixelPredictionOptions()
prediction.tile_size = 64
prediction.work_bytes = 8 * 1024 * 1024
stream.prediction = prediction
options.pixels = stream

request = q.ProjectBatchRequest(project, options.pixel_model.transform, options=options)
first = q.run_project_batch(request, "batch-output")
resumed = q.run_project_batch(request, "batch-output")
for image in resumed.images:
    print(image.state, image.inferred_blocks, image.hashed_blocks, image.verified_blocks)
```

The request captures an immutable project snapshot, selections, model and
options. Empty selection uses matching area objects in every project image;
explicit `ProjectBatchSelection` values restrict images and object IDs. The
model supplies its own stain transform and exact natural-grid resolution.
The legacy transform argument does not replace the model's transform. There is
no automatic resampling. `nucleus` and `publish_cells` are invalid for pixels.

`ProjectBatchJob` provides owner-side request/poll/cancel/wait and runs the same
worker. Progress reports the current image, phase and completed objects/images;
block counters are included in terminal image reports. Native cancellation is
checked during source scans, inference, output verification and publication.
Operating-system reads and bounded metadata decoding can finish first.

## Checkpoints and reuse

Each image specification has its own directory. An exclusively created
`pixels-<token>` bundle contains an ownership marker, `object-<id>.qps` files and
`result.qpxproj`. Each QPS is published atomically and verified before adoption
as a schema10 historical descriptor. The final project is published only after
all selected targets succeed. Generated descriptor paths are relative to that
final project, so moving the batch directory preserves its output links.

Resume compares the parameter manifest (including model, vote threshold,
tile/block layout and limits), captured project/input metadata, project
identity/generation and a fresh [hash-only source scan](pixel-input-digests.md).
It checks the checkpoint's canonical data checksum, reads bounded descriptors
without loading a second embedded image, verifies every generated QPS block,
compares each file with its descriptor, then compares the aggregate saved input
hash with the current source scan. A valid reused image performs no forest
inference. The checkpoint file guard remains active through this verification.
Source pixels can invalidate reuse even when serialized input metadata is equal.
The session project revision is retained in audit reports but resets on reopen;
pixel reuse keys the stable generation and serialized input instead, allowing
a saved live Python project and the CLI to share checkpoints.

Completed bundles survive cancellation. A restart can reuse committed images
and generate the remaining images. Normal failure/cancellation removes only
known outputs from the current exclusively owned incomplete bundle. Unknown
files, foreign ownership markers and previous bundles are preserved. A process
crash can leave incomplete directories; these are ignored on restart and are
not automatically reclaimed. Corrupt/missing completed outputs fail the image
under the selected stop/continue policy. Disable `resume` to generate a separate
fresh bundle without replacing the old one. Existing historical descriptors in
the input project are preserved; their external files are not batch outputs and
are not required for resume.

`snapshot_budget`, `materialize_budget`, `result_budget`, `csv_limit` and
`checkpoint_limit` retain their existing batch meanings. The pinned model is
charged to capture. `pixels.prediction.work_bytes` bounds producer work; there
is no full-target mask allocation. `pixel_file_limit` bounds each QPS and
`pixel_total_limit` bounds their combined size per image (both default to 1 TiB,
maximum 64 TiB). Actual files are checked against remaining allowances before
verification. At most 10,000 historical stream records fit in an image, including
existing input records, and the normal 100,000-target/10,000-image batch limits
also apply. QPS header/model decoding uses the reader's existing bounded metadata allowance;
source storage and bookkeeping are outside the producer estimate. These estimates
are allocation contracts, not measured process RSS.

The image report exposes `inferred_blocks`, `hashed_blocks`, `verified_blocks`
and `stream_bytes`. These count processed work, including work before an image
failure; they do not assert that failed-image files remain. A fresh image can
also hash current inputs first when prior checkpoints exist but do not match.
The CSV has one row per target/class, with target/record IDs, model/content
hashes, block/valid/classified totals and class counts. Paths in CSV rows are
relative to the image bundle, even when the CSV is exported elsewhere.

## CLI and desktop

```text
qupathxx_batch project.qpxproj output --kind pixels --pixel-model model.qpc \
    --block-size 128 --tile-size 64 --pixel-work-bytes 8388608
```

Additional pixel flags are `--pixel-file-bytes`, `--pixel-total-bytes`,
`--max-pixels`, `--max-blocks` and `--min-vote`. The normal image/object selection,
failure policy, no-resume and resource flags apply. Python and CLI can reuse
each other's checkpoints with equal options. JSONL image reports include the
block counters; normal batch exit codes and interrupt cancellation apply.

In ImGui, open Project batch with Ctrl+Alt+B, select **Pixel streams**, and pin
the active model with **Use active pixel model**. If no model is pinned, Run
captures the active pixel model. Configure block size, prediction tile size,
memory allowance and minimum vote, then select scope and reuse/failure policy.
The image inspector can train or load the active model. Scripted desktop callers
can supply the same `ProjectBatchOptions` through `GuiOptions.batch_options`.

Outputs remain historical data. Reuse does not publish contours or predictions
into the live project. Stream stitching/contour publication, packaging/copy
management, unmatched-grid resampling, research/QuPath comparisons, TMA and
hardware/frame-budget acceptance remain separate open work.


## Local performance and cross-platform probe

Two synthetic embedded RGB images with one target each use the portable fixture
model, 128-pixel blocks and 64-pixel prediction tiles. At 1024² per image, fresh
execution takes 1521.5 ms and verified resume 391.2 ms (3.89× observed). At 2048²,
fresh takes 6406.6 ms and resume 1536.4 ms (4.17×). Resume infers zero blocks and
includes input serialization, the source scan, the canonical checkpoint checksum
and complete QPS verification. Every block's input/numeric values are compared
against direct window inference and all output/source project bytes stay
unchanged. These are single shared-workstation runs, not hardware thresholds.

The capture estimate grows from 6,459,620 to 25,333,988 bytes with the embedded
source images. The conservative producer work estimate stays 1,352,748 bytes;
maximum array storage per block is 147,456 bytes. Source, metadata and caller
ownership are separate from that producer estimate. See the retained
[1024](benchmarks/project-pixel-batch-1024-windows.json) and
[2048](benchmarks/project-pixel-batch-2048-windows.json) records and
`tools/benchmark_project_pixel_batch.py` for the reproducible probe.

The native Linux executable exports and reuses the Windows-created 1024² source
and model. Both generated checkpoint projects reopen on Windows; every one of
128 blocks has identical numeric/input hashes and label/valid/vote arrays.
Platform provenance can change the enclosing file commitment. Compiler/platform
parameter differences intentionally prevent cross-environment checkpoint reuse;
opening historical output remains portable. Real Windows Ctrl+Break and Linux
SIGINT probes interrupt after observing a QPS temporary file, exit 130, preserve
source/model bytes and leave no incomplete bundle or stream output.
