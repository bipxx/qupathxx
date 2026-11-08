# Quantitative pixel classifier numeric contract

Current consumer status: native T1-T3 and the Windows installed Python workflow
have local receipts in the [delivery plan](api-delivery-plan.md). Official
[raw CLI commands](raw-pixel-cli.md) now have [installed local evidence](benchmarks/raw-pixel-cli-local.json)
at `f463110`. [Multi-image classifier batches](project-raw-pixel-batches.md)
have [installed local evidence](benchmarks/raw-pixel-batch-local.json) at `c2c0fa0`.
ImGui/managed viewer, other installed profiles and integrated qualification remain open. Cross-tile contour
stitching and scientific/resource/release acceptance remain separate gates.

Earlier source-bound numeric baseline: native features/models and bounded owner-captured sampling/prediction
jobs have [local Windows/Linux evidence](benchmarks/raw-pixel-workflows-local.json)
at `7beb80b`. Archive/retention source `440f610` now has a separate
[local receipt](benchmarks/raw-pixel-archives-local.json): the affected suite
passes on both Windows/Linux hosts; ten unchanged regression suites retain
their source220304d results. Failed development rounds remain in the receipt.
Positive backend training/trained archives have [local Windows evidence](benchmarks/raw-pixel-training-local.json)
at `e7e4b66` (three backend-ON suites and one backend-OFF workflow suite).
Native live replay and bounded contour apply/undo have [local evidence](benchmarks/raw-pixel-replay-apply-local.json)
at `92edba8`: four Windows GEOS-ON and four Linux GEOS-OFF suites plus three
Windows training-ON suites. Later rational-window streaming and installed
Python/CLI receipts are recorded above; UI, cross-tile contours and integrated
platform/backend training remain open. RGB `pixel-v1` and QPC1/QPC2 stay unchanged.

The existing path is `PixelTrainingRequest` → `extract_pixel_features` →
float32 training rows → `ForestModel` → `PixelPredictionRequest` /
`PixelPredictionStream` → classifier archive/stream files and checkpoints.
Both the feature extractor and `classify_pixels` currently take display
`PixelBuffer`. The new private `raw_pixel_features.hpp` authority instead
consumes the immutable `ScientificRawGrid`; request capture, model codecs,
source guards and frontends must subsequently compose this authority.

## Numeric recipe

- Use the existing typed, level-zero scientific raw sampler. It rounds exact
  finite weighted means once to binary64 and preserves source dtype/bits,
  selection, calibration and missing-data identity in its input commitment.
  All ten sample types are eligible. Do not convert a 64-bit integer before
  its weighted mean. Classification does not promise to distinguish integers
  that round to the same sampled binary64 or final float32 value.
- Each selected channel requires an explicit finite binary64 offset and
  positive finite scale. Evaluate binary64 `shifted = mean - offset`, then
  `normalized = shifted * scale` as separate operations; no adaptive range,
  clipping, hidden FMA, imputation or image-wide statistics. Reject a nonfinite
  intermediate or `abs(normalized) > FLT_MAX/32`. This declared bound keeps
  centered derivatives within the forest's existing `FLT_MAX/4` domain.
  Choose normalization that fits the scientific task; it is part of the model,
  not a presentation setting. Negative zero recipe parameters are rejected.
- Each scale is a pair `(sigma_x, sigma_y)` in sampled-grid pixels, in `[0,16]`.
  Pairs increase componentwise and cannot repeat; at most eight pairs and 128
  feature columns. A zero component is an identity kernel on that axis.
  Physical sigma must be converted using each axis's validated sampled pixel
  spacing and captured in the later request recipe. Unknown physical
  calibration must reject a physical request. The private kernel infers none.
- For each channel, convolve the valid-value numerator and support using
  Gaussian weights `exp(-d*d/(2*sigma*sigma))`, radius `ceil(3*sigma)`, along
  each axis, then divide by support once. Unsupported source samples have
  zero numerator and support. Missing one channel never erases another.
- Feature eligibility requires valid source support at the center. Smoothing
  never fills an invalid center. Derivatives use smoothed left/right/top/bottom
  values only where that neighbor's original channel support is valid;
  otherwise substitute the center. Gradient is `hypot((R-L)/2,(B-T)/2)`;
  Laplacian is `L+R+T+B-4*C`, in sampled-grid units.
- Order columns by selected channel, scale pair, then Gaussian, optional
  gradient and optional Laplacian. Filter in binary64; round only the final
  features to IEEE binary32 nearest/even. Subnormal/underflow rounding is
  explicit, not a source-zero claim. Invalid features carry NaN plus a
  per-feature mask. A forest row requires every feature to be valid; no
  implicit missing-value training or prediction.

The caller supplies per-axis context of at least `ceil(3*sigma_axis)`, plus
one sample when either derivative is enabled, including unsupported context
outside the image. A streamed block must retain the full-image grid phase;
block edges are not image edges. Native preflight charges raw sampling,
metadata, captured HWC support, feature output, temporary planes and scratch
against the explicit feature work budget before IO. This is allocation
admission, not a process RSS claim. Cancellation publishes no partial result.

## Identity and remaining integration

Feature schema names include dtype, absolute selected channel, offset, scale,
both sigmas and operation. Input identity also includes the sampler's captured
input commitment. Result identity includes feature identity, geometry, support
and canonical float32 bits; invalid NaNs have one hash encoding. The later
model recipe must additionally capture channel labels, series/Z/T selection,
grid policy, calibration/physical units and normalization policy. Prediction
must validate all of those semantics rather than accepting equal column counts.
Do not put a particular training image hash into the reusable model schema.

The existing classifier archive and stream/checkpoint versions must migrate
together; old RGB bytes retain their semantics. Additive native request types
or recipe variants are to be chosen at that consumer integration, not exposed
prematurely. Python/NumPy, CLI/batch, managed viewer and ImGui all call the same
native authority. Save/reopen/resume, stale publication, undo, source/model
mutation and physical-calibration rejection remain required integration gates.

## Independent reference and acceptance

