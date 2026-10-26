# Native raw classifier commands

`qupathxx_batch`, packaged beside the Python extension, exposes the same native
raw training/checkpoint and prediction/file workflows as Python. It does not
require Python to run. Use a project with a quantitative TIFF source and at
least two labelled area classes. Training requires the optional OpenCV backend;
prediction and historical verification use the portable model.

## Train and retain

```text
qupathxx_batch --train-raw-pixels input.qpx 1 model.qrm --checkpoint training.qtc --archive training.qtt --channels 2 --x-axis 0,80000,4000,4 --y-axis 0,60000,3000,4 --output-project trained.qpx
```

Positionals are project, positive image ID and model destination. Checkpoint and
archive destinations are required. `--object ID` may repeat to select labelled
areas; omission selects all labelled area objects. Unlabelled/non-area objects
are not training labels. The native sampler owns overlap handling and balancing.

Provide both axes as `origin,extent,samples,context`, in level-zero coordinates.
The exact rational spacing is extent/samples; context is a sample count. Omitted
axes mean the unit level-zero lattice, within native axis limits (at most one
million samples per axis). Larger images need explicit admitted axes. No sampled
spacing is silently rounded or adapted to a model.

Training flags select `--first-channel`, `--channels`, `--z`, `--time`, comma-
separated `--offsets`/`--scales`, paired `--sigmas X,Y,...`, `--gradient`,
`--laplacian` and `--physical-sigmas`. Defaults are explicit identity-normalized
intensity features. Native recipe validation governs channel counts, units,
calibration and sufficient context. The project's TIFF source determines series;
RGB display controls do not define quantitative values.

Sampling supports `--max-samples`, `--samples-per-class`,
`--max-scanned-samples`, `--unbalanced` and `--seed`. Forest settings are
`--trees`, `--max-depth`, `--min-samples`, `--active-features` and
`--forest-work-bytes`. Record effective model parameters for byte-identical
retraining, including the admitted forest work budget.

A QTC1 prefix is historical progress. For example, add `--max-new-windows 1`,
then run the same command with `--resume training.qtc --replace-existing` to
finish. A partial run exits 1 with state `partial` and a null model identity;
it publishes neither model nor training archive. Complete sampling fits the
forest, writes QRM1/QTT1 and optionally retains the training record in a new
project. Resume repeats prefix reads/features; forest fitting itself is not
checkpointed.

## Predict, resume and verify

```text
qupathxx_batch --export-raw-pixels input.qpx 1 model.qrm prediction.qws --x-axis 0,80000,4000,4 --y-axis 0,60000,3000,4 --tile-width 64 --tile-height 64 --max-new-windows 1
qupathxx_batch --export-raw-pixels input.qpx 1 model.qrm prediction.qws --x-axis 0,80000,4000,4 --y-axis 0,60000,3000,4 --tile-width 64 --tile-height 64 --resume prediction.qws --replace-existing --output-project predicted.qpx
qupathxx_batch --verify-raw-pixels prediction.qws
```

Prediction positionals are project, image ID, model and QWS1 output. The model
owns channel/feature/normalization/plane selection. Both axes must match its
recipe spacing and calibration policy. `--object ID` restricts prediction to one
area; `--region X,Y,W,H` restricts traversal in global sample indices. Additional
controls are `--min-vote` (tree vote fraction), `--max-samples`,
`--tile-width`, `--tile-height` and `--max-windows`.

Resume validates the previous prefix and hashes its current source inputs before
inferring the suffix. Changed model, geometry, policy or source rejects without
replacing the earlier prefix. Retained header and block bytes stay exact. A
fresh process records its own provenance clock/generation, so independent fresh
files can differ in bytes while numeric blocks and input/result chains agree.

Verification checks the complete saved prefix, including blocks and commitments.
A valid partial file reports `verified` with `summary.complete=false`; this does
not claim whole-slide completion or live document/apply authority. Optional
`--output-project` retains a verified QWL1 link in a new project. It currently
performs an additional bounded full-file verification after the save job.

## Stream contours into a new project

```text
qupathxx_batch --contour-raw-pixels input.qpx 1 model.qrm contours.qpx --x-axis 0,80000,4000,4 --y-axis 0,60000,3000,4 --tile-width 64 --tile-height 64 --object 7 --name-prefix Tissue
```

