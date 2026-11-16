# Native region labels, spots and offline masks

This source checkpoint adds bounded 2D SLIC regions, per-cell subcellular spots
and offline unsigned TIFF label import to the native library and `qupathxx_batch`.
All three use one captured request/result boundary for source checks, rational
coordinates, raw statistics, publication, project records and exports. Python is
optional; these commands neither import Python nor execute an external model.
The [local evidence](benchmarks/label-analysis/README.md) states the tested scope.
The local development executable is
`build/gui-aggregation/gap-closure/cleanup/runtime/qupathxx_batch.exe`. The regular CMake
`qupathxx_batch` target includes these commands; this workflow requires enabled
GEOS, TIFF and project capabilities. Python remains optional.

This is a native/CLI workflow. The published candidate9 desktop, SDK and wheels
remain frozen. New desktop controls, Python bindings, a packaged release,
biological validation and whole-slide execution are outside this checkpoint.
Live service integration was excluded from this label checkpoint. The later
[public OMERO client](omero-integration.md) is a separate owner-selected outcome.

## SLIC regions

Select an existing area object in a native project:

```powershell
qupathxx_batch --slic study.qpx regions-new.qpx --image 1 --roi 1 --region-size 32 --iterations 10 --compactness 10 --color-space srgb-lab
```

The native implementation uses deterministic local color/spatial clustering and
four-connected cleanup. `region-size` and spatial compactness use level-zero
processing pixels. `srgb-lab` explicitly assumes sRGB-encoded input;
`encoded-rgb` clusters the decoded byte components directly. This does not apply
an ICC transform or establish colorimetric equivalence with QuPath.

Only fully opaque decoded RGBA8 samples whose centers lie in the area participate.
Statistics are explicitly labeled `decoded-device-RGB8`, distinct from original
quantitative channel measurements. Output detections are children of the selected
area. The independent implementation follows the published SLIC algorithm
description; no EPFL code, binaries or evaluation-only assets were admitted.

## Per-cell subcellular spots

Use a quantitative image, an area and an explicit cohort of cells inside it:

```powershell
qupathxx_batch --subcellular study.qpx spots-new.qpx --image 1 --roi 1 --cell 2 --cell 3 --first-channel 0 --channels 3 --channel 0 --z 0 --time 0 --threshold 10000 --min-pixels 1 --max-pixels 10000 --connectivity 8
```

`channel` is relative to the selected raw channel block. A spot is a connected
component strictly above the threshold, restricted to one parent cell. Adjacent
cells never merge. Selected cells must be fully inside the area, have compatible
planes and contain processing centers; overlapping parent masks refuse.

Optional `--sigma-x/--sigma-y` and wider
`--background-sigma-x/--background-sigma-y` enable the existing native ImageJ
float32 Gaussian or difference-of-Gaussian primitive. Sigmas are in processing
pixels and use nearest-edge extension of this captured crop. Filtered processing
requires a wholly valid, finite crop. Unfiltered processing reports excluded
invalid/nonfinite detection samples. Small and oversized components are rejected
and reported; there is no estimated count for an unresolved cluster.

Measurements always use original raw samples, with missing and nonfinite values
kept distinct from zero. Unsafe integer-to-binary64 intensity conversion refuses.
Every selected parent receives a spot count and spot-pixel count, including zero.
Child detections and parent counts publish in one native undo command. Existing
count fields, locked parents or stale captures refuse before any change.

## Offline label masks

First obtain the exact native input identity and its crop/plane/channel/type
description:

```powershell
qupathxx_batch --label-input study.qpx --image 1 --roi 1 --first-channel 0 --channels 3 --z 0 --time 0
```

Submit that exact level-zero raw crop to an independently selected producer.
The digest binds the crop, ROI, channel block, plane, raw metadata, calibration
and decoded raw values. It is not a SHA-256 of a rendered screenshot or just a
hash of the producer's TIFF. The producer must preserve the declared crop lattice.

Import its result with explicit producer/version, model or recipe artifact
SHA-256, and the input digest returned above:

```powershell
qupathxx_batch --import-label-mask study.qpx imported-new.qpx --image 1 --roi 1 --channels 3 --mask labels.tif --producer MY_PRODUCER --producer-version VERSION --model-sha256 MODEL_OR_RECIPE_SHA256 --input-sha256 NATIVE_INPUT_SHA256
```

The adapter reuses the admitted native TIFF reader. It accepts exactly one
UInt8/UInt16/UInt32 channel, series, Z and time plane, matching the processing crop
dimensions. Zero is background. Sparse positive IDs are normalized to dense
native labels, with exact original IDs preserved in objects and CSV. Disconnected
parts of one external ID remain one multipart object. Foreground outside the ROI,
missing pixels, signed/float/probability masks and guessed resize/affine/plane
conversions refuse. Published geometry is clipped through the shared contour
authority; raw measurements use the original image, never the mask's display.

The producer/model identifiers are explicit provenance assertions; importing a
mask does not independently prove which software generated it. No model runtime,
weights, vendor adapter or automatic download is included. This is a generic
offline file contract, not qualified Cellpose/StarDist version compatibility.
Local inputs do not become admitted dependencies, fixtures or package assets;
the [material policy](licensing/restricted-materials.md) still applies.

## Keep, replay and export

Creation keeps a native record and publishes the detections/counts into a new
project. `--record-only` keeps the record without publishing objects or counts.
Existing project/output files and protected source paths are never replaced.

```powershell
qupathxx_batch --list-labels regions-new.qpx --image 1
qupathxx_batch --verify-labels regions-new.qpx --image 1 --record RUN_SHA256
qupathxx_batch --export-labels regions-new.qpx measurements-new.csv --image 1 --record RUN_SHA256
qupathxx_batch --export-labels regions-new.qpx labels-new.qpl --image 1 --record RUN_SHA256 --format raster
qupathxx_batch --export-labels spots-new.qpx counts-new.csv --image 1 --record RUN_SHA256 --format counts
```

Records store settings and exact input/result/raster identities. Opening a project
does not run analysis. Explicit verification and export recompute the saved recipe
and require all three identities to agree; they do not silently relax to numerical
closeness. Original images and, for imported results, original mask files must
remain available and unchanged for that replay. Current captures also verify file
versions before publication and export. Project relocation with the same admitted
image data does not inherit a previous process's source-currentness token.

CSV includes the measurement domain, native and original label IDs, parent IDs,
sample counts and optional original-sample statistics. Missing statistics are
blank, not zero. Counts CSV includes every selected parent. QPL is the canonical
checked native local label raster; it does not infer a world frame. Retain the
project's ROI/grid/plane association when using that raster elsewhere.

The C++ APIs in `label_workflow.hpp` also provide an owned asynchronous job with
cancel/stale states and undoable record/publication removal. Removal refuses
user-edited geometry/facts/counts and added descendants rather than discarding
them. User metadata and the original absence of parent properties are preserved.

Default work is256MiB (maximum1GiB), with independent kernel, membership, region
and contour limits. `--work-bytes`, `--max-regions`, `--save-snapshot-bytes` and
`--project-bytes` control creation; exports accept `--csv-bytes`. SLIC additionally
accepts `--max-assignment-tests`. The existing default contour cap is10000
components. These are bounded crop/workspace contracts, not measured total process
or whole-slide resource acceptance. JSON reports expose rejected components and
invalid/nonfinite detection samples. See `--help` for the complete command syntax.