`tools/reference_raw_pixel_features.py` uses stdlib Decimal at 80 digits for
direct two-dimensional normalized convolution, independent of native separable
filter traversal. Integer source values and binary64 affine rounding are
explicit; reference features round to binary32 only at the final boundary.
Its committed generated header is consumed by the native feature suite.
Missing support must match exactly. Finite features require
`abs(actual-reference) <= 4 * ulp32(reference) + 1e-12 * max(1,max_abs_normalized)`.
Sigma-zero intensity cases additionally require exact float32 bits. These
tolerances are declared before native verification and are engineering numeric
criteria, not biological/model-equivalence tolerances. Include all ten dtypes,
64-bit extremes, subnormals, both-axis and one-axis smoothing, missing channels,
outside support, derivative seams, cancellation and exact work admission.

A1 numeric/reference preparation and A2 native checks do not complete SCI1-A:
A3-A6 still require training/prediction, codecs/resume, installed frontends and
the full frozen qualification. Biological labels, model comparisons and real
hardware acceptance retain their separate prerequisites.


## Native raw model and captured-grid prediction (QRM1; legacy raw QPR1)

`RawPixelRecipe` captures dtype, series, selected channel indices/names and Z/T,
explicit affine normalization, both rational sampled-grid spacings and feature
operations. `physical_sigmas=true` interprets the requested sigma pair in
micrometres, requires known positive calibration on both axes and resolves it
to per-axis grid units. The kernel limit applies after that conversion; a
100-micrometre request on 100-micrometre sampled pixels is a one-pixel kernel.
A physical model matches exactly equal binary64 sampled physical spacing on
each axis. A grid-unit model matches exactly equal reduced rational spacing.
No biological tolerance or automatic approximate resolution match is implied.

The reusable model has no training image dimensions, ROI/origin, filename or
content hash. Changed dtype, channel labels/order/selection, series or Z/T
reject; inference can use a new image or subwindow with matching sampling
semantics. `RawPixelClassifier` validates the recipe against the existing
`ForestModel` schema and reuses its bounded votes. `classify_raw_pixels` takes
an immutable captured `ScientificRawGrid`; it retains both grid axes, owning
labels, validity, vote fractions, class counts and model/feature/input hashes.
An invalid row remains unclassified. A low-confidence valid row has label zero
and retains its vote fraction. This is not calibrated probability.

New raw models use QRM1, distinct from RGB QPC1/QPC2 and the RGB QPR1 record
wrapper. The original raw QPR1 framing is read only through explicit model
parsing, preserving its bytes/hash. The new model binary header is `Q R M 0x01`; integers and binary64 bits are little-endian uint64. Fields in
order are dtype, series, first channel, channel count, Z, T, sampler work budget;
X numerator/denominator then Y numerator/denominator; physical flag and optional
sampled X/Y micrometre spacing; one uint64-length UTF-8 name per channel; one
(offset, scale) binary64 pair per channel; scale count followed by X/Y sigma
pairs; gradient/laplacian bits 0/1 and feature work budget; forest byte count
and the existing forest bytes. The final 64 ASCII bytes are SHA-256 of the
preceding bytes. Reject unsupported versions, noncanonical/repeated scales,
negative zero/nonfinite parameters, invalid selections, oversized text/arrays,
truncated/trailing data, checksum/schema mismatch and noncanonical re-encoding.
The model is bounded by 64 MiB plus 128 KiB; names total at most 64 KiB.

Loading uses the established regular-file guard and verifies stable source
bytes. Saving uses the existing atomic exporter. New-file mode refuses an
existing destination; replacement first validates an existing QPR model and
never overwrites an unrelated file. Pre-cancellation preserves destinations.
The native feature suite contains a separately authored QPR framing oracle,
exact forest vote cases, physical calibration mismatch/unknown controls,
streamed/full-grid checks and exact allocation admission tests.

These model and captured-grid APIs now compose through the owner-captured
controllers below. The candidate protocol below adds retention and archives;
its local acceptance is recorded separately. The replay/bounded apply path
below also has local acceptance. Native checkpoints and Windows Python now
have the separate later receipts below; official CLI/batch, desktop actions and
broader installed profiles remain open.
Those remaining A3-A6 integrations must preserve the same raw authority rather
than routing quantitative values through the scalar-downsample RGB result.


## Owner-captured native workflow contract

`RawPixelTrainingRequest` captures sorted labelled area IDs, immutable ROI/class
snapshots, one explicit scientific grid, raw selection, normalization and the
model recipe on the owner. Existing `ObjectFeatureRequest` and
`RawAnalysisRequest` remain the document/source authorities. Row coordinates
are sampled-grid indices; label membership uses each rational axis center
rounded once to binary64 and the native ROI containment rule. Same-class
overlap counts once with the lowest selected annotation ID; different-class
overlap is ambiguous and excluded. All feature columns must be supported.
Deterministic bounded per-class reservoirs reuse the RGB priority/comparison
implementation, then optionally balance classes. Return owning immutable rows,
labels, coordinates, annotation IDs, counts and input/result commitments.

`RawPixelPredictionRequest` captures the model, full explicit grid and optional
area ROI through the raw-analysis authority. Native workers read raw data only,
apply the shared classifier and clear validity/labels outside the captured ROI.
The result retains both axes, model identity and an ROI/calibration-bound input
commitment. A model/result is never published to Document by a worker.

The captured Document calibration governs physical sigma resolution, including
explicit overrides of file metadata. Unknown physical calibration rejects.
The actual source metadata epoch is separately captured and compared after
sampling; overrides cannot conceal source metadata changes. Physical model
matching still requires exactly equal sampled spacing on each axis.

Preflight charges the capture, source metadata/observations, sampler/feature
workspace, reservoir/final rows, prediction copy and vote scratch before pixel
IO. The current implementation admits one bounded explicit grid. It does not
claim a phase-preserving streamed-window implementation for arbitrarily large
sample arrays; that remains required for the large-slide/checkpoint workflow.
Do not approximate fractional grid origins by natural scalar-downsample tiles.

