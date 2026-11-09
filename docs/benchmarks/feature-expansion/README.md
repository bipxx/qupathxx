# Native feature-expansion progress — 2026-11-08

The [22-item roadmap](../../feature-expansion-plan.md) remains the completion
authority. This receipt covers S01/V01/V02/I01/I02/R01/R02 locally, plus incomplete
L01 admission and F01 native foundations. GUI claims cover only the named gene workflows below; no new
release, speed or biological-accuracy claim.

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
tooling, with no product Python dependency. Complete enabled-dependency/source-kit/
package membership and the actual release-stage integration remain pending;
historical archives are preserved. Restricted datasets/weights were not downloaded.

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
