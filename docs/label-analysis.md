# Native region labels, spots and offline masks

This source checkpoint adds bounded 2D SLIC regions, per-cell subcellular spots
and offline unsigned TIFF/PNG label import to the native library and `qupathxx_batch`.
All three use one captured request/result boundary for source checks, rational
coordinates, raw statistics, publication, project records and exports. Python is
optional; these commands neither import Python nor execute an external model.
The [local evidence](benchmarks/label-analysis/README.md) states the tested scope.
The local development executable is
`build/gui-aggregation/gap-closure/png-labels/runtime/qupathxx_batch.exe`. The regular CMake
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

For a project with saved image-source verification, the CLI checks the
linked image before reporting a new crop digest or starting label analysis.
Review and adopt a changed image before generating a mask for it.

First obtain the exact native input identity and its crop/plane/channel/type
description:

```powershell
qupathxx_batch --label-input study.qpx --image 1 --roi 1 --first-channel 0 --channels 3 --z 0 --time 0
```

Submit that exact level-zero raw crop to an independently selected producer.
The digest binds the crop, ROI, channel block, plane, raw metadata, calibration
and decoded raw values. It is not a SHA-256 of a rendered screenshot or just a
hash of the producer's mask file. The producer must preserve the declared crop lattice.

Import its result with explicit producer/version, model or recipe artifact
SHA-256, and the input digest returned above:

```powershell
qupathxx_batch --import-label-mask study.qpx imported-new.qpx --image 1 --roi 1 --channels 3 --mask labels.tif --producer MY_PRODUCER --producer-version VERSION --model-sha256 MODEL_OR_RECIPE_SHA256 --input-sha256 NATIVE_INPUT_SHA256
```

The adapter reuses the admitted native TIFF reader. It accepts exactly one
UInt8/UInt16/UInt32 channel, series, Z and time plane, matching the processing crop
dimensions. `--mask-format tiff` is the default, preserving existing TIFF recipes.
Plain one-bit TIFF masks retain their stored codes as UInt8. Code 0 is background
and code 1 is foreground for either black-and-white display polarity.
Select PNG explicitly and configure its admitted native provider before the command:

```powershell
qupathxx_batch --reader-plugins C:/ABSOLUTE/reader-plugins.json --import-label-mask study.qpx png-new.qpx --image 1 --roi 1 --channels 3 --mask labels.png --mask-format png --producer MY_PRODUCER --producer-version VERSION --model-sha256 MODEL_OR_RECIPE_SHA256 --input-sha256 NATIVE_INPUT_SHA256
```

If the project image also uses a separately installed reader, pass one
`--reader-plugins ABS_MANIFEST` per pack before the command in either
`qupathxx_batch` or `qupathxx_labels`.

PNG uses `org.qupathxx.png` under `png-static-native-samples/1`, with no parser
options. Its one grayscale channel retains exact 1/2/4/8-bit source codes as UInt8
and 16-bit source codes as UInt16, including 65535. For an indexed PNG whose
stored indices are the intended IDs, pass `--mask-format png-indexed`. This
profile reads exact 1/2/4/8-bit indices as UInt8, including 255. Index zero is
background. Palette colors and transparency do not change the indices or
merge labels. The grayscale and indexed profiles each require the matching PNG
color type. RGB and explicit alpha-channel planes refuse. All profiles require
the exact crop dimensions and one series/Z/time plane; no resizing or
registration is inferred. Opening reserves a bounded decoder allowance plus host
metadata within remaining work, and releases the full PNG decoder before label
normalization/retention. PNG decoding never falls back to TIFF or another reader.

Zero is background. Sparse positive IDs are normalized to dense
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
PNG records additionally bind their explicit format and exact reader implementation
(provider/version/contract/options, module and dependency hashes). Verification and
export need the same configured provider; changed or missing decoders refuse even
if another decoder could produce the same numbers. Use the same leading
`--reader-plugins ABS_MANIFEST` on `--verify-labels` and `--export-labels` for PNG
records. Legacy TIFF records retain their original parameters and exact identities.

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

## Open NGFF label masks

The later selected NGFF profile extends native label import through the optional
`org.qupathxx.ngff-labels` provider, contract `ngff04-zarr2-integral-labels/1`.
It reads an explicitly selected original level-zero OME-NGFF 0.4/Zarr 2 label plane
using the matching native decoder. The thin `qupathxx_labels` executable calls
the same C++ label authorities used by `qupathxx_batch`; it adds no Python or
model runtime. [Local native evidence](benchmarks/ngff-labels/README.md) records
candidate2's exact raw property text, integer-fill validation, native/CLI/SDK
workflow, matching source/package/ZIP and installed run/removal checks.
Candidate1 remains separately preserved under its own source/binary identity.

Obtain the target input SHA-256 from the existing image, ROI and raw channel/plane
selection, then import with those same choices. This example explicitly attaches
a 2D mask to reference/target Z plane 1:

