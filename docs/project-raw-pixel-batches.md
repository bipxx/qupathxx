# Project raw-classifier batches

`ProjectBatchKind.RAW_PIXELS` / native `RawPixels` exports a pinned raw classifier
across selected area objects and images. Each completed image has a checkpoint
project, one QWS1 file per target and a class-count CSV. The input project stays
unchanged. `RAW` / `--kind raw` continues to measure raw channel statistics;
`PIXELS` continues to classify RGB display inputs.

```python
import qupathxx as q

project = q.Project.open("input.qpx")
options = q.ProjectBatchOptions()
options.kind = q.ProjectBatchKind.RAW_PIXELS
options.raw_pixel_model = q.load_raw_pixel_classifier("model.qrm")
stream = q.RawPixelStreamOptions()
stream.tile_width, stream.tile_height = 64, 64
stream.work_bytes = 64 * 1024**2
options.raw_pixels = stream
options.raw_pixel_file_work_bytes = 32 * 1024**2
options.pixel_file_limit = 1024**3
options.pixel_total_limit = 4 * 1024**3

request = q.ProjectBatchRequest(
    project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), options=options)
first = q.run_project_batch(request, "batch-output")
resumed = q.run_project_batch(request, "batch-output")
for image in resumed.images:
    print(image.state, image.error, image.inferred_blocks,
          image.hashed_blocks, image.verified_blocks)
```

The stain argument is required by the shared batch constructor but does not
transform raw input. The model owns dtype/channel order, channel normalization,
Z/time/series and feature/calibration policy. Training uses the separate
[raw CLI](raw-pixel-cli.md) or Python workflow.

Empty selection means matching area annotations in every image. Select another
object kind with `object_kind`, or supply `ProjectBatchSelection` image/object
IDs. Targets are area objects; omission does not create a whole-image annotation.
Each output traverses the declared global lattice and masks its target ROI.
Several ROIs therefore repeat traversal; sparse ROI prediction planning remains
an optimization opportunity, with no whole-slide performance claim.

## Exact lattice compatibility

Omitted `raw_pixel_x`/`raw_pixel_y` use one level-zero sample per source pixel.
Different image sizes are compatible with a unit-spacing model when their dtype,
channels and other recipe properties agree. Native axis limits still apply.
For a sampled model, set both axes explicitly to `ScientificGridAxis` values:

```python
options.raw_pixel_x = q.ScientificGridAxis(0, 80000, 4000, 4)
options.raw_pixel_y = q.ScientificGridAxis(0, 60000, 3000, 4)
```

Each axis keeps its origin, extent, sample count and context. The exact rational
extent/sample spacing must match the model on each image. An explicit axis keeps
its declared coverage even on a differently sized image; it does not silently
stretch to cover that image. Unsupported readers, channel layouts, calibration
or lattice/model combinations fail that image before quantitative decoding.
`ProjectBatchFailure.STOP` stops on the first image failure; `CONTINUE` reports
all image outcomes. Validation errors in the batch options reject capture.

## Native CLI

```text
qupathxx_batch input.qpx batch-output --kind raw-pixels --raw-pixel-model model.qrm --image 1 --object 10 --raw-pixel-tile-width 64 --raw-pixel-tile-height 64 --raw-pixel-work-bytes 67108864
```

Use `--raw-pixel-x-axis O,E,N,C` and `--raw-pixel-y-axis O,E,N,C` for explicit
axes. Other controls are `--raw-pixel-file-work-bytes`,
`--raw-pixel-max-samples`, `--raw-pixel-max-windows` and `--raw-pixel-min-vote`.
The existing `--pixel-file-bytes` / `--pixel-total-bytes` fields deliberately
supply shared RGB/raw output quotas. `--continue-on-error`, `--no-resume`,
selection, snapshot/materialization/result/CSV/checkpoint limits and structured
JSONL/exit conventions remain those of the existing batch executable.

## Completion, reuse and ownership

The existing output lock and owned `pixels-TOKEN` bundle apply. QWS1 outputs are
named `object-ID.qws`; `result.qpxproj` marks completed image publication. Native
writers and the existing database seal protect atomic publication and metadata.
Raw QWL1 and RGB QSL1 descriptors coexist in the same stream table. Typed reads
select the right tag and preserve inherited records without a schema migration.

