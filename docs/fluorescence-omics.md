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

## Native marker rules

[marker_rules.hpp](../cpp/include/qupathxx/marker_rules.hpp) evaluates versioned
marker gates over intact saved F01 cell observations. It does not reread image
pixels or recompute intensities after geometry edits. Initial capture uses the
existing open quantitative-source authority; closed/unsupported sources refuse.
Rules explicitly select `original-channel-samples` (the default) or
`qpx.dark-flat-corrected-samples/1`. RGB display/optical-density or corrected values
are never silently interpreted as original samples. Thresholds refer to the named
value domain; normalization does not transfer a biological cutoff automatically.

A gate identifies an original channel, nucleus/cell/cytoplasm compartment,
mean/sum/minimum/maximum/sample-SD statistic, strict/inclusive comparison, finite
threshold and minimum finite sample count. Units are `SourceSample` for intensity
statistics or `SourceSampleSum` for sums; incompatible units refuse. These are
original sample values, without a hidden micrometre/area normalization or a
universal biological positivity threshold.

Absent/nonfinite values, an unmeasured channel, insufficient finite samples and
empty cytoplasm are **Unknown**. Sample SD requires at least two finite samples.
Actual finite zero remains a measured value. AND is Negative if any input is
Negative, otherwise Unknown if any input is Unknown; OR is Positive if any input
is Positive, otherwise Unknown if any input is Unknown. Every rule has distinct
positive/negative/unknown output class names.

For example, a native two-marker phenotype can be constructed without a forest
or Python:

```cpp
qpx::MarkerRule rule;
rule.name = "A and B";
qpx::MarkerGate a; a.name = "A"; a.channel = 1; a.threshold = 250;
qpx::MarkerGate b; b.name = "B"; b.channel = 2; b.threshold = 400;
rule.gates = {a, b};
rule.nodes = {{qpx::MarkerOperation::Gate, 0, {}},
              {qpx::MarkerOperation::Gate, 1, {}},
              {qpx::MarkerOperation::And, 0, {0, 1}}};
auto saved = qpx::keep_marker_rule(document, rule);
qpx::MarkerRuleRequest request(document, cell_ids, rule);
auto preview = qpx::preview_marker_rule(request);
auto unknown_ids = preview->cohort(qpx::MarkerTruth::Unknown);
qpx::apply_marker_rule(document, request, *preview);
```

Thresholds in this example illustrate construction, not calibrated biological
cutoffs. Nodes form a topological DAG with the last node as root; all nodes/gates
must be used. Limits are64 gates,256 nodes,1024 edges and100000 selected cells.
Capture/evaluation/publication work is bounded by a declared4..512MiB budget
(default64MiB), separate from existing reader caches and document history.
Geometry-integrity hashing also checks its transient workspace before allocation.

Requests capture only selected scalar observations, stable IDs/revisions and
native identity. Workers never read a live Document. Results own row IDs/root
states, flat row-major gate states, per-gate/root counts, cohorts and exact input/
result hashes; there are no Python callbacks or per-object binding loops. Capture,
preview and publication accept cancellation. Publication requires the exact live
request/result capture ticket; an equivalent hash from another capture is not
permission to apply it.

`apply_marker_rule` reuses the native compound class edit: one Undo restores the
cohort, geometry/raw observations remain unchanged, and existing class policy
clears explicit object colors/probabilities while retaining lock state. Selected
object edits invalidate old previews. Cosmetic labels/classes are excluded from
the scientific observation digest, so a fresh replay can reproduce the same
analysis identity after classification or rename. These are classifications of
saved observations, not a claim that the original image has been remeasured.

`keep_marker_rule`, `marker_rule_ids`, `load_marker_rule` and `remove_marker_rule`
store the exact typed `qpx.marker-rule/1` recipe once in native image properties.
They preserve unrelated metadata and double bit patterns; malformed schema/units/
graphs or changed stored identities refuse. Keep/remove have their own Undo actions;
keeping a rule does not invalidate unchanged observation previews. Load the saved
rule and create a fresh request to replay. Classifications and rules survive
normal native project Save/reopen. The native CLI is available below; desktop
rule editing/preview controls remain pending, so F02 stays unchecked.

### Marker rule files and native CLI