```powershell
qupathxx_labels --reader-plugins C:/ABSOLUTE/ngff-label-reader-plugins.json --label-input study.qpx --image 1 --roi 1 --channels 3 --z 1

qupathxx_labels --reader-plugins C:/ABSOLUTE/ngff-label-reader-plugins.json --import-ngff-label-mask study.qpx ngff-new.qpx --image 1 --roi 1 --mask-directory C:/ABSOLUTE/labels.zarr --label-group labels/cells --reference-group . --origin authored --author MY_NAME --input-sha256 NATIVE_INPUT_SHA256 --channels 3 --z 1 --reference-z 1 --max-regions 100000

qupathxx_labels --reader-plugins C:/ABSOLUTE/ngff-label-reader-plugins.json --verify-labels ngff-new.qpx --image 1 --record RUN_SHA256
qupathxx_labels --reader-plugins C:/ABSOLUTE/ngff-label-reader-plugins.json --export-labels ngff-new.qpx ngff-new.csv --image 1 --record RUN_SHA256 --format csv
qupathxx_labels --reader-plugins C:/ABSOLUTE/ngff-label-reader-plugins.json --export-labels ngff-new.qpx ngff-new.qplm --image 1 --record RUN_SHA256 --format raster
```

`--origin` and `--reference-group` are required explicit declarations. Here `.`
names the store root; `image-label.source.image: "../../"`, or its recorded
normative default for `labels/cells`, must resolve to that declared group within
the same store. An explicit source-reference path is retained separately from
whether it was defaulted. External/escaping references and remote URLs refuse.
`--label-z`/`--label-time` select source indices;
`--reference-z`/`--reference-time` declare reference indices, which must equal
target `--z`/`--time`. An absent label Z/T axis requires source index 0 and uses
the caller's explicit reference-plane declaration; no plane is inferred.

Authored masks use `--origin authored --author NAME`, with producer/version/model
fields empty. For a producer result, use `--origin producer`,
`--producer NAME --producer-version VERSION --model-sha256 EXACT_MODEL_OR_RECIPE_SHA256`,
with no author field. Both routes require the native `--input-sha256`; these
identities are provenance assertions, and importing never executes that model.

The selected label lattice accepts UInt8/16/32/64 and Int8/16/32/64. Exact IDs
remain integer values, including negative IDs, INT64_MIN, UINT64_MAX and adjacent
values above 2^53. Zero is background. Each distinct nonzero source ID maps to one
dense native label; disconnected parts remain one multipart instance and holes
remain background. Source properties and declared RGBA remain metadata, without
automatic class mapping. Measurements use original target-image raw channels.
Uncompressed, zlib and gzip chunks and partial edge chunks are supported by the
selected native profile; floating/probability masks refuse. Label-array
`fill_value` is an in-range JSON integer or `null`. Floating fill tokens,
including `0.0`, and implicit decimal/integer fill conversion refuse. Null fill
does not supply background for missing chunks.

The reference full image dimensions must match the selected target image. Native
capture supplies the ROI crop, channel block, calibration and target raw-input
identity; the complete crop must fit the label lattice. XY source/reference units
must both be absent or explicitly convertible, with equal sampling and an integer
origin offset. Present reference Z/T axes also require matching target native
spacing/time calibration and coincident selected physical coordinates. An absent
reference axis does not invent calibration or a new plane. Unit conversions use
the admitted micrometre/second vocabulary; scale/calibration comparisons use
relative tolerance `1e-12` and spatial/plane alignment uses `1e-9` reference-index
tolerance. Original metadata, integer codes, decoder/model identities and saved
replay associations remain exact. Resizing, automatic registration, guessed units
and foreground outside the selected ROI refuse.

Keeping retains original label/reference attribute text, source-code maps,
selection and full-store/decoder identities in native ImageData properties.
Publishing retains original IDs, properties, RGBA metadata and attribution on
objects, in one undo operation. CSV writes exact original decimal IDs and source
metadata alongside raw measurements. For NGFF runs, `--format raster` writes a
QPLM1 bundle containing the dense raster, exact source codes/description, target
frame/plane and identity context. Its wire format is distinct from the existing
QPL1 raster; old TIFF/PNG records, CSV and QPL1 identity contracts are unchanged.

Original property objects retain their raw JSON text through description,
publication, saved/replayed metadata, CSV quoting and QPLM1. This preserves
whitespace, member order and finite unknown numeric token spellings, including
integers beyond semantic label-ID ranges and long decimals. These property
tokens remain opaque text; exact label IDs, plane indices, calibration and
transforms keep their separate native validation contracts.

Replay, keep, publication and exports check observed membership and native file
versions throughout the mask store, including non-primary chunk additions,
deletions and changes. Project save/save_new, CSV and QPLM1 outputs anywhere
inside that store are protected, and
existing outputs are preserved. Opening saved metadata and saving a project
elsewhere work without the provider; replay/export require the exact captured
decoder and unchanged source. Configure the manifest on each new CLI process.

Each original attribute file is bounded to 24576 UTF-8 bytes and the complete
neutral description to 65536 bytes. Exact source-code map text is bounded to 1 MiB;
the existing 1 MiB global ImageData-properties limit still applies to all retained
records and metadata together. Full metadata refuses rather than truncating.
The native default work budget is 256 MiB, maximum 1 GiB, with bounded decoding,
inventory, normalization, contour and persistence storage. These checks do not
claim total-process or whole-slide resource acceptance.

The SDK add-on contract is package `qpx_ngff_labels`, target
`qupathxx::ngff_labels`, above the matching frozen SDK; example sources are
at `examples/ngff_label_plugin`. Candidate2 native, standalone and public SDK
checks pass, including exact raw properties, strict integer/null fills and the
selected host built-in-decoder-OFF route. The local handoff admits all364 package/
ZIP members, checks the relocated SDK configuration and installed CLI/SDK routes,
and preserves33 user files after owned-member removal. No new frozen wheel/binding/UI, general NGFF/Zarr3, broader
R15, biological, speed or platform acceptance follows from this selected result.