`RawPixelTrainingJob.sample` delivers a table without requiring a forest backend.
`train` checks backend capability before starting IO, then calls the existing
native forest trainer. Backend-enabled source `e7e4b66` now verifies this path
on real two-channel uint16 TIFF data through direct and owner-job training,
portable row votes, trained archive/model files and retained project snapshots.
The forest records its effective admitted `work_bytes` in model identity.
Historical tables retain fewer live capture resources, so fresh admission may
choose a larger cap and yield a different model hash with identical votes.
For byte-identical retraining, use the retained model's `forest().parameters()`;
the local check proves both recorded-parameter equality and fresh-admission
training-matrix/vote equality. This is engineering, not biological acceptance. `RawPixelPredictionJob` uses the same native prediction
kernel. Controllers enforce owner-thread operations; cancellation alone is
thread-safe. Busy reflects a running worker, while `await_completion` joins
without delivering a result. Poll/wait validates owner/source state before
making completed results available; cancellation and edits before delivery
produce no table/model/result. File-backed publication checks fresh primary and
companion versions with the existing file guards, including changes after the
worker has finished. Worker exceptions deliver failure, allow retry and never
modify the document or an earlier owned result. The archive/retention candidate
below now supplies locally verified owner-stage keep and fresh replay paths.
Bounded contour apply/undo is also available. Streamed resume and all frontend
integrations remain required.


## Retained archives and project record protocol

Implementation source `440f610` has [local acceptance](benchmarks/raw-pixel-archives-local.json)
for both hosts' affected workflow suite, including real-TIFF project/snapshot/
recovery readback. Earlier failures and unchanged regression identities remain
explicit. The API queue defines the remaining integration and platform gates.
Source-only provenance has explicit QPV4 encoding/decoding opt-in; existing
object consumers continue rejecting absent object identity by default.

New raw models use `QRM1`. The pre-release raw `QPR1` reader remains explicit,
strict and byte preserving; loading it retains the old model hash. Constructing
a new model from its forest/recipe emits QRM1. The existing RGB pixel-record
wrapper also uses QPR1 and remains unchanged; its magic alone must never be
used to identify an old raw model. `QXS1` is the bounded canonical recipe frame.

`QXT1` archives capture sampled training tables with an optional actually
matching trained model. Sampling without the optional native training backend
is an explicit archive purpose. Attaching a model requires exact recipe,
classes, row count and native forest training-matrix hash agreement. `QXP1`
archives capture a model, exact scientific grid, ROI, document calibration,
raw source metadata/provenance and prediction arrays. Owner prediction hashes
use `qpx.raw-pixel-region/2`, which can be recomputed from retained fields.
Training hashes retain their existing numeric domain. All frames have bounded
lengths, little-endian scalar/array fields and SHA-256 checksums; invalid float,
count, geometry, class/schema, vote/threshold and semantic-hash data rejects.
Archive bytes are limited to 128 MiB and retained payload to 256 MiB.

Decoded tables/results are historical immutable data. They contain no live
capture token. Document keep methods require the original matching capture
and a fresh owner/source check; a decoded archive cannot manufacture that
authority. Standalone archive writes use existing regular-file guards and
atomic export, refuse accidental overwrite and validate an existing archive
before explicit replacement.

`RawPixelRecord` stores source-bound archives in the native Document catalog.
The existing SQL `pixel_classifier_records` table dispatches RGB QPR1 versus raw
QXR1 wrappers explicitly; SQL schema 13 and older-project migration behavior
remain unchanged. Snapshot saves and recovery snapshots carry the immutable
raw records. New raw payloads fail explicitly in older readers that do not
understand QXR1. Historical reads do not mark results current or verified and
do not open a decoder. The fresh replay and bounded contour APIs below supply
separate live authority. Phase-preserving streams/checkpoints, cross-tile
contours and installed frontends remain required.


## Fresh replay verification and bounded contour publication

`RawPixelTrainingRequest::replay(document, record_id)` and the prediction
equivalent construct new live owner captures from retained records. They reuse
the original grid, selection, requested features/options and archived model.
Training checks selected annotation IDs, revisions, kinds, classes and geometry;
prediction checks its captured object/mask, model and threshold. Both compare
reader/locator/file-set scope, raw metadata and effective document calibration.
File observation generations may differ after reopen: fresh bounded sampling
and input/result commitments establish content equality. Capturing the replay
does no pixel IO. It never restores a historical capture token.

`Document::verify_raw_pixel_training` and `verify_raw_pixel_prediction` require
matching fresh request/result captures and repeat actual primary/companion file
guards. A parameter mismatch or stale/wrong authority throws; changed content
returns false. Training can verify the sampled table without retraining; an
optional supplied model must match the archived model. Each existing raw job
also exposes owner-thread `verify_result(document, record_id)`. Cancellation,
edits or file changes before verification reject. Verification leaves document
revision and archive bytes unchanged and stores no persistent verification flag.

`trace_raw_pixel_prediction(request, result, options, cancelled)` returns an
owning `RawPixelContours` built only from a matching live prediction. The existing
bounded GEOS converter borrows core labels and the captured ROI; it maps edges
through both original rational axes and omits feature-halo context from the output
raster. Classes retain their labels and disconnected parts form multipolygons.
`RawPixelContourJob` performs the same conversion using immutable native snapshots;
the source/request stays on its owner until the worker is joined. Cancellation
alone is thread-safe, including completed-but-undelivered cancellation. Poll/wait
checks fresh source state before delivery. GEOS-unavailable builds refuse.

`publish_raw_pixel_contours` repeats source/request checks and requires the
captured document revision. One `Document::execute` creates all class annotations,
using the captured ROI as parent when clipping is enabled. Invalid/stale apply
changes nothing; ordinary undo/redo restores geometry, classes, parents and IDs.
A second apply of the same capture rejects after the first revision change.
For a new result, apply before retaining it if both are desired; retaining first
changes the document revision and requires fresh replay before apply. Verification
alone does not grant publication authority to old decoded arrays.

Local evidence covers reopened real TIFF records and Windows GEOS-positive
rational/context geometry, exact raster membership/area, budgets, cancellation,
late source mutation and atomic undo/redo. Linux and Windows training configurations
also exercise GEOS-unavailable refusal. These are bounded single-grid contours;
streamed cross-tile stitching, installed frontends and platform/research acceptance
retain their own gates.


## Phase-preserving raw window sampling and features

Source `ae0ba29` has [local native evidence](benchmarks/raw-lattice-windows-local.json)
for five selected suites each Windows/Linux and two Windows training regression
suites. The window model/owner path below has its own later local evidence.
Window training, streams, checkpoints and frontend integration remain required.

