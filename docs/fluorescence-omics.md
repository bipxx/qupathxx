# Native fluorescence and spatial-count workflows

These are two bounded C++ workflows using open formats. Python is optional.
Fluorescence reuses the quantitative TIFF/raw measurement pipeline; spatial counts
have a native Matrix Market/TSV reader and project importer. See the
[scope/checklist](fluorescence-omics-plan.md) and [verification](benchmarks/fluorescence-omics/README.md).

## Build

From a standalone source checkout:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release -DQUPATHXX_BUILD_TIFF=ON -DQUPATHXX_BUILD_TESTS=ON
cmake --build build/native --config Release --target qupathxx_batch qupathxx_region qupathxx_spatial_counts qupathxx_fluorescence_tests qupathxx_spatial_counts_tests
ctest --test-dir build/native -C Release --output-on-failure --no-tests=error -R '^(fluorescence_workflow|spatial_counts|spatial_counts_cli)$'
```

Python defaults OFF. Project storage defaults ON and is required for the importer.
TIFF is required for fluorescence; the count reader itself needs no HDF5, Zarr,
GEOS, OpenCV or Python dependency. GUI viewing additionally requires
`QUPATHXX_BUILD_GUI=ON` and the `qupathxx_app` target. For a full installation,
build all default targets, then use `cmake --install ... --prefix ...`; see
[build requirements](native-build.md). Visual Studio places executables under
`build/native/Release/` with `.exe`; single-configuration builds use `build/native/`.

## Fluorescence: measure original channel values

Open a supported OME-TIFF through the quantitative TIFF reader, create the desired
ROIs in its project and save it. Choose a specific image, channel range and Z/T
plane. For example, three channels on Z1/time0:

```sh
qupathxx_batch fluorescence.qpx new-measurements --kind raw --image 1 --raw-channel 0 --raw-channels 3 --raw-z 1 --raw-time 0 --raw-explicit-axes
```

Image/object IDs are project IDs. Channel, Z and time indices are zero-based;
channels form a contiguous range. Add `--object ID` to select a specific ROI,
`--nucleus` for an existing cell nucleus, and `--raw-max-pixels N` for a tighter
per-ROI admission limit. The existing batch output contains per-image projects,
CSV/checkpoint reports and its normal resume/reuse semantics; choose a new output
directory for a new run. It does not overwrite the input project.

Measurements are finite/nonfinite counts, means, minima and maxima on original
typed samples. Channel windows, LUTs and gamma are presentation controls and do
not alter those values. Calibration is preserved when known and stays unknown
otherwise. Reopen the saved project in the native viewer to inspect retained raw
records and their provenance/replay status. Do not use optical density or
brightfield stain deconvolution for this fluorescence recipe.

The new engineering check covers lossless uint16/float32 OME-TIFF, three named
channels and two Z planes, with an all-zero channel, values above255 and float
NaN/infinity. It verifies explicit axes, raw values, display independence,
CSV/save/replay, batch and stale/no-overwrite cases. It does not establish
fluorescence cell-detector accuracy, spectral unmixing, compensation, positivity
thresholds or general multiplex/vendor compatibility.

## Spatial counts: import selected genes at explicit positions

Prepare three **uncompressed** open text files following this association profile.
Matrix rows correspond exactly to feature-table order; columns correspond exactly
to position-table order. Positions are already registered level-zero image pixels.

`counts.mtx`:

```text
%%MatrixMarket matrix coordinate integer general
2 3 3
1 1 5
2 1 2
1 2 7
```

`features.tsv` has a header and one feature ID per line:

```text
feature_id
G1
G2
```

`positions.tsv` has tab-separated fields; these lines show actual tab separators:

```text
observation_id	x_px	y_px
A	1.5	2
B	3	4.5
Zero	0	0
```

Import selected stable feature IDs onto image1 of an existing project:

```sh
qupathxx_spatial_counts input.qpx new-counts.qpx 1 counts.mtx features.tsv positions.tsv --gene G1 --gene G2
qupathxx_app --project new-counts.qpx
```

The importer adds point detections named by observation ID. Existing objects stay
present. Select an imported detection in the viewer and expand **Imported properties
(historical)** to inspect `omics.count:G1`, `omics.count:G2`, observation IDs and
SHA256 digests of the three input snapshots. Counts are imported facts; moving
geometry does not recompute them. `--z N --time N` chooses a concrete zero-based
plane (default0/0); switch to that plane in the viewer to see the points.

Sparse omissions mean zero, so the Zero observation above retains two explicit
zero measurements. Selected counts up to2^53 inclusive persist exactly in the
existing double-valued measurements; larger selected values refuse. Parsing keeps
unsigned64-bit counts internally. The output embeds the selected-gene projection,
not the full source matrix. Keep the original input files for further genes.

Native validation rejects duplicate IDs/sparse coordinates, negative or malformed
counts, out-of-range indices, wrong dimensions, missing/extra records, unsupported
headers, absent selected genes, nonfinite/out-of-image positions and invalid
planes. Units, coordinate transforms and cell assignments are never guessed.
Files must be regular, bounded inputs; captured bytes are owned and hashed.
Existing output paths refuse, and new project publication is atomic. Cancellation
before publication leaves no new output project; CLI cancellation returns130.

Default limits:32MiB per input file,128MiB accounted reader/projection work,
100,000 features,10,000 observations,1,000,000 encoded entries,64 selected genes
and4,096 bytes per line. The first applicable limit wins; dimension limits do not
promise that every combination fits the work budget. CLI byte budgets are
`--file-bytes N --work-bytes N`; C++ callers can also set dimension/entry limits.
The full feature-by-observation matrix is never densified.

The C++ API is [spatial_counts.hpp](../cpp/include/qupathxx/spatial_counts.hpp):
`read_spatial_counts` provides a selected-gene projection; `import_spatial_counts`
uses the same reader and existing native object/project publication services.
No Python binding or separate GUI import dialog is required for this CLI import
and native-viewer inspection flow.

H5AD, OME-Zarr, direct vendor-platform imports, automated registration, molecular
normalization/clustering and differential expression are outside this delivered
slice. Format, reader/codec and fixture licensing follow the
[open-format admission policy](reader-license-policy.md).

## Spatial expression and landmark alignment

Analyze → Gene expression selects imported genes and displays linear/log1p colors
with an explicit legend. Comparison views share navigation and can share the
display range. These are display transforms; original counts are unchanged.
Analyze → Import spatial expression provides a native Matrix Market/TSV preview
and publication wizard; the selected-gene projection remains its initial storage
route. Full disk-backed matrices are tracked separately in the expansion plan.

Analyze → Landmark registration opens a separate alignment canvas for an imported
expression source. Choose the source explicitly when multiple cohorts exist.
Add landmark pairs and enter source/target coordinates in pixels, or select the
source-circle/target-square drag mode to move a handle over the image. Right drag
pans and the wheel zooms. Cyan circles are original source landmarks, orange
squares are target landmarks, and fitted expression positions appear in green.

Choose translation (at least one training pair), similarity (two distinct source
positions), or affine (three noncollinear source positions). More well-distributed
pairs permit an overdetermined least-squares fit. Similarity reflection is an
explicit option; axes/reflection are never inferred. Mark independent landmarks
held out to assess their error; they never affect fitting. No held-out landmarks
means no held-out error estimate, rather than a reported zero.

Fit preview reports per-pair residual distance, training RMS and held-out RMS in
target pixels. A changed pair invalidates the old preview. Accept registration
stores the definition, original pairs and exact fit evidence in one native undo
command. Save the project to retain it; Load saved landmarks uses the entered
registration name and restores stored coefficients without refitting. Stale
expression/image/calibration inputs refuse acceptance or reuse. Native Undo/Redo
restores the whole registration state together.

Accepted registration places gene overlays in both the main and comparison views.
Raw object geometry and source counts remain unchanged. Generic object outlines,
selection and other analyses still use their own declared coordinate domain;
this alignment does not silently move source objects or change ROI memberships.
The current UI uses source/image pixels; the native coordinate-frame API also
supports explicitly calibrated pixel/micrometre conversions.

Native callers use [landmark_registration.hpp](../cpp/include/qupathxx/landmark_registration.hpp):
`fit_landmarks` returns owned immutable coefficients/residuals and held-out
statistics; `accept_landmark_registration` validates the current request before
atomic publication; `stored_landmark_fit` restores exact evidence;
`active_registration_field` produces mapped positions without rewriting the
dataset. Fitting is bounded to1,024 finite, uniquely named pairs, with a declared
work budget and cancellation. Affine fitting uses normalized Householder QR with
a1e-12 relative rank threshold; transforms retain the coordinate API's finite,
invertible, condition-bounded contract. No Python runtime or new dependency is
required by this service. Nonlinear/automatic registration remains outside this
landmark workflow.

## Fluorescence cell API

The raw detector/reducer, source-bound job, native object publication, saved
recipe/result-identity records, CLI and desktop tool are implemented. F01 passes
its selected pilot contract with the explicit recipe below; this is not universal
fluorescence or biological accuracy.
This is distinct from the delivered raw measurement and import workflows above.

[fluorescence_cell_workflow.hpp](../cpp/include/qupathxx/fluorescence_cell_workflow.hpp)
is the C++ entry point. Construct `FluorescenceCellRequest` on the document owner,
with a level-zero `RawRegionRequest` selecting a contiguous original channel
block and explicit Z/T. An object ID uses that object's area ROI; ID0 uses the
explicit core crop inside the image, with honest source-only provenance. The
segmentation channel is an index within the selected block. TIFF series comes
from the opened native source and is retained in the captured metadata.

`FluorescenceCellOptions` selects strict threshold or Otsu, Gaussian sigma,
distance-peak prominence, nucleus area limits and optional cell expansion in
pixels or micrometres. Physical units require both effective document calibration
axes. Context is requested separately per axis (default32 pixels), clipped at the
source edge and required to cover available Gaussian support. Context may smooth
the ROI but does not expand its eligible object domain. Boundary flags remain
explicit; this is not a whole-slide tile ownership/resume contract.

Call `detect_fluorescence_cells` synchronously for a native recipe, or submit to
`FluorescenceCellJob`: `poll(document)` for UI control and `wait(document)` for a
blocking native consumer. The worker owns immutable native snapshots and performs
one original raw-region read; it never reads a live Document or an RGB projection.
Results retain global region/core geometry, source axes/names, ROI/provenance,
immutable masks/measurements and exact input/result identities. `pixel_center`
maps a local sample index to its level-zero image center.

Owner delivery rejects stale ROI/calibration/source/metadata; cancellation and
destruction join the worker before releasing captured owners. `matches` checks
live document state cheaply; `publication_matches` rechecks actual file versions
and metadata. `require_current_fluorescence_cells` checks both the capture ticket
and publication currentness before a later consumer writes results. Identical
content hashes do not transfer publication authority between requests/documents.
An ROI Undo advances its revision: capture a fresh request for the restored ROI.
Display windows/colors do not change raw scientific result identity.

The accounted work limit covers request snapshots and raw/raster/measurement
buffers, with admission before pixel IO. Reader-owned decode/cache memory and
process/GPU memory are separate; bound TIFF decode/cache through its existing
open options. No Python runtime or binding is needed for these native APIs.

[fluorescence_cell_objects.hpp](../cpp/include/qupathxx/fluorescence_cell_objects.hpp)
adds the native persistence/publication path. `keep_fluorescence_cell_run` retains
the exact options, original selection/context, source-shape/calibration contract,
input/result identities and cell count in existing typed image properties. It
also retains zero-cell outcomes without inventing objects. List, replay, verify
and remove through `fluorescence_cell_record_ids`, `replay_fluorescence_cell_record`,
`verify_fluorescence_cell_record` and `remove_fluorescence_cell_record`. Keep/remove
use native Undo/Redo and preserve unrelated image metadata. Duplicate records
refuse. These compact records store recipes/digests; replay regenerates masks from
the original source rather than embedding another source image or result raster.

`trace_fluorescence_cells` uses the existing optional GEOS contour capability,
typed raw labels and level-zero coordinates. It preserves every label, clips to
the selected ROI/crop and requires each body to cover its nucleus. The shared
cell normalization handles zero-area clipping remnants with exact geometric
predicates; positive-area escape refuses. Its contour budget includes retained
run and prior contour buffers. Detection/recipe retention can be used without
GEOS; native contour publication requires `QUPATHXX_BUILD_GEOS=ON`.

`publish_fluorescence_cells` creates all Cell objects, nucleus geometry and tagged
raw observations in one native undo command. Prepare/currentness/budget errors
leave the study unchanged. Publication supports at most128 selected channels and
the existing per-object property limits. Repeated publication of the same run
under its source parent refuses instead of silently duplicating cells. Z/T and
parent planes come from the capture; the current display plane does not choose
them. No RGB CellFeatureLink is attached to these raw observations.

Saved property names use original channel indices: for example
`fluorescence.nucleus.C0.mean`, `fluorescence.cell.C0.sum` and
`fluorescence.cytoplasm.C1.finite_samples`. Counts are sampled pixels, distinct
from a fractional ROI-clipped geometric area. Undefined intensities and empty
cytoplasm omit their measurement keys; `fluorescence.cytoplasm` records present/
absent and boundary flags remain explicit. Source names, exact recipe and hashes
are retained in the `fluorescence.*` metadata. These are observed capture values;
editing geometry does not silently recompute them.

After save/reopen, `replay_fluorescence_cell` creates a fresh request from an intact
published cell. Detect and verify with `verify_fluorescence_cell_publication`, or
verify a complete cohort through `verify_fluorescence_cell_publications`. The
cohort method checks source currentness once and requires every label exactly
once. It detects incomplete/duplicate groups and changed observations. Cosmetic
names/classes are independent of scientific observation identity. A stored record
or published value is not automatically a current analysis of changed source data.

## Native fluorescence cell commands

The batch executable operates on the image association already saved in a native
project. Its TIFF series comes from that association; create/select the intended
series in the project before detection. Z/T and channel selection are explicit
analysis inputs. These commands require no Python runtime:

```text
qupathxx_batch --detect-fluorescence study.qpx cells.qpx --image 1 --channels 2 --method otsu --min-area 5 --expansion 3
qupathxx_batch --list-fluorescence cells.qpx --image 1
qupathxx_batch --verify-fluorescence cells.qpx --image 1 --record RECORD_SHA256
```

Detection writes a **new** project and refuses existing outputs or aliases of
protected project/image/companion/journal inputs. The source project is unchanged.
Omit an ROI/crop to use the full level-zero image; use `--roi ID` for a saved area
object, or `--x N --y N --width N --height N` for a crop. Do not combine these two
selection modes. Large full-image requests can exceed the declared native work
budget; this command does not yet implement tiled whole-slide processing.

`--first-channel` and `--channels` select an original contiguous channel block;
`--segmentation-channel` is zero-based **within that block**. For example, original
channel3 alone uses `--first-channel 3 --channels 1 --segmentation-channel 0`.
Original indices still identify saved measurements. `--z` and `--time` default to0.
Default selection is one channel starting at0, with local segmentation channel0.

Choose `--method explicit --threshold N` (strict greater-than) or `--method otsu`;
an explicit threshold with Otsu refuses. `--sigma`, `--prominence`, `--min-area`,
`--max-area`, `--expansion` and `--units px|um` map directly to native options.
Physical units require both calibration axes. `--context N` supplies per-side raw
smoothing context (default32, native maximum4096), clipped only at source edges;
insufficient Gaussian support refuses. Boundary-touching cells remain flagged.

By default nonempty results publish native cells and observed compartment facts
using GEOS, then retain a compact run record. `--record-only` keeps the recipe and
result digest without cells or contour tracing, including with GEOS disabled.
Zero-cell runs retain a record without fabricated objects. Record replay verifies
the regenerated raster/measurement digest; it does not audit subsequent edits to
published cell geometry/properties. Use the native publication-verification APIs
for that separate question.

`--work-bytes`, `--max-cells`, `--save-snapshot-bytes` and `--project-bytes` bound
analysis/publication and new-project staging. Ctrl+C cancels through the existing
batch cancellation flag. JSONL stdout reports the record, run digest, detected and
newly published counts, and boundary count. Exit codes:0 success,1 runtime failure,
2 invalid arguments,130 cancellation. No biological accuracy follows from merely
saving or replaying a run. The selected annotated pilot and native consumer
evidence are scoped in the feature-expansion receipt.

## Fluorescence desktop tool

Open **Analyze → Fluorescence cells...** or **Workflow → Tools → Fluorescence
cells...** with a quantitative TIFF image active. Expand **Fluorescence settings**
to choose original channel block, segmentation channel within that block, Z/time,
ROI or explicit crop, threshold, units, expansion and bounded work/context.
The current TIFF series remains the source association; use the existing raw
channel tool to choose/open another series. Without a selected area, turn off
**Use selected ROI** and choose a crop or the whole image.

**Run fluorescence** starts the shared native job; **Cancel fluorescence** cancels
analysis or contour tracing. A ready run supplies nucleus/body preview, detected
and boundary counts, the resolved threshold and **Inspect compartment measurements**.
The preview uses level-zero source coordinates and is hidden if displayed Z/time
is different. At most1024 samples per axis are uploaded for a large preview;
small objects can disappear in that presentation sampling, while analysis masks
and published measurements retain the original lattice.

**Keep fluorescence cells** traces off the UI thread and publishes one native
undoable cohort when the captured source/ROI is still current. It requires GEOS.
Publication waits during project save/load or edit blocking. **Keep fluorescence
run** retains the compact recipe/result identity, also for zero cells and without
GEOS. Preview and analysis alone do not create objects or persist a run.

Choose a saved record to replay/verify or remove it. Removal is undoable. Existing
project Save stores the retained records and published cells. Reopening a project
and replaying uses a fresh native capture against the original associated source;
missing/relinked/changed inputs can refuse rather than silently substitute data.
These controls alone do not establish annotated accuracy. The separate selected
pilot and finite boundary checks pass under their declared contracts; whole-slide
tile ownership/resume remains D02.

The first predeclared annotated BBBC039 pilot passes raw-moment and foreground
checks but fails its object-F1 target (0.683 versus0.70). That failure remains
recorded;130 published/replayed detections alone do not establish correct instance
segmentation. The train-selected follow-up below passes the unchanged gates. See the [first annotated result and next training step](benchmarks/feature-expansion/README.md#f01-first-annotated-panel-result-fails-object-f1).

### Selected F01 pilot recipe

On the admitted BBBC039 DNA panel, a recipe selected from the separately
preselected open training panel uses **Otsu256, sigma2px, prominence0.5px,
minimum nucleus area5px² and cell expansion3px**. The existing native options
express it directly; no product default or hidden model was changed:

```text
qupathxx_batch --detect-fluorescence study.qpx selected-cells.qpx --image 1 --method otsu --sigma 2 --prominence 0.5 --min-area 5 --expansion 3 --units px
```

Use an explicit original channel block/Z/time/ROI for your data. In the desktop
tool, expand Fluorescence settings and enter the same values. Keep sufficient
Gaussian context for a cropped/selected ROI (the default32px covers this sigma);
the full-image pilot needs no outside-source context. Measurements always use
original samples, independent of smoothing/display colors.

The frozen follow-up has118 predictions:82TP/36FP/28FN, F1=0.7193 and Dice=0.9225.
It passes all unchanged pilot gates, including raw moments and split/merge/mean
checks. Native publication and exact saved replay pass for every selected cell;
the actual CLI gives the same run digest. This is one reused validation panel,
with both original failure and training selection disclosed. It is not an unseen
held-out study or a guarantee for other markers, tissues or acquisition settings.