Reuse scans the current raw source inputs without forest inference, then fully
verifies every external stream, descriptor, model/request identity, expected
bundle path, completion and combined input digest. Missing/tampered files or
checkpoints reject; metadata-only project reopen does not grant reuse authority.
`inferred_blocks`, `hashed_blocks`, `verified_blocks` and `stream_bytes` report
these stages. Hashing/verification still performs proportional IO. With
`resume=False`, a new owned bundle preserves previous output bytes.

Interruption retains completed images for restart. Unfinished current-image
outputs are owned work, cleaned only under the existing ownership rules; unknown
files are preserved. This is completed-image reuse. Per-file prefix resume is
provided separately by the raw CLI/file jobs. Moving a complete bundle preserves
relative output links; original image sources must still resolve separately.
Removing a retained record does not delete its external output.

`ProjectBatchJob` uses the same frozen project snapshot and background runner.
Later live annotation edits do not rewrite that snapshot. Workers materialize
isolated documents; results never publish into the live project. Keep reader and
job shutdown on the established lifecycle. No automatic abandoned-work cleanup,
streamed contour stitching or new live apply authority is introduced here.

[Local evidence](benchmarks/raw-pixel-batch-local.json) binds `c2c0fa0`, 377 compiler
inputs and the installed Windows cp312 training/TIFF/project-enabled,
GUI/GEOS/OpenSlide-disabled profile: 136 Python passes, all 14 new batch cases,
four native suites and explicit profile skips. A backend-OFF native suite and
CLI save/reuse of an actual trained model pass, with 27 numeric blocks matching
the ON installed consumer. Typing analyzes three valid files and rejects seven
errors in two invalid files. This is local engineering evidence; broader
installed platforms, desktop, T5 and scientific/resource/release gates stay open.
Readers and required codecs must satisfy the open-source admission policy.


## Streamed contour projects

Set `ProjectBatchOptions.kind = ProjectBatchKind.RAW_PIXEL_CONTOURS` to save
annotations instead of external QWS1 prediction files. Supply the same
`raw_pixel_model`, optional paired `raw_pixel_x`/`raw_pixel_y` and `raw_pixels`
producer settings. `raw_contours` holds the streamed boundary/geometry limits;
`raw_contour_prefix` defaults to `Raw pixel` and
`raw_contour_publication_bytes` defaults to 64 MiB. GEOS is required. Both axes
retain exact global phase and must match the trained model recipe.

Each selected area is traced with the shared native stream job kernel and
published into an independent per-image document. All selected areas must
finish before the output project is committed. The displayed/input project
is unchanged. Result `objects` counts selected source areas; `records` counts
generated annotations and may be zero. CSV rows identify each output annotation,
its source parent, class/name and level-zero area/perimeter. `inferred_blocks`
counts predicted windows; hash-only reuse increments `hashed_blocks` without
re-running the forest. No QWS1 files or new historical apply authority are made.

```text
qupathxx_batch input.qpx output-directory --kind raw-contours --raw-pixel-model model.qrm --raw-pixel-x-axis 0,80000,4000,4 --raw-pixel-y-axis 0,60000,3000,4 --raw-contour-prefix Tissue
```

Prediction settings use the existing `--raw-pixel-*` flags. Contour settings
use `--raw-contour-work-bytes`, `--raw-contour-total-work-bytes`,
`--raw-contour-boundary-work-bytes`, `--raw-contour-max-boundary-runs`,
`--raw-contour-max-components`, `--raw-contour-max-edges`,
`--raw-contour-max-vertices`, `--raw-contour-publication-bytes`,
`--raw-contour-prefix` and `--raw-contour-no-clip`. Common result/materialization/
CSV/checkpoint budgets also apply. Limits are conservative admission estimates,
not measured process RSS. Stop/Continue and `--no-resume` retain their existing
meaning. Cancellation preserves previously committed images and discards the
unfinished image; rerunning verifies and reuses completed checkpoints.

The ImGui batch Workflow selector includes **Raw streamed contours** and
**Raw prediction files**. Load a pinned QRM model, enter both axes or leave both
empty for unit sampling, select image/area scope and run through the existing
batch controls. Advanced memory controls are under **Raw batch limits**.
The Python `GuiOptions.batch_options` preset uses exactly the same native options.

Reuse checks the current model/parameters, source input commitments and the
existing sealed project-data checksum, including geometry and CSV. Changed
inputs recompute; changed settings select a distinct checkpoint scope. Modified
checkpoint data rejects. File versions are checked again before final output
publication. A checkpoint is historical output, not a live document capture.