`ScientificRawWindow` takes image dimensions, the original two
`ScientificGridAxis` descriptors, and a bounded first-X/first-Y/core-width/
core-height range. Its axes retain global origin/extent/sample denominators.
`first_core_index()` and `core_samples()` locate the local output; context comes
from each original axis. Edges and centers use global sample indices and exact
rational arithmetic. This constructor admits window geometry, not an entire
RGBA8 source or sampled-output raster. A million-by-million global lattice can
therefore supply a small window even when constructing ScientificGrid would
reject whole-raster admission. Traversal, exact-weight and allocation limits still
apply to the actual window before pixel IO.

`read_scientific_raw_window`, `digest_scientific_raw_window` and
`scientific_raw_window_work_bytes` reuse the existing sampler, metadata snapshot,
bounded source chunks, support precedence and source-bit commitments. The owning
`ScientificRawWindowData` is a distinct type with `window()`, source metadata,
capture width/height/channels, HWC binary64 means, per-channel support/reasons and
decoded/input commitments. It cannot masquerade as a full ScientificRawGrid or
enter an old full-grid codec. Its input domain is `qpx.window-raw-area-exact/1`;
it captures global axes plus local first/count. Legacy full-grid grammars remain
unchanged, and tiled commitments are not relabeled as legacy full-grid hashes.

Window weights reduce each global extent/sample pair by its gcd, preserving the
same rational area mean. Dense large lattices then use small exact weights.
Irreducible ratios can still require a denominator larger than 32 bits despite
only a few source-pixel visits. The shared exact accumulator now has a separate
window mode with a checked total below 2^72; its existing 68-word/2176-bit numerator
holds the required <=2170 bits. Integer long division rounds once to binary64
nearest/even, retaining signed-64/unsigned-64, extreme finite and subnormal input
semantics. The legacy 28-bit mode and limits are unchanged. Window admission
charges 68 division-word visits for small weights or 2304 division-bit visits
for wide weights; these are traversal/allocation budgets, not process RSS or
latency measurements.

The private quantitative feature authority accepts this window data, sharing its
normalization, Gaussian/support filters, gradients, Laplacians and final float32
projection. It returns only the core and requires the existing per-axis halo;
tile edges do not become image edges. Feature input uses the separate
`qpx.raw-pixel-window-feature-input/1` domain. The local test compares 9,072 exact
float32/support slots against full-context features across twelve dtype cases,
including nonbinary sampling ratios, anisotropic filters and image/invalid-source
seams. Independently generated [Fraction references](benchmarks/raw-window-fraction-reference.json)
check thirteen wide means and twenty large irreducible-lattice uint64 capture
slots. Hash-only equality, bounded chunk reread mutation, outside support,
cancellation and exact budget refusal also pass. Training-enabled regression
checks cover the existing whole-grid workflow; window-aware training remains open.


## Window-aware model matching and owner prediction

Source `642f728` has [local native acceptance](benchmarks/raw-window-classifier-local.json)
for eight selected suites each Windows/Linux and three Windows training suites.
The same `RawPixelRecipe` captures or matches a ScientificRawWindow using global
extent/sample ratios and the existing channel, dtype, series, Z/T, normalization
and physical-spacing rules. Core size and first index are not model properties.
Equivalent full-grid/window recipes and models have identical QXS1/QRM1 bytes;
existing legacy raw QPR1 compatibility and RGB formats remain unchanged.

`classify_raw_pixel_window(ScientificRawWindowData, model, ...)` returns a distinct
owning `RawPixelWindowClassification`: window geometry, core labels, per-row
validity, vote fractions, class counts, model/feature/input/result commitments and
work estimate. It reuses the full-grid feature planner and bounded forest vote
batching. Invalid rows remain unclassified with NaN fractions and a canonical
invalid commitment encoding; low-confidence supported rows retain their votes
with label zero. Its result
domain is `qpx.raw-pixel-window-classification/1`, not the full-grid domain.

`RawPixelWindowPredictionRequest(document, model, window, object_id, ...)`
captures immutable raw metadata, effective document calibration, an optional area
ROI, model and window, plus document identity/revision. It admits source copies,
output and feature/vote work before pixel IO. Window authority becomes stale on
any document revision or identity change. Global centers use first-core index
plus local output index, preserving fractional phase even on huge lattices.
`predict_raw_pixel_window` is the synchronous native computation; its owner result
uses `qpx.raw-pixel-window-region/1` and binds mask/calibration and global geometry.

`RawPixelWindowPredictionJob` owns the request/source until its worker is joined;
workers borrow the reader and hold only immutable native snapshots. Controller
methods run on the owner thread; cancellation is thread-safe. Poll/wait refuses
cancelled, edited, copied-document, changed-metadata or changed-file results
before ready delivery, including changes after worker completion. Fresh primary
and companion file guards run at delivery, rather than each ordinary progress
poll. Worker errors permit retry and leave the document and earlier owned output
unchanged. Physical matching uses captured document overrides separately from
the actual file metadata epoch.

Source metadata payload is admitted before capture copies it. The sampler's
actual quota is capped by the remaining owner work budget and its expected series
is checked before decoder IO. These protections also apply to the existing
full-grid training/prediction workers, preserving their result/hash/codec meaning.
The local controls grow metadata or change series after capture and prove refusal
before reading pixels, rather than detecting it only after an oversized read.

Evidence includes 42 exact full/window support-label-vote comparisons, real TIFF
fractional-window ROI prediction and late source refusal, and an independently
positioned ROI on a synthetic million-by-million lattice. The inherited wide
Fraction and feature-seam controls still pass. One window is not a tiled stream:
global reservoir training, tiled orchestration, versioned window/stream records,
interrupted/resumed execution, contours/frontends and full platform/research
acceptance retain their own implementation and qualification gates.


## Sparse owner-captured tiled training

Source `1273c3e` has [local native acceptance](benchmarks/raw-tiled-training-local.json)
for the affected workflow on Windows GEOS-ON, Linux GEOS-OFF and Windows
training-ON. Both source snapshots authenticate 348 inputs. Sixteen passing
regressions retain source `fb7ddb0`; the successor changes only metadata-quota
fixture allocation restoration. Original failures and 22 original/final binaries
are preserved. This packet completes T1's local native checks.