[marker_rule_io.hpp](../cpp/include/qupathxx/marker_rule_io.hpp) supplies native
JSON parsing/formatting, stable file load/new-file save and result reporting.
It uses the existing optional project/JSON capability (`has_marker_rule_io()`),
without Python. JSON is a human representation of the typed rule; use these
functions to export it rather than treating raw project metadata as the file
format. Duplicate/unknown keys, wrong enum names/types, invalid graphs and
nonfinite thresholds refuse. Input is limited to1MiB and nesting depth16.

A complete single-marker file looks like:

```json
{
  "schema": "qpx.marker-rule/1",
  "name": "DNA example",
  "domain": "original-channel-samples",
  "positive_class": "Example high",
  "negative_class": "Example low",
  "unknown_class": "Example unknown",
  "gates": [{
    "name": "DNA", "channel": 0, "compartment": "nucleus",
    "statistic": "mean", "comparison": "gt", "units": "source-sample",
    "threshold": 1000.0, "minimum_finite_samples": 1
  }],
  "nodes": [{"operation": "gate", "gate": 0}]
}
```

This is a threshold-construction example, not a biological cutoff. Compartments
are `nucleus`, `cell`, `cytoplasm`; statistics `mean`, `sum`, `min`, `max`,
`sample_sd`; comparisons `gt`, `ge`, `lt`, `le`. Sums require
`source-sample-sum`; other statistics require `source-sample`. AND/OR nodes use
`{"operation":"and","inputs":[0,1]}` or `or`, referencing earlier nodes.
The last node is the root. All rule/output/gate fields are explicit.

```text
qupathxx_batch --preview-markers cells.qpx --image 1 --rule-file rule.json --all-cells
qupathxx_batch --apply-markers cells.qpx classified.qpx --image 1 --rule-file rule.json --all-cells
qupathxx_batch --list-marker-rules classified.qpx --image 1
qupathxx_batch --preview-markers classified.qpx --image 1 --rule RULE_SHA256 --all-cells
```

Choose exactly one `--rule-file` or saved `--rule`, and exactly one `--all-cells`
or repeated `--cell ID`. `--all-cells` uses `marker_cell_ids` to select stamped
native fluorescence cells in the rule's explicit domain across **all planes in
that image**, excluding unrelated cell types and known opposite-domain cells. Corrupt matching publications stay selected so capture reports the
problem instead of silently dropping them. Preview reads saved observations and
writes nothing. JSONL reports input/rule/result identities, root/gate counts,
positive/negative/unknown cohorts and output class names.

Apply retains the exact rule once and writes a **new** native project; it never
overwrites the original, rule file, image/companion or protected journal path.
The owner commit guard uses `verify_marker_rule_publication`: expected class
labels and unchanged observation content must match, while revisions changed by
Apply itself are allowed. This historical check does not authorize applying an
old preview to edited data. Normal preview/apply stale checks still require the
exact current request/result capture. No duplicate scalar table or pixel re-read
is needed for the final publication check.

`--work-bytes` controls the marker request; apply additionally accepts
`--save-snapshot-bytes` and `--project-bytes`. Ctrl+C uses native cancellation;
exit codes are0 success,1 runtime/data failure,2 command-line errors,130 cancelled.
Normal project opening retains its own reader work; marker evaluation does not
remeasure pixels. Saved-rule preview creates fresh authority and reproduces the
scientific result for the same observations/cohort.

Desktop marker controls are reached through **Fluorescence cells → Marker rules**.
Choose all native fluorescence cells in the loaded rule's domain (across image
planes) or the selected cell,
then Preview markers. Counts distinguish Positive, Negative and Unknown; expand
Assignments and cohorts to inspect object IDs and proposed classes. Apply marker
classes uses the native compound class command; normal project Undo restores the
previous classes. Keep marker rule is a separate undoable action; Apply alone
does not retain a recipe. Save the project to persist retained rules and classes.

Expand Load and retain rules to load native rule JSON or a retained rule. To
replay after reopening, load the saved rule, select the same cohort and Preview.
Expand Edit marker rule for original channel, compartment, statistic, comparator,
threshold, finite-sample minimum and output classes. Loaded nested graphs survive
individual gate edits. Combine-all or add/remove explicitly replaces the topology
with the labeled AND/OR combination; inspect Rule graph before using custom rules.
These are literal threshold assignments, not validated biological phenotypes.

