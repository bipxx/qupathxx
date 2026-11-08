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
