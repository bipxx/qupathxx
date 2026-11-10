# Native feature-expansion progress — 2026-11-10

The [22-item roadmap](../../feature-expansion-plan.md) remains the completion
authority. This receipt covers delivered S01/V01/V02/I01/I02/R01/R02/F01/F02
plus F03/F04/D01/D02/O01/O02/A01/A02/A03/A04/P01/L01 and P02 native candidate8. GUI claims cover only the named workflows
below; no new release, speed superiority or biological phenotype accuracy claim.

S01 adds an immutable native ExpressionDataset over the existing imported point
projection, with stable IDs/positions/frame/plane/source identity, exact uint64
counts or declared continuous values and explicit validity. Legacy projection
admission remains bounded by2^53. Captures own their buffers and survive live
document edits. Read-only content identity is not live publication authority.

V01 adds shared gene-display requests/fields, linear/log1p display, native minmax/
interpolated percentile/manual ranges, explicit zero/missing colors, native
palettes/opacity and a bounded owner-thread LRU. Raw buffers remain exact and
unchanged; transformed values are presentation only. Manual ranges support a
shared scale. Cache identity includes dataset, feature, row order and applicable
display policy. Cancellation is checked even on a cache hit. Capacity accounting
is conservative buffer/work accounting, not process-RSS qualification.

I01 adds read-only owned import previews and a cancellable native job. Existing
CLI import calls the same preview/publication authority and reuses the parsed
projection. Preview checks the strict pixel/ID profile, dimensions, selected genes,
plane, image bounds, output and resource limits. Native file-version guards reject
stale matrix/ID/position/project inputs before atomic no-overwrite publication.
Exact source hashes, locators/options and recipe lineage survive project reopen;
successive imports retain prior recipes. No second parser or Python workflow.

Decisive local checks in the existing gridfrontends167-package Release cache:

- `qupathxx_expression_tests`: analytical counts, full uint64 ownership, measured
  zero/missing/continuous domains, stable versus edited identity, legacy boundary,
  malformed cohorts, source ambiguity, wrong planes, ranges/colors/log1p, empty/
  constant/extreme/all-missing fields, budgets, cache eviction, cancellation,
  controller ownership and native project save/reopen.
- Its real-data argument opens the admitted mouse-brain project:2,702 spots,
  six markers, all measured; all marker fields compute and counts retain identity.
- `ctest -C Release --output-on-failure --no-tests=error -R
  '^(expression|spatial_counts|spatial_counts_cli)$'`: three affected cases pass.
  Import cases cover read-only/async preview, publication, retained recipe lineage,
  changed source/project refusal, collision, cancellation, strict parsing and the
  existing CLI/save/reopen route. No GUI or bindings compiled for these changes.
- `python -m pytest tests/test_material_staging.py -q`: two cases pass, one Windows
  symlink-creation case unavailable. The test covers exact existing font bytes,
  missing inventory, restricted/unreviewed/NC declarations, restricted source/path,
  restricted-derived lineage, missing/cyclic parents and excluded pools.

L01 reads added material metadata in existing inventories rather than a second
license database. Unknown files/terms refuse admission. Original font lineage and
CC0 downloaded fixture records are annotated. The offline checker is development
tooling, with no product Python dependency. At that initial slice, complete source/
package membership and release-stage integration were pending; the final L01
section below closes those local checks. Historical archives remain preserved.
Restricted datasets/weights were not downloaded.

Waste review: native-only targets reused the existing cache; an extra affected
build covered newly added persistence/recipe checks. Source provenance intentionally
regenerates CMake when compiler inputs change, so settle each batch before build.
No binding rebuild, second engine cache, full scientific campaign or packaging.

R01 continuation: native CoordinateFrame/Affine2D/RegistrationRequest and a
versioned typed registration definition preserve original coordinates/counts.
Explicit matrices cover translation/rotation/scale/reflection, inverse/composition
and calibrated anisotropic pixel/µm conversion. Ill-conditioned/nonfinite transforms
and unknown physical calibration refuse conversion. Linked source identity reuses
verified primary/companion versions and series; embedded RGB hashes exact bytes.
Neither display windows nor process generations enter persistent frame identity.
Acceptance recaptures the point cohort and target frame before the existing
undoable image-data-properties command. Separate transformations retain outside
points and save/reopen without rewriting source observations.

Registration/expression CTest:2 cases pass (0.21s); the admitted2,702-spot real
project also passes mapping/acceptance with unchanged coordinate/count identity.
Fixtures check analytical transforms/inverse/composition, unit scale, numerical
limits, budgets, cancellation, stale counts/geometry/image/calibration, immutable
metadata, native undo/redo, persisted record validation and linked/lazy reopen.
No interactive landmark-registration or GUI claim is made from this native check.
The companion OME-TIFF real example additionally passes a display-window change
with identical frame identity, checking the raw/display boundary on the real
two-channel source. Original real projects were opened read-only and not saved.

V02 continuation: Analyze → Gene expression (also Workflow → Tools) opens the
native gene selector. Search imported IDs/symbols and page the catalogue; enable
spot-center colors and choose linear/log1p, minmax/percentile/manual ranges,
palette/opacity and glyph size. The legend reports the actual transformed units,
range, measured zeros, missing values and clipping. Hover gives exact raw counts.
Compare opens two image canvases with one shared navigation state; shared
automatic percentiles use pooled measured values, never the extrema of separate
percentiles. Constant/missing fields retain their semantics. Presentation/source
choices live in workspace preferences, without rewriting project measurements.

Native GeneViewSession owns immutable snapshots and one worker with latest-
generation coalescing/cancellation. UI uses native fields/palette colors. New
overlay fields draw outside the geometry cache, so cached geometry cannot retain
an old gene color. Comparison tile/context ownership stays on the GUI thread.

Final native expression/gene_view CTest:2 cases pass (0.55s). Independent pooled
percentile oracle is [0,2,10,100,200,300,400] → P25=6/P75=250; checks include
constant/all-missing, raw integer preservation, rapid switches, stale generations,
cancel/owner thread, catalogue paging and exact native preference/source round trip.
Native GUI replay passes exact colors at scientific point locations, zero/missing,
source persistence with two lineages and a40-screen-pixel drag measured in both
comparison maps while the main camera/project remain unchanged. Real Aqp4/Gad1
comparison covers2,702 spots with a shared log1p scale and unchanged source identity.
Evidence: workspace-root build/feature-expansion/gui-v02-r3; first failed oracle
capture remains in gui-v02 and the intermediate passing comparison in gui-v02-r2.

The first oracle incorrectly treated Viewport::origin as image coordinates;
native image_to_screen resolved it, and comparison drag now uses screen translation
too. Source persistence/drag checks were added for named final-review gaps.
Only affected cached targets compiled; Python bindings, packages and historical
science qualification were not rebuilt. The tool reuses the completed layout's
bounded-window/collection helpers; other concurrent UI source is preserved.

I02 continuation: Analyze → Import spatial expression (also Workflow → Tools)
opens a four-step native wizard. Choose an image in a saved project or a PPM/TIFF
file, choose local Matrix Market/TSV inputs and a new output, declare ID/X/Y column
names and units, select genes from the paged native catalogue, then inspect mapped
spot centers over the image before import. Position rows follow matrix-column
order and feature rows follow matrix-row order; no ID/axis/unit guessing. Known
image calibration is required for µm conversion; explicit scaling/offsets apply
after that conversion. Original coordinates, units, source hashes and the full
recipe are preserved separately from image-coordinate point geometry.

Catalogue and import reuse the same native feature parser; UI uses the same
native search/page function as gene display. SpatialImportJob owns catalogue,
preview and publication work. Preview owns its image/coordinates and never mutates
the live study. Its verified source identities refuse stale files/catalogues.
Import requires a completed image preview and a new output; Open delegates to the
existing app loader/unsaved-work protection. Local file choice scans bounded pages,
does no decoding/download and introduces no proprietary reader. This first storage
route embeds selected genes; full backed matrices remain the D01 work item.

Final native expression/spatial_counts/CLI checks:3 cases pass (1.37s); gene_view
passes separately for shared search compatibility. Added cases cover catalogue
identity, explicit header permutation, independently expected anisotropic µm/px
mapping, raw-coordinate persistence, direct PPM input and unknown-calibration
refusal. Existing exact-count/strict-profile/no-overwrite/stale/cancel cases remain.
Repeated imports now have recipe-scoped cohorts selectable independently; old
objects without that optional scope retain their exact previous identity. Scope
distinguishes import occurrences, not biological replicates.

Native GUI replay clicks the actual Import and Open controls, verifies exact
retained counts and mapped/original coordinates, unchanged prior project/live
state, and the canonical new-project path handed to the loader. It also imports
the admitted real2,702-spot/six-marker demo without a shell. Captured overlay/values
are readable. Final evidence: workspace-root build/feature-expansion/gui-i02-final;
initial passing GUI evidence remains in gui-i02. A missing /utf-8 option on the new
MSVC test target caused the initial compiler failure; the translated sources were
unchanged. No binding build, new dependency, new engine cache or package campaign.

R02 adds native immutable landmark fitting, using normalized translation/similarity
least squares and Householder QR for affine fitting. Explicit similarity reflection
never guesses axes; held-out pairs do not influence coefficients. Reports retain
raw pairs, residual vectors/distances, training RMS and an optional held-out RMS,
target units and an exact versioned identity. Rank/finite/condition, pair/ID/work
and cancellation checks refuse invalid fits. Stored model/unit integers are
range-checked before conversion; residuals/statistics/definition/identity are
validated on load. Stored coefficients are restored exactly without refitting.

The existing native current-registration recapture guard now serves both manual
and landmark acceptance. A single image-data-properties command stores the
definition, fit evidence and active ID, providing atomic Undo/Redo/save/reopen.
No point coordinates or counts are overwritten. Active mapping uses the selected
source/frame/image/calibration identities; stale accepted mappings refuse reuse.

Analyze/Workflow Tools → Landmark registration opens a compiled private canvas
controller over the existing viewport/tile services. Users add/edit source and
target pairs, drag the selected handle kind, mark held-out observations, choose
the model and inspect preview/errors. Accept and saved-evidence load call the
same native services. The main gene overlay and comparison renderer consume
accepted mapped positions. The UI labels source object geometry as unchanged;
selection and other analyses do not silently acquire registered ROI membership.

Native CTest registration and landmark_registration pass2 cases (0.25s), with
independently known affine/similarity/reflection/translation, a noisy overdetermined
oracle, held-out isolation, large origins, rank/invalid/budget/cancel refusal,
stale publication, atomic undo and exact project persistence. The admitted real
mouse-brain project also passes the native persistence contract. Logs and the
retained test projects are under workspace-root build/feature-expansion/r02-native.

Final native GUI replay passes actual target dragging and preview invalidation,
Fit/Accept, disabled publication, Undo/Redo, stored-fit load and actual reload
after save/reopen under persistent project/image identity. On an in-memory copy
of the admitted mouse-brain project, all2,702 source positions are deliberately
shifted(+10,-8); a translation fitted to one point with two held-out points
recovers(-10,+8), restoring every registered position to the original published
location. Raw positions/counts remain exact relative to that prepared baseline;
the original fixture file is untouched. This is a controlled geometric test,
not biological registration accuracy evidence. Final captures/projects/log:
build/feature-expansion/gui-r02-final; the earlier passing replay remains gui-r02.

The actual app gene replay passes exact native colors at transformed positions,
alongside existing zero/missing, comparison navigation and workspace/source
checks. Final capture/log: build/feature-expansion/gene-r02-final. Native alignment
controls/residuals and real overlay are readable on inspection. App and affected
test targets build; native-only CI now includes the landmark case (hosted execution
is not claimed). No new dependency, restricted data, binding compilation or release.

Corrections retained: the initial native undo oracle dereferenced absent default
metadata; compare optional metadata explicitly. A field/function typo stopped the
first panel compile. The new gene oracle needed its explicit lineage/linear setup.
Its second failure exposed a stale gui.obj from source edits during a concurrent
desktop compile; forcing only gui.cpp after that build ended restored the actual
consumer. Keep shared translation units unchanged during another build; new
isolated files can be prepared independently. The compiled panel avoids recurring
large gui.cpp recompilation for controller implementation changes. Failed native
log: r02-native-failure; failed gene captures: gene-r02 and gene-r02-fixed. The
durable shared-build lesson is recorded in compilexx's canonical notebook.

Frontend checks ran on the combined integration tree, including the separately
owned sidebar-form edits retained in this checkout. They qualify the named
behavior on that tree, not a frozen standalone binary release from this feature
commit. Release/source/package identity remains P02 work.

F01 native foundation (not a completed checklist item):
[fluorescence_cells.hpp](../../../cpp/include/qupathxx/fluorescence_cells.hpp)
provides a pure level-zero raw-raster detector and a reducer for arbitrary
channel counts. The existing physical Gaussian, metric distance/watershed,
calibrated expansion and compensated moment implementations are reused without
RGB conversion, optical density or preview values. Explicit strict intensity
threshold or256-bin Otsu precedes distance watershed. The splitter receives the
binary foreground so its existing regional-mean gate cannot silently threshold
smoothed intensities again. Expansion retains the shared strict distance<radius
and deterministic collision rules. Nonfinite/invalid segmentation pixels are
excluded; original raw values supply measurements even when detection is smoothed.

Each body includes its own nucleus; foreign-nucleus ownership refuses. Cytoplasm
is body minus all nuclei and is absent when geometrically empty. Geometric and
valid pixel counts are separate; each channel reports finite/nonfinite counts,
optional sum/mean/min/max and optional sample SD. An all-nonfinite channel is
missing, measured zeros remain zero and one finite sample has no sample SD.
Signed intensities remain signed; int64/uint64 values outside the declared exact
binary64 admission range[-2^53,2^53] refuse rather than lose precision. Malformed
storage/type/validity/labels and work limits refuse before scientific publication.
ROI/raster/segmentation-support boundary contacts are retained and flagged, never
asserted as complete objects. No live-document authority is attached to a pure
raster result; the source-bound workflow still needs its own currentness checks.

Final affected native CTest fluorescence_cells passes (0.15s total). Independent
fixtures prove exact two-nucleus masks, radius expansion and four-channel raw
compartment values, empty/missing/invalid/zero cases, signed/nonfinite samples,
integer precision refusal, anisotropic area/radius, constant/Otsu input, a Gaussian
impulse with original-value measurement, strict nextafter-threshold membership,
deterministic identities, boundary flags and budget/cancel/malformed refusal.
Final log: workspace-root build/feature-expansion/f01-native-final/ctest.log.
Native-only CI includes this target; hosted execution is not claimed. The admitted
OME demo produces832 candidate objects in an initial raw-source smoke. The newly
admitted BBBC039 raw TIFF produces130 candidates in the final smoke. Neither is
an annotation comparison or biological-accuracy result. Initial and final logs
remain f01-native and f01-native-final. No GUI/binding/package was built for F01.