Preview evaluates saved observations without remeasuring pixels. Input changes
discard stale authority, and publication disables while editing is blocked.
Applied results remain visible while observations/classes are unchanged; Undo or
an incompatible edit invalidates them. Cancellation never publishes classes.


## Native dark/flat foundation

`qupathxx/channel_source.hpp` defines `ChannelSource`: immutable quantitative
metadata, a value domain and observed-source-version identity, bounded block IO
and explicit currentness checks. `original_channel_source(server)` adapts native
raw IO without requiring a TIFF display view. This additive identity does not
replace legacy TIFF/F01 hashes. File identities record observed versions, not
full-file pixel fingerprints; in-memory source identities are process-local.

`dark_flat_channel_source(signal, dark, flat, DarkFlatOptions{scales, minimum})`
creates an immutable Float64 source in `qpx.dark-flat-corrected-samples/1`.
The three inputs must be original-sample sources with identical spatial/C/Z/T
axes, channel names and calibration. Scales explicitly name one positive finite
normalization value per original channel. Initial correction reads level-zero
blocks; no implicit field broadcasting, channel matching or tile-local statistics.

For each valid channel, evaluation is `(signal-dark)/(flat-dark)*scale`, with
division before multiplication. Denominators must be finite and strictly greater
than the nonnegative minimum. Nonfinite inputs, invalid denominators and overflow
produce NaN for the affected channel. Input validity masks are intersected for
missing pixels; a valid zero remains a measured zero and signed outputs survive.
Integer inputs outside +/-2^53 refuse double conversion. Original samples are
never rewritten. The pure `correct_dark_flat` function uses the same kernel.

`read_work_bytes` reports the accounted native snapshot/buffer peak, excluding
existing reader/OS caches. `read` rejects insufficient work before pixel decoding,
reads bounded owned blocks, and checks source metadata/file versions before and
afterward. Source and correction identities include exact normalization, source
versions and algorithm domain. They are recipe/source identities, not a promise
of an evaluated result archive. Cancellation throws without publishing a result.

This foundation has analytical and synthetic-TIFF native tests. Retained recipe,
CLI and explicit corrected cell analysis routes are described below. Correction desktop controls are described below. Existing original-sample measurements and rules
keep their original domain unless explicitly changed.


## Retained dark/flat recipes and CLI

`qupathxx/dark_flat_workflow.hpp` adds an immutable `DarkFlatRequest` over a
native signal document and explicit dark/flat readers. Its source can be read on
a worker without reading Document. Owner controls check document/source/series/
calibration identity; Keep rechecks actual source versions at publication.
`read_dark_flat_block` returns owned corrected samples, region, source identity,
exact block digest and finite/nonfinite counts. It does not publish cell objects
or replace any original-sample measurements.

Keep stores a typed `qpx.dark-flat-recipe/1` once in image properties, preserving
source locators, reader/series, observed source-version identities and exact options.
Keep and Remove each use one normal metadata Undo command and retain unrelated
properties. Remove refuses while cells or retained fluorescence run records still
reference a correction recipe. Save the project to persist them. The record identity differs from
its corrected-source identity; a block identity also includes selected region,
channels, plane, validity and actual corrected double values.

Initial automatic replay supports native TIFF sources. It opens the selected
field series and refuses changed/missing sources or mismatched derived identities;
there is no implicit relink, fallback decoder or recalibration. The source/kernel
capability itself remains reader-independent. Correction compares intrinsic raw
source calibration across fields; document calibration is an owner capture guard.
Source identities describe observed versions, not full-file content fingerprints.

```text
qupathxx_batch --keep-dark-flat input.qpx corrected.qpx --image 1 --dark dark.tif --flat flat.tif --scales 100,200
qupathxx_batch --list-dark-flat corrected.qpx --image 1
qupathxx_batch --verify-dark-flat corrected.qpx --image 1 --record RECORD_SHA256
qupathxx_batch --preview-dark-flat corrected.qpx --image 1 --record RECORD_SHA256 --width 256 --height 256 --channels 2
```