`ScientificRawLattice` holds source dimensions and original global axes without
whole-raster admission. `RawPixelTiledTrainingRequest` captures immutable labelled
annotations through `ObjectFeatureRequest`, source metadata/calibration and strict
document identity/revision. It finds conservative global center ranges from ROI
bounds, unions candidate tile intervals before allocating the plan, and rejects
window/scan limits before decoder IO. Exact ROI membership still chooses rows.
Each planned core contributes once; halo cells supply filter context only.

`sample_raw_pixel_tiled_training` uses the same per-class reservoir as full-grid
raw training. Priorities and retained coordinates use original global sample
indices. Same-class overlaps choose minimum annotation ID, conflicting classes
are excluded, and balancing occurs after global selection. Captures, plan,
spatial index and reservoirs are charged before allocation; each actual window's
feature/sampler workspace is admitted before its own IO. The request's work
estimate is the admission ceiling, not a measured process-memory result.

The distinct owning `RawPixelTiledTrainingTable` exposes recipe/classes, feature
rows, labels, global coordinates, annotation IDs, eligible/retained/ambiguous
counts and commitments. Input domain `qpx.raw-tiled-annotation-input/1` binds
recipe, lattice, tile policy, annotation revisions/kinds/labels/geometry, sampling
policy and per-window input/ownership. Result domain
`qpx.raw-tiled-pixel-training/1` binds those inputs and retained row bits. Tile
policies can produce identical training matrices with distinct input/result
commitments. The table has no implicit full-grid QXT1 representation.

`train_raw_pixel_tiled_classifier` and `RawPixelTiledTrainingJob.sample/train`
reuse the existing forest and portable model. Identical explicit effective
parameters produce identical full/tiled model bytes and prediction votes on the
Windows backend. Fresh admission can change effective work parameters; retain
that model-identity rule. Backend-OFF job training refuses before pixel IO.
Workers borrow the reader retained by the owner request, use immutable snapshots
and join before owner release. Await does not deliver results. Poll/wait refuses
cancelled, copied/edited-document, calibration/metadata or fresh primary/companion
file changes before ready delivery. Failure permits retry without publication.

Tests compare raw and filtered rows across three tile policies with the full-grid
authority, including overlap ownership and ambiguous counts. A million-by-million
lattice with two tiny labelled regions needs one 64-by-64 candidate core and
retains four correctly indexed rows. Real TIFF fractional sampling, actual
backend prediction, lifecycle controls and late file mutation are covered.
Bounded tiled prediction, versioned tiled records/checkpoints, interrupted resume,
frontends, cross-tile contours and integrated/research/resource/release acceptance
remain open under T2-T5 and their separate gates.


## Bounded raw prediction streams and owner delivery

Source `8d544eb` has [local native acceptance](benchmarks/raw-pixel-stream-local.json):
eight selected suites each Windows/Linux and three Windows training suites pass
with 352 matching inputs and nineteen retained executables. T1's sparse training
and existing window/model/archive/replay and exact numeric controls remain in
the selected suites. T2 adds native streams and bounded owner delivery.

`RawPixelTiledPredictionRequest(document, model, lattice, options, region, object_id)`
captures one existing window authority. An optional `RawPixelStreamRegion`
contains first global X/Y sample indices and core width/height; omission selects
the complete lattice. `RawPixelStreamOptions` declares tile axes, sample/window
limits, work budget and minimum vote fraction. Window geometry is computed in
row-major order from its index, retaining original rational axes and halos with
no whole-raster ScientificGrid or allocated tile-plan vector. Initial capture
admits the largest core and owner/producer state; every actual later window is
admitted before that window's IO. Work estimates are admission ceilings.

`RawPixelPredictionStream.next()` returns one owning
`RawPixelWindowClassification`, sharing the existing typed sampler, feature,
forest and global-center ROI kernel. Returned arrays remain valid independently
of later calls. The producer releases each delivered block; caller-retained blocks
are outside its work budget. Exhaustion alone returns null. Cancellation or
failure throws, and concurrent consumers reject. Only cancel and progress getters
may run concurrently; join the consumer before destroying a stream. Source
metadata and fresh primary/companion observations are verified before each
synchronous block is returned. A synchronous captured computation does not grant
live Document mutation authority; owner publication uses the controller below.

`RawPixelPredictionStreamJob.request` starts one worker. Poll/wait reports
`block_ready`; await alone does not deliver. `take(document)` checks document
identity/revision and fresh files, transfers the pending block and starts the
next worker. While output is pending there is no next decode or unbounded queue.
Produced and delivered counters stay distinct. All controller methods except
cancel require the owner thread; only immutable captures and a borrowed reader
enter the worker, and destruction joins before releasing owner-held sources.
Cancellation, edited/copied/calibration-stale documents and changed metadata or
files drop unpublished output. Decoder failure permits a fresh request. Owned
already-delivered blocks remain valid historical results.

Request domain `qpx.raw-pixel-stream-request/1` binds model, captured metadata,
reader, global geometry, coverage/tile/options and effective calibration/ROI.
This header alone is not a source-content hash. Ordered domains
`qpx.raw-pixel-stream-input-chain/1` and
`qpx.raw-pixel-stream-result-chain/1` chain the request header, previous commitment,
block index and each decoded input/result commitment. Partial/cancelled/failed
streams refuse final hashes; jobs additionally require every block to have been
taken. Individual window result domains and legacy codecs remain unchanged.

Tests compare three fractional filtered tilings and a nonzero-index crop against
full-context support, labels and vote bits, and verify window result identities,
unique coverage, aggregate counts and deterministic chains. A trillion-cell plan
has O(1) traversal state; two 16-by-16 blocks keep decoder requests bounded and
weak ownership proves consumed output is released. A distant rational-index ROI
checks original global centers. Real TIFF matches full results with an actual
trained model on the Windows backend. Backpressure, pending late-file mutation,
metadata/series/scan/work refusal, cancellation during/after IO, retries, copied
owners and physical calibration have distinguishing controls.

T3 still needs versioned tiled-table and stream files, atomic save/reopen,
hash-only source revalidation and interrupted/resumed execution. T4 installed
bindings/frontends, cross-tile contours, T5 integrated platforms and independent
research/process-memory/hardware/release acceptance remain open.


## Prediction prefix files and hash-only resume