Positionals are the input project, image ID, trained model and a new project
path. The operation consumes prediction tiles without retaining the full label
raster, joins boundaries through the shared GEOS kernel, applies all resulting
annotations atomically in the privately opened document and commits the new
project. The input project and model remain unchanged. Both axes must match
the model; `--region X,Y,W,H` uses global sample indices. `--object ID` selects
one area; default geometry clips to that area and image bounds. `--no-clip`
removes ROI geometry clipping while retaining sample-center masking and image
bounds. This can extend contour cells beyond the selected ROI boundary.

The command accepts `--work-bytes`, `--tile-width`, `--tile-height`,
`--max-windows`, `--max-samples` and `--min-vote` for prediction. Contour limits
are `--contour-work-bytes`, `--boundary-work-bytes`, `--max-boundary-runs`,
`--max-components`, `--max-edges`, `--max-vertices` and `--total-work-bytes`.
Publication and project staging use `--publication-work-bytes`,
`--snapshot-bytes` and `--project-bytes`. These are conservative admission
budgets, not a process-RSS promise. GEOS is required for this command.

The destination must be absent, including project sidecars and aliases to any
input. `--resume`, `--replace-existing`, `--file-bytes`, `--max-new-windows` and
`--output-project` do not apply. Cancellation or quota failure before commit
leaves no destination project. Captured model/project/reader versions are
rechecked at publication and immediately before the atomic save commit.

JSONL schema `qpx.raw-pixel-contours/1` reports running/terminal state and source
fingerprint. A saved result includes model/request/input/prediction commitments,
completed windows, sample/class counts, geometry counts and new object IDs.
Only a successful atomic save reports `saved`. Historical QWS1 files are a
separate workflow and cannot authorize this live operation.

## Budgets, publication and reporting

`--work-bytes` controls the sampler/features or prediction producer.
`--checkpoint-bytes`/`--checkpoint-work-bytes` control training checkpoint
metadata independently; `--file-bytes`/`--file-work-bytes` control prediction file
admission independently. Native limits validate every value. Buffers and job
slots stay bounded; outputs are not collected into a whole-slide raster.

The input project is preserved. Guarded destinations cannot alias project data,
reader files, the input model or one another. Existing output replacement is
explicit and validated by the native format writer. Output projects must always
be new files. Model, archive, checkpoint and project commits are independent;
a later error or cancellation does not remove a successfully committed
historical output. Ctrl+C/Ctrl+Break cancels and joins work before readers release.

Stdout is JSONL with `qpx.raw-pixel-train/1`, `qpx.raw-pixel-export/1` or
`qpx.raw-pixel-verify/1` (and `qpx.raw-pixel-contours/1` above), source fingerprint, state, error and native commitments.
Argument errors use the existing batch-error stderr record. Exit codes are 0
success, 1 failure/partial, 2 invalid arguments and 130 cancellation. A verified
prefix may be partial; inspect its completion field. Existing `--kind raw`
continues to mean raw measurements; see [raw project batches](project-raw-pixel-batches.md)
for multi-image raw classification. Streamed contour batches remain planned.

[Local evidence](benchmarks/raw-pixel-cli-local.json) binds source `f463110`,
376 compiler inputs, 104 installed Python passes (14 new CLI cases), three
native suites and a separate backend-OFF refusal witness. Windows CPython 3.12,
training/TIFF/project ON and GUI/GEOS/OpenSlide OFF is the installed profile.
The fresh wheel consumer reuses test dependencies; this does not qualify clean
machines, other platforms, desktop or biological/resource/release acceptance.
Only open-source readers and required codecs are eligible.


[Streamed contour CLI local evidence](benchmarks/raw-stream-boundaries-local/cli-246/receipt.json)
verifies installed runtime `4e91d1e`: all 18 new cases, nine native suites and
240 installed Python passes/three declared skips, generated stubs and typing.
The Windows cp312 profile enables GUI/GEOS/training/TIFF/project; OpenSlide is
OFF. Complete SDK identities and packaged notices match. This is local
engineering acceptance with reused test dependencies; project contour batches,
historical QWS1 conversion and full platform/scientific/resource/release gates
remain open.