Scales above are illustrative source-sample normalization values, not estimated
from a tile. Keep accepts `--minimum-denominator`, explicit `--dark-series` and
`--flat-series`, and save budgets `--save-snapshot-bytes`/`--project-bytes`.
The signal series comes from the selected project image. Preview requires width
and height; X/Y, first channel, Z/time default0, channels default1. Select them
with `--x`, `--y`, `--first-channel`, `--channels`, `--z`, `--time`; bound work with
`--work-bytes`. It reports native block identity and counts, not a second frontend
calculation or an exported image. Verify checks the saved recipe/source association;
it does not scan a whole image or claim biological accuracy.

Keep saves a new project only, protecting original project, image/field and
SQLite journal/WAL/SHM paths. Preview/list/verify do not modify project or source
bytes. Native cancellation returns130; runtime/data errors1, invalid options2,
success0. Correction recipes do not automatically select a derived domain for
existing fluorescence tools or marker rules. Correction desktop controls and explicit native/CLI derived analysis are described below.


## Explicit corrected fluorescence analysis

The additive `FluorescenceCellRequest(document, correction, region, ...)` overload
consumes a `DarkFlatRequest`. Keep its correction recipe first; a cell/run stores
its reference once, with the field associations retained in image properties.
The existing segmentation, compartment reducer, contours, jobs, publication and
Undo services operate on corrected Float64 samples. The default constructor
continues to use original samples. Choose thresholds in the selected domain.

Corrected runs expose `measurement_domain()` and `correction_record()`. Their
source-input and serialized cell recipe use version2, and published cells use
`qpx.fluorescence-cell-publication/2` with an explicit corrected domain/reference.
Original run hashes, version1 recipes and publication metadata stay unchanged.
Geometry/observations remain integrity-checked, without adding an RGB feature link.
The existing retained-run APIs also preserve zero-cell corrected outcomes.

```text
qupathxx_batch --detect-fluorescence corrected.qpx cells.qpx --image 1 --correction CORRECTION_SHA256 --channels 2 --method explicit --threshold 90 --context 0 --expansion 1.5
qupathxx_batch --list-fluorescence cells.qpx --image 1
qupathxx_batch --verify-fluorescence cells.qpx --image 1 --record FLUORESCENCE_RECORD_SHA256
```

These numbers illustrate the synthetic field test, not a marker-independent
biological threshold. Detection requires a new output project. The CLI reports
measurement domain and correction reference; its output guard also protects field
paths. Saved corrected cell/run replay reopens the retained correction association
and refuses changed/missing sources or a removed/corrupt reference.

Marker rules use the same saved observation authority in either named domain.
`marker_cell_ids` retains its original-domain default;
`marker_cell_ids_for_domain` explicitly selects corrected observations. The CLI's
`--all-cells` uses the rule's domain. The desktop displays the loaded rule's actual
domain and uses the same native cohort helper. An explicit wrong-domain cell
refuses capture. Historical marker evaluation does not reread correction fields
or remeasure pixels. No automatic threshold conversion is performed.

The desktop workflow below makes corrected analysis an explicit choice; original
samples remain the default. This is local engineering delivery, not a new release
or biological validation.


## Dark/flat desktop workflow

1. Open **Fluorescence cells → Dark/flat correction**. Expand Correction fields
   and normalization; enter dark/flat TIFF paths and explicit series selections.
   Fields must match the signal's declared spatial/channel/Z/T axes and intrinsic
   calibration. No broadcasting, channel guessing or resizing occurs.
2. Choose each original channel index and its positive source-sample scale, and
   the minimum denominator. Initial scales are1. Reset normalization to source
   channels explicitly resets all scales to1; no field mean is estimated.
3. Load correction fields. Opening runs on a native worker; owner capture waits
   while editing is blocked. Set a preview crop/channel block/Z/T, then Preview
   corrected samples. Inspect the actual domain, coordinates, validity/finite
   counts and pixel values, including negatives. The optional grayscale preview
   samples at most320 pixels per axis; its range changes presentation only.
   Nonfinite channels are magenta and absent source pixels transparent.
4. Keep correction recipe, then Use for fluorescence. The fluorescence tool
   names the selected correction and enables its explicit corrected-source mode.
   Set thresholds in corrected units; Run, inspect compartments, Keep cells or a
   run record and save the project. Original samples and older observations are
   unchanged. Cell and metadata Undo/Redo update their displayed counts/labels.