Source `c843dfa` has [local native acceptance](benchmarks/raw-stream-file-local.json)
for the affected workflow on Windows GEOS-ON, Linux GEOS-OFF and Windows
training-ON. Sixteen passing regressions retain `125afbb`; its initial 8/8/3 suites
all passed. Review then corrected absent-destination publication and added its
competing-writer control; only the file implementation and test helper changed.
Both source snapshots authenticate 356 inputs; 22 original/final binaries remain.

`save_raw_pixel_stream(request, path, options, cancelled)` writes QWS1 metadata,
QWB1 numeric window records and a QWE1 partial/complete footer through the
existing atomic export writer. The header embeds QRM1/explicit legacy raw model
bytes, captured provenance, global lattice/coverage/tile options, effective ROI
and calibration, source metadata epoch and request commitment. Each record has
its own checksum, canonical support/label/vote values, feature/input/result hashes
and derived original window geometry. Ordered input/result/file-content chains
and prefix counts/totals close the footer. Legacy QXT1/QXP1 and individual window
hash grammars retain their meanings.

`RawPixelStreamFile` opens bounded metadata and framing without opening a live
image. `read_block` verifies its window, canonical numeric/ROI/source-footprint
semantics and result commitment. `verify` checks all saved prefix blocks, chains
and aggregate totals. Declared summaries are not a full verification result;
verification records the observed file version, and subsequent reads guard that
version again. Cancellation or failed reads clear the verified flag. Historical
arrays own their storage and acquire no live Document capture authority.

`RawPixelStreamSaveOptions.max_new_windows` makes an explicit atomic checkpoint;
zero creates a metadata-only empty prefix. Set `resume_path` to a saved prefix and
use replacement explicitly for an in-place update. Resume first rejects changed
model/recipe, coverage/tile/options, effective ROI/calibration or source identity/
file set. Unchanged file observations may resume through a newly opened reader;
a new reader's process generation alone does not invalidate the same files.
Every retained window then rereads decoded inputs through the hash-only sampler,
shares the feature-input commitment grammar, and must match its stored input.
It restores stream counters/chains without feature planes or forest inference;
only the suffix is predicted. The previous prefix remains intact if validation,
decoding, IO or cancellation fails. Explicit checkpoints copy/re-encode their
retained prefix in bounded memory; they do not promise constant IO cost.

File limits cover bounded metadata and one historical output block. A simultaneous
live producer retains its separate request budget; the two limits are not one
combined process-memory claim. File/output limits reject before source pixel IO.
Primary/companion source paths and hardlink aliases cannot be destinations.
Replacement is conditional on a destination observed before computation; a
competing file created at a previously absent path is preserved, even when the
caller permitted replacement. Tests place that writer while source IO is blocked.

Independent little-endian header/model/block/footer offsets and SHA checks,
rehashing a malformed support flag, truncation/oversized framing, file-version
mutation and reader cancellation distinguish codec errors. Multi-stage prefixes
resume to exactly the uninterrupted bytes for the same request. Real TIFF closes
and reopens with fresh authority, resumes the same files, preserves trained-model
prediction commitments on the Windows backend, and refuses changed source files.
Changed decoded bits, model/ROI/calibration/coverage, companion observations,
source-output aliases and cancellation preserve earlier checkpoints.

T3 still needs sparse training/trained-table archives and retained records,
training reservoir checkpoints/resume and native owner export/read/retain jobs.
Automatic checkpoint scheduling/abandoned-temporary maintenance and broader
crash/power-loss qualification remain open. T4 installed frontends, T5 integrated
platforms, cross-tile contours and independent research/process-memory/hardware/
release acceptance remain separate requirements.


## Sparse sampled/trained archives and retained replay

Source `cc6fab4` has [local native acceptance](benchmarks/raw-tiled-archives-local.json)
for the affected workflow on Windows GEOS-ON, Linux GEOS-OFF and Windows
training-ON. Sixteen passing regressions retain `30d0659`; initial three workflow
failures correctly rejected an out-of-image fixture. The successor changes only
that fixture to a valid in-image ROI containing no sample centers. Both source
snapshots authenticate 360 inputs; 22 original/final executables are retained.

`archive_raw_pixel_tiled_training(table, model)` adds an explicit QTT1 variant
inside the existing `RawPixelArchive`. `tiled_table()` exposes the distinct sparse
table; `table()` and `result()` remain empty for that kind. `purpose()` returns
`tiled-sampling` or `tiled-training`. `lattice()` provides source dimensions and
original axes for all variants; `grid()` refuses a tiled archive instead of
constructing a whole raster. This distinction also supports a trillion-cell
lattice with a few retained rows. Existing full-grid QXT1/QXP1 meanings remain.

The frame stores QXS1 recipe, pure global geometry, actual raw source metadata/
provenance, training/tile options, optional QRM1/legacy raw model, all selected
annotation identities/revisions/kinds/classes/geometry, candidate window/scan
counts, feature rows/global coordinates/owners, eligible/retained/ambiguous counts
and input/result commitments. Decoding rebuilds the bounded sparse candidate
plan and spatial index through the same authority as live sampling, checks the
recorded coverage and clears capture authority. Full and tiled variants reuse
bounded binary/source/provenance/recipe primitives without changing old fields.

Validation checks unique supported coordinates, exact feature/schema dimensions,
finite values, class membership/conflicts, minimum same-class owner, canonical
global priority order and balancing/count limits. Trained models must match the
recipe, classes, row count and exact forest training-matrix hash. Reopened
historical retraining uses recorded effective parameters for identical model
bytes; fresh work admission remains part of model identity. Re-encoding a decoded
table does not create live authority.

`Document.keep_raw_pixel_tiled_training` requires the matching live capture plus
strict document identity/revision and fresh source checks. It stores the existing
QXR1 record wrapper in the existing SQLite schema 13. Mixed full/tiled project
save/open, background snapshot saves and autosave/recovery preserve the encoded
bytes. Record decode and document restoration now share source/ID bounds checks
for every selected annotation, including unretained/no-center selections in both
legacy full and new tiled variants. No SQL migration or fake full-grid geometry
is introduced.

`RawPixelTiledTrainingRequest.replay(document, record_id)` recaptures selected
annotations, recipe, global geometry and source on the owner. It refuses changed
annotations/calibration/recipe before pixel IO. A fresh sample/train job and
`verify_result`, or `Document.verify_raw_pixel_tiled_training`, compare actual
input/table/model commitments after fresh source checks. Historical decoded
arrays alone cannot verify or retain a live result. Changed source bits produce
failed equality; copied/stale owners and changed files refuse publication.