The [official BBBC039v1 page](https://bbbc.broadinstitute.org/BBBC039) declares CC0
for its fluorescence images/annotations. This is distinct from the excluded
BBBC017/020 NC material. The first filename in metadata/validation.txt was selected
before inspecting image pixels or detector results:
IXMtest_A02_s1_w1051DAA7C-7042-435F-99F0-1E847D9B42CB.
The original TIFF, PNG mask and validation list have exact hashes/attribution in
the existing [fixture inventory](../../../tests/fixtures/manifest.json), under
fluorescence_evaluation; ordinary reader downloads are unchanged. The offline
material-staging guard passes all3 original selected files. Annotation decoding
and comparison against declared split/merge/measurement tolerances remain open.

Acquisition is retained locally at build/feature-expansion/bbbc039-inputs:
capture.py/capture.json record member paths, CRC-checked lossless extraction,
ETags, archive sizes and transfer counts. HTTP ranges transfer441,446 image-
archive bytes and63,881 mask-archive bytes; metadata.zip is17,816 bytes. No full
77,915,748-byte image archive was downloaded. Selected staging files total742,334
bytes. Unknown/restricted models/code were not fetched; the upstream decoder gist
is reference only, not imported code. The first metadata inspection encountered
an Apple resource-fork entry; explicitly inspect the named metadata members and
skip __MACOSX instead of treating those auxiliary bytes as UTF-8 scientific input.

Next F01 work: capture series/Z/T/ROI/calibration/source association in the native
request, reuse existing jobs/currentness and object commands, store the exact
recipe/results, wire consumers, and qualify an admitted annotated panel with
explicit context/boundary/seam behavior. Do not route raw masks through an RGB
ScientificRasterKernel by fabricating RGB provenance. Reuse its contour machinery
through an appropriate typed raw-raster adapter. F01 remains unchecked; overall
completion stays7/22. L01 final package membership also remains open.

F01 source-bound continuation:
[fluorescence_cell_workflow.hpp](../../../cpp/include/qupathxx/fluorescence_cell_workflow.hpp)
adds immutable native request/run types and an owner-thread job over the tested
raw service. RawAnalysisRequest supplies existing document/ROI/calibration
authority; the existing raw lattice source snapshot and file/metadata guards
supply actual source currentness. Object-scoped runs capture object-specific
provenance; ID0 retains explicit source-only provenance. No RGB raster/projection,
Python or second scientific implementation is introduced.

The request freezes original channel block, series/Z/T, level-zero core/crop,
ROI and effective calibration, expands a bounded source context and reserves
snapshot/raster/measurement work before decoding. Available context must cover
the Gaussian radius per axis; source edges clip it explicitly. One original raw
region is read, with finite ROI centers selected on the owner-captured geometry.
Immutable output retains region/core and a native local-to-global pixel-center
mapping, typed raw metadata, provenance, masks, measurements and exact source/
recipe/output hashes. These content hashes do not authorize publication: a
private capture ticket binds the run to its request, and live currentness is
rechecked before later publication.

The job owns a single native worker and joins cancellation/destruction before
releasing captured owners. Workers never read a live Document. Owner delivery
rejects stale ROI/calibration/source/metadata; ordinary subsequent progress polls
use cheap document/source observations. The explicit publication guard rechecks
actual file versions and metadata without rescanning raw pixels. Completed
duration is captured on future delivery, including time after the last running
poll, and stays frozen afterward. No general scheduler or extra cache was added.

Native pure-raster and source-workflow CTest pass2 cases (0.25s). Final additional
source-workflow check after the terminal-duration correction passes (0.26s total).
Analytical synthetic source counters prove capture does no pixel IO, execution
reads raw once and no RGB fallback occurs. Cases cover original channel/Z/T
values, ROI/core/world centers, anisotropic document calibration, finite Gaussian
context agreement across crops, insufficient context, pre-IO work refusal,
independent document/capture authority, decoder-shape/errors, closed source,
metadata/ROI/calibration refusal and deterministic native read barriers for stale,
cancelled, ready and destructor-join paths. An independently authored uint16 OME
file proves raw values and display-independent result identity; appending to that
local fixture proves publication rejects actual file changes despite decoder
observations/cache. Original admitted fixtures are never modified.

The source-bound native job also runs the admitted CC0 BBBC039 TIFF and returns130
candidates. It checks native currentness but is still no annotation comparison.
Passing logs: workspace-root build/feature-expansion/f01-source-final and
f01-source-timing-final. Initial failures remain f01-source/f01-source-failure
and f01-source-r2. Native-only CI includes both affected targets; hosted execution
is not claimed. No new library, binding build, GUI/package or restricted material.

Corrections: the lattice helper's honest source-only provenance needed an explicit
object-specific capture for ROI runs. A later test reused an old request after
ROI Undo; native Undo advances object revisions, so recapture for the next job
instead of weakening currentness. The metadata oracle now starts from a proven
current capture, isolating metadata refusal from ROI staleness. Final review also
fixed terminal duration retaining the last running poll's value; a controlled
barrier proves elapsed time advances before terminal delivery.

Next F01: adapt the existing typed label contour machinery to these immutable
raw masks, publish/store native objects and exact recipes, wire GUI/CLI and perform
the admitted annotated comparison with declared tolerances. The pinned mask is
RGBA8; preserve its explicit annotation-channel semantics rather than silently
relaxing the existing NuCLS RGB8 decoder. No F01 checkbox or full tile/biological
acceptance is claimed from this source-job delivery.

F01 native contour/publication continuation: the typed raw-mask adapter reuses
the existing contour engine and exact cell-body containment normalization.
Native cell/fact publication is one undoable command; compact recipe/result
records also retain zero-cell runs through existing image properties. These
records regenerate results from the original source, rather than embedding masks
or claiming a complete provenance archive. Save/reopen replay checks exact saved
source/recipe/result identity. Full-cohort verification performs one shared
source-currentness check instead of repeating file checks per cell.

Affected native `fluorescence_cell_objects`, `fluorescence_cell_workflow` and
legacy `cells` CTest cases pass3/3 in1.96s; logs are retained under workspace-root
`build/feature-expansion/f01-objects-final`. The admitted real TIFF traces,
publishes and verifies all130 candidates (`real.log`), which is geometry and
persistence evidence, not annotation or biological acceptance. Tests cover
fractional ROI containment, original-channel moments, Z/T, absent cytoplasm,
atomic undo/redo, stale/cancel/budget refusal and zero-cell record replay. The
initial fractional-boundary failure remains under `f01-objects`; reuse the
existing zero-area normalization without weakening final containment predicates.
Local geometry checks use enabled GEOS; native-only CI additionally registers the
target and tests explicit refusal when GEOS is unavailable. Hosted execution of
that added target is not claimed here.

Next: wire native CLI and desktop consumers, then perform the admitted annotated
comparison under documented conventions and tolerances. F01 remains unchecked;
the plan remains7/22 complete,15 pending. No bindings/package rebuild, restricted
material admission or speed/accuracy superiority follows from these checks.

F01 native CLI continuation: `qupathxx_batch` now dispatches detection into a new
project, record listing and exact retained-run replay. The command layer reuses
native request/run/contour/publication/record authorities; it does not parse or
measure pixels. New-output publication protects original project, image,
companion and journal paths, and rechecks source currentness at commit. Record-
only mode and zero-cell outcomes need no GEOS contour tracing. Verification here
checks saved recipe/result identity, not later edits to observed cell properties.

Final affected `fluorescence_cli` CTest passes1/1 (0.30s total), including nonzero
original-channel/local-segmentation mapping and explicit crop with independent
nucleus sum2941, saved raw facts, list/replay, zero-cell retention, argument errors,
protected inputs/no overwrite, cancelled work and bounded-save failure. Logs:
workspace-root `build/feature-expansion/f01-cli-final-ctest.log` and build logs
`f01-cli-build.log`/`f01-cli-mapping-build.log`. The follow-up build added missing
channel/crop consumer coverage; no numerical kernel change or binding build.

The actual native batch executable runs help/detect/list/verify on the admitted
CC BY4.0 OME tubhiswt two-channel demo:832 candidates published,26 boundary flags,
exact replay digest, original project SHA256 unchanged. Machine-readable command
results are retained at `build/feature-expansion/f01-cli-real/results.json`.
These observations do not establish annotated accuracy or speed superiority.
The added native-only CI target is registered; hosted execution is not claimed.
Next F01: desktop integration and the admitted annotated comparison. F01 remains
unchecked; no source/SDK/package release qualification was performed.

F01 desktop continuation: compiled private `GuiFluorescence` reuses the native
request/job/run, tracing, atomic publication and compact record services. Analyze
and Workflow/Tools expose the tool. Controls select original channel block/local
segmentation channel, explicit Z/T, selected ROI or crop, units, threshold,
context and work limits. Nucleus/body preview is a separate bounded presentation
texture (maximum1024 samples per axis); masks/measurements remain original-lattice.
Preview is placed in level-zero source coordinates after the existing canvas cache
and hidden for a different display Z/T. Boundary counts and textures cache by
immutable run rather than scanning/reuploading every frame.

Keep traces contours off the UI thread. Owner delivery defers while edit/save/load
is blocked, rechecks capture/source currentness and publishes one undoable cohort.
The tool retains/removes compact run records and replays their exact identity;
missing cytoplasm remains absent. Record verification does not silently certify
later edits to published cells. No public Document/GuiOptions layout or binding
object changed. No new dependency or restricted material was introduced.

Final affected `gui_fluorescence` CTest passes1/1 (0.53s total), retained at
workspace-root `build/feature-expansion/f01-gui-final-ctest.log`; final build log is
`f01-gui-final-build.log`. Actual ImGui clicks cover Run, registered raw preview,
independent nucleus sum1541/body mean102, blocked trigger and ready-contour
delivery, atomic Keep/Undo, exact record replay/save/reopen, duplicate-publication
error/recovery, changed ROI, same-address Document replacement, image switching,
zero-cell Keep and cancellation without publication. A framebuffer pixel at an
independently known level-zero nucleus center verifies overlay registration.

The same actual tool runs the admitted CC0 BBBC039 TIFF:130 candidates are traced,
published, retained and exactly replayed; original TIFF SHA256 is unchanged.
`build/feature-expansion/f01-gui-real/real.log` records the run and preview captures
remain alongside it. Standalone `qupathxx_app --project ... --smoke` exits0 from
System32 with Windows-only PATH, leaving the admitted OME demo project unchanged
(`app-smoke.json`). Captures were inspected for readable controls and native mask
placement. These are consumer/geometry/persistence checks, not annotation accuracy.

This is working-tree evidence with concurrent desktop edits preserved, not a
frozen release identity. `f01-gui-real/source.json` and its small source snapshot
record the exact main GUI, panel, test, layout/catalog and CMake inputs; no matching
wheel/SDK/package claim is made. Added CMake GUI target is exercised locally;
hosted GUI execution is not claimed. Prior failed logs remain `f01-gui-ctest.log`,
`f01-gui-diagnostic-ctest.log`, `f01-gui-input-ctest.log` and
`f01-gui-delivery-ctest.log`, plus their corresponding build logs.

Corrections: avoid new private Document access; use native request authority for
staleness, including same-address replacement. Poll early with publication
blocked until the frame knows its actual edit/save/load state. Include pending
owner delivery in frontend busy, since worker completion alone can precede result
consumption; this was the intermittent missing-result root cause. Synthetic
hidden-window input uses explicit hover/event delivery. The stale-ROI test now
enlarges a parent instead of violating already-published child containment.
Terminal contour failures/cancellation no longer remain labelled tracing. No
assertion was weakened. Next F01: freeze annotation conventions/tolerances before
comparison and complete declared context/boundary acceptance. F01 stays unchecked;
overall completion remains7/22.

### F01 predeclared annotated-panel protocol (v1)

Frozen before reading annotation pixel values or computing comparison metrics;
canonical machine-readable policy is `fluorescence_evaluation.protocol` in the
existing fixture manifest. Keep the first official validation filename already
selected and the previous untuned Otsu256/sigma0/prominence0.5/min-area5/expansion3
pixel-unit recipe. No parameter search or validation-mask tuning belongs to this
pilot. If these criteria fail, retain the failure and select a separate open
training input before method changes; do not loosen the thresholds after seeing
results or substitute an easier validation panel.

The [official dataset page](https://bbbc.broadinstitute.org/BBBC039) supplies the
CC0 declaration and links its [decoding example](https://gist.github.com/jccaicedo/15e811722fca51e3ae90e8b43057f075).
Implement independently: strict RGBA8 PNG, take first channel, split nonzero
**equal values** into8-connected regions. Other color/alpha channels do not
choose labels. Keep all image-edge instances in evaluation and report their
counts; no border-object exclusion. Do not import the example code or relax the
existing RGB8 NuCLS profile.

Match nuclei at IoU>=0.50 with deterministic maximum-cardinality one-to-one
matching over sparse overlap pairs. Report TP/FP/FN, precision, recall, F1,
foreground Dice and matched mean IoU. A ground-truth split overlaps at least two
predictions, each covering>=10% of that ground-truth area; a merge overlaps at
least two ground-truth labels, each covering>=10% of the prediction area. Divide
split/merge counts by total ground-truth/prediction counts respectively. These
rates supplement matching; they do not hide unmatched instances.

Pilot gates: foreground Dice>=0.75, object F1>=0.70, split and merge rates<=0.20,
and median absolute matched nuclear-mean difference divided by annotation nuclear
mean<=0.35. Measure both masks on original uint16 DNA samples. Independently
recompute every predicted nucleus/body/cytoplasm sample count, sum, mean and
sample SD; native values must agree within1e-12*max(1,abs(reference)). Zero mean
with nonzero difference is unavailable/failing, never an invented finite relative
error. No matches fails the mean gate. Empty analytical metric cases have explicit
conventions and are tested separately.

This checks one useful pilot, not held-out population accuracy or body/cytoplasm
annotation (the annotations label nuclei only). Keep annotation-mask differences
separate from numeric reducer correctness. Full-image tile ownership/resume
remains D02; F01 still needs its own finite ROI/context/boundary contract checks.

### F01 first annotated-panel result: fails object F1

Protocol was committed as9a048d13 before annotation decoding/evaluation. The
native evaluator independently decodes equal-value8-connected RGBA-first-channel
instances, computes sparse maximum-cardinality matching and checks direct raw
uint16 moments against all predicted nucleus/body/cytoplasm masks. It reuses the
existing bounded LodePNG dependency without changing the NuCLS RGB8 profile.

| Frozen gate | First result | Outcome |
| --- | --- | --- |
| Foreground Dice>=0.75 | 0.935545504754228 | Pass |
| Object F1>=0.70 at IoU>=0.50 | 0.6833333333333333 | **Fail** |
| Split rate<=0.20 | 21/110 =0.19090909090909092 | Pass |
| Merge rate<=0.20 | 5/130 =0.038461538461538464 | Pass |
| Median matched nuclear-mean relative difference<=0.35 | 0.021731382210805245 | Pass |
| Native original-sample counts/sum/mean/sample SD | Independent two-pass oracle agrees within1e-12 relative | Pass |

There are110 reference and130 predicted instances:82 TP,48 FP and28 FN.
Precision=0.6307692307692307, recall=0.7454545454545455, mean matched
IoU=0.8709824792627062. All edge objects remain:23 reference/15 prediction border
instances. This discrepancy cannot be erased by excluding borders or relaxing
matching. No annotation-derived method tuning or extra validation search was run.
The complete first report, command/exit1 and exact original manifest bytes/hash
are preserved under workspace-root `build/feature-expansion/f01-evaluation-v1`.
`first-result.json` retains every matched pair and its raw mean comparison.

Analytical decoder/metric/original-sample oracle CTest passes1/1 (0.15s total),
`f01-evaluation-oracle-ctest.log`. Expanded finite boundary/source workflow CTest
passes1/1 (0.25s total), `f01-boundary-ctest.log`. Its new cases compare nucleus and
body labels, all compartment moments/validity, empty cytoplasm and boundary flags
for identical cores under sufficient versus larger context, a fractional ellipse,
calibrated source-edge crops and the full image. Known uniform pixel/marker means
prove no invented out-of-source context and original-sample counts. Insufficient
available smoothing support still refuses. These checks do not implement or
qualify D02 whole-slide tile ownership/resume.

The native-only CI registers the evaluator self-test, not an automatic real-data
download or hosted accuracy claim. Build failures remain `f01-evaluation-build.log`
(Windows near macro) and `f01-evaluation-build-r2.log` (missing explicit ROI sample
policy); both fixes use existing names/contracts without product or gate changes.
Only evaluator and source-workflow targets were built, not bindings/GUI/packages.

Next selected input is the **first official training filename**, chosen before
reading training pixels/results, separately recorded under the existing fixture
manifest's training section. Exact CC0 hashes and license evidence pass offline
staging3/3. Acquisition reused the bounded HTTP-range reader and existing metadata
ZIP, transferring476,499 bytes rather than full archives; receipt/script remain
under `build/feature-expansion/bbbc039-inputs/training` and `capture-training.py`.
The frozen small native recipe grid is sigma{0,1,2}px × prominence{0.5,1.5,3}px;
keep other v1 parameters and gates. Choose by training F1 among non-F1-gate-passing
candidates, breaking ties by Dice then lower sigma/prominence. If none pass, report
no accepted candidate. Freeze that choice before another original validation
comparison; retain v1 and disclose validation reuse rather than calling it unseen
held-out accuracy. Training bytes are admitted, but no training metrics/search
have run in this slice. F01 remains unchecked, total7/22 complete.

### F01 training selection frozen before validation follow-up

Native selection evaluated exactly the nine previously declared candidates on the
separately selected first official training panel. The first complete report and
exact input inventory are retained under workspace-root
`build/feature-expansion/f01-training-v1`; no validation candidate search was run.
One decoded annotation/raw reference is reused across the grid, with independent
original-sample verification for each result. Native selection/tie/no-candidate
self-tests pass1/1 (0.13s total), `f01-training-ctest.log`.

The declared rule selects index6: sigma2px/prominence0.5px, unchanged Otsu/min-area5/
expansion3/pixel units. Training F1=0.8533333333333334 and Dice=0.9528470709989004;
all non-F1 gates and original-sample moments pass. Candidate0 (old baseline) fails
its training split-rate gate and is excluded despite F1=0.805. No other candidate
is selected based on subsequent validation. `followup_protocol` in the existing
manifest freezes the selected recipe as qpx.bbbc039-pilot/2, links the complete
training report hash/input identities, and keeps every v1 acceptance threshold.
The original v1 protocol/failure remain unchanged. Follow-up validation has not
run at this freeze checkpoint; its reused panel cannot be called unseen held-out
evidence. No product default, new model or scientific kernel was changed.

### F01 selected pilot and workflow delivered

Freeze728aae2a precedes exactly one recipe follow-up on the original validation
panel, after training-only selection. First v2 report/command/manifest remain
`build/feature-expansion/f01-evaluation-v2`; no gate, matching/border convention or
panel was changed. V1 failure remains independently retained.

| Result | V1 untuned | V2 train-selected |
| --- | --- | --- |
| Sigma / prominence (pixels) | 0 /0.5 | 2 /0.5 |
| Predicted instances | 130 | 118 |
| TP / FP / FN | 82 /48 /28 | 82 /36 /28 |
| Object F1 (gate>=0.70) | 0.6833333333333333, fail | 0.7192982456140351, pass |
| Foreground Dice (gate>=0.75) | 0.935545504754228 | 0.922521339461589 |
| Split / merge rate (each<=0.20) | 21/110 /5/130 | 17/110 /4/118 |
| Median matched mean relative error (<=0.35) | 0.021731382210805245 | 0.03056354885393146 |
| Original-sample numeric oracle | Pass | Pass |

V2 mean matched IoU=0.8570614643734306, precision=0.6949152542372882 and
recall=0.7454545454545455. It retains all23 reference border instances and16
prediction border instances. Fewer false positives improve F1; recall does not
improve and foreground Dice decreases. Report those tradeoffs rather than calling
every output superior. This is a finite engineering pilot on reused validation,
not unseen-held-out or population accuracy, SOTA or QuPath equivalence.

The final affected evaluator self-test passes1/1 (0.12s total),
`f01-selected-evaluator-ctest.log`. Existing native object tests plus selected-real
extension pass in `build/feature-expansion/f01-selected-integration/native.log`.
All118 selected labels trace, publish and batch-verify; their exact native record,
geometry/facts and cohort survive project save/reopen and fresh replay. The real
extension accepts a test-only sigma; no product API/default/kernel changed.
The actual existing batch executable detects/publishes118 cells from a preserved
seed project, saves and verifies exact run SHA256
`4deb4bb86e9bb65fe6aae3ead3a83ab89878a7edee488ab53e1f92dbf95bb67a`, matching v2.
`results.json` records commands/results and confirms original TIFF/seed hashes
unchanged. GUI job/options/preview/Keep reuse their earlier scoped passing checks;
no repeat GUI/binding build or package qualification was needed.

F01 closes its declared bounded native workflow: analytical/raw-type/missing/
calibration checks, finite core/ROI/source boundaries, native job/cancel/stale
ownership, contours/atomic publication, records/undo/replay/save/reopen, CLI and
desktop consumers, and the selected admitted annotated pilot with fixed gates.
D02 whole-slide seam ownership/resume remains open, as do F02–F04 and the other
listed packets. Completion is8/22 with14 pending. Next deliver F02 typed marker
positivity/phenotype rules over immutable observed native measurements. L01 still
needs final source/package membership; proprietary, NC and unreviewed pools remain
excluded and distinct. No public release or broad-platform acceptance is claimed.

### F02 native marker-rule backend

The additive native rule API now supplies original-channel/compartment/statistic/
units/threshold gates, bounded topological AND/OR expressions and explicit
Positive/Negative/Unknown. Scalar snapshots bind intact F01 geometry/raw facts to
native source/document authority and selected-object revisions. Only relevant
measurements are retained, with flat gate-state buffers and reusable node scratch;
no feature forest, new workflow engine, pixel read, per-object recipe duplication
or Python scientific authority is added. F01's saved observation-integrity helper
is shared directly, not reimplemented or converted into current-image acceptance.

Publication uses existing apply_classifications, with its one-Undo/color/probability
policy and immutable geometry/raw facts. Exact qpx.marker-rule/1 recipes live once
in existing typed image properties; Keep/remove preserve unrelated metadata and
remain separate undoable commands. Rule metadata changes do not invalidate
unchanged observation captures. Input/result digests exclude cosmetic class/name
state but cannot replace private request/result authority.

Final affected `marker_rules` CTest passes1/1 (0.20s total), workspace-root
`build/feature-expansion/f02-native-final-ctest.log`; build logs are
`f02-native-build.log` and `f02-native-final-build.log`. Coverage includes the full
nine-entry AND and OR three-state tables, strict/inclusive/signed/zero thresholds,
NaN/Inf/absent/insufficient samples, all five native statistics, empty cytoplasm,
unmeasured channels, original-domain/units validation, DAG reachability/cycles/
limits, exact threshold-bit preservation, malformed/tampered records and the
maximal declared64-gate/256-node codec graph.

Enabled local GEOS integration captures independent two-blob/three-channel native
observations, including an all-NaN measured marker. It checks exact root/gate
cohorts, saved-rule replay, one compound class Undo/Redo, unchanged geometry/raw
facts, native color/probability policy, cancellation, stale/capture/owner refusal,
same-address Document replacement, empty-cohort no-op and **zero additional pixel
reads**. A real independently authored quantitative TIFF and native project prove
saved rule/class/raw-fact associations replay exactly after reopen. The actual
`classified.qpx`, `rules.qpx` and source fixture are retained under the latest
`marker-rules-*` directory in the integration cache.

The final test-only follow-up closes two named boundary gaps: edited body geometry
refuses capture of stale raw facts, and a valid-size100000-ID/64-gate request hits
its4MiB work limit before allocating its observation table/looking up IDs. No
scientific behavior changed after the initial pass. The native-only CI registers
the new target; without GEOS it explicitly runs pure logic/codec checks only.
Hosted execution and missing GUI/CLI behavior are not claimed. No new dependency,
material/model download, binding/GUI rebuild or package qualification occurred.
F02 remains open for thin native CLI/desktop consumers and their selected workflow
acceptance. Overall completion remains8/22; next wire those consumers, not another
rule evaluator or observation store.

### F02 native rule IO and CLI continuation

Shared native IO now parses/exports human qpx.marker-rule/1 JSON, loads verified
files, saves new files and formats typed result counts/cohorts. The existing
project/JSON capability remains optional; no Python parser or new dependency is
introduced. Parsing rejects duplicate/unknown keys, unsupported enums/types,
noninteger indices, invalid graphs and excessive1MiB/depth16 input. Exact logical
recipe/threshold bit identity survives JSON/file/project round trips.

Native cohort selection includes stamped fluorescence cells across the selected
image's planes and excludes unrelated cell types; corrupt matching publications
are retained for capture refusal. Preview/apply/list batch commands use the shared
marker evaluator. Apply saves a new project with original/rule/image/companion/
SQLite-sidecar path protection. The owner commit guard checks expected class
labels and exact unchanged saved observations, permitting revisions changed by
its own class transaction. Capture and post-publication verification share the
same input-hash stream. One final verification occurs at the new-project commit
boundary; no second scalar table or pixel analysis is constructed.

Affected `marker_rules` and `marker_cli` CTest pass2/2 (0.45s total), workspace-root
`build/feature-expansion/f02-cli-ctest.log`. Tests cover native post-Apply/class-
change/Undo/rename verification; exact Unicode/negative-zero JSON/file roundtrip;
strict schema/enum/units/key/index/graph refusal; no file replacement; guard/cancel;
native all/explicit cohorts; Unknown rather than Negative; actual preview/apply/
new-project save/reopen/saved-rule replay; protected missing journal/WAL/SHM paths;
input-byte preservation and bounded-save failure without output. Enabled local
GEOS exercises source/class integration; without it CI runs IO/list checks only.
The added target is registered in native-only CI; hosted execution is not claimed.

The actual batch executable passes help/preview/apply/list/saved-rule replay on
the admitted118-cell native project. A declared illustrative OR rule uses a DNA
mean threshold1000 and an unmeasured C1:1 Positive,117 Unknown,0 Negative; all118
class labels are saved and exact replay agrees. These are literal rule outcomes,
not validated biological phenotypes. Source project, original TIFF and rule-file
SHA256s are unchanged. Commands/output and identities remain under
`build/feature-expansion/f02-cli-real/results.json`, with the demonstration rule.

Initial compile stopped at three new test assertions lacking diagnostic strings;
`f02-cli-build.log` is preserved, with repaired build `f02-cli-build-r2.log`. No
schema/scientific assertion was weakened. Only affected native core/tests and
batch built; no binding/GUI/package build or new material download. F02 remains
unchecked for desktop rule editing/preview/publication/replay controls and their
selected workflow acceptance. Overall completion remains8/22.


## F02 completed: native desktop rules

A private compiled marker panel is reached through the existing fluorescence
tool, with no public Document/GuiOptions or binding layout change. It calls the
same native rule IO, cohort capture, scalar preview, class publication and rule
storage as the CLI. Gate edits preserve loaded graphs; explicit combine/add/remove
rebuilds the labeled topology. Counts distinguish Unknown and assignments are
virtualized. Keep recipe and Apply classes remain separate undoable actions.
Owned preview futures stay busy through owner delivery; source/document identity
is checked cheaply each poll, with row/content verification on document changes.
No per-frame full observation hashing or Python scientific logic is introduced.

Initial affected GUI tests pass2/2 (f02-gui-ctest-initial.log,2.46s). Extended
checks add actual malformed/valid JSON Load and saved-rule Load, then exact
class/project save/reopen verification. The first extended run exposes stale
button coordinates after an error changes layout; retain f02-gui-ctest.log.
Settling layout before reading bounds fixes test input only. The final changed
marker suite passes1/1 in1.64s (f02-gui-ctest-r3.log); the unchanged fluorescence
suite also passed in that preceding run. No scientific assertion was weakened.

The actual native ImGui test also passes on all118 admitted F01 cells. Its
illustrative DNA>-1 AND absent-channel999 rule yields118 Unknown, with gate
counts118 Positive and118 Unknown respectively. Actual Keep, file/saved Load,
blocked Apply, Apply/Undo, selected-cell scope, cancel-without-publication,
document replacement and exact saved replay pass. Desktop-applied classes and
unchanged observations verify after project reopen. Original project and TIFF
hashes are unchanged. Commands, exit codes and before/after hashes are under
workspace-root build/feature-expansion/f02-gui-real/results.json; the readable
panel capture is marker-panel.png. No biological phenotype accuracy is claimed.

The standalone app opens the desktop-saved classified project from System32 with
Windows-only PATH, exit0; the result receipt records executable SHA256. This is
local Windows/native evidence, not a new package or hosted platform qualification.
Builds use the single cache; the first GUI build also consumes pending unrelated
main-GUI changes, preserved unstaged. Later changes compile only affected private
panel/test/provenance inputs. Existing linker CRT warning remains in logs; native
execution passes. No new material, dependency, bindings or release package.
F02 closes under its selected rule workflow contract:9/22 complete,13 pending.
F03 dark/flat correction is next; L01 final staged membership remains open.


## F03 foundation: quantitative sources and dark/flat blocks

Native ChannelSource abstracts immutable quantitative metadata, original/derived
value domain, observed source-version SHA, work admission and currentness/read.
The adapter uses existing native raw IO and file guards, with no TIFF display
view prerequisite. It leaves all legacy TIFF/F01 code and identities unchanged.
A dark/flat source retains three original-source associations and exact options;
it emits Float64 blocks with per-channel explicit scales and denominator policy.
No data/model/dependency was downloaded and no Python path supplies values.

The first affected cached build succeeds (f03-source-build.log) and native
`channel_source` CTest passes1/1 in0.12s (f03-source-ctest.log). Independent
analytical readers have no TIFF view and forbid RGB reads. Checks cover negative
and true-zero output, validity intersection, channel-specific NaN/nonfinites,
strict denominator threshold, exact selected-channel mapping and Z/T values,
whole-region versus tiled seam equality, normalization/source identity and
refusal of recursive correction or mismatched field calibration. Exact-limit
work succeeds; one byte less refuses before any decoder pixel read. Invalid
storage/shape, decoder failure, cancelled reads and metadata/generation/closed
source changes refuse. Uint64 2^53 passes;2^53+1 refuses before precision loss.

The enabled synthetic TIFF branch proves adapter bytes/validity match existing
raw IO exactly, identity survives decoder reopen, and actual file mutation refuses
reading. The native-only CI target includes these tests; hosted execution is not
claimed. Read work covers captured native metadata and simultaneous buffers,
not process RSS, preexisting decoder caches or a throughput superiority claim.
Initial subset is level-zero correction with identical spatial/C/Z/T axes,
channel names/calibration, explicit normalization and no implicit broadcasting.

This is the F03 source/kernel slice, not completion: retained recipe/reopen,
CLI/desktop controls and explicitly derived analysis remain. Overall9/22 stays
unchanged. The source SHA identifies observed inputs/recipe, not stored output
pixels. No original-sample result is relabeled as corrected or vice versa.


## F03 continuation: retained correction recipes and CLI

Native DarkFlatRequest captures a TIFF signal document and explicit field readers,
source/series/version identities and exact options. Worker block reads use owned
immutable native sources; owner controls reuse RawAnalysisRequest identity without
public Document/friend/layout changes. Typed image properties store one versioned
recipe, not copied corrected pixels or modified original-channel observations.
Keep/Remove use existing metadata Undo and preserve unrelated values. Source,
recipe-record and evaluated-block identities have distinct meanings. Initial
saved-source reopening supports native TIFF only; block IO remains generic.

The first native build/test passes1/1 (f03-workflow-ctest.log,0.28s). Review
strengthens the changed-field Keep test so it removes the existing record before
attempting Keep; it now requires WorkflowInputChanged rather than accidentally
accepting a duplicate-record refusal. The final suite passes1/1 in0.21s
(f03-workflow-ctest-r2.log). Independent constant dark/flat TIFF fields establish
signed/zero corrected values and exact counts. Checks include worker reads,
owner-only controls, bounded work/cancel, typed negative-zero/identity integrity,
Keep/Remove Undo, actual project save/reopen source/recipe/block replay,
same-address replacement and actual changed/missing fields.

Native CLI Keep/list/verify/preview shares those authorities. Its tests establish
new-project save, exact native/CLI block identity and counts, original/field and
missing SQLite journal/WAL/SHM path protection, no replacement, input-byte
preservation and no output on cancellation/save-budget failure. Invalid duplicate
options refuse. No frontend correction or per-sample Python logic is introduced.

The actual batch executable also passes Keep/list/verify/two independent Preview
commands from System32 with Windows-only PATH. Both previews yield exact block
SHA261840f53fa38f34fa461b773f49ccaf56cb56894e387e688d91d9f3329566db,
48 valid pixels and48 finite/0 nonfinite samples per selected channel. Before/
after project/signal/field hashes are unchanged. Commands, stdout and executable
identity are in workspace-root build/feature-expansion/f03-cli-actual/results.json.
These are independently authored synthetic TIFFs, not biological acceptance data.
The copied flat retains the fixture's harmless trailing mutation byte; it is
admitted as a new observed source, never substituted into the old saved recipe.

One active cache builds only affected core/test/batch inputs. Concurrent UI header
changes trigger extra CMake regeneration, preserved in logs; a later live compiler
belongs to napachan and is not a lock on this cache. Existing CRT linker warning
remains recorded. No GUI/binding/package build, dependency/data download or broad
segmentation rerun. Native-only CI includes the new suite; hosted execution is
not claimed. F03 stays open for desktop controls and explicitly derived analysis;
overall9/22 complete,13 pending.


## F03 continuation: corrected cell analysis and marker domains

An additive fluorescence request overload consumes a retained DarkFlatRequest.
It reuses the existing segmentation/reducer, jobs, ROI/context, contours and
atomic publication, not a second analysis pipeline. Original constructors keep
their exact input/recipe/hash path. Corrected runs expose a distinct measurement
domain and correction reference; source input, serialized recipe and publication
profile use explicit version2 interpretation. Field associations remain once in
image properties. Native deletion refuses references from cells or run records;
pending capture refuses a missing/changed correction association. Polls cache
unchanged immutable properties pointers, without per-frame recipe parsing/IO.

Marker rules support explicitly named original/corrected domains. Native cohort
selection excludes known opposite-domain cells and retains malformed/unknown
matching publications for refusal. Explicit wrong-domain IDs refuse. The marker
CLI/private panel use the same native helper and the panel labels the actual
loaded domain. Historical scalar classification does not remeasure fields.
Fluorescence CLI detection opts in with a retained --correction; its saved output
reports domain/reference and protects correction field paths.

One affected cached build succeeds (f03-cells-build.log), and all8 affected tests
pass in2.96s (f03-cells-ctest.log): original source workflow/object/CLI, correction
recipe workflow, native marker/CLI, corrected fluorescence and existing marker
GUI. The early corrected end-to-end check also passes. The GUI result establishes
default-route compatibility, not correction desktop authoring acceptance. The
new native test is included in native-only CI; hosted execution is not claimed.

Independent15x9 TIFF signal and constant fields yield the same14-pixel nucleus /
27-pixel body geometry as the raw route at the explicitly adjusted cutoff. The
corrected nuclear C0 sum is1401 versus original1541; body C0 mean92 and cytoplasm
C1 mean2513/13-20 agree with the analytical oracle. Tests establish distinct
identities/profiles, finite signed samples, empty outcomes, work/cancel limits,
shared-reference retention/stale refusal, one Undo publication, source/cell/run/
project replay and corrected marker persistence. At cutoff105, the original cell
is Positive and corrected cell Negative; explicit cohorts keep them separate.
Both native and CLI paths preserve input bytes. Changed fields refuse corrected
publication while the default original route remains independent.

The actual batch executable verifies the retained real F01 project:118 cells and
exact original run SHA4deb4bb86e9bb65fe6aae3ead3a83ab89878a7edee488ab53e1f92dbf95bb67a.
It also detects/publishes/reopens one corrected cell from the existing synthetic
8x6 recipe example, verifies its exact run replay and previews the explicitly
corrected marker cohort. Windows-only PATH/System32 execution succeeds; all
project/image/field/rule SHA256s remain unchanged. Commands, stdout and executable
identity are in workspace-root build/feature-expansion/f03-cells-actual/results.json.
This is source-scoped compatibility and synthetic analysis evidence, not a new
biological-accuracy study, throughput claim, release or binding qualification.

Waste: shared native algorithms and existing storage/publication; no field recipe
copies on every cell, raw metadata copy on the default capture, new scheduler,
Python science, dependency/data download or binding/package campaign. Public API/
recipe changes justify affected original-route checks and one retained real
identity verification; no pilot/training sweep repeats. No failed build/test or
retry. Concurrent UI source is preserved. Next finish correction desktop controls
on these settled APIs; F03 remains unchecked, overall9/22 complete,13 pending.


## F03 completed: native correction desktop workflow

A private compiled tool, reached from Fluorescence cells, opens fields on native
workers and captures Document on its owner. Saved load and CLI replay share the
same native restore identity check; preview reads the delivered owned block API.
Explicit Keep/Use selects a retained correction for the existing fluorescence job.
Default runs stay original. Marker editing now offers the explicit measurement
domain. Preview shows crop/channel/Z/T, validity/finite counts and actual signed
pixel values, with separately labeled presentation range and bounded grayscale
texture. No frontend correction/reduction or Python science is introduced.

Affected native restore/corrected suites pass2/2 in0.81s
(f03-gui-native-ctest.log). Final affected GUI suites pass3/3 in5.96s
(f03-gui-ctest-r3.log): correction, existing fluorescence and marker controls.
The correction case proves missing-field errors, worker/blocked owner delivery,
negative C1 preview (-184 at the named crop origin), recipe Keep/Undo/Redo/saved
load, actual Use and corrected Run/Keep, cell Undo/Redo counts, exact project
reopen/replay, original-domain switch, corrected marker Preview/Apply/Undo,
referenced-remove refusal, stale fields, cancellation and replacement.
Corrected C0 nuclear sum1401 and C1 nuclear sum-1259, plus cytoplasm C1 mean
2513/13-300, agree with the independent field oracle. Counts and retained run
labels reconcile after Undo/Redo without per-frame fact hashing.

The initial GUI run catches a real metadata view cache issue: Keep followed by
Undo can return to the cached pointer before the UI observes the intermediate
state. Own metadata commands now force list reconciliation; loaded/last-kept IDs
restore truthful labels. The next failure frame shows a test-only focus error
when two hidden windows reappear: parent covers child. Reopen through the actual
parent button, without changing assertions or product behavior. Preserve
f03-gui-early-ctest.log, f03-gui-ctest-r2.log and their fixture/frame evidence.
Failure diagnostics now retain states/errors and a screenshot. No science gate,
threshold, tolerance or assertion was relaxed.

A handoff run repeats the actual native UI course but stops before deliberate
fixture mutation. The resulting source/project remains reviewable and replayable
under workspace-root build/feature-expansion/f03-gui-handoff. Both its native UI
driver and standalone app pass from System32 with Windows-only PATH. Native
project, signal, dark and flat hashes stay unchanged; commands, exits and executable
identities are in results.json. The inspected signed-correction-preview.png shows
readable negative value, domain, channel, counts, crop/plane and display range.
The full CTest route still executes all source/cancel/replacement guards.

Builds use one cache and preserve concurrent UI changes. Wait for confirmed cache
owner process195808 to finish rather than starting another build. Cache immutable
recipe lists and selected IDs, discover unused-tool defaults lazily, and refresh
publication counts only on structure changes. Repairs rebuild private controllers
or the test and relink affected consumers, retaining compatible expensive objects.
Native restore checks were reused after UI-only repairs. No binding/wheel/package,
new dependency/data/model or repeated F01 accuracy campaign. Existing native
source/legacy identity receipts remain valid under their own stated scope.
F03 closes locally under the declared TIFF/level-zero/matching-field contract:
10/22 complete,12 pending. F04 linear compensation/unmixing is next.


## F04 native unmixing foundation and typed matrix record

F04 is still open. Native `LinearMixingMatrix`, `LinearUnmixModel`, block
evaluation and `linear_unmix_channel_source` are implemented, including strict
versioned ImageDataValue matrix records. Numerical convention is measured =
matrix * components; signed residuals are measured minus reconstructed. The
prepared thin SVD is reused, with full-column-rank/condition admission and no
clamping or automatic spectrum estimation.

The existing OpenCV4.11.0 SDK is reused, source commit
31b0eeea0b44b370fd0712312df4214d4ae1b158 and archive SHA256
bb15d5c85bab38c1377c0c7dd7ff201b078bcdb41cc4ff9f85ac61aa56b5f631.
The selected static core library and four notices match its retained manifest;
IPP/nonfree/Python and other optional runtimes are disabled. Local receipt
workspace-root `build/feature-expansion/f04-native/backend.json` records the
checks. A separate core-only CMake algebra option avoids requiring ML training.
The capability definition stays source-local rather than rebuilding all core
objects. No new dependency, dataset or model was downloaded.

Independent native oracles establish square signed mixtures, noisy rectangular
least squares and residual signs, row/column permutations,1e-150/1e150 scaling,
rank/condition refusal, nonfinite/overflow and validity behavior. Source checks
prove exact tiled/subset equality, direct contiguous versus sparse input groups,
no gap reads, corrected-source composition, pre-read work refusal, integer
precision refusal, cancellation and source/file/metadata guards. Shared raw
sample dispatch preserves F03 behavior without per-pixel variant dispatch.

Typed record checks preserve exact signed-zero coefficient/hash identity through
immutable native properties, admit a64x64 matrix within existing property limits,
and refuse unknown/missing fields, schema/orientation/domain changes, malformed
dimensions, wrong scalar types, bad indices, oversized labels and nonfinites.
These tests run before optional-backend dispatch; parsing does not factorize.

Initial affected native tests pass2/2 in0.35s. After adding the typed record, the
affected cached build and tests pass2/2 in0.23s (`records-build.log`,
`records-ctest.log` in that receipt directory). The initial foundation executable
also passed from System32 with Windows-only PATH (`standalone.json`, exact
executable digest); that receipt precedes the typed-record addition and does not
claim its standalone qualification. No product build/test failure or retry.

Work is bounded to requested outputs and actual selected input groups, using
one block workspace rather than per-pixel allocations/solves. No GUI, bindings,
packaging, F01 biological campaign or speed comparison was rerun. Pending F04
scope: retained source associations, exact project replay, derived analysis and
marker domains, matrix file IO and CLI/desktop controls. Overall remains
10/22 complete,12 pending.


## F04 native retention, exact operator replay and CLI

Native `LinearUnmixRecipe`/`LinearUnmixRequest` now retain typed matrix, signal
locator/series/source identity, optional retained correction reference, algorithm,
prepared operator and input/derived source identities. Prepared model identity
includes matrix, pinned OpenCV4.11.0 thin-SVD algorithm, inverse coefficient bits
and condition, computed once. Derived source identity explicitly uses
`qpx.linear-unmixed-source/2`; output domain stays `qpx.linear-unmixed-samples/1`.
Earlier foundation source-schema1 receipts precede retained project support.
No accepted retained project identity was silently reused; TIFF/F03 identities
and numerical kernels are unchanged.

Keep/Remove use existing image-properties Undo. Corrected capture requires its
parent to be retained, and referenced correction removal refuses. Cheap owner
polls cache immutable parent properties rather than parsing/hashing each frame.
Workers read owned sources without Document access; final publication verifies
live document/calibration/source/parent. Replay compares exact stored recipe and
operator identity, not only numeric closeness, and refuses orphan/corrupt sources.
Initial automatic source reopening remains native TIFF.

Native tests independently recover original components (6*i-200)/7 and
(4*i+800)/7, and corrected components (6*i-240)/7 and (4*i+680)/7 for48
synthetic TIFF pixels; signed residuals are zero within the declared1e-11
relative/absolute floor. Same-backend project replay compares sample bytes exactly,
including retained signed-zero coefficients. Owner/worker, Undo/Redo, parent
reference removal/orphan/corruption/domain, calibration/document replacement,
source/field changes and cancellation cases pass. Pure recipe schema checks run
before optional numerical dispatch.

Affected native suites pass4/4 in0.78s (`f04-native/workflow-ctest.log`). The first
build compiled the service but found a test-only metadata shared-pointer access
error; preserve `workflow-build.log`. Correct pointer access and rebuild in the
same cache (`workflow-build-r2.log`); no assertion/contract relaxation.

Bounded native JSON/file IO adapts the same typed matrix schema, with duplicate,
unknown/malformed fields, depth/node/byte bounds, exact signed-zero round trips
and unsafe integer conversion refusal. Thin CLI Keep/list/verify use the same
requests/retention/replay and existing guarded native project save. Tests prove
original/corrected routes, retained replay after authoring JSON is moved, exact
native recipe equality, protected project/source/field/matrix/sidecar outputs,
existing-output refusal, save budgets and cancellation. The updated workflow/CLI
suite passes1/1 in0.47s (`f04-native/cli-ctest.log`); reuse prior unchanged kernel/
correction checks instead of rerunning them after consumer-only changes.

The handoff driver leaves valid sources/projects before deliberate stale-fixture
mutation; full CTest still executes those guards. From System32 with Windows-only
PATH, that driver and actual batch list, original/corrected verification, Keep and
new-project verification all exit0. Exact native source/operator identities match
and input project/TIFF/JSON hashes are unchanged. Commands, executable hashes,
outputs and input hashes are in workspace-root
`build/feature-expansion/f04-cli-handoff/results.json`; valid projects/fields remain
there. This proves native CLI mapping/runtime, not a new science/performance claim.

No GUI/bindings/package, new dependency/data/model or biological campaign. One
verified live GUI build (process145116) temporarily owned the shared cache; no
competing build was started, and the handle was confirmed missing before the CLI
build. A redundant read-only corrected verification in the actual runner was
unnecessary; future callers can reuse its returned parent ID. F04 remains open
for native derived cell/marker integration and desktop authoring/preview/selection.
Overall stays10/22 complete,12 pending.


## F04 native unmixed cells, recipe-scoped markers and CLI

Cell requests now consume explicitly retained `LinearUnmixRequest` sources while
using original measured-channel authority for source/ROI capture. Component and
residual output selection is validated against derived axes, including outputs
beyond the measured channel count. A channel-specific ROI plane refuses rather
than being interpreted as a component plane. Existing segmentation, compartments,
contours, native jobs and atomic publication are reused. Version3 input/cell/run
records identify the unmix recipe; original/corrected versions1/2 keep their
existing paths. Publications preserve output names and signed measurements.

The independent15x9 synthetic two-channel TIFF has measured values i and i+100.
Explicit matrix rows [1,-0],[2,1] recover DNA=i and Marker=100-i. Within the
selected ROI, threshold100.5 gives14 nucleus/27 body pixels, DNA nucleus sum1541,
Marker nucleus sum-141, body mean-2 and cytoplasm sum87/mean87/13. Explicit dark/
flat fields produce DNA=i-10 with threshold90.5 and nucleus sum1401; Marker and
geometry remain the same. These thresholds have a margin from integer samples
so tiny SVD rounding at an exact decision boundary is not confused with measurement
correctness. Numeric oracles use1e-11 relative tolerance with absolute floor1.
Residuals, component reorder/subsets and exact saved same-backend run replay pass.

Unmixed marker rules require `unmix_record`, binding component meanings to one
retained recipe. Version2 typed/JSON rules add the scope; legacy original/corrected
rules remain version1. Native cohorts omit intact other recipes but retain
corrupt/unknown matching-domain facts so capture refuses them. Wrong domain/order/
recipe refuses; absent scalars remain Unknown. Keep/Apply/Undo and exact project
rule/result replay pass. A retained marker rule also prevents unmix recipe removal;
cell/run references protect it independently, and its parent correction stays
protected. Capture/verify check stored associations without rereading pixels.

Additional checks cover missing associations, stale native-job owner delivery,
record/geometry/fact integrity, empty records, one-command Undo/Redo, work and
cancellation, explicit output axes, and changed correction fields. Both routes
are established current before intentionally mutating a field, isolating the
stale guard. Scalar capture caches immutable scope properties; default marker
requests skip that optional snapshot. Other-recipe integrity hashing is charged
to the cohort work budget before allocation.

The first affected build and8 native/CLI suites pass in1.92s
(`f04-native/cells-build.log`, `cells-ctest.log`). Review then identified the
cohort integrity temporary-work charge and added job-delivery coverage. The
changed marker/unmixed consumers and actual batch build pass, with2 affected
suites in0.58s (`cells-scope-build.log`, `cells-scope-ctest.log`). Reuse the first
unchanged segmentation/object/corrected/recipe checks; no repeated pilot or full
qualification. No product build/test failure or assertion relaxation occurred.

A native handoff driver leaves valid synthetic projects/fields before intentional
stale mutations. Full CTest retains all guards. The driver and actual batch
verify, recipe-scoped marker preview, unmixed detect/save/replay and marker Apply
all pass from System32 with Windows-only PATH. Reported run and marker identities
match native expectations; project/TIFF/JSON inputs are byte-unchanged. One actual
CLI replay of the retained real F01 project still produces118 cells and exact
run4deb4bb86e9bb65fe6aae3ead3a83ab89878a7edee488ab53e1f92dbf95bb67a.
Commands, executable hashes, results and unchanged input hashes are in workspace-
root `build/feature-expansion/f04-cells-handoff/results.json`; valid saved results
remain there. This is engineering/native consumer evidence, not new biological
accuracy, superiority or universal speed acceptance.

No new dependency/data/model, GUI/bindings build or package. F04 remains open for
desktop matrix authoring/loading/preview, explicit unmix/original/corrected source
selection and marker recipe-scope controls. Settle those private controller/test
interfaces before a combined GUI build, preserving independent layout work and
the default source routes. Overall stays10/22 complete,12 pending.


## F04 desktop completion: explicit matrix, signed preview and shared replay

The compiled private unmixing controller loads native matrix JSON or edits typed
drafts with explicit measured names/indices, component labels, domain, coefficients
and condition. New zero drafts require full-rank coefficients. Native workers
prepare one SVD and open correction fields; owner capture reuses that immutable
model through new prepared-request/restore overloads. Saved unmixed fluorescence
replay also opens/prepares asynchronously and calls native preloaded-source replay,
with exact recipe/operator identity checks and no repeated field opening/solve.
Native originals/correction/default identity paths remain unchanged.

Preview reads bounded owned channel sources asynchronously. It shows signed
component/residual values, explicit crop/output/Z/T, validity and finite counts,
with a separate display range and bounded320px presentation. Keep/Remove/load and
Undo use native metadata authorities. Source controls explicitly choose original,
corrected or unmixed; derived measurement labels use output indices/names. Marker
editing exposes all three domains and an explicit retained unmix scope, calling
the same native rule cohort helper. GUI Pimpl layouts remain intact; concurrent
layout source edits are preserved. No per-frame recipe parsing/hash/factorization.

The actual native ImGui course verifies malformed JSON and singular drafts, a
real coefficient edit from2 to1 and Apply, blocked owner delivery, preview value90
changing to-15, exact retained coefficient/signed-zero replay, Keep/Remove/Undo/Redo
labels, explicit Use/Run, signed nucleus/body/cytoplasm values, cell/run Undo/Redo
and exact reopened asynchronous Replay. Original-domain switch, marker scope combo
and Preview/Apply/Undo, referenced deletion refusal, changed fields, cancellation
and same-address replacement pass. Inspected signed-unmixing-preview.png is
readable: Marker output1 near-15, corrected input/unmixed output domains, crop
10,7 /3x1 /Z0T0,3 valid/finite and0 nonfinite, range-32..128.

All6 affected native/GUI suites pass in10.20s (`f04-native/gui-ctest.log`):
prepared model/source retention, unmixed cells, new desktop course and existing
fluorescence/marker/correction desktop routes. The initial build compiled core
but failed linking a native test with LNK1181 while another cache build was live.
Preserve gui-build.log. After confirming prior live handles terminal/missing and
core library present, the cached retry succeeds (`gui-build-r2.log`) without
contract/assertion changes. This is observed transient shared-cache availability;
no blanket scientific failure or successful concurrent-build policy is claimed.

Handoff mode stops before deliberate fixture mutation; full CTest retains the
guards. That driver, standalone app, and native CLI list/verify all exit0 from
System32 with Windows-only PATH. Saved run SHA is
d86425f01c748565438c171ff1095b1aec03c9800230e0d933e3c43082db76a0, with one
unmixed cell. Project, TIFF fields and authoring JSON hashes remain unchanged.
Commands, executable hashes and input hashes are in workspace-root
`build/feature-expansion/f04-gui-handoff/results.json`; its valid project/fields
and inspected signed-unmixing-preview.png remain available. PNG conversion is
lossless against the original captured PPM pixels.

F04 closes locally under its declared small full-column-rank linear, level-zero,
saved-native-TIFF source contract. No new dependency/data/model, bindings, package,
biological campaign or speed claim. Overall11/22 complete,11 pending. Next D01:
bounded disk-backed expression queries using the existing full sparse source and
native import services. Final L01 membership and SDK/release remain open.


## D01 native indexed expression foundation

Native ExpressionMatrixSource and streaming Matrix Market/axis import reuse
pinned SQLite3.54.1. STRICT sparse tables index (feature, observation); counts use
uint64 eight-byte blobs, never doubles or signed SQLite counts. IDs are unique;
display symbols may repeat. Qpx headered single-column axes and explicit Tenx
headerless ID/name/type plus barcode profiles remain separate. Existing selected-
gene parser contracts and identities are preserved.

Inputs stream once in64KiB blocks with bounded lines. Private staging commits
before bounded sequential atomic-export copy publishes a new-only sidecar.
Failure/cancellation cannot advertise incomplete output. Source identity binds
input/profile manifest and exact store bytes; per-gene integrity covers canonical
entries and axes. Opening fingerprints the store and compares any supplied
expected digest. Queries guard versions and verify full selected gene chunks
before cohort filtering; they scan no other gene and never revisit original text.
Catalogues are paged; immutable sources use separate per-query connections.
Accounted work includes buffers/cache reservations, not process RSS.

Workspace-root build/feature-expansion/d01-native/build-r3.log completes the native
build. ctest-r3.log passes expression_matrix in0.42s; earlier ctest.log retains
passing existing expression and spatial_counts suites. New coverage includes
unsorted sparse/cohort/explicit and implicit zeros,2^53+1 and uint64-max, IDs/pages,
work/store bounds, malformed/duplicate/truncated data, cancellation, atomic output,
corruption, WAL refusal, changed versions and exact relocation/reopen with text
inputs offline. No failed assertion was dropped. ctest.log/ctest-r2.log preserve
two observer failures: cached pages hid staged-file growth, then Windows sharing
blocked the attempted write. The repaired watcher proves it fired. Explicitly
denied writes must preserve input and yield a complete stable snapshot; actual
changes and cancellation refuse publication. Linux CI includes this suite; no
local Linux pass is claimed.

The retained CC BY4.0 10x V1_Adult_Mouse_Brain source from
[the existing demo receipt](../real-data-demo/README.md) was reused, without new
downloads or restricted material. Workspace-root build/feature-expansion/d01-real/
contains real.json, results.json, stdout/stderr and mouse-brain.qmx. Native execution
from System32 with Windows-only PATH exits0. Orchestration only captures native
reports and hashes inputs; before/after matrix/features/barcodes hashes agree.
An environment-key casing setup error occurred before native launch; its corrected
run is the sole full import.

| Observation | Actual result |
| --- | --- |
| Matrix source bytes |197229475 |
| Features / observations / entries |32285 /2702 /16031101 |
| Store bytes |354279424 |
| Import elapsed, this host/run |40426.1ms |
| Peak accounted work,256MiB request |139078440 bytes |
| Independent six-gene comparison |All2702x6 counts exact;14550 nonzeros |
| Additional gene |ENSMUSG00000051951; exact result after reopen |

Store SHA256:
`79f407bf9b0810f417410961dd79db6a6ce1ff8999e085d1ef8f7865eaca7401`.
Public source SHA256:
`77b62ca7dbeb474a6df7a0cc1af9884c3a72069f15e677d145455e66ef2cf044`.
Additional gene block SHA256:
`740e8ed8202c8a188533f687a486925a2570570347e5c998a9ff27685b9872b6`.
results.json binds executable and all three input hashes. The store is a lossless
index derived from those admitted counts/axes; retain CC BY4.0 attribution and
source lineage for any later distribution. This is local evidence, not a newly
bundled fixture; final actual staging membership remains L01/P02.

No QuPath speed, biological, process-RSS, GUI, installed SDK or binding acceptance
is claimed. D01 stays unchecked for project-relative/owned associations, verified
project relink and backed import/gene consumers:11/22 complete,11 pending. No GUI,
binding/package build or repeated fluorescence campaign. Next settle project and
gene-view request contracts before the affected consumer build.


### D01 project association persistence

Typed expression_matrix_project.hpp APIs retain native sources in existing
ImageDataProperties with one undoable command, preserving unrelated metadata.
Scientific identity is the sealed source digest; location/ownership changes do
not substitute new counts or axes. External locators are absolute live and
relative on disk where representable; missing external files remain identifiable
and savable but refuse opening. Exact reopened sources validate dimensions/domain
and both file/source digests. Relink accepts exact content only.

Owned retention requires an already saved project and creates a new-only exact
copy in .qpx-expression/<file-sha256>.qmx beside it. Relative owned locators remain
stable when the bundle moves. Save As and snapshot saves ensure the destination
copy exists before the project commits; conflicting existing bytes refuse without
overwrite. Normal same-location saves do not rehash/copy the full matrix. Keep,
Remove and Undo never delete shared sidecars. A failed project commit may leave a
complete unreferenced owned copy; it cannot advertise an incomplete matrix.
Serialization hooks validate typed locators and preserve old projects with no new
association. Project save refuses overwriting associated matrices or hardlinks.
No observation objects, inferred coordinates or sample values are added by Keep.

Workspace-root build/feature-expansion/d01-native/project-build.log succeeds.
project-ctest.log passes expression_matrix_project0.24s, expression_matrix0.41s and
existing project0.43s, total1.22s. New native tests exercise exact2^53+1 values,
external/owned relocation, explicit verified relink/Undo, original-source offline,
owned new-file Save As, native async snapshot save, preserved notes, missing
external metadata, conflicting copies, malformed stored paths, cancellation,
changed-count refusal and source/alias protection. Owned data in a destination
project opens without the original file or original project folder. Tests retain
fixtures under the shared cache expression-project-*; no copy temporaries leak.

The initial shared-cache gate found live GUI build handles; no competing build
was launched. After those handles disappeared and a fresh full gate cleared, one
native batch passed. No binding/package/GUI build or repeat full197MB import.
D01 remains open for explicit spatial associations and backed import/gene-view
consumers;11/22 complete. This is native persistence evidence, not SDK/release or
new biological/speed qualification.


### D01 explicit spatial join and native gene-view integration

ExpressionMatrixSource now batches observation-ID lookups through one connection/
prepared statement. ExpressionMatrixSpatialSnapshot joins explicit barcodes to
axis indices, preserving caller row/object/coordinate order; no positional guess.
Duplicate barcodes/object IDs, absent IDs, invalid coordinates and over-budget
work refuse. Snapshot identity binds the sealed matrix, coordinates, cohort,
frame/units and plane. Selected ExpressionDatasets use that joined lineage as
their source identity; dataset_frame() supplies the matching existing native
registration contract. Thus changing units/provenance with identical numeric
coordinates cannot reuse a scientific identity.

Point detections use qpx.expression-matrix-points/1 metadata with count source,
barcode and explicit image-frame digest. No sample values are copied into double
measurements. Owner capture validates point shape, frame/calibration and plane;
project matrix removal refuses referenced observations. Existing native object
and image-data persistence/Undo retains point/frame/source associations. Selected
projection uses indexed sparse reads and allocates only requested feature columns,
not the full source. Implicit zeros remain measured; raw uint64 values stay exact.

The existing GeneViewSession accepts immutable spatial snapshots. Its existing
worker resolves selected stable feature IDs, obtains their values and uses the
same legend/transform/color service. Generation coalescing, cancellation, clear/
replacement and current-source checks govern backed work; no new scheduler or
Python path. Source file fingerprinting is retained across gene changes. The
legacy selected-dataset bind route remains supported. Actual desktop catalogue
and backed import controls are still pending.

Workspace-root build/feature-expansion/d01-native/spatial-build.log succeeds;
spatial-ctest.log passes all5 affected suites in1.16s: new spatial matrix, matrix
project, matrix storage, expression and gene_view. New cases cover deliberately
scrambled observation/object/feature order, exact2^53+1 counts, measured zeros,
original text offline, empty cohort, malformed/duplicate/unknown IDs, work/cancel,
rapid latest-generation changes, comparison legends, absent gene errors, pending
clear, legacy rebinding, source mutation, point/frame save/reopen, calibration
Undo and referenced removal/Undo.

Boundary review found the need to bind typed frame semantics rather than just
frame-name text; no failing assertion was removed. spatial-build-r2.log succeeds,
and spatial-ctest-r2.log passes the changed suite0.13s with same-numbers/different-
units identity and existing registration-field reuse checks. Other passing tests
cover unchanged paths and were not repeated. No full real-matrix import, GUI,
bindings/package build, dependency or material download. D01 stays unchecked,
11/22 complete. Next extend the existing import preview/controller to the indexed
source and reuse the native gene session behind an actual paged desktop catalogue.


### D01 native backed import and first-save ownership

BackedSpatialImportRequest/Preview reuse the native SpatialImportJob, shared base
image/project selection, unit conversion/alignment, coordinate parser and atomic
new-project publication. Existing selected-count request/preview layouts remain
unchanged. Preview opens the indexed source once, verifies exact barcodes and
explicit frame/plane/image bounds, and publishes no files. Count-free detections
retain original coordinates/units and recipe provenance. Distinct coordinate-frame
IDs keep overlapping barcodes in separate scopes; an identical import scope refuses
duplication. One count association is reused, with exact ownership upgrade when
requested. Shared parser/default behavior is covered by existing spatial tests.

Owned mode now supports an unsaved project: keep a verified absolute origin only
in unpublished native state, then copy/hash a canonical owned child before first
project commit. Persisted owned locators remain relative canonical children;
absolute stored owned paths refuse. This supersedes the earlier saved-project-
first prerequisite. Unsaved recovery resolves its captured owned origin, and its
first primary save creates independent destination ownership. No preliminary
project save, second publication, sidecar deletion or unsafe copy overwrite.
Common point frame metadata/hashes are prepared once, not recomputed per object.

Workspace-root build/feature-expansion/d01-native/import-build.log preserves a
new-test include failure: JSON is intentionally private to core. The test now
links the existing pinned JSON target; import source/test share the existing
project feature boundary. import-build-r2.log succeeds. Existing expression-matrix
project/spatial, spatial_counts and project suites pass1.52s in
import-regression-ctest.log. Review also moved the cheap output collision check
before input verification, matching legacy behavior when inputs later go offline;
import-build-r3.log succeeds and import-ctest.log passes backed_spatial_import0.30s.
No assertion or source-integrity check was relaxed.

New native tests cover preview without files, scrambled2^53+1 values, full count
association, first-save owned copy/reopen with original store offline, calibrated
physical-coordinate conversion followed by alignment, existing annotations and
async worker publication, scope separation/duplicate refusal, ownership upgrade,
unknown/duplicate/malformed coordinates, expected hashes, plane/bounds/work/cancel,
stale coordinates, output/copy conflicts and unsaved owned recovery to first save.
Failed publication leaves no advertised project or leaked copy temporary; a
complete unreferenced owned copy may remain if a later project commit fails.

Workspace-root build/feature-expansion/d01-import-real/ contains real.json,
results.json, logs, mouse-brain-backed.qpx and its canonical .qpx-expression child.
One native handoff exits0 from System32 with Windows-only PATH. It reuses the
previous full store and admitted CC BY4.0 base/coordinates/projection; no raw matrix
re-import or download. The owned project retains32285 genes /2702 observations /
16031101 entries. All2702 coordinates/barcodes and every2702x6 reference count are
exact after project/source reopen. The native gene worker compares unprojected
ENSMUSG00000051951 with a retained marker, both2702 measured values. Before/after
hashes of the store, base, positions, features and projected counts agree.

Owned count file SHA remains
`79f407bf9b0810f417410961dd79db6a6ce1ff8999e085d1ef8f7865eaca7401`;
source SHA remains
`77b62ca7dbeb474a6df7a0cc1af9884c3a72069f15e677d145455e66ef2cf044`.
real.json records spatial/selected/request/recipe identities; results.json binds
the executable, five input hashes and output project bytes/hash. Retain CC BY4.0
attribution and source lineage for the derived store/project; these are local
proof artifacts, not newly bundled fixtures. Final staging membership is L01/P02.

This proves native import/ownership/reopen and selected display, not actual desktop
controls, an installed SDK/package, biology, RSS or speed superiority. D01 remains
unchecked at11/22 complete. Next connect the existing desktop wizard and gene
panel to these services with asynchronous indexed catalogue search/selection;
reuse real owned project and numerical evidence instead of another import campaign.


### D01 desktop completion: full indexed import and paged gene maps

Desktop wizard now chooses selected/project counts or a full indexed source. It
opens a store or indexes raw Matrix Market plus explicit Qpx/Tenx axes through
existing SpatialImportJob operations. Its prepared preview reuses the opened
source pointer, joins barcodes/coordinates and publishes/opens through existing
controls. Header/reader/scientific interpretation stays native. Bounded literal
ID/symbol searches read metadata only; stable IDs disambiguate repeated symbols.
Source scopes and immutable project-open requests keep live Document off workers.

The gene panel caches opened sources, coalesces metadata queries to latest input
and shares equal query pages. The native gene worker fetches selected features
and reuses existing colors/legends/comparison. Exact scientific identity does not
freeze a runtime locator: same-identity bind now refreshes a different source
handle, and live owned Save As follows its newly verified file. Point/reference
identity, exact raw counts and stale-source refusal stay intact. Matrix relink/
Undo is verified through the native API; no separate GUI relink browser is claimed.

Workspace-root build/feature-expansion/d01-gui/build.log succeeds. ctest.log passes
all4 affected suites in3.78s: native backed import, native spatial matrix, legacy
GUI gene/registration and GUI spatial import. New cases actually index all40 genes,
click Import/Open, page a16-item catalogue, choose unprojected Idx32 and get exact
[0,19,0] values, follow owned Save As with old locator offline, and refuse a changed
store. Native checks cover literal/case/offset bounds, explicit scope listing,
immutable worker opening and same-scientific-ID/new-runtime-source binding.
Legacy selected data, colors, meaningful registration and workspace routes pass.
No failing invariant or tolerance was weakened.

The settled handoff driver adds the existing real indexed scope to the full-app
capture route; handoff-build.log succeeds. Final controller cleanup compares query
text rather than unused buffer tails and avoids copying project path per frame.
No failed build/test in this GUI packet. Existing numerical science is reused.

Workspace-root build/feature-expansion/d01-gui-real/results.json records3 exit0
runs from System32 with Windows-only PATH: actual import driver, actual full-app
comparison driver and standalone qupathxx_app --project/--layout/--smoke. Real
import uses the previously owned store, never reindexes the197MB raw matrix, and
retains32285 features /2702 coordinate observations. Project SHA256 is
`a31dcd399f65c47a8b51acdd48333c47f11abff621fc4f15c94004358cb9bd7e`,
14368768 bytes, with a verified relative external matrix association. Native real
owned import/count/coordinate equality remains covered by the prior receipt.

Source store, original base and coordinate input hashes before/after agree; all38
retained binding-object hashes match the prior baseline. No binding, wheel, SDK,
package or new data/model/dependency download. Drivers also retain their passing
synthetic action/legacy checks. genes/real-indexed-result.json proves the expected
2702-point scope, settled tiles, comparison and unchanged project; workspace keeps
gene/source choices separately. Scope SHA is
`704ef0059ee49c4aa8eafba91455dacd0e6c266871663f2a9681439028fb17bb`.

Inspected import/real-indexed-preview.png shows2702 mapped tissue spots, all32285
features, readable output and Import action. Inspected genes/real-indexed-comparison.png
shows aligned matching positions, clear gene IDs and a shared0..4.45435
log1p(counts) scale:2451/577 zeros and0 missing in each respective map. The first
ID is unprojected ENSMUSG00000051951; the second is Gad1/ENSMUSG00000070880.
captures.json binds PNG/PPM hashes and verifies lossless identical RGB conversion.
Reuse admitted CC BY4.0 dataset attribution/lineage for these local derivatives.
No cosmetic QuPath-parity, population/biological, RAM or speed-superiority claim.

D01 closes under the local indexed uint64/Qpx-or-Tenx TSV, explicit image-coordinate
subset. Twelve of22 items are complete;10 remain. Matching optional bindings/SDK
and frozen release are P01/P02, final material membership L01. Preserve independent
UI styling edits and their receipts; these local GUI results include that working
UI state and are not a new frozen package. Next D02: bounded metadata/value/color
retention and named-host sparse interaction, then deterministic tiled raw-cell
context/seams/checkpoint-resume. Do not replay accepted import/fluorescence science.

D02 selected-value reuse (partial, not D02 closure)
------------------------------------------------

Native GeneViewSession now reuses its one immutable selected projection for
presentation changes when spatial identity and selected feature order match.
Binding a new spatial/runtime source clears that projection. Current source
checks run before reuse and before owner publication; cancellation/generation
publication is unchanged. No multi-entry value history or new scheduler.

Local evidence: build/feature-expansion/d02-values/build.log and ctest.log. One
cached build of qupathxx_gene_view_tests/qupathxx_expression_matrix_spatial_tests
succeeds; both suites pass0.28s total. New checks keep the old result alive and
prove count-buffer ownership is shared while transform/colors change, preserve
9007199254740993 exactly, and refuse a mutated source after values are cached.
Existing gene-switch/coalescing/relink checks remain passing. This is correctness
and avoided-read evidence, not a measured latency/RSS or whole-slide scale claim.
D02 sparse scale, remaining cache bounds and tiled seam/resume are still pending.

D02 native sparse scale slice (desktop responsiveness still open)
---------------------------------------------------------------

`qupathxx_expression_scale_tests --generate DIRECTORY` is an explicit native
qualification target, excluded from default ctest. `--reopen DIRECTORY` reuses a
fingerprinted store and writes a distinct receipt. New generated inputs/results
never overwrite an existing fixture/receipt. Synthetic mathematical values/IDs
introduce no external data or licensing requirement.

HEQPC, System32, Windows-only PATH: one generation/import/run exits0. Evidence
`build/feature-expansion/d02-scale/{build.log,run.log,scale.json,host.json}` binds
executable SHA f35f52d731f3e86864f23edb9ba5a2fbcdfffa65ea3b9d12dee94e3e524d249f,
input/store hashes and options. Shape100000 observations/30000 features/5000000
unique entries, covering every axis member. Independent per-observation50-feature
formula checks every selected sparse count/implicit zero and all200000 two-gene
projection counts. High-axis catalogue page, exact-count reuse on recolor,
latest-generation delivery after cancellation and active indexed-read cancellation
pass. Source store SHA71d646ebba28ab6d37198f285b70c5685411d127a2f0fc45dd3d5175c4f8eb61.

Declared256MiB per native operation. Import peak accounted work135276000 bytes;
conservative retained spatial/dataset/two-field sum194971982 bytes (shared dataset
counted repeatedly, an upper bound). Matrix text72129046 bytes, store114733056.
Local single-run ms: import6167.604, open75.004, metadata9.973, sparse read/oracle
10.235, coordinate join1854.777, comparison106.345, recolor51.842, cancel/latest
105.133, active-read cancellation after signal0.475. These are native host/run
observations, not process RSS/GPU/QuPath speed or interactive-frame guarantees.

Scale exposed an actual desktop gap: coordinate joins are on the owner thread,
and GUI sessions still use64MiB defaults. Move immutable joins onto the existing
worker and admit the bounded desktop budget before claiming large interactive
support. Preserve this fixture instead of regenerating/reindexing. D02 remains
open for desktop responsiveness and deterministic tiled-cell seam/resume evidence.

D02 asynchronous joins and bounded desktop session
------------------------------------------------

Spatial requests now capture only native document facts/provenance on owner.
ExpressionMatrixSpatialRequest::resolve joins axis IDs on the existing native
GeneViewSession worker before projecting selected genes. Existing generation and
cancellation delivery cover the join; no scheduler or extra worker. Request and
session work ceilings both apply, retained request storage is reserved before
join/projection, and desktop backed capture/gene session use256MiB. Synchronous
snapshot capture remains available to explicit native callers. Count reuse and
source-version checks remain intact.

`build/feature-expansion/d02-join/build.log` succeeds for affected native suites,
scale driver, GUI drivers and app. `ctest.log`: all5 suites pass3.75s (backed import,
spatial matrix, native gene view, legacy GUI gene and actual indexed import/gene
controller). New native checks capture facts, edit the original document, resolve
on worker and verify the captured identity; unchanged requests do not rerun;
cleared active joins cannot publish. Existing source mutation, coalescing and
actual desktop Save As/relink tests remain passing. No screenshot-style assertion
or scientific tolerance change.

Host/executable/baseline receipt in d02-join/host.json, values in
`build/feature-expansion/d02-scale/reopen.json`, output in d02-join/scale-reopen.log.
One --reopen reuses the existing store and agrees on selected-block/source/file
identities. HEQPC single-run ms: immutable request6.9255, owner dispatch0.0825,
worker join+comparison2024.6858, recolor55.8682, cancel/latest108.0944, active-read
cancellation0.5943. Conservative retained accounting202073662 bytes including the
request, below268435456. These are native dispatch/accounting observations; full
100k desktop frame responsiveness, process/GPU memory and speed comparison are
not inferred. Remaining cache accounting/coalescing and tiled-cell seam/resume
still keep D02 open. No reindex/data download/bindings/package campaign.

D02 display duplication removed; existing cache services reused
--------------------------------------------------------------

`build/feature-expansion/d02-display/build.log` succeeds for affected native/GUI
suites and app; ctest.log records all5 passing3.50s (expression, native gene view,
spatial matrix, legacy gene GUI and indexed import/gene GUI). Shared automatic
comparisons now prepare each transformed field once, resolve one pooled range
and paint once, keeping final manual-field request identities, colors, validity,
clipping and requested comparison range policy. Per-field percentile sorts are
skipped for pooled ranges; manual/minmax fields allocate no percentile scratch.
Implicit full-row cohorts come from known iota and need no uniqueness tree;
explicit caller cohorts retain duplicate/out-of-range validation. Radius-only
edits retain ready result/generation and queue no native work. Added checks compare
pooled output with explicit known-limit manual fields and assert radius reuse.
Existing exact counts, cancellation, bounded cache eviction and GUI checks pass.
No new timing/speed claim or repeated100k matrix import.

Existing service bounds reused by source inspection: each GuiTiles has64MiB GPU
LRU; each TileScheduler has64MiB decoded cache,64MiB active reservation,256 maximum
requests/1024 cached entries; viewport planning caps192 tiles. Duplicate requests
coalesce and changed desired viewport generations reject stale delivery. These
are per-service bounds, not a combined process/GPU memory ceiling. Gene session
retains one selected projection/current pair, and serial GeneDisplayCache has
its own byte/entry eviction; no new history cache or scheduler was introduced.
D02 now proceeds to tiled-cell halo/ownership/global identity/resume work. The
existing F01 context covers smoothing, not complete halo segmentation; dependency
on cut foreground components must be detected before watershed filtering.

D02 bounded tile kernel and seam ownership (workflow/resume pending)
------------------------------------------------------------------

New native `segment_fluorescence_tile` reuses the existing segmentation/measurement
kernels over one expanded segmentation domain. Its explicit global sample/core/
analysis rectangles validate level-zero containment and Gaussian+2*expansion halo.
A complete cell belongs to the half-open core containing its first global
row-major nucleus pixel. Global IDs bind source, recipe, channel block/Z/T,
sample type and anchor; local watershed labels are omitted. Output carries global
nucleus/body RLE geometry and original raw compartment moments. Work admission
reserves cut-component traversal, per-label state and worst-case output/vector
growth alongside the existing raw/kernel workspace; no slide-sized raster.

An optional internal hook marks connected thresholded foreground that touches
cut/invalid/ROI/smoothing support before watershed and area filtering. Interior
children inherit their cut parent's dependency. Output reports unresolved
foreground in core even if all candidate labels are filtered, plus unresolved
owned labels; these cannot be published as a complete tile. Per-tile Otsu refuses:
the following orchestrator must supply one globally resolved explicit threshold.
Untiled calls leave the hook absent and retain their output/recipe contract.

Evidence `build/feature-expansion/d02-tiles/{build.log,ctest.log}`: affected native
build succeeds, all4 suites pass0.79s (fluorescence_cells, fluorescence_tiles,
fluorescence_cell_workflow, fluorescence_cell_objects). New analytical four-nucleus
raw fixture, independently segmented through the untiled ROI service, matches all
returned nucleus/body pixels and compartment raw moments exactly across3 tile
layouts/reversed order. Global IDs/one-owner coverage agree. A connected cut
parent's interior child and area-filtered cut foreground both remain unresolved;
halo/work/cancel/per-tile-Otsu guards pass. Existing source currentness and native
published-cell/recipe replay remain passing. No new data/license/dependency input,
GUI/bindings/package build, scale indexing or biological-accuracy claim.

D02 remains open: source/job orchestration, one global Otsu/normalization contract,
source/options-bound checkpoint/resume and independent admitted real-ROI seam/
interruption equality, then thin desktop controls. This pure bounded primitive is
not a completed whole-slide processing workflow or frozen release.

D02 source-backed tiled processing and one global Otsu (resume pending)
--------------------------------------------------------------------

FluorescenceTiledRequest captures native source/ROI/calibration and layout/options
on the owner without raw reads. A lazy window iterator reads bounded original-
channel halo regions and streams immutable tile results rather than building a
whole-slide raster. Source identity binds observed files/metadata/calibration
(excludes ephemeral observation timestamps/file-reader generation for file-backed
sources); request identity additionally binds ROI, channels/Z/T, options and tile
layout. Non-file sources require an observable reader generation. Currentness is
checked around reads and final delivery. The existing FluorescenceCellJob shares
one execution slot between untiled/tiled requests and retains native cancel/stale
owner delivery; typed tiled reports distinguish ready from unresolved.

Global Otsu shares the untiled Gaussian score preparation and256-bin/lowest-tie
threshold implementation. Only unique core pixels within the full ROI contribute:
range pass, histogram pass, then explicit-threshold segmentation. Constant scores
skip the histogram reread. Raw input hashes must agree between passes. Explicit
thresholds use one segmentation pass. ROI-empty/nonfinite halo tiles can stream
empty results, while a wholly nonfinite ROI cannot claim successful completion.
Input/summary manifests and caller-owned streaming sinks remain bounded by the
native work contract; correction/unmixing adapters and persistence are not yet
included in this original-channel route.

Evidence `build/feature-expansion/d02-source/`: build.log succeeds. Initial
ctest.log retains passing pure-cell/tile/published-replay suites and one failed
new constant oracle. The oracle assumed constant100 stays bitwise constant after
binary64 Gaussian normalization; repaired only that exact-constant test to sigma0.
The sigma.7 whole-ROI/tiled equality remains unchanged. build-r2.log succeeds and
ctest-r2.log passes the source suite0.22s (0.29s total); no scientific tolerance or
product behavior weakened. Tool observation delay did not indicate a hung test;
initial ctest elapsed1.19s.

Source tests use an authored80x52 raw fixture, comparing global Otsu threshold,
complete body geometry and raw marker moments exactly with untiled ROI processing.
Two layouts keep threshold/geometry/global IDs equal while changing request ID;
all4160 finite pixels are counted once,3 bounded passes occur and no read covers
the whole source. Mutation between threshold passes refuses. Native job read-
barrier cancellation/calibration-stale/ready delivery and exact100/zero-cell
constant Otsu pass. Existing F01 pure/source/publication/replay checks pass. No
biological accuracy, real-ROI completion, universal speed/RSS or release claim.
D02 still requires checkpoint/resume, derived-source adapters, admitted real ROI
and thin desktop consumer checks. No new dependency/data/reader/model inputs.

D02 source-bound checkpoint/resume (derived sources/real ROI/desktop pending)
-------------------------------------------------------------------------

`checkpoint_tiled_fluorescence_cells` and the existing cell-job checkpoint overload
write a dedicated directory with threshold.qft, tile-N.qft and complete.qft.
Use existing bounded binary checksum codec/regular-file guards and new-only
atomic flushed export. Each complete tile stores exact global IDs/RLE masks/raw
moments; unresolved tiles cannot be reused as complete. Completion is published
only after all tiles and final source checks. Resume validates source/request,
threshold, checksums, bounds/ownership/IDs, canonical masks, compartment geometry/
validity and moment missingness, then verifies each current raw tile hash before
skipping its segmentation. Both reused/new tiles stream to the same native sink.
Canonical output identity excludes path, timing and runtime allocation counters.

Default per-record4MiB and total2GiB caps are typed options, with codec/manifest
buffers reserved within the native work budget. Directory startup scans existing
members once rather than statting every possible tile; recognized orphan atomic
staging contributes to storage quota and is preserved. Foreign/symlink members
refuse. No existing record/input is overwritten, and failure retains committed
progress. The completion marker alone is not current scientific acceptance;
resume still validates source/raw bytes/records before returning ready.

Evidence in `build/feature-expansion/d02-checkpoint/`: initial build.log succeeds;
ctest.log preserves passing pure-cell/tile/published-replay suites and a real
file-backed reopen failure. TIFF observed.reader includes an encoded display view;
source profile/2 now binds actual raw reader_id, raw metadata/series/calibration,
file-version set/scope, while excluding that presentation descriptor. Non-file
sources retain reader/generation binding. Old source-profile hashes refuse rather
than aliasing new identities; untiled recipes are unchanged. build-r2/ctest-r2
pass the source suite0.43s; scan/quota refinement build-r3/ctest-r3 passes0.45s
(0.51s total). Assertions/tolerances remain intact.

Authored80x52 fixture: interruption after3 of8 committed tiles leaves no completion
record. Resume reuses3/segments5 with8 verification reads; all serialized output,
including masks/IDs/raw moments/completion, equals uninterrupted bytes. Fully
completed job reuses8/segments0. Changed layout rejects before raw reads; changed
raw bytes/corrupt records refuse. A completion-quota failure retains all tiles,
and raised quota reuses8 with identical output identity. Unresolved foreground
stores no complete tile/summary. Orphan staging exceeds a constrained quota before
raw reads without deletion; unrelated content is preserved/refused. An actual
independently authored OME-TIFF reopens through a fresh reader after display-
channel changes and reuses its exact zero-cell record; actual file mutation
refuses. No real biological acceptance, full D02 or frozen-package claim.

Next: corrected/unmixed source adapters, admitted real-ROI seam/interruption
checks and thin desktop control over this same native authority. Optional Python/
SDK/release remain P01/P02; no new reader/data/dependency/model/license input.

D02 corrected/unmixed tiled source adapters
-----------------------------------------

FluorescenceTiledRequest now accepts retained DarkFlatRequest and
LinearUnmixRequest captures, including corrected unmixing. Source snapshots reuse
existing ChannelSource metadata/read/currentness and fixed normalization/model,
without per-tile normalization/factorization or rewritten original channels.
Unmixed selections address component/residual outputs; ROI C must be independent.
Derived source identity pins measurement domain, source and correction/unmixing
records. Owner checks invalidate removed recipes; read/final-delivery checks cover
actual signal/field versions. Existing F03/F04 recipe identities/rules are reused,
not migrated. Raw source-profile/2 stays unchanged for original requests.

Admission reserves derived snapshot/recipe payloads and read_work_bytes together
with checkpoint buffers; peak read accounting is included alongside segmentation
rather than assuming component output dimensions bound measured-input work.
Reports carry measurement_domain/correction_record/unmix_record. Native sinks,
cell-job delivery, global Otsu and checkpoint/resume remain the same authority.

Evidence `build/feature-expansion/d02-derived/{build.log,ctest.log}`: cached build
succeeds and all5 affected native suites pass1.86s. New independently authored
80x52 uint16 TIFF signal/dark/flat fields exercise fixed subtraction and diagonal
mixing with component and residual selection. Corrected, original-unmixed and
corrected-unmixed whole-ROI/tiled masks, global Otsu and every raw compartment
moment agree exactly; sums also match independent equations and residual sums0.
Interrupted2/8-tile resume reuses2/segments6 with byte-identical persisted science.
Fresh native field/operator replay reuses8/segments0. Host Release compiles the
linear algebra backend and actual unmixed/combined completion directories exist;
coverage is not inferred from an optional-backend skip. Removed/unretained or
renormalized recipes refuse, Undo restores job delivery, all input bytes remain
unchanged before deliberate authored flat-field corruption, and actual changed
field version invalidates publication/resume. Existing pure/tile/original-source/
checkpoint/published-replay suites remain passing. No tolerances/identities relaxed.

One build/test batch, no failures/retries/new data/dependencies/models/readers,
GUI/binding/package or repeated matrix import/biological campaign. This closes
native derived-source integration, not D02: admitted real-ROI seam/interruption
and desktop consumer delivery remain. No whole-slide performance/release claim.

D02 desktop tiled control and admitted real-field seams/resume
------------------------------------------------------------

The existing fluorescence panel now submits native tiled requests/checkpoints for
original/corrected/unmixed domains. Run/resume folder, tile/halo/storage controls,
state, certified-cell/reused/segmented/unresolved counts, threshold/domain and
artifact location use the same native authorities. Single-region preview/object
publication keeps its typed authority. Owner/option changes clear stale results;
save/edit blocking and native errors remain visible. App and affected GUI drivers
build together; no Python science or second scheduler.

`build/feature-expansion/d02-desktop/build.log` succeeds; ctest.log records4 passing
suites2.73s (native source/checkpoint, fluorescence GUI, correction GUI, unmix GUI).
Actual tiled Run processes6 tiles, rerun reuses6/segments0, blocked Run does not
start, calibration changes clear ready results, option mismatch shows native error,
and the actual mode toggle returns to untiled behavior. Inspected tiled-results.png
shows readable ready/0-cell/6-tile/threshold10000/domain/path; capture.json verifies
lossless same RGB as original1280x1000 PPM. This is a legitimate zero-cell control
fixture, not a real-data overlay or cosmetic QuPath comparison.

An explicit opt-in native real driver (not default ctest) pins the retained CC0
BBBC039 TIFF SHA9eacee0e5a4c29af70307af4d310320632e31ed84b4d8b5fd4fce42930f780ec.
No annotation/biology campaign is replayed. It independently traces raw thresholded
full-field connected components before watershed filtering and applies the declared
4px support guard; old per-label edge flags alone are insufficient. Every certified
candidate must match whole-ROI nucleus/body pixels and all moment bits, not only
approximate counts. Global Otsu, alternate layout and interruption/resume are exact.

First real.log/real-host.json succeeds but review identifies that certified cells
in an unresolved tile were streamed but not persisted. Partial QFU records now
save those candidates with unresolved foreground/label facts. They are reprocessed
and byte-verified, never reused as complete. Complete/partial status and every
candidate contribute to output identity; no completion marker while unresolved.
partial-build.log succeeds and partial-ctest.log passes only the2 affected suites
1.12s; earlier correction/unmix GUI results are retained, not repeated.

Final real-v2.log, real-v2-host.json and real-field-v2/real.json: HEQPC/System32/
Windows-only PATH, exit0.696x520 field,130 whole-reference candidates;104 certified
candidates and26 boundary/dependency-uncertified. All104 masks/moments/global IDs
match across128/160-wide layouts (30/25 tiles). Threshold399.4921875,14/13 unresolved
tiles remain explicit. Resume reuses8/segments22 and all complete/partial records
match baseline bytes; input and output identities agree. Same-layout output SHA
70e5d36d1045a794c353d3d018497ecdf76d64dc00259bcff6ecc21a320d4ab0. Maximum
reported accounted work155830828 bytes, below256MiB; allocator/process/GPU memory
is not inferred. Input hash before/after agrees. No SOTA, biology or speed claim.

D02 remains open for an actual admitted whole-slide ROI; this real field does not
prove that exit or whole-slide responsiveness. Official OME sample docs provide a
CC-BY4 candidate, LuCa-7color_Scan1.ome.tiff (24960x34560x5,2085508976 bytes via
HEAD): https://ome-model.readthedocs.io/en/stable/ome-tiff/data.html . Only metadata
has been checked here. Next acquire/admit exact bytes/codecs and run one bounded
nonzero-origin ROI using existing open TIFF services. No WSI acceptance or new
fixture admission is claimed from metadata alone. No new dependencies/bindings/
package, reader, model, restricted data or cleanup deletion.

### D02 actual whole-slide ROI closure — 2026-11-09

Official OME sample-data documentation admits LuCa-7color_Scan1.ome.tiff as
CC-BY-4.0, PerkinElmer via OME:
https://ome-model.readthedocs.io/en/stable/ome-tiff/data.html . Exact source URL,
license/attribution,2085508976-byte length and observed SHA are now in the existing
fixture inventory, opt-in extended only; the2.085GB image is external build data,
not bundled source/package or automatic CI. Existing open libtiff reads its tiled
512x512 LZW BigTIFF/OME metadata and all5 uint8 channels, no new reader/dependency.
Source SHA50cc938e259673c4e62385ceb49c8707cead252566932bf47ed20f2afc5614f0.

Evidence: build/feature-expansion/luca-inputs/acquisition.json and source-ome.xml;
d02-wsi/build.log, limit-build.log, origin-build.log, case-v2.json,
real-v3.log, host-v3.json and wsi-roi-v3/real.json. Acquisition preserved the
385069056-byte single-stream prefix and fetched only disjoint remaining ranges
with matching ETag/Content-Range, retaining one image. Initial real.log/host.json
candidate-limit refusal and real-v2.log/host-v2.json oracle failure are preserved.
Dense unsmoothed input exceeded the declared1000 peak cap; the case now declares
10000 while keeping256MiB, threshold and all exact assertions. The old independent
reference's4px support guard was local to the small full-source fixture; it now
uses global physical source edges for interior crops. ROI-cut-component and
boundary-touching refusal stay unchanged; no product algorithm/tolerance changed.

On HEQPC, native-only System32/Windows PATH, final exit0: source24960x34560x5,
level0 ROI(12000,16000,768,512), DAPI0 segmentation and all5 measured channels.
Physical sampling0.49799447890790055um in both axes. Whole reference1527 labels,
1374 certified and153 boundary/dependency-uncertified; not annotation accuracy.
All1374 certified nuclei/bodies/raw compartment moments match bitwise and all
IDs agree across24-tile128x128 and20-tile160x128 layouts, halo256. Global Otsu
32.00390625 agrees with the independent whole-ROI run. Interrupted/resumed run
reuses4/segments20; every saved complete/partial record is byte-identical to
baseline, with the same status/input/output identities and unchanged source.
Same-layout output SHA119cc343ee027d842705ac1f846c583981e1278d501a77304eaf5a12c4c3b9e3.
Maximum accounted native work240760252 bytes <268435456; baseline232240796.
Local elapsed baseline8035.3657ms, alternate6841.1351ms, resume5259.639ms.
Cancellation requested at a committed tile boundary unwinds0.4172ms; this is
not a measured mid-kernel cancellation latency. Both layouts remain honestly
unresolved (16/14 tiles), persist all certified output and create no complete
marker. No full-slide throughput, universal speed, biology, RSS/GPU or100k GUI
FPS claim. Existing scale/active-read cancellation/dispatch, bounded cache and
actual desktop evidence above remain in scope without repeating those campaigns.

D02's declared native interaction and actual WSI ROI seam/resume exit is closed.
13/22 complete,9 pending; next O01 native H5AD, then O02 local OME-Zarr. Optional
bindings/SDK and release remain P01/P02. Waste: one diagnosed transfer adjustment,
one candidate-cap refusal and one reference-coordinate correction, then one
successful WSI run; only affected explicit-driver builds, no broad suite rerun,
new scientific kernel/backend, GUI requalification, matrix reindex or packaging.

### O01 first native decoder slice — 2026-11-09 (item remains open)

Pinned HDF52.2.0 source archive SHA
5b8d75125ae7b4fef55d3b39e8d3dbfd238cdf63b6518afd67fa69ac80f06542,
BSD-3-Clause/full LICENSE notices, optional QUPATHXX_BUILD_H5AD (default OFF).
Static C, native local connector; shared admitted prefixed zlib1.3.2 plus
shuffle/Fletcher32. No plugins, SZIP, network, high-level/C++/Fortran/Java/tools.
cmake/PinnedZlib.cmake now owns that one archive/header/notice setup for both
readers, preserving the existing tiff-zlib notice location even for H5AD-only.
No new Python/anndata/h5py product/runtime dependency; h5py3.15.1 is development-
only independent fixture authoring, never imported by CTest or product code.

Native H5adReader requires explicit count/continuous domain and a declared
CoordinateFrame, matrix X or named layer, and an Nx2 obsm coordinate field. It
captures exact file/options/frame identity and ordered UTF8 observation/feature
IDs; streams exact uint64 or declared finite continuous values in bounded numeric
blocks. It accepts unsorted CSR/CSC while refusing duplicate/out-of-range indices,
invalid pointer axes/order/end, unsupported dtype/version, fractional/negative
counts, nonfinite values, duplicate IDs and oversized file/axis/chunk/string input.
A scope-exclusion summary discloses unselected AnnData metadata; no full AnnData
validation/round-trip claim. Sparse absence is measured zero, not missing data.
External/soft selected links, external dataset storage, virtual datasets and
unreviewed filters refuse before payload read. File-version guards reject changes.
stream_work_bytes reserves decoder/catalogue/cache/chunk/index work for composing
with the future indexed writer; it is not total allocator/process RSS.

HDF5 forbids Windows threadsafe static builds. Private handles are therefore
serialized under one native cancellation-aware gate, with actual queued-call
cancellation tested while another stream holds the gate. Attribute allocation
needs special care: H5Aread has no DXPL and unmodified HDF5 has no VL-to-fixed
string converter (h5py registers its own). Public native VOL attr-read supplies a
private allocator, with paired H5VLopen_lib_context/H5VLclose_lib_context. Scalar
VL attributes and ID strings refuse allocations beyond257 bytes; fixed attributes
read their bounded exact dtype. No global-default mutation, external connector,
internal HDF5 header/source patch, unbounded attribute fallback or Python callback.

Evidence: build/feature-expansion/o01-native/{configure.log,configure-v2.log,
build-v2.log,build-v3.log,ctest.log,attribute-build.log,attribute-ctest.log,
vlen-build.log,vlen-ctest.log,context-build.log,context-ctest.log,
native-only.log,native-only-host.json}. Retain the unsupported static/threadsafe
configure, missing-include compile, failed fixed conversion and missing VOL API-
context crash; each next action changes the diagnosed cause, not the assertions.
Final H5AD test passes0.16s, ctest0.22s;29 independent authored fixtures516561
bytes with individual hashes in tests/fixtures/h5ad/manifest.json. Hand-calculated
references cover exact9007199254740993 counts, negative/fractional continuous
values, big-endian IO, unsorted CSR/CSC, all IDs/axes and explicit frame identity,
selected layer, all refusal cases, active/queued cancellation and source mutation.
Existing matrix/raw_image suites pass0.47/0.45s after the shared-codec extraction;
they are not rerun for H5AD-only fixes. Native System32/Windows-only PATH execution
on HEQPC exits0; executable SHA
2ba6e83ad9d1faf2fea004b73362aa3e4156588f04d3c5a0d21be01756c42ae3.

O01 remains open: feed this stream into the existing transactional indexed store
(count/continuous interpretation), then existing import job/UI, plus one admitted
real H5AD comparison. Figshare article13604177 metadata reports CC-BY4/Giovanni
Palla, original10x citation and94,259,482-byte visium_hne_crop.h5ad candidate;
metadata only retained in o01-admission, no payload/acquired-encoding acceptance.
13/22 complete,9 pending. No GUI/binding/wheel/SDK/release work or large-fixture
reindex was repeated. First activation built HDF5/core once; affected retries
preserved those objects. Existing C4819 upstream UTF8-comment and baseline linker
warnings are retained, not a warning-free-build claim. No full rebuild for warning
cosmetics. Next compose reader/writer budgets and reuse matrix persistence rather
than introduce another store, generic workflow engine or file-conversion pipeline.

### O01 shared indexed native import and offline project route — 2026-11-09

`import_h5ad_matrix` now streams the delivered H5adReader into the existing matrix
persistence authority. Matrix Market and H5AD share `detail::matrix::Writer` for
SQLite schema creation, exact scalar blobs, per-gene identities, quotas/private
staging, guard verification and atomic new-only publication. No temporary Matrix
Market/TSV conversion, H5AD-only store, new worker/scheduler, Python application
logic or new dependency. Decoder catalogue/cache/chunk/index reservations are
subtracted before the writer's SQLite allowance; native tests use64MiB total and
require reported accounted peak within it. This does not assert allocator/RSS.

Native sparse reads/project associations/spatial projections now retain explicit
Counts versus Continuous interpretation. Continuous signed/fractional values stay
finite doubles; counts retain uint64 beyond2^53. Embedded coordinate pages carry
their own frame, exact raw doubles and row/aggregate identities. Source-coordinate
hash preparation happens once per writer/page, not for every row. Physical frame
conversion/alignment remains the existing native authority. An empty backed-import
position path selects these embedded rows; an explicit TSV retains its guarded
legacy behavior. The existing SpatialImportJob owns H5AD indexing and the same
preview/publication path. Owned projects preserve sidecar bytes and reopen after
the original H5AD and external store disappear; point metadata contains references,
not copied floating counts. Unicode frame IDs, planes, calibration and extents
round trip through native metadata.

Schema3 also retains the exact `X`/layer and spatial keys. Reopen recomputes the
pure native H5AD recipe identity from original input SHA, keys, value domain and
frame; a changed selector refuses even without the original file. The reader and
sidecar verifier share that identity function, callable without the HDF5 backend.
Legacy schema1 source/block contracts remain unchanged. The frozen native tiny
import still produces SHA174b4ea68d6022fbab3a488b0a070d0a550853e42e410880707e6f5d8b0b6018.
The old test's final one-byte stale marker was excluded when capturing that
canonical reference; its original retained file was not changed (legacy-reference.json).

Evidence: workspace build/feature-expansion/o01-index/{build.log,build-v2.log,
ctest.log,recipe-build.log,recipe-ctest.log,native-only-v2.log,
native-only-v2-host.json}. Initial compilation missed the namespace qualification
on the coordinate identifier helper; retain the failed log and unaffected objects.
The first repaired batch passes all6 native suites1.99s. Review then identifies
opaque selection SHA without reconstructable keys. Preserve that passing draft as
passing-schema2.patch (base dc2df39852549244053281dfef5eda4264131796,
SHA41e4993fb907c6080a96a8db174deaa942887addfe7337df1390faceb682951d) and its
source receipt. Schema3 fixes the named provenance gap rather than changing old
interpretation silently. The frozen32KiB schema2 fixture is admitted in the main
fixture inventory, SHA0d23001c2383856f3acd5c544bafca5aa7d7d269f3ece326361a46b93db867fe;
its exact values/coordinates remain readable, without inventing unknown old keys.

Final recipe build succeeds; all6 suites pass2.28s: new H5AD matrix integration,
standalone H5AD decoder, legacy backed import, spatial matrix, project matrix and
legacy indexed matrix. Covers dense/CSR/CSC in both domains, cohorts/catalogues,
all source IDs/axes/values, embedded-coordinate/source-offline reopen, exact frame
codec, saved nondefault layer replay, quota/composed-budget/pre-cancel refusal,
malformed-input cleanup, corruption (including outside selected cohort), native
index/preview/publication and owned continuous project reopen with calibrated
micrometre-to-pixel alignment. Original fixture bytes remain unchanged. Existing
legacy suite still exercises active cancellation and changed-file read guards
through the shared writer. No biological or universal performance qualification.

HEQPC native-only run with System32/Windows PATH exits0; executable SHA
d37bf96326e8643801a1457a9ab58caba969659d708843ed6183c7ed624183d4.
Earlier passing draft native receipt remains separately identified, not transferred.
No full D01 real reindex, broad corpus, GUI, binding, wheel, SDK or package campaign.
O01 remains open for actual desktop selection/import and admitted real H5AD
count/ID/coordinate evidence.13/22 complete,9 pending; preserve independent UI work.


### O01 desktop and real-count closure

Local O01 closure adds the existing desktop import controller, with a third H5AD
input kind and a Coordinates step for explicit keys/value domain/source frame/
units/plane. Counts and continuous values remain distinct; embedded coordinates
need no TSV. Saved native metadata is inspectable, omitted AnnData metadata is
disclosed, and input edits invalidate opened source/preview. No Python runtime.
The app and import GUI targets build (o01-desktop/layout-build.log). Two affected
native suites pass0.63s (native-ctest.log); the six earlier indexed integration
suites retain their original source scope. The cache-only decoder change keeps
all numeric/scientific identity unchanged:1MiB per dataset avoids a64KiB cache
smaller than the admitted256/128KiB chunks. Three numeric caches +4MiB metadata
fit the existing8MiB fixed reservation; no before/after speed claim is made.

Real fixture: full CC-BY4 10x V1_Adult_Mouse_Brain, attributed to10x Genomics,
2020-06-23 ([official dataset/license](https://www.10xgenomics.com/datasets/mouse-brain-section-coronal-1-standard-1-1-0)).
This is a disclosed development-only h5py3.15.1 lossless current-profile H5AD
construction from the admitted native full raw-count store and retained native
hires positions, not a downloaded upstream legacy H5AD. All32285 genes,2702
observations and16031101 entries are retained, without normalization/subsetting.
Gene names/types are extra unselected metadata; var index IDs are the imported IDs.
Coordinates retain published fullres column/row times0.17011142, in the retained
1921x2000 hires pixel frame. No guessed registration/calibration. Fixture/preparer
and their receipts remain external build evidence, not bundled/automatic CI.

Exact identities in tests/fixtures/manifest.json and o01-real/fixture.json:

- original full .qmx SHA79f407bf9b0810f417410961dd79db6a6ce1ff8999e085d1ef8f7865eaca7401;
- position TSV SHAdd313edd021d0f3fa95b7beed61e6e7ed495c9a3b50b24d5582ab6aeef5f9b4c;
- H5AD35492060 bytes, SHAca8515e12ad0b35d4603ee8c13e2ea66d23a983bed1df2091acbdb6ef3d91317;
- new .qmx352706560 bytes, SHA13a7990d6b96e83b7c43bf184ce7878ae236f513793e7074ffccfdfa0cba0d8c.

Opt-in native qupathxx_h5ad_real_check imports once and compares EVERY sparse
entry/offset/observation index in batched32-gene reads, all feature/observation
IDs/order and all exact independently ID-joined TSV coordinates. Original file
hashes remain unchanged. o01-real/native.log exits0: import16252.5628ms,
153567784 peak accounted work bytes under256MiB; full comparison26803.2883ms.
These are local observations under concurrent build load, not process RSS,
QuPath timing/throughput comparisons or biological acceptance. Exact three-record
admission.json passes the unchanged material-staging guard (admission.log); full
historical group-level fixture inventory is not a per-byte staged membership list.
That wider release-membership task remains L01.

Actual GUI run o01-desktop/gui-absolute.log exits0: independent continuous H5AD
is indexed via Next actions, preview preserves raw embedded positions/negative
values, actual publish/open works, changed selection invalidates stale results.
Existing selected/indexed/paged-gene and refusal checks pass in that same run.
Real desktop publication reuses the once-imported full H5AD sidecar and explicit
retained TSV, renders2702 registered spots and publishes2702 objects. Inspected
h5ad-preview.png and real-indexed-preview.png for readable domain/actions and
meaningful overlay registration. It does not reindex the real source or duplicate
native count validation. First gui.log reaches publication but fails a relative
root vs absolute Open assertion; preserved gui/ is diagnostic. The same binary
passes with an absolute evidence root, without assertion/product changes.

source-receipt.json retains host, executable and source/dirty-overlay identities;
independent desktop changes are disclosed and preserved. Native real/decoder
scope precedes the frontend-only layout change. No binding, SDK, wheel or package
rebuild; those remain P01/P02. No all-AnnData/legacy encoding, biology, full parity
or speed superiority claim. O01 closes14/22;8 pending. Next O02 local OME-Zarr.


### O02 native local source: first slice

O02 remains open:14/22 complete,8 pending. Optional QUPATHXX_BUILD_OME_ZARR adds
OmeZarrSource over the existing ChannelSource contract, implemented natively in
ome_zarr.cpp, ome_zarr_metadata.cpp and ome_zarr_store.cpp. Original scalar values
stay typed, including uint64/int64 beyond2^53 and nonfinite float samples. It reads
selected C/Z/T across bounded local chunks, C/F layout and LE/BE types, preserving
explicit NGFF axes/units and composed scale/origin. Integral isotropic XY pyramids
retain level transforms; physical XY pitch may differ. Mapping cancels common
origins and retains integer quotients before applying offsets, so a1e20 origin
does not erase small pixel coordinates. Unsupported geometry/precision refuses.

Read guards selected metadata/chunks before/after consumption. Whole-store
require_current compares a bounded full file-version/membership observation at
operation boundaries. This is an observed-version identity, not a pixel-content
fingerprint. Readonly local stores reject links/reparse paths; missing chunks
use only a declared fill, null is undefined, and file loss/corrupt compressed
bytes cannot silently become zeros. Final absent-chunk checks also inspect
parent links and symlink_status so a dangling link is not mistaken for fill.
No new code/codec dependency: use already pinned nlohmann-json and prefixed zlib
with null/zlib/gzip only. Blosc/filters/remote/Zarr3 remain explicit refusals.

Independent development-only standard-library author tools/make_ome_zarr_fixtures.py
freezes169 files /21669 bytes under tests/fixtures/ome-zarr, exact per-file hashes
in its manifest; the parent manifest itself is admitted in tests/fixtures/manifest.json
under AGPL-3.0-only. These are synthetic values with no biological claim. Tests
cover multichannel C/Z/T axes, edge chunks/whole-versus-tile seams, C/F and endian
combinations, raw/zlib/gzip, scalar extrema and nonfinite samples, scale/origin,
padding versus valid fill, undefined fill, malformed metadata/unsupported codec/
filter/dtype/geometry, limits, pre-cancel, corruption, deleted/changed files and
new file membership. This is not yet all reader/persistence/frontend acceptance.

build/feature-expansion/o02-native/hole-guard-build.log exits0; final
hole-guard-ctest.log passes OME-Zarr and existing ChannelSource1.69s. Native-only
System32/Windows PATH run exits0 (native-only-final.log/host.json), executable SHA
35ceae16344d906f2750c11991ee3d644336d9f33327cacda502b006e22f2849 on HEQPC.
Earlier ctest/native logs are diagnostic intermediate results, not qualification
transferred to the final hole-guard source. Final source-receipt.json retains the
base/owned source delta and exact new-file/binary identities. The unchanged O01
app/runtime stays separate; no GUI/binding/SDK/wheel/package rebuild is claimed.

Waste/corrections: direct zlib target usage initially propagated a global include
and triggered broad core recompilation. Stop the identified build tree, retain
build.log/partial objects, scope headers/feature macro to the Zarr unit and use
link-only codec linkage. MSBuild tracking was already invalidated, so the scoped
retry still pays a core recompile; dependencies/GUI were reused. Native core then
builds, but test helper copy conflicts through filesystem ADL; rename helper,
retain failure log and rebuild affected units only. Review fixes common-origin
cancellation, exact XY pyramid scale admission, metadata capacity accounting and
absent-chunk path handling. Final retry compiles only store/provenance and relinks
native checks. No broad corpus, repeated scientific campaign or new build cache.

Next: integrate the directory descriptor and versioned native provenance without
truncating it into the existing63-companion contract. Add an ImageServer/view
adapter and save/reopen identity checks over this same decoder; preserve legacy
TIFF/codec bytes and independent UI edits. Complete-store real fixture candidate
is the admitted CC-BY4 OME Consortium tubhiswt-2D512x512x2 pair, preserving BOTH
original channels/all samples and attribution. Native viewport/raw measurements
on that full disclosed derivative remain required. Do not close O02 or claim a
released/general OME-Zarr reader before those exits.

### O02 native image adapter and complete real store

open_ome_zarr wraps the SAME immutable decoder through native ImageServer RGB/raw
reads, explicit channels/windows/Z/T and independent projection close state.
Generic raw_view/with_raw_view/require_raw_current preserve old TIFF APIs through
default delegation. Document planes/set_raw_view use quantitative capability;
Zarr has its own versioned/checksummed view codec. Typed values never come from
RGB. View metadata, output, decoder and full-inventory checks compose work limits.

ReaderSource.directory_store retains whole-directory observed membership/file-
version SHA plus primary metadata, not a truncated63-file set. QPV5/object and
QPV6/source-only, QSV2 verification and JSON3 retain it. Legacy QPV1..4/QSV1 and
TIFF view branches remain unchanged. New raw-source identity binds the token only
for the new scope. A changed chunk fails directory verification even when .zattrs
fingerprint still matches. FileFingerprintJob hashes that primary file and returns
matching_primary_metadata. Reports cannot write into the source tree, including
Windows physical/case aliases. This is neither whole-store byte hashing nor an
atomic filesystem transaction.

o02-server/build.log builds the batch. Initial ctest.log has five passes and a new
server-test refusal: tile2 is below the EXISTING raw-statistics minimum16. Correct
only the request; retain validator/expected results and add the settled opt-in
full-data route to the same driver. real-build.log exits0; final-ctest.log passes
six suites2.78s: ome_zarr, ome_zarr_server, source_identity, channel_source,
raw_image and project. Covers RGB/padding, raw means, generic planes/immutable
view changes, codecs, primary-only hashes/exports, bad descriptors and stale
whole-directory refusal. Existing TIFF/project/source regressions pass.

The full real fixture is a disclosed development-only Pillow/stdlib lossless
Zarr2 derivative of BOTH retained OME tubhiswt-2D512x512 uint8 images. OME Consortium
attribution and the [CC-BY4 statement](https://downloads.openmicroscopy.org/images/OME-TIFF/2016-06/tubhiswt-2D/readme.txt)
remain with COPYING in the store. Original XML has no physical pixel calibration
or channel names: preserve unknown units and numeric label placeholders. All524288
samples remain, without crop, quantization, normalization or biological inference.
One level, TCZYX1x2x1x512x512, chunks1x1x1x128x128, zlib4. Complete37-file store:
255605 bytes, exact lineage/hashes in o02-real/fixture.json/admission.json and main
fixture inventory. The unchanged staging guard passes all37 files. External
build fixture/preparer are not package inputs or automatic CI data.

o02-real/native.log/host.json records a System32/Windows-only PATH run. EVERY raw
sample/valid mask, C/Z/T/spatial axis, sample type and calibration matches existing
native TIFF, followed by raw means/min/max/finite/nonfinite counts over262144
pixels. Explicit C0=7..48, C1=0..255, disabled blue yields exact whole-image RGBA
equality. Inspected real-zarr.png for meaningful channel/geometry interpretation;
this is native read_region output, not a desktop interaction claim. Original TIFF
SHAs a1d99f06...813ef2 and c17cb626...f1a2a51f remain unchanged. No full-slide speed,
biology or general NGFF compatibility claim.

A live independent GUI build (CMake115076/MSBuild192952, references disabled)
occupied the shared cache. Wait for verified terminal state while preparing the
fixture independently. Common SourceIdentity/ImageServer/Document ABI changes
legitimately rebuild native consumers once. GUI objects were compiled after those
headers; app and two GUI tests then relink against new core with references
disabled (app-relink.log/gui-abi-relink.log). gui-abi-ctest.log passes shell/
organization0.34s. This fixes ABI integration and does not transfer older desktop
acceptance to a Zarr workflow. No binding objects, SDK, wheel or package rebuilt.
Frozen Python binaries retain their original ABI; old objects cannot relink this
new core until the settled P01 rebuild.

At this adapter checkpoint O02 remained open:14/22 complete,8 pending. Project reader/link/save/reopen still had to
retain expected directory/view identity and compare directory changes even if
primary bytes match. Then actual desktop source/channel/plane controls and saved
real-store acceptance. Direct backend/rendering proof cannot close those exits.


### O02 project and desktop closure

O02 closes locally:15/22 complete,7 pending (L01, A01–A04, P01/P02). Next A01
QC/normalization over the full intended expression universe, preserving exact raw
counts and explicit zero/missing/continuous-domain semantics. Do not replay O01/O02
or rebuild bindings/packages until their planned integration checkpoint.

Project native reader/factory supports ome-zarr and .zarr auto-detection. A private
checksummed versioned link payload reuses existing quantitative-view storage with
an explicit reader discriminator. It retains selected view, resource limits and
expected directory token. Offline open/save retains view/history without opening
the store; lazy reads enforce that token. TIFF payload branches stay unchanged.
Raw measurement capture/replay uses the generic capability; QRA5/natural and
QRA6/grid retain directory identity, leaving legacy1..4 unchanged. Source review
compares directory inventory even when primary .zattrs bytes match; its coverage
is directory_primary_only, never matching whole-store byte hashes. Registration
identity binds the token; shared export guards protect input directory subtrees.

Actual Quantitative channels controls change C/Z/T/windows through the native
worker, then adopt its already prepared candidate. No second decode on the owner
and no full-directory scans during frame polling. App/chooser reader selection
adds OME-Zarr; one image group has one series. No Python product logic was added.

Local evidence is build/feature-expansion/o02-project (external build evidence,
not packaged data). consumer-build.log exits0; native-ctest.log passes five affected
suites1.60s. frontend-ctest.log also passes application_entry/project_save, while
retaining the initial new-driver failure. gui-final-test.log passes actual GUI
channel/plane/measure/keep/save/reopen controls0.69s. Exact rectangle/axes/valid/
finite/nonfinite counts and min/max accompany mean tolerance1e-12. The existing
Welford update gives errors0,-1.1369e-13,+4.5475e-13 on this small integer example;
mathematical-integer bit equality was an incorrect new-test assumption. No product
raw value, archive/source identity or stored replay assertion was relaxed.

real-project.log exits0 on the complete retained37-file tubhiswt store under
System32/Windows-only PATH: all512x512x2 are measured, saved, reopened with their
record and identical RGB. Reuse the earlier ALL524288-sample independent TIFF
comparison; no decoder campaign repeated. real-app-project.log and
real-app-source.log exit0 for saved project and explicit source startup.
real-gui.log passes; inspected real-desktop.png shows native real red/green tiles,
uint8/two-channel metadata, explicit unknown physical calibration and the saved
record. This qualifies the selected local workflow, not general NGFF, biology,
throughput or a release.

Waste retained: two test-only include ownership fixes; isolated GUI driver lacked
the established deterministic event setup AND hover-before-press frame, then
used incorrect bit equality for floating means. Logs preserve each failed
attempt. Test-only repairs reuse settled core/GUI objects with references disabled;
source-receipt.json distinguishes final owned source from the independent dirty
UI overlay compiled into this runtime. No SDK/wheel/binding/package rebuild.


### A01 native full-universe QC and normalization

A01 native foundation is delivered; the checkbox remains open for actual desktop
QC/raw/normalized/log selection and recipe keep/reopen. Progress remains15/22
complete,7 pending. New standalone native header/services do not change existing
common ABI or rebuild bindings. Keep the independent UI overlay separate.

Expression QC reads all source features/observations in conservative bounded
batches, not the current display projection. Exact uint64 observation/group sums
and nonzero detection preserve2^53+1 and reject overflow. Explicit zeros and
sparse absence remain measured zeros. Zero-library normalized values are valid
zero; group fractions are undefined. Only explicit raw-count matrices qualify;
continuous/already-normalized sources refuse. Optional named source-index groups
may overlap. No biological feature-group inference or silent missing-value fill.

One retained QC result feeds lazy continuous library-size/log1p matrix sources.
Only requested gene blocks are transformed. Parameters and full parent/QC/group
identity enter versioned recipe/source/feature/block identities. Derived sources
have empty raw locators/digests so raw-sidecar association cannot mislabel them.
Existing spatial snapshots and gene display consume their continuous values.
Project metadata stores a checksummed canonical ASCII recipe in bounded scalar
chunks, not dense/QC arrays or a copied matrix. A captured native open request
reopens the existing raw association and recomputes/verifies QC once at replay;
retain it afterward. Keep/remove are native undoable metadata commands.

Workspace-root build/feature-expansion/a01-native/build.log exits0. ctest.log
passes expression_normalization, expression_matrix, expression_matrix_project
and expression_matrix_spatial1.50s. Covers analytical sparse values, explicit/
implicit zeros, all-feature denominators, zero libraries, overlapping groups,
exact large counts/overflow, parameter/source identities, budget/cancel/stale,
Unicode/chunked project persistence/replay, accidental raw-association refusal
and downstream native gene projection. Post-review correction additionally
reserves the caller's group definitions while the QC result copy is live.
real-build.log exits0; real-native.log executes all those assertions again on
the corrected source before the full-data route and exits0.

The dev-only independent real-oracle.py reads every one of16,031,101 original
retained 10x MTX entries using Python integer sums/detection, never as a product
fallback. Source SHA0e216380...45fd26 matches D01 admission; expected TSV SHA
393677ce...13084. All32285 features/2702 observations are included, total counts
85825294, no zero libraries. Explicit group is source features0 and32284, without
biological labeling. No new download or reimport: native code reads retained
d01-real/mouse-brain.qmx, exact SHA79f407bf...a7401. Existing CC-BY4 lineage applies
to the derivative oracle. real-oracle.json preserves exact identities.

System32/Windows-only PATH native run matches EVERY observation total, detected
feature count and group total against that independent oracle, then checks
selected derived values and saves/reopens the native project.256MiB QC uses253
batches;64MiB saved replay uses385 and preserves exact QC, recipe and derived
block identity. Full feature universe and sparse geometry remain intact.
real-run/real.json records QC4289.23ms on HEQPC; this is a cached host-local
observation, not QuPath comparison, throughput acceptance or RSS qualification.

Waste: first combined build/tests passed. One focused follow-up rebuilt only the
new kernel/consumer after the input-capacity reservation correction and opt-in
real driver; existing native objects were retained. No broad campaign, repeated
import, normalized-store duplication, GUI/binding/SDK/package rebuild. Next wire
existing native workers/controllers to retained QC/derived sources and saved
recipes, explicitly rebinding spatial coordinates from their raw parent. Avoid
per-frame QC, a second frontend algorithm and applying a display log twice.


### A01 desktop QC and normalization closure

A01 closes locally:16/22 complete,6 pending (L01,A02–A04,P01/P02). Next A02
regional summaries and pseudobulk with explicit cohorts/overlaps/replicate groups
and exact sums. Preserve all raw counts and do not label spots as biological
replicates. SDK/optional Python remain P01; one packaging handoff remains P02.

The existing native GeneViewSession worker now owns QC/normalization alongside
selected-gene projection and colors. No extra GUI analysis future or scheduler.
Raw spatial capture is retained, with explicit ID/coordinate rebinding for the
derived source. Complete exact QC survives mode/target/gene/color changes;
color-only changes also reuse the same derived source/spatial snapshot. Source
changes invalidate by scientific identity; compatible owned Save As locators
retain QC while the new parent is verified. Projected/continuous inputs cannot
provide raw libraries, and log-normalized values reject a second display log.
Current worker failures clear cached QC/normalization; cancelled generations
cannot publish late results. Gene-view preference serialization is unchanged.

Gene expression exposes a separate QC / normalization tab with full-universe
feature/observation counts, explicit source observation index, exact library total
and detected features, optional stored groups, target and raw/normalized/log
choices. Native Keep/Apply/Remove controls use existing metadata/undo. Saved
records are filtered by raw parent and discovered on document revisions. No
per-frame matrix scan, owner-thread QC or copied normalized sidecar. Optional
feature-group authoring remains native API; the desktop presents saved groups.

Workspace-root build/feature-expansion/a01-desktop/build.log exits0 for the settled
controller/GUI/consumer batch. native-ctest.log passes expression_normalization
and gene_view0.56s, including cache pointer/source reuse, exact raw restoration,
coordinate/ID preservation, target change, saved QC reuse and late cancellation.
gui-regression.log passes existing gui_gene/gui_spatial_import5.35s, including
paged native gene selection and existing import/save-as paths.

Initial gui-ctest.log fails at the first QC click. Inspected missing-action.png
shows the test's independently placed viewport covering the product's default
centered tool window. Load a test-only saved left-side tool position; keep all
actual controls/clicks/assertions. driver-build.log rebuilds only this consumer
with references disabled; gui-final-ctest.log passes2.41s. No product algorithm
or numerical assertion changed for the driver correction.

real-gui.log/real-host.json records a System32/Windows-only PATH run on the
retained admitted real indexed project. Actual QC action covers32285 features/
2702 observations and85825294 total counts; raw/linear/log/Keep/save/reopen/Apply/
Remove/Undo pass. QC is reused within a session, exact derived values replay
after reopen, and every observation ID/coordinate stays equal to raw capture.
Original input project SHAa31dcd39...b9bd7e is unchanged. The real source math
reuses the independent all16,031,101-entry native QC oracle from the prior slice;
no new MTX oracle, source import or download.

Inspected real/reopened-panel.png: real native image tiles and aligned expression
glyphs, readable full-source QC, source row0 total20935/detected5230, target10000,
explicit log-normalized units and saved recipe controls. Zero2451/missing0 for
selected ENSMUSG00000051951 remain distinct. This is meaningful values/geometry
proof, not cosmetic parity or biological interpretation. real-app.log exits0
opening that saved project in the integrated application.

Waste: one test-only window-occlusion correction diagnosed by its preserved
framebuffer. Compatible numerical evidence, core/GUI libraries and old binding
objects were retained. Public controller headers changed, so their native/GUI
consumers were rebuilt coherently before checks. No binding/wheel/SDK/package
rebuild, broad platform campaign or comparative throughput/RSS claim. Source
receipt separates owned changes from the independent dirty UI overlay.

Final ownership check: ownership-test.log passes2.34s after making the authored
source owned and keeping the panel live across Save As. Compatible QC/mode/recipe
survive the verified physical parent change. Actual Keep clicks while editing is
blocked publish nothing. Only the test driver changed; ownership-build.log reused
settled libraries. real-final-gui.log/real-final-host.json repeat the consumer
route on that final driver, preserve before/after executable/input identities,
and pass. Use real-final/reopened-panel.png as the final inspected capture.
The earlier real-gui/captures remain diagnostic evidence of the unchanged UI.


### A02 regional expression summaries and pseudobulk

A02 closes locally:17/22 complete,5 pending (L01,A03/A04,P01/P02).
[Usage/native contract](../../expression-aggregation.md). Next planned scientific
packet is A03; do not replay delivered A01/A02 or start a release campaign.

New shared C++ services own ID/ROI cohort capture, sparse reduction, specification
interpretation, persistence, replay and export. The batch CLI only parses typed
options and invokes them. Original matrices, point coordinates and ROI geometry
are unchanged. Raw sums remain uint64 (including above2^53), means are binary64,
sparse absence is measured zero, and an empty cohort has zero sum/absent mean.
Results are feature-by-cohort matrices; observations-by-features stay sparse.

ROI membership uses existing SpatialIndex/Roi::contains at spot centres with
explicit source or retained-registration frames and independent/reject overlap
policy. Captured origin fingerprints include exact spatial/frame/geometry/
registration/overlap identities. Replay uses the saved source-axis memberships,
not subsequently edited live ROIs. Pseudobulk requires full raw counts, disjoint
memberships and explicit sample/replicate/group labels; one replicate cannot
change sample/group between columns. Regional summaries may explicitly select
an A01 normalized/log recipe. No spot-to-biological-replicate inference or DE
acceptance claim is made. Initial bounds:64 cohorts/65536 total memberships,
768KiB chunked recipes and16..1024MiB accounted work; existing project quotas apply.

Workspace-root `build/feature-expansion/a02-native/build.log` records the combined
cached core/batch/affected-test build (exit0). The initial aggregation CTest passes
0.36s. `final-ctest.log` preserves four passing existing matrix/spatial/project/
normalization suites and the later ROI test-fixture failure. That new document
fixture used a negative-x rectangle; native ROI admission correctly refused it.
Only the driver rectangle was changed to select the same exact observations
inside the image. `driver-correction-build.log` recompiles that test only;
`aggregation-final-ctest.log` passes0.67s. No product algorithm/assertion was
weakened and no passing existing suite was repeated for the fixture correction.

Coverage includes hand-calculated overlapping/empty/zero cohorts and means,
source-ID joins independent of spatial row order, explicit registered membership,
raw values above2^53 and overflow refusal, required/consistent replicate labels,
normalized regional values and normalized/selected-feature pseudobulk rejection,
malformed blocks, stale/cancel/budget failure, owned save/reopen/undo, canonical
checksums, new-only atomic CSVs and exact observation association exports.
A5000-member chunked recipe replays exactly across different bounded read batches.
Both source and registered ROI specifications run through the native application
service; native CLI creation/verification/results/membership export are exercised.

`process-smoke/host.json` records actual qupathxx_batch create/verify/CSV/member
processes and existing-output refusal from System32 with Windows-only PATH and
PYTHONPATH removed. All eight analytical sums and five exported observation
memberships agree with the native fixture. Before/after executable/input hashes
match. It is a local native consumer check, not a new installed SDK or package.

`real-native.log`, `real-host.json` and `real/real.json` record the retained admitted
10x full matrix through whole-image/empty native ROI specification, save and
replay:32285 features,2702 spots and85825294 exact counts. The all-region total
reuses the independent original-MTX QC baseline from A01. No new source import,
download or full numerical reference campaign.256/64MiB replay uses253/1077
batches with exact recipe/membership/count identity. The original project and
native driver hashes are unchanged. This single-mouse regional engineering check
provides no biological pseudobulk/DE or comparative performance claim.

The A01 desktop dirty-source handoff was first preserved as verified
`a01-desktop/source-handoff.zip` plus ownership/evidence hashes. A02's final
`source-handoff.json`/`source-owned.zip` retain the new native sources, shared-file
snapshots, runtime/core/test identities and separate scope for concurrent UI.
The existing38 binding objects remain byte-identical. Native compiler/runtime
inputs are distinct from the earlier frozen products; no old qualification is
transferred. No GUI/binding compilation, new dependency, SDK/wheel/package,
release, merge, source reset or cleanup occurred for A02.

Waste/correction: one missing cmake PATH launch (no compilation), then one added
ROI test-fixture admission correction. Absolute cached CMake paths and test-only
rebuilds resolved them; the settled core/CLI libraries and prior passing results
were reused. Existing link/cache warnings remain in their logs. Independent UI
edits, original fixtures, previous products and failed logs are preserved.


### A03 sparse PCA and deterministic clustering

A03 closes locally:18/22 complete,4 pending (L01,A04,P01/P02).
[Native method/format/usage](../../expression-reduction.md). Next planned packet
is A04 with admitted independent biological replicates; the single-mouse demo
is not DE acceptance data. Stop after this A03 handoff.

Shared C++ services own implicit-centred sparse products, bounded two-pass
orthogonalized seeded subspace iteration, existing OpenCV4.11.0 thin SVD,
explicit-k k-means++/bounded Lloyd updates and typed owned results. No process
RNG/thread settings change. Only low-rank sketches/results are dense. Original
counts/coordinates stay intact; direct integers above2^53 refuse. Raw/continuous
or explicit A01 normalized/log domains and selected feature universe are retained.
Uncentred SVD/energy is labeled separately from centred PCA/covariance.

A checksummed open native .qpr archive preserves exact scores, loadings, means,
singular values/energies, centres/labels, preprocessing/seed and source axes. A
small undoable project association and scoped native save/open/output hooks
preserve exact owned bytes during Save As. Opening/exporting never refits;
explicit replay verifies exact fitted identity on the supported backend. Work
telemetry does not change that identity. Numerical closeness is not model identity.

The new authored tests/fixtures/expression-reduction/reference.json is an
independent NumPy2.5.4/LAPACK oracle from development-only
`tools/reference_expression_reduction.py`, with hash/AGPL attribution in the
existing fixture inventory. Native tests read frozen JSON without Python. Raw
centred/uncentred and normalized/log cases compare means, singular values,
energies and sign/subspace-invariant score Gram/loading projector at absolute
1e-8 + relative1e-9 tolerance. Analytical rank2 energies302/6, total308 and cluster
inertia8 are independent checks. No numerical tolerance was relaxed.

Workspace-root `build/feature-expansion/a03-native/build.log` records one combined
cached core/batch/affected-test build, exit0. ctest.log passes all7 selected native
suites3.21s: reduction, matrix project, normalization, aggregation, project, save,
recovery. Coverage includes full uint64 seed, repeated singular subspaces,
rank/constant/cluster/iteration limits, global RNG preservation, bounded sparse
reads/operations/distances/cancel, stale/malformed data, canonical archives and
invalid labels/checksums, exact owned relocation, undo/output guards and CLI/CSV/
SVG. A500-feature/2000-observation sparse fixture retains exact fitted ID across
different read batches/work sizes.

Source review completed XML-illegal control-character refusal at the SVG output
boundary and escaped-ID/one-component regressions. export-build.log and
final-reduction-ctest.log pass0.90s. Numerical/project logic was unchanged; six
passing existing regressions reused. process-smoke/host.json records actual
standalone create/verify/refit/CSV/loadings/SVG, operation-budget and existing-
output refusal from System32 with Windows-only PATH/PYTHONPATH removed. Input,
executable and saved project hashes match. The native-process score Gram also
matches the independent oracle. This is local consumer evidence, not an SDK or
new package/release.

real-native.log, real-host.json and real/real.json reuse A01's admitted retained
10x matrix/normalization recipe: explicitly first512 of32285 genes, all2702
observations,4 components/clusters, seed123. Converges in10 iterations;24 feature
batches,21870284 accounted operations,118888 cluster distances,259178240-byte
native reservation. These are named-host engineering observations, not RSS or
throughput comparisons. Owned save/open and explicit64MiB refit preserve exact
arrays/labels/ID. Original project/driver hashes remain equal. Full-source A01
normalization is used; no source reimport/download, recommended feature set,
cell-type/biological clustering or DE claim.

Rendered inspection found clipped17-digit vertical tick labels despite passing
XML coordinate checks. Only SVG margin and6-significant-digit presentation labels
were repaired. Exact arrays, CSV/tooltips and numeric assertions remain intact.
scale-build.log and scale-ctest.log pass0.95s; scale-export-host.json re-exports
retained synthetic/real archives without refitting. plot-readable-check.json
verifies all6/2702 plotted IDs, cluster colours and CSV coordinates, and unchanged
unit fitted IDs. Inspected process-smoke/scatter-readable.png and
real/scatter-readable.png show readable scales/domain/seed/legend/convergence/
identity. Original clipped views remain diagnostic evidence, not cosmetic targets.

Browser preview host was unavailable; a development-only MIT resvg_py0.5.0
renderer reads the unchanged native SVG. Its --target installation and logs stay
outside product source/runtime/package inputs. The venv lacked pip; installed uv
resolved that launch failure without changing the venv. Native scientific and
application/export authority remains C++; Python is only independent oracle,
verification and render control. The admitted optional OpenCV core is reused;
no new product dependency or reader was introduced.

A02's source archive was verified and its passing executables preserved before
cache links changed. A03 source-handoff.json/source-owned.zip retain owned sources,
fixtures/shared snapshots and initial-real/final-export runtime/source scope.
All38 binding objects remain identical. Independent UI edits and frozen
products are preserved. No GUI/binding compilation or install, SDK/wheel/package,
release/merge, worktree retirement or recursive cleanup.

Waste/corrections: lookup-only paths, browser-host/pip availability, XML boundary,
visually demonstrated scale clipping and a status preflight matching two phrases
instead of one (stopped before file writes). Affected export/native links and
saved models were reused; no additional numerical campaign. Existing cache/link
warnings remain visible in logs.


### A04 replicate-level differential expression

Delivered locally through shared native C++ services and batch CLI. See the
[guide](../../expression-differential.md) and public expression_differential.hpp /
expression_differential_workflow.hpp. Native A02 raw full-universe pseudobulk is
the input authority; disjoint cohorts of one biological replicate are summed
before normalization. Sample and replicate IDs are one-to-one and unpaired,
with two explicit groups. No Python scientific/runtime or new GUI consumer.

`welch-log1p-pseudobulk/1` computes full-library target10000 log1p values, equal
replicate-weight group means/unbiased variances, B-minus-A mean-log1p effect,
Welch t/df and two-sided complemented Student-t tails. BH uses all testable
full-source features with deterministic tie ordering. All-zero, insufficient,
zero-variance and tail-underflow states are explicit; absent statistics export
as empty fields. Different constant groups do not manufacture significant p0.
Empty/zero-library replicates, overflow and invalid sample mappings refuse.

Optional Boost.Math1.88.0 standalone is pinned by exact archive SHA and retained
BSL-1.0 text/header notices; only its include tree participates. It is OFF by
default and adds no DLL. `statistics-disabled-script.log/json` proves isolated
MSVC compilation of the numerical translation unit without Boost/Python includes;
this is not a complete disabled-configuration product build. The initial command
quoting failure is retained separately; an explicit .cmd resolved it before any
compiler ran. Existing Python-free/default-option contracts remain unchanged.

Evidence root is build/feature-expansion/a04-native (local, outside source-kit
fixture/package inputs). Configure passes in the existing gridfrontends167 cache;
retention-build.log and retention-ctest.log pass, focused suite0.46s. Coverage:
frozen independent NumPy2.5.4/SciPy1.16.2 Welch/BH over authored integer counts;
closed df2 tail p=1-1/sqrt(2); split-cohort raw consolidation; reverse groups;
insufficient/constant/zero-library/overflow/underflow; budget/cancel/stale and
malformed/wrong-result identity; native direct-retain nonpublication, undo,
owned save/reopen/relocation, thin CLI and atomic new-only gene/replicate CSV.
No assertion or exact stored identity was weakened.

Real original-author Tabula Muris FACS figshare5829687v8 has explicit CC BY4.0
terms and mouse/sex metadata. The complete Kidney table contributes23433 genes,
865 cells,956991 nonzeros and309205946 raw counts. Its six independent mice are
2 female (group A) and4 male (group B); cells/plates never increase n. The full
open CSV is losslessly transcoded for the existing native MTX/TSV input route,
with no count/gene/cell filtering or product fallback. Input/derivative hashes,
original license evidence, attribution and ancestry are in the fixture inventory.
The real check is numerical/association acceptance, not biological discovery,
cell-type effects, covariate adjustment, power or DESeq2 acceptance.

real-native.log, real-host.json and real/real.json record native import/A02/A04,
owned project save, exact64MiB replay and export from System32 under Windows-only
PATH with PYTHONPATH removed. Six mice yield17002 testable genes,6431 all-zero;
15788817-byte DE allocation reservation (not RSS). Exact source is
88caef1a6501805188359e5bfb075d3c6caabb1964c79229751893c786881db5;
pseudobulk c736f2c46e760188d8fa308efb50e6782b741026a31afdba5c13e641c43d76ba;
DE record3a56e5983b8bd260428ffe950c57d6ccaa07154109dd6a0f7a4016e8ffb0be17;
result41ecfcf7350f3c1c05460b4252958f82d91a0c40249e4147e8acb47b7bfecfd9.

real-oracle.json independently re-reads original CSV/author metadata, sums each
mouse and uses SciPy Welch/BH. All gene IDs/indexes, library totals, status and
statistics agree at rtol2e-10/atol2e-12. Max absolute errors: effect8.88e-16,
t2.14e-14, df7.55e-15, p2.50e-15, q4.67e-15. Development reference packages are
isolated outside the product. Frozen authored JSON lets native suites run with
no Python. Raw import, numerical and source-association proof is reused after
the final metadata-currentness-only fix; no real reimport or earlier campaign.

process-smoke-host.json covers actual native CLI create/verify/CSV, existing
output refusal, absent group and output budget; failures publish no partial
result. process-final-host.json repeats only changed successful retention/create/
exact replay/export against the retained real A02 project with the final binary;
CSV/record/result identities equal the numerical baseline. Original project and
executable hashes remain unchanged during each run; PE imports contain no Python.
The final launcher initially copied case-insensitive Windows environment keys
into a plain dict and stopped before native execution; the correction reads
SystemRoot before the copy and reuses the verified empty output directory.

Native records are checksummed canonical recipes referencing exact retained A02
and source identities plus method/options and expected result digest. Verify/
export recompute this declared deterministic recipe and require exact identity;
no closeness substitution or duplicate floating result sidecar. Direct retain
checks native source currentness before metadata publication as the app does.

The first build's wrong Sha256 namespace was corrected in one line. A stale-test
ordering edit overlapped compiler file read; later object timestamps masked the
old body, so the first test stopped on invalid snapshot reuse. Logs/fixtures are
retained. Finish builds before source mutation; a fresh changed test compiled,
and native currentness checks remained strict. Baseline LNK4098 warnings remain
visible in logs. A mistargeted documentation append anchor changed no file.

admission-selected.json/admission-check.json run the existing offline guard over
all10 A04 original/derivative/backend/notice records and every declared ancestor,
checking exact bytes/hashes. Whole historical-inventory loading instead stops on
older unhashed qupathxx-authored-h5ad (admission-initial.json). This selected input
admission is not final L01 membership closure; repair/enumerate the actual final
release inputs there. No restricted/NC/unreviewed data was downloaded or admitted.

source-owned.zip/source-handoff.json preserve all A04 owned sources, current
shared-file snapshots, predecessor archive references, exact binary/core/runtime
hashes and configuration scope. A03 products are preserved before replacement;
all38 existing binding objects remain byte-identical. Independent UI edits and
frozen candidate products are preserved. No GUI/binding/SDK/wheel/package build,
merge/release or cleanup. A04 closes at19/22; P01, final L01 and P02 remain.


### P01 relocatable native SDK

Delivered locally: Windows x64/MSVC19.44/v143 Release dynamic CRT / iterator
level0, C++17 minimum. The [guide](../../native-sdk.md) documents exact supported
scope, native use and optional components. No Python binding, GUI package,
wheel, new scientific kernel, release publication or platform campaign.

CMake opt-in QUPATHXX_INSTALL_SDK installs only NativeSDK, independently of
existing app/Python layouts. Exported qupathxx::core, density_workflow and
nucls_import use SDK-local private dependencies; header-only JSON/Boost do not
leak include requirements. Configure audits the actual native target closure
and refuses unmapped dependencies. Headers expose source/version/features and
force ABI checks. SDK package discovery uses only Threads/toolchain facilities,
not Python/nanobind or system reader/backend searches. Unknown components,
static CRT and Debug CRT refuse with expected diagnostics.

Evidence root build/feature-expansion/p01-native-sdk retains configure/build,
CRT correction, install, relocated consumer, raw TIFF/JPEG, negative and byte/
path/PE checks. Native SDK staging installs180 exact input-matched files:
14 application/private/import libraries,3 DLLs, public/generated headers and
required notices. sdk-membership-check.json maps every file to its actual build
origin and checks byte hashes. Only OpenCV core/ml/private-zlib are copied;
unused development-SDK codec archives are excluded. No restricted inputs are
admitted by the SDK wrapper. Final L01 release-wide/source membership remains.

Archive CRT inspection diagnoses the old LNK4098 source: only libjpeg-turbo's
archive requested LIBCMT; core/all other static inputs requested MSVCRT. Upstream
WITH_CRT_DLL=OFF overrides CMAKE_MSVC_RUNTIME_LIBRARY. The wrapper now follows
the parent's CRT, retaining original source/codec/options. The old archive is
preserved under prior-products/jpeg-static-MT.lib with its hash. The new JPEG
archive requests MSVCRT/OLDNAMES; native raw TIFF passes0.42s. SDK consumer link
has no LNK4098, and no default library is suppressed. Other backend libraries
are reused byte-for-byte; no wholesale native rebuild or numerical campaign.

One metadata/component build preceded the audit; first reconfigure took162.5s
under host load. Two stale NuCLS objects compiled with otherwise retained inputs.
The diagnosed JPEG build is followed by one source-provenance-only refresh for
an external JPEG observer argument-order correction, made after the live build
finished. Core numerical authorities and API layouts are unchanged. Existing
A01/A02/A03/A04 numerical receipts are reused at their original scope, with the
new source identity and installed consumer checks recorded independently.

The SDK is moved, after absolute-boundary/no-link/no-executable/process-use
checks, from sdk-stage to a new external TEMP prefix containing spaces. Its
original prefix is absent. A copied consumer uses Visual Studio's independent
native CMake3.31.6, with Windows-only PATH, Python paths removed and package
registries disabled. No original checkout/cache/dependency SDK is a build input.
All installed public headers compile.469 compiler include records, exported
properties and library logs refer only to installed SDK and VS/Windows toolchain
inputs; no source/cache/dependency/old-prefix requirement is found. This is an
input/dependency audit, not an OS filesystem-denial or fresh-host qualification.

consumer-host.json records real native component entry points, complete authored
matrix import, exact100-count QC libraries, normalization, raw pseudobulk, seeded
PCA/clustering, Welch/BH, owned native save/reopen and exact identities/CSV.
Source SHA bb6008f879db2f2eb88b1c62af8e286b3a2e7ab031005691df531ba49caf20d7
agrees between installed headers/library. DE record
5e9a72c050764965f98180a7624d6ad330067046211b7d164e34adae991ed932,
result85f1cbeac61e6f6981b9485741397e84feda5d155ea8e6e119e417cf506e098b,
PCA738bf72602ba9116d530f3f6c148c587d9e91b3c75fc7ca2a3559eb2cb51d8e2.
This authored consumer is an API/installation/identity check, not biological data
acceptance. Its native raw TIFF route also matches eight independent decoded
JPEG fixtures exactly (scalar/RGB, strips/tiles, complete/shared-table streams).
The existing admitted reference generator supplies these small new-CRT checks;
no old409-case, installed-Python or Linux/macOS campaign is replayed.

negative-checks.json verifies expected static CRT, Debug and unknown-component
refusals. SDK/executable hashes remain unchanged through consumer execution.
PE inventories of its executable and three SDK DLLs contain no Python imports.
The cached optional Python build flag is ON from earlier work; native SDK
configure/build/run uses neither that interpreter nor its bindings. Plain native
CMake still defaults Python OFF. All38 binding objects remain byte-identical.

The local qupathxx-native-sdk-windows-x64-msvc1944-p01.zip contains the180 native
install members plus portable documentation and a relative-path file manifest.
sdk-package.json records its exact size/hash and verified ZIP-member bytes.
The source handoff retains owned SDK CMake/templates/example and current shared
snapshots, exact core/dependency products, predecessor receipts and the canonical
notebook update. External consumer sources/products/results and negative logs
are preserved in a verified archive. Automatic review rejected recursive deletion
of the temporary test tree ("blocked by policy") before execution. It remains
inactive and intact; no alternate deletion is attempted and no user data removed.
P01 closes at20/22, leaving final L01 and P02. Frozen native products and
independent desktop work remain intact; no SDK claim is transferred to an old
published candidate, wheel or different compiler/platform.


## L01 — final native material membership

Delivered locally2026-11-10. Evidence is under the integration workspace's
`build/feature-expansion/l01-admission/`: `source-handoff.json`,
`installed-membership.json`, `source-inputs.sha256`, `release-final/admission.json`
and `dependency-sources-final/dependency-sources.json`. The existing fixture/font
inventories remain the authorities, now with1186 annotated exact material records;
there is no parallel license database or automatic admission of future files.
See [native release material admission](../../native-release-materials.md).

Native source ID is
`68bf2d43fbbb0bd0bcfac19f598bbca91496e737f3c50d5d223ae1905d000be0`,
805 exact CMake fingerprint inputs at HEAD39823dee+dirty. This integrates the
owner's completed UI export/review work; its final standalone receipt has the
same app SHA256 `d379888ba1911fb158f9dc6097587b78aa85f47c19a250a4597ade3e0348b8eb`.
Two intermediate installs became stale as that work advanced; they are retained,
not paired with newer source. The initial pin/FTL-only scope description does not
qualify later UI changes. P01's separate SDK/source ID and consumer evidence stay
frozen; its SDK archive hash13f8050a... remains unchanged.

All three final directory stages and actual ZIP members pass both admission and
exact path-to-material mapping. Unknown payloads, restricted/NC/unreviewed parents,
unsafe/colliding paths, links, oversized content, admitted-file substitution,
changed binaries/source and extra installed members refuse. The collector is
pinned/offline by default and requires matching canonical native source identity.
The dedicated admission CI workflow runs the authored boundary tests; no remote
job or publication was dispatched for L01.

| Local archive | Members | Bytes | SHA256 |
| --- | ---: | ---: | --- |
| qupathxx-windows-x64-native-l01.zip |69|33,108,325|`f3ac1e1366ca41423cf47677900bfaa7371498af24890a0d932a6f733aa117f0`|
| qupathxx-windows-x64-sdk-l01.zip |182|55,245,539|`f1bdcbf9bd6829b0c1062ccba544b74065745d9c7802d67f50d5169c9c62adf9`|
| qupathxx-windows-x64-source-l01.zip |1041|5,694,792|`902c579ee39fbf7837387dc6570e4f8241df846dc7a04cb55ea449a530e86c11`|
| qupathxx-dependency-sources.zip |17|118,600,395|`a8a69aa64f334dc12d174bbf1d83b40811672fe3360c287dd5797e0ab068cc62`|

Native stage has68 install files plus generated release metadata: app, batch and
region CLI, three required DLLs, four fonts, UI resources and notices. SDK has181
install files plus metadata, including120 public/generated headers and14 native
libraries/import libraries. Source kit has1037 selected source/guide/helper/notice
files plus exact inventory snapshots, source manifest and metadata. Generated
records retain explicit lineage in each output's `generated-materials.json`.
No historical screenshot, research dataset or model weights are copied by a
blanket checkout/archive rule. Small admitted authored oracles remain included.

The companion contains fifteen reviewed source artifacts for GEOS, OpenCV core/ML,
SQLite, JSON, OpenSlide and its bundled dependencies, zlib, TIFF, tinyxml2, JPEG,
ImGui, GLFW, HDF5, RmlUi, FreeType and Boost.Math. OpenSlide's mixed components,
OpenCV/STB/Khronos and font licenses have exact individual notice records.
FreeType selects FTL with its required credit; upstream FTL/GPL alternatives are
retained. GLFW3.4 is pinned to its existing immutable commit. Native Python flag
is OFF; all38 binding objects remain identical. Six staged PE images have no
Python imports; no optional Python build/wheel or frozen runtime was changed.

The full OpenCV carrier contains Lena photographs without an independently
reviewed open-image grant. New collection/build inputs instead use its exact,
unmodified selected code members. ImGui's selected source also omits unused font
files and legacy libraries. Selection manifests preserve retained member hashes
and exclusions. No full-carrier fallback is allowed. Code-only OpenCV first
failed configure because doc/data CMake scripts were omitted; preserve that first
archive/log, add only those two unmodified scripts and the configure passes.
The actual source kit configures with all selected native backends/GUI/SDK and
Python OFF. Its first invocation omitted explicit existing SDK notice paths;
correct those flags and configure passes1.9s. These are configure-only source
completeness checks, not a second engine build or reproducible-binary claim.

Focused guard/collector/stager tests:25 pass, one host symlink privilege skip;
archive symlink rejection runs independently. Original logs, interim products and
failed snapshots are preserved. The final path/hash refinement required a source
metadata repack only; no additional native/kernel/binding compilation. Shared UI
churn required cheap CLI metadata relinks; it is not hidden as accepted evidence.
Numerical receipts retain their original scope. P02 must check the current SDK
consumer and final integrated install/use/removal, preserving user data, before
publication. This closes L01 at21/22 locally; no new platform, biological, speed,
stable/signing or full-project acceptance is claimed. Stop before P02.


## P02 — matching Windows native engineering release

Published2026-11-10 as [candidate8](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.8),
tag/source commit `bffcc606407c68ec59ac96d6a2a3386f78da17bc`. The exact native
source remains68bf2d/805 inputs, built at39823dee+dirty. The release Git commit
also contains later validation/docs; its compiled blobs differ from the source
ZIP only through Git newline normalization. No native rebuild or qualification
was transferred to a different body. The four L01 archives retain their hashes,
with candidate8 filenames; validation receipts, release manifest and checksums
complete seven assets. GitHub uploaded states/sizes/SHA256 digests all match.
Anonymous checksum/manifest downloads return the exact verified local bytes.

The current SDK is extracted into a new external per-user prefix. Its included
example/all120 public headers compile with native Visual Studio CMake;471 include
records use installed SDK/toolchain inputs, no CRT conflict or Python import.
Native save/reopen retains exact DE/PCA recipes/results. Installed batch verifies
and exports the same DE CSV bytes, replays seeded PCA and writes its plot; native
region samples match an authored independent crop. Docked GUI startup opens the
saved project without mutation. All251 package-owned files are then removed;
all18 research files and an in-prefix user note stay exact. Three removal tests
prove refusal of modified/out-of-prefix inputs before deletion and preservation
of user files on success. No engine/kernel/GUI/binding/wheel build was needed.

The [deployment receipt](../windows-native-candidate8/README.md),
[publication record](../windows-native-candidate8/publication.json) and
[support guide](../../windows-native-candidate8.md) define scope. Raw local logs,
failed Store-Python prefix and incompatible GUI invocation, SDK consumer cache,
protected outputs and the old rejected P01 TEMP folder remain preserved. The
controller corrections and Windows module-path capitalization are recorded,
without treating unrelated failures as passing negative checks. Source/ref
staging used a separate Git index and release branch; active integration branch,
user edits and remote main were not reset, merged or force-pushed.

P02 closes the22 selected contracts. Support is local Windows x64/MSVC19.44,
C++17, Release /MD engineering prerelease, with host VC runtime/OpenGL prerequisites.
No new Python binary, fresh VM/OS, Linux/macOS package, biological/speed, stable/
signing or full QuPath acceptance is claimed. Stop; no automatic next feature phase.