5. To reopen, select the saved fluorescence record and Replay for exact analysis
   verification. To author a changed corrected run, load the saved correction in
   its tool and Use it first. A referenced correction cannot be removed. Missing
   or changed fields produce errors; they do not silently become original data.
6. Marker rule editing offers Original samples or Dark/flat corrected samples.
   Preview/Apply uses only the rule's native domain cohort. Disable Use selected
   dark/flat correction in the fluorescence tool to return to original samples;
   choose an appropriate original-domain threshold explicitly.

Keep/Remove and cell/class publication use the existing native Undo authorities.
Cancel discards pending worker delivery; replacement invalidates captured identity.
Field opening/preview do not publish measurements, and action controls disable
while their owner is blocked. Python is optional and supplies no correction logic.
The selected F03 desktop workflow is delivered under the documented TIFF/level-zero
subset; its evidence is in the feature-expansion receipt.


## Native linear unmixing (F04 delivered)

`LinearMixingMatrix` declares ordered measured channel indices/names, ordered
component names, row-major dimensionless coefficients and the input domain. The
orientation is **measured = matrix * components**. The initial contract permits
1..64 components, at most64 measured channels, and full column rank. A native
`LinearUnmixModel` prepares one thin SVD and refuses deficient or excessively
conditioned matrices under the explicit policy (default maximum condition1e8).
Enable `QUPATHXX_BUILD_LINEAR_ALGEBRA` with the admitted pinned OpenCV core SDK;
the existing classifier-training option also enables that available core. Python
and classifier training are not required by the algebra option.

`linear_unmix_channel_source` consumes original or explicitly dark/flat-corrected
samples with exact channel-name/index binding. Its Float64 output contains the
components first, then signed `residual:<input name>` channels in matrix-row order,
with residual = measured - reconstructed. Nothing is clamped to zero. Invalid
pixels stay invalid; a nonfinite required input makes the coupled outputs unknown.
Unrepresentable component values are NaN, with unknown residuals. Integer values
beyond exact binary64 admission refuse conversion. Level-zero geometry, Z/T and
calibration are retained; output domain is `qpx.linear-unmixed-samples/1`.

Capture a prepared model once, reuse it across native block reads, and choose the
output channel subset explicitly. Contiguous input groups avoid a gather copy;
sparse groups read no channel gaps. Source work admission includes input, output,
model and scratch storage before pixel reads. Cancellation and upstream source
guards apply. `LinearMixingMatrix::to_value/from_value` supplies strict versioned
native typed records, preserving coefficient bits (including signed zero), order,
labels and condition policy without requiring the numerical backend.

`LinearUnmixRequest` captures the document and either its original source or an
explicit, already retained `DarkFlatRequest`. Keep/Remove use one metadata Undo
command. Typed recipes retain signal path/series/source identity, matrix, input
and derived source identities, algorithm and prepared operator identity. Corrected
recipes reference their parent correction, which cannot be removed while used.
Replay requires the exact stored operator/source identities; changed inputs do
not fall back to original samples. Initial automatic reopening is native TIFF.

The prepared operator identity records matrix, pinned backend/algorithm, inverse
coefficient bits and condition once per model. Source identity is now explicitly
`qpx.linear-unmixed-source/2`; the output measurement domain stays
`qpx.linear-unmixed-samples/1`. Historical foundation receipts using source schema1
precede retained project support. Original TIFF/F03 identities remain unchanged.

Native derived cell/marker integration and desktop controls are described below. No estimated spectra, pretrained model, new
external material or nonlinear unmixing is included.


## Retaining and verifying unmixing with the native CLI

Matrix files use the same strict `qpx.linear-mixing-matrix/1` schema as native
typed records. For a two-channel image whose actual channel names are C0/C1:

```json
{
  "schema": "qpx.linear-mixing-matrix/1",
  "orientation": "measured=matrix*components",
  "input_domain": "original-channel-samples",
  "inputs": [{"channel": 0, "name": "C0"}, {"channel": 1, "name": "C1"}],
  "components": ["A", "B"],
  "coefficients": [1.0, 0.25, 0.5, 1.0],
  "maximum_condition": 100000000.0
}
```