Tests cover balanced/unbalanced matrices, independent QTT1 field offsets,
rehash-tampered owners/coordinates/values/counts, explicit kind separation,
trillion-cell lattice/record decode, unused selected ID bounds for both archive
families, cancellation/atomic files and source/companion/hardlink output refusal.
Actual Windows backend models round-trip and retrain identically with recorded
parameters. Real TIFF retains mixed records, saves/reopens/recovers them and
verifies through fresh owner jobs; stale files and historical authority reject.

This is a completed sampled/trained table representation, not a training
checkpoint. Training reservoir interruption/resume now uses deterministic admitted-prefix
replay as described below; serialized reservoir-state restore remains deferred. Owner prediction
file adapters, installed frontends, automatic checkpoint maintenance, cross-tile
contours and integrated/research/resource/hardware/release acceptance stay open.


## Tiled training prefix checkpoints

`raw_pixel_training_checkpoint.hpp` adds QTC1 alongside completed QTT1 archives.
`save_raw_pixel_training_checkpoint(request, path, options, cancel)` executes at
most `max_new_windows` beyond an optional `resume_path`. It returns historical
checkpoint metadata, replayed-window count, and a live captured table only when
coverage is complete. `max_new_windows=0` creates an empty checkpoint with no
pixel IO; resuming a completed checkpoint still replays and verifies every core.
Use `RawPixelTrainingCheckpoint.from_bytes` or `load_raw_pixel_training_checkpoint`
for bounded historical readback; these restore no Document authority or table.

QTC1 stores the original ten-scalar global geometry, existing raw source/provenance
encoding, total/completed windows, request hash and cumulative prefix hash,
followed by the existing SHA-256 checksum framing. Request identity binds the
original training input header, explicit work policy, source reader/metadata
epoch and effective calibration. The hashed training input header includes recipe, tile/sampling
policy and ordered annotation IDs/revisions/kinds/classes/geometry. The domain
is `qpx.raw-tiled-training-checkpoint-request/1`; each window extends
`qpx.raw-tiled-training-prefix/1` with the previous commitment, ordered index and
window geometry/feature-input/label/owner digest.

Fresh owner capture must match the saved request and source observations before
pixel IO. Replay uses the same global reservoir and original table input-hash
accumulator, verifies the completed-prefix boundary, then processes new windows.
All candidates and eligible/ambiguous counters are reconstructed before final
balancing. Original table/hash grammars are unchanged. Source publication guards
check actual primary/companion files again before atomic commit; source paths
and hardlink aliases refuse output. A previous destination must be a valid QTC1
file and remain unchanged. A destination observed absent uses no-replace commit,
even when replacement was allowed. Cancellation/errors preserve prior commits.

File metadata has independent explicit byte/work limits; replay retains ordinary
per-window live work admission. Resume repeats prefix decoding and features:
there is no constant-IO/time claim. A completed table can train through
`train_raw_pixel_tiled_classifier` with explicit effective parameters; QTC1 does
not resume interrupted forest fitting. Worker callers must join before releasing
the borrowed reader and use existing owner verification to publish their table.
Retained stream-file references now have the separate QWL1 acceptance below.
Automatic cadence, abrupt-process/power-loss acceptance and installed frontends
remain open.


## Native file job delivery

`raw_pixel_file_jobs.hpp` adds three typed controllers over the existing QWS1 and
QTC1 implementations. `RawPixelStreamSaveJob.request` exports/resumes prediction;
`RawPixelTrainingCheckpointJob.request` saves/resumes sampled training. Their
`poll(document)`/`wait(document)` return status, while `take(document)` transfers
one result only after fresh capture/document/source/output checks. Partial QTC1
results have no table; completed results retain the fresh request capture and
can use existing `archive_raw_pixel_tiled_training`/Document retention.

`RawPixelStreamFileReadJob.open` verifies a complete historical prefix in its
worker. `take_file` transfers its historical file handle. An explicit `read(file,
index)` and `take_block` transfer one owning block; there is no decode-ahead.
Neither historical file nor block restores live document authority. The new
`RawPixelStreamFile.check_unchanged` checks cached identity/stamps without a block
scan or upgrading an unverified file into a verified one.

Each controller has one future/result slot. Pending output prevents replacement
until taken, invalidated or cancelled; `await_completion` joins without publishing.
All controller methods run on the constructing thread except native atomic
`cancel`. Destruction cancels and joins before releasing captured readers. Workers
never inspect a live Document. The common private helper shares only lifecycle;
each typed job supplies its own source/document/file validation.

Prediction export verifies the saved prefix in the worker and matches totals and
commitments to the save result before delivery. Training checkpoint readback
validates the complete bounded metadata blob. Poll/take use lightweight output
identity/stamp guards and fresh live-source checks, not a full output scan on the
owner. Full verification costs file IO proportional to prefix size, with one
bounded historical block live. Stats report the final completed-window count;
they do not imply per-window in-flight progress during an opaque file operation.

Cancellation, owner edits or file changes suppress pending results. An atomic
file commit that already succeeded remains historical output; late owner
staleness/cancellation does not delete it or restore an older file. This differs
from cancellation detected before commit, which preserves the prior checkpoint.
External raw stream link/record retention and asynchronous relinking now have
the separate QWL1 acceptance below. Installed frontends, automatic maintenance and abrupt-process/power-loss acceptance remain
open. Reuse the existing RGB external-link lifetime/path model with a distinct
raw metadata representation rather than embedding whole streams in project blobs.


## Retained external raw stream links

`RawPixelStreamFile.link()` and `prepare_raw_pixel_stream_link` require complete
prefix verification and a fresh file identity/stamp check. They produce small
immutable historical metadata: original global lattice, region/options, source
object/revision history, model/request/header hashes and exact prefix summary.
No full model, feature planes or prediction raster is retained in the link.
QWS1's shared file-size arithmetic checks expected bytes/coverage; an empty
prefix can describe a trillion-cell lattice without reading source pixels.