Use the image's exact names and zero-based indices; no name guessing occurs.
Coefficients are flattened by measured-channel row. Native JSON IO bounds bytes,
nesting/nodes and dimensions, refuses duplicate/unknown fields and nonfinite or
invalid values, and preserves binary64/signed-zero round trips. Small integer
coefficients convert exactly; integer literals beyond2^53 refuse rather than
silently rounding. Supply an explicit floating representation for that intended
binary64 value. `load/save_linear_mixing_matrix_file` use guarded native IO and
new-file-only atomic export; no Python parser or numerical runtime is involved.

```text
qupathxx_batch --keep-unmix input.qpx unmixed.qpx --image 1 --matrix mixing.json
qupathxx_batch --list-unmix unmixed.qpx --image 1
qupathxx_batch --verify-unmix unmixed.qpx --image 1 --record RECIPE_SHA256
```

For corrected input, use matrix input_domain
`qpx.dark-flat-corrected-samples/1` and add `--correction-record CORRECTION_SHA256`
to Keep. The correction must already be retained in the input project. Both the
parent selection and matrix domain are explicit; neither is inferred or rewritten.
Keep saves a new project, preserving the input and its existing records. Optional
`--save-snapshot-bytes` and `--project-bytes` bound save resources. Output guards
protect the project and its SQLite sidecars, linked images, selected correction
fields and matrix input. Existing outputs are never overwritten.

Verify restores the saved source associations and prepares/verifies the exact
operator; it does not claim a new biological evaluation or read every source pixel.
JSON reports include record, matrix/operator/source identities, algorithm, domain
and parent reference. The matrix is retained by value, so later replay does not
require the authoring JSON file. Missing/changed source fields, incompatible domains
and stale identities refuse. Cancellation returns130.


## Native unmixed cell measurements and marker rules

Pass an explicitly retained `LinearUnmixRequest` to `FluorescenceCellRequest`.
The selected first channel/count and segmentation channel refer to the derived
component/residual axes. The segmentation index is relative to that output block;
published C indices are absolute derived output indices. Component names are
stored alongside measurements. Source authority remains bound to the original
measured image and the retained unmixing recipe. ROI planes must use c=-1 because
a measured-channel-specific ROI has no automatic component meaning. Z/T, geometry,
calibration, context, work limits and source/currentness guards remain explicit.

Signed Float64 output flows through the same segmentation, compartment reduction,
contours, native jobs and atomic publication as original/corrected sources. No
component is relabeled as an original-channel value. Version3 cell/run recipes
retain `unmix_record`; the measurement domain is `qpx.linear-unmixed-samples/1`
and published profile is `qpx.fluorescence-cell-publication/3`. Missing/changed
recipes refuse replay and stale pending results cannot publish. Cell/run records
and retained marker rules prevent deleting their unmix recipe; that recipe in
turn protects a referenced correction. Original/corrected recipes remain versions1/2.

```text
qupathxx_batch --detect-fluorescence input.qpx cells.qpx --image 1 --roi 1 --unmix UNMIX_SHA256 --channels 2 --threshold 100.5
qupathxx_batch --verify-fluorescence cells.qpx --image 1 --record RUN_RECORD_SHA256
```

`--unmix` and `--correction` are mutually exclusive. A retained unmix recipe may
already include a correction; do not select it twice. Thresholds use the selected
component units. Native matrix/SVD arithmetic can differ slightly at a threshold
exactly on a sample value; choose scientifically intended thresholds rather than
assuming mathematically equivalent transforms make boundary decisions identical.

An unmixed `MarkerRule` uses domain `qpx.linear-unmixed-samples/1` and requires
`unmix_record` to select exactly one retained recipe. Version2 typed/JSON rules
add that field; legacy original/corrected rules retain version1 encoding and hashes.
Gate C indices refer to that recipe's derived outputs. `marker_cell_ids_for_rule`
selects its cohort, using the caller's4..512MiB work budget for temporary integrity
checks. Intact other recipes are excluded; corrupt/unknown matching-domain facts
remain for explicit capture refusal. Missing scalar observations stay Unknown.
Marker capture verifies stored facts and associations without rereading pixels.