QWL1 encodes a record ID/name, rebased external path, ten geometry scalars,
region/options, source history/header length, identity hashes, prefix summary
and checksum. It is distinct from RGB QSL1 and uses the existing stream-record
table in SQL schema 13. Metadata decode does not open external files or restore
the transient file-verification observation. Full raw QXR1 and RGB record formats
retain their original meanings. Project snapshots charge bounded link metadata.

`Document.keep_raw_pixel_stream` requires verified, unchanged output and source
dimensions compatible with the document. The record is a historical reference,
not a live result or apply capability. `raw_pixel_stream_records`/record/count
expose it. `remove_raw_pixel_stream_record` removes metadata only and never
deletes external output. Save/save-as, background snapshots and recovery rebase
paths without copying file bytes; absent output does not prevent metadata reopen.
These project files therefore do not promise self-contained stream bundles.

`RawPixelStreamSaveJob.take_link(document)` is an alternative to taking the save
summary. Historical `RawPixelStreamFileReadJob.take_link()` transfers a verified
link after open. `open_link(link)` verifies the external prefix in the worker
and compares all saved identities, geometry, options and totals before delivery.
`Document.relink_raw_pixel_stream_record` accepts only fully verified matching
content at its new path, preserving ID/name and incrementing one revision. A
different valid prefix/model/policy refuses without mutation. Decoded metadata
cannot be kept/relinked until external verification supplies a current token.

Growing a checkpoint in place changes its prefix identity: an old partial link
does not silently become a completed result. Prepare a new link/record for the
new prefix. Missing, moved, changed and tampered files remain historical metadata
until explicitly resolved; verification restores file trust, never live document
authority. The Windows Python API now has separate acceptance below. Official
CLI/batch/viewer/ImGui consumers, other Python profiles and integrated,
abrupt-process/power-loss, scientific/resource/hardware/release acceptance remain
open.


## Installed Python API

The raw classifier types/functions are public `qupathxx` exports, with generated
PEP 561 stubs from the installed module. Python performs no raw feature, sampling,
reservoir, forest or file algorithm. Full-grid and indexed-window/tiled types
remain distinct, and synchronous predictions return owning results. Pending job
results, partial checkpoint tables and historical archive variants are nullable.

`values_numpy`, `labels_numpy`, `coordinates_numpy`, `annotation_ids_numpy`,
`valid_numpy`, `vote_fractions_numpy` and raw-window `numpy`/`reasons_numpy` return
read-only views by default. Their owner stays alive as long as the view; `copy=True`
returns independent mutable storage. Raw HWC means are float64; training features
and vote fractions are float32, labels uint32, support/reasons uint8, global
coordinates int64 and annotation IDs uint64. Training labels are zero-based;
prediction zero means unclassified and class labels are one-based.

Capture, Document mutation and owner delivery retain the GIL. Blocking owner
`wait(document)` performs poll, releases the GIL only for native join, then polls
again with it held. Synchronous immutable native work and historical file IO
release the GIL; native cancel can run from another Python thread. Python-owned
job wrappers release the GIL while their native destructor cancels/joins, avoiding
a shutdown dependency on worker-held Python `shared_ptr` captures. These wrappers
share lifecycle forwarding only and preserve native thread/authority checks.

Bytes decoding snapshots explicit bounded input before native parsing. Model,
archive and checkpoint encodings retain their existing C++ grammars. Decoded
historical arrays/links do not gain capture or file-verification authority.
Retained file metadata includes the observed file path allocation in its memory
charge. Use the existing owner jobs for cancellable work and join before closing
borrowed readers; await does not itself publish a result.

The installed example `examples/raw_classifier_workflow.py` loads labelled areas
from a TIFF project, uses an explicit identity-normalized intensity recipe,
checkpoints/resumes training and prediction, retains model/table/file history and
writes a new project. Its output directory is new; it does not overwrite the
input project or imply biological accuracy. Windows cp312 training-enabled local
acceptance covers that example and the declared tests; official CLI/batch,
desktop/managed viewer, other installed profiles and integrated qualification
remain open.


## Desktop and managed bounded contours

[Installed local Windows evidence](benchmarks/raw-frontend-local.json) at
`0dc916c` covers the raw classifier panel and managed viewer with GUI, GEOS,
training, TIFF and project support enabled; OpenSlide is disabled in that profile.
This is local engineering acceptance, with broader platform/scientific/resource
and release gates remaining in the project plan.

Open **Raw pixel classifier** in the desktop toolbar. Training uses labelled
areas, explicit channels and exact axes; empty axes select unit level-zero
spacing. **Preview and files** owns a bounded indexed window, model IO and QWS
export/resume. **Retained files** verifies/browses external files; deleting a
record leaves its file intact. Historical file/model legends do not enable live
contour application.

In **Contours**, set inference/geometry limits, trace a fresh bounded full grid,
review its class-colored geometry, and apply one undoable annotation edit.
Changing axes or geometry parameters retires the trace. The annotation name
prefix can change without retracing. Oversized or stale work refuses without
editing the document. This uses a bounded full grid, not stitched streamed
contours; indexed-window previews have a separate purpose and budget.

Python may initialize the panel with `GuiRawPixelOptions` through
`GuiOptions.raw_pixel`. The getter returns a settings copy; assign it back after
changing nested fields. `GuiRawPixelReport` exposes the live/historical results,
contour preview state, applied IDs and errors.

Managed producers submit copied settings while `show(session)` runs on the
owner/main thread, following the existing ViewerSession lifecycle:

```python
preview_ticket = session.contour_raw_pixels(image_id, model, grid, object_id=roi_id)
preview = session.wait(preview_ticket)
if preview.state != "succeeded":
    raise RuntimeError(preview.error or preview.state)
# Consume the completed ticket after inspecting its state.
session.take(preview_ticket)
apply_ticket = session.contour_raw_pixels(
    image_id, model, grid, object_id=roi_id, apply=True,
    expected_revision=preview.document_revision,
)
```

A preview returns immutable historical geometry. Apply recomputes a fresh owner
capture; the optional revision check rejects intervening edits before capture.
Later document/source changes reject live delivery. The viewer reserves the
conservative retained snapshot/model/prediction/geometry payload and anticipated
IDs before mutation. This admission value is not a process-memory measurement.
Cancel tickets or close the viewer to cancel/join active work before readers
release. See the public headers and generated stubs for the complete signatures.