Set the JSON schema to `qpx.marker-rule/2`, domain as above and add
`"unmix_record": "UNMIX_SHA256"` to the existing marker-rule fields. Native marker
CLI `--all-cells` uses the recipe-scoped cohort. Marker Preview/Apply/Undo and
retained rule replay use the existing scalar evaluator/classification commands.
A matching index from another matrix/order is never accepted as the same component.
Desktop matrix/selection and marker-scope controls use these same native services;
the selected F04 consumer workflow is delivered under the declared subset.


## Desktop linear unmixing

1. Open **Fluorescence cells → Linear unmixing**. Load an explicit mixing-matrix
   JSON file, or expand **Edit explicit mixing matrix**, choose row/component
   dimensions and create a zero draft. Enter the known coefficients, exact measured
   channel names/indices, component names, input domain and condition limit. A zero
   or deficient matrix refuses Apply; the tool estimates no spectra.
2. For a corrected matrix domain, explicitly choose a retained input correction.
   Original-domain matrices use None/original input. Apply the draft after edits.
   JSON parsing, matrix factorization and correction field opening run on a native
   worker; document capture waits for its owner to be unblocked. Matrix coefficients
   and the prepared operator are reused by preview, analysis and saved replay.
3. Expand **Preview crop and outputs** to choose level-zero crop, output subset
   and Z/T. Preview unmixed samples, select a component/residual and inspect signed
   values, validity and finite/nonfinite counts. Derived output indices are distinct
   from measured channel indices. Display lower/upper affect the preview only;
   nonfinite values are magenta and absent pixels transparent.
4. Keep the unmixing recipe, then Use unmixing for fluorescence. Set the cell tool's
   output block, segmentation channel and threshold in component units. Run, inspect
   compartments, Keep cells/run and save the project. Cell and metadata Undo/Redo
   update displayed counts and retained labels. Select original or corrected mode
   explicitly to return to those sources; original samples stay the default.
5. After reopen, Replay the saved fluorescence record. Unmixed replay opens and
   prepares the retained source asynchronously before native owner capture and
   the usual cell job. It verifies exact saved identity and does not silently
   select original data if a field or recipe is missing or changed. Cancel and
   document replacement discard pending owner delivery.
6. Marker editing offers Original, Dark/flat corrected and Unmixed domains. The
   unmixed domain requires a retained unmixing recipe scope; select it before
   setting derived output gate indices. Native Preview/Apply uses only the scoped
   cohort. Changing domains clears the recipe scope. Missing observations remain
   Unknown and wrong component recipes refuse capture.

Load saved unmixing reuses its retained coefficients and source associations.
Remove/Keep and marker/cell publication use existing native Undo commands. A
referenced unmix recipe or parent correction cannot be removed. Ordinary drawing
does not hash/parse all recipes or refactor matrices; immutable-property caches
refresh on changes and reconcile own writes/Undo. Python is optional.

F04 covers explicit full-column-rank linear systems, signed components/residuals
and the documented native-TIFF saved-source subset. Automatic spectrum fitting,
nonlinear/learned unmixing and model downloads are outside this contract. The
feature-expansion receipt records the source-scoped numerical and desktop checks.


## Native disk-backed expression foundation

`qupathxx/expression_matrix.hpp` provides import_expression_matrix and
open_expression_matrix in native project/SQLite builds. Import preserves exact
uint64 counts, stable unique IDs and explicit zeros in a new-only indexed sidecar.
Choose Qpx headered single-column axes or the explicit Tenx headerless features/
barcodes profile. Positions require a separate association, never an inferred
join by catalogue order. Duplicate coordinates refuse rather than sum.

Retain the immutable ExpressionMatrixSource after opening, supplying an expected
file digest when known. Page catalogues, resolve exact IDs and read explicit
feature indices with an optional sorted observation cohort. Null cohort means all;
empty cohort returns no values. Sparse offsets group requested features in order,
using global observation indices; absent entries are measured zeros. Counts never
pass through double. Reads verify selected gene chunks, not other genes or the
original text. Set work/file/store bounds and native cancellation. Changed files
and SQLite writer sidecars refuse queries. Exact relocation retains identity.

The full retained mouse-brain source passes native import/query/reopen and exact
independent six-gene checks; see the feature-expansion receipt. This is a native
foundation: project-owned/relative associations, relink, backed import UI and gene
display remain pending D01. Old selected-gene projects require no migration.
Python exposure waits for the settled API batch. No proprietary/NC material added.
