# QuPath workflow inventory

Selected next phase: [open-format fluorescence and spatial counts](fluorescence-omics-plan.md).
Existing native TIFF/raw measurements are reused; the end-to-end fluorescence
consumer and Matrix Market/TSV spatial-count importer are planned, not delivered.
Advanced fluorescence and broader omics methods remain outside this finite slice.

The current [speed follow-up](benchmarks/speed-gap-followup/README.md) preserves
exact outputs with a guarded integer-grid proof and a corrected steady timing
protocol. [Current task](current-task.md) and [coverage](speed-gap-coverage.md)
retain the open parity target. Historical profiles remain source-scoped; published
releases and broader feature/support scope stay frozen.

This is a reference catalogue, not a mandatory parity backlog. Under the owner's
[scope policy](api-delivery-plan.md#result-fidelity-and-scope-policy), unselected
Partial/missing rows do not block a release or count as required remaining work.
Small cosmetic differences are acceptable; result correctness and meaningful
display/interaction behavior remain required for selected workflows.

Current status is maintained only in the [current task](current-task.md). Follow the [validation policy](api-delivery-plan.md#validation-and-build-policy); historical snapshots are preserved in [history](process-status-history-through308.md).

Remaining implementation and acceptance work is scheduled once in the
[R0-R16 completion queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue).
Reader/display gaps map to R2/R3, annotation/project gaps to R4/R5,
analysis/classification/spatial/TMA to R6-R10, scripting/batches to R11,
whole-slide resources/performance to R12 and release/remaining parity to R13-R16.
Task closeout updates the affected row only when its evidence changes.

R1 reconciliation (2026-11-04): all23 Partial rows below distinguish delivered
mechanisms from actual remaining work. [Qualification308](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md)
is the declared engineering baseline. The later R2-CODEC1 JPEG, R2-SI1/R2-UNIT2 calibration and
[R2-META1 metadata-companion](benchmarks/binaryonly-ome-r2/README.md) changes have
local installed evidence; their new code does not inherit308 platform acceptance. Historical failure/pending labels below its history headings
retain their original dates and do not schedule new campaigns.

Fixture basis: the pinned [fixture manifest](../tests/fixtures/manifest.json)
identifies CC0 Aperio small/full and Hamamatsu NDPI files; generated TIFF/OME,
geometry and numeric fixtures exercise exact semantics. Vendor files/crops are
not an independent labeled biological cohort. The captured QuPath0.7 outputs
are implementation references, not pathologist ground truth. Reuse these inputs;
select additional specimen labels/metrics before claiming biological accuracy.

Baseline: [QuPath v0.7.0](https://github.com/qupath/qupath/tree/v0.7.0), revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`. Source areas below refer to modules
under that revision. Each row names an implementation responsibility, not a
claim of upstream parity. Acceptance applies only to the listed fixture scope.

| Workflow | QuPath source area | Native owner | State | Required evidence |
| --- | --- | --- | --- | --- |
| Dense image regions | qupath-core | io | Partial | Existing region, padding, lifetime and exact-grid implementations are engineering verified in the308 baseline. R2: extend only missing reader/scientific-grid cases; arbitrary large-window sampling still needs R12 resource evidence. |
| OpenSlide regions | qupath-extension-openslide | io | Partial | Existing Aperio/Hamamatsu read, CLI, lifetime and replay gates are covered by308. R2: broader eligible vendor/metadata/damage cases; R12: representative whole-slide budgets. Reuse pinned SDKs and cached fixtures. |
| Metadata and calibration | qupath-core, qupath-extension-openslide | core/io | Partial | Existing typed axes and per-axis calibration are implemented. R2-SI1 is locally verified: the shared OME quantity parser now normalizes the standard SI range and day with unchanged raw samples and companion checks. Explicit BinaryOnly XML/TIFF masters are also locally verified. Remaining convertible non-SI lengths are locally verified in [R2-UNIT2](benchmarks/ome-nonsi-r2/README.md). JPEG-compressed quantitative TIFF has [R2-CODEC1 local evidence](benchmarks/tiff-jpeg-r2/README.md); other eligible partial/vendor representations and ICC remain R2/R3 gaps. |
| Whole-slide navigation | qupath-gui-fx | render/gui | Partial | Current tiled viewer, overview, controls, font assets and managed owners are delivered. R3: physical monitor/DPI delivery, dense-scene behavior and uncovered international text/shaping cases; R12: actual frame/GPU/process budgets. Historical placeholder observations do not mean the delivered font pipeline must be rebuilt. |
| Background tile reads and cache | qupath-core, qupath-gui-fx | jobs/io | Partial | Native scheduling, generation rejection and budgets are delivered. R12: representative concurrent/vendor workloads and total process memory beyond cache charges; retain source-close/cancel behavior. |
| Image display and channels | qupath-gui-fx | render/gui | Partial | RGB/gray/LUT/window/gamma and TIFF channel/Z/time views are delivered. R3: advanced fluorescence, explicit ICC/color handling and calibrated display edge cases. Keep display transforms out of quantitative measurements. |
| Rectangle annotation | qupath-core, qupath-gui-fx | geometry/gui | Partial | Existing draw/edit/history and pinned geometry/sampling comparisons are delivered. R4: broader actual edit workflows and only demonstrated missing behavior; reuse current commands and accepted fixture results. |
| Polygon and ellipse tools | qupath-core, qupath-gui-fx | geometry/gui | Partial | Existing holes, multipart/analytic geometry, vertex edits, Boolean operations and history are delivered. R4-MERGE1 true object merge is [locally verified](benchmarks/annotation-merge-r4/README.md), including input replacement, new identity, class inference and atomic history. Broader geometry/editor comparisons remain R4 work. Preserve explicit quantization and boundary policies rather than changing old recipes. |
| Object hierarchy and classes | qupath-core | core | Partial | Existing containment, class edits, counts, selection and cell/nucleus history are delivered. R4-CLIP1 [locally verifies annotation copy/paste](benchmarks/object-clipboard-r4/README.md) with explicit selection/descendants, retained supported state, fresh IDs/parents, compound history and saved reopen through native/Python/ImGui. R4/R5-CLASS1 [locally verifies explicit class ancestry/raw ARGB and ordered project defaults](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05) through native/Python/ImGui, history, imports and save/reopen. R4-CLASSHISTORY1 [locally verifies sparse class-only subtree rename/history admission](benchmarks/class-history-r4/README.md), preserving probability, raw/parent state, geometry/index and snapshot/undo semantics. R4: broader upstream hierarchy/edit flows and applicable invalid/large-tree cases; reuse the native hierarchy model. |
| Spatial measurements | qupath-core-processing | geometry/analysis | Partial | Nearest distances, [candidate3 neighborhoods](benchmarks/neighborhood-workflow/README.md), [candidate4 tissue density](benchmarks/density-workflow/README.md) and [signed boundary distances](signed-distances.md) have scoped native/Python/CLI/desktop/retained-table delivery. The [project-density runner](project-density.md) adds multi-image orchestration and resume over candidate4. Tiled aggregation remains optional; no full parity or speed superiority is claimed. |
| Undo and redo | qupath-gui-fx | core/gui | Partial | Existing compound ROI/class/hierarchy/calibration, selection and vertex operations are delivered. CLASSHISTORY1 [locally verifies field-only class subtree rename](benchmarks/class-history-r4/README.md), including small-budget admission, independent history copies, probability preservation and atomic refusal. R4/R5: close remaining upstream edit/save workflows and add regressions only for uncovered mutations. |
| Projects and relinking | qupath-core, qupath-gui-fx | project | Partial | Existing transactional snapshots, recovery, relink, migration, autosave, retention and async ownership are delivered. R5-QPPROJ1 has [local reviewed manifest/assisted-data import evidence](benchmarks/qupath-project-r5/README.md). R5-QPDATA1 adds [local native version3 saved-data decoding](benchmarks/qupath-saved-data-r5/README.md). R5-RELINK1 has [local verified-reader handoff evidence](benchmarks/project-relink-r5/README.md). R5-SOURCE1 adds [locally verified persisted source review](benchmarks/source-verification-r5/README.md), including explicit replacement and primary-only compound coverage. R5: recovery reconciliation confirms existing killed-writer/marked-cleanup coverage; R5-PROP1 [locally verifies metadata/measurement retention](benchmarks/object-properties-r5/README.md); R5-EDITSTATE1 adds [retained editing state](benchmarks/object-edit-state-r5/README.md); R5-PLANE1 [locally verifies native object planes](benchmarks/object-planes-r5/README.md); R5-SOURCEAXES1 [locally verifies retained-plane compatibility at source adoption](benchmarks/source-plane-replacement-r5/README.md). R4/R5-CLASS1 [locally verifies project class registries/defaults](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05), including repeats, zero images, async/recovery and authoring. Close only demonstrated remaining failure boundaries; R11: abandoned batch work. |
| Annotation interchange | qupath-core, qupath-gui-fx | project | Partial | Native/Python/file/ImGui GeoJSON and captured nested/flat round trips are implemented. CLASS1 [locally verifies class paths/raw colors and native/assisted palette precedence](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05), including v1/v2 and owning SDK/GUI paths. R5-QPPROJ1 manifest preview/import with explicit source/builder/annotation losses and hash-bound assisted exchange is [locally verified](benchmarks/qupath-project-r5/README.md). Pinned native .qpdata version3 decoding is [locally verified](benchmarks/qupath-saved-data-r5/README.md), including native ROI kinds and nuclei. R5-PROP1 [locally verifies supported metadata/measurement retention](benchmarks/object-properties-r5/README.md); R5-EDITSTATE1 [locally verifies locks/color/reported probability](benchmarks/object-edit-state-r5/README.md), including interactive unlock and exact history. R5-PLANE1 [locally verifies declared plane import/view/selection/analysis/save/export](benchmarks/object-planes-r5/README.md). R5-SOURCEAXES1 closes the declared retained-plane source-adoption boundary. R4-CLIP1 adds [locally verified clipboard transport](benchmarks/object-clipboard-r4/README.md) through the same loss-aware GeoJSON authority. R5-ROOT1 [locally verifies historical image facts](benchmarks/root-facts-r5/README.md), including native root import, Python/ImGui access, schema19 save/reopen and shared fact ownership/BLOB lifetime repairs. R5-SETTINGS1 [locally verifies saved type/stain defaults](benchmarks/image-settings-r5/README.md), including explicit full-basis adoption and schema20 snapshots/history/reopen. R5-DATAPROP1 [locally verifies portable typed ImageData properties](benchmarks/image-data-properties-r5/README.md), with explicit whole-property loss review and schema21 snapshots/history/inspection/reopen. R5-TMASAVED1 [locally verifies recognized native saved TMA grids](benchmarks/saved-tma-r5/README.md), preserving slots/IDs/children and existing TMA state/history/store/use. R5-ASSISTSTATE1 [locally verifies explicit assisted native/GeoJSON state routes](benchmarks/assisted-state-r5/README.md), reusing complete native facts/defaults/properties/TMA with source binding and no silent downgrade. R5-CALIB1 [locally verifies saved physical Z/time calibration](benchmarks/axis-calibration-r5/README.md), including explicit unknown/prefix times, history/source-domain compatibility, schema22 and actual Python/desktop use. R5-ENTRYMETA1 [locally verifies owning per-entry string maps](benchmarks/entry-metadata-r5/README.md), with manifest/assisted geometry routes, snapshot/schema23 and Python/desktop inspection/save. R5-ENTRYDETAILS1 [locally verifies nullable description and ordered tags](benchmarks/entry-details-r5/README.md), with atomic native/Python pair updates, source-bound geometry import, snapshot/schema24 and combined inspector/save. R5-ENTRYEDIT1 [locally verifies GUI entry authoring](benchmarks/entry-editor-r5/README.md), with bounded drafts, stale/refusal guards, actual text/Tab input and installed mouse/Apply/save/reopen through the settled APIs. R5-PROJECTMETA1 [locally verifies project-wide string metadata](benchmarks/project-metadata-r5/README.md), with schema25/snapshots/recovery, empty-project import, independent entry namespace and shared GUI authoring/save/reopen. Class ancestry/project defaults, other calibration/project properties, versions and records remain open. Dense/multiline-list, scale/display/host behavior stays under R3. Do not repeat already delivered nested export or recovery repairs. |
| Region measurements | qupath-core, qupath-core-processing | analysis | Partial | Existing RGB/OD/stain/shape and typed raw records, digests, replay and tables are delivered. R7-REGION1 [locally verifies natural retained RGB/OD/stain sum/extrema/sample SD](benchmarks/region-statistics-r7/README.md#n3-installed-workflow-closeout--2026-11-05), immutable owning values, schema27/replay/ROI CSV2, legacy quotas and installed SDK/managed/ImGui/batch/CLI with singleton absence. R7: broader raw/grid feature/plugin definitions and independent feature-domain tolerances; R12/R13: applicable resources/runtime/release scope. |
| Stain separation | qupath-core, qupath-core-processing | analysis | Partial | Supplied-vector kernels, bounded estimation, model identity/reload and GUI application are implemented. MODEL1 [locally verifies native file/JSON model reuse](benchmarks/stain-model-r7/README.md#m3-installed-file-reuse-closeout--2026-11-05), old estimate import, exact full-basis/white/hash, SDK/async GUI/CLI batch and saved-recheck adoption, with active-source/project protection and stale load rejection. R7: additional stain/estimation comparisons and registry workflows, with explicit white/vector normalization and specimen limits. |
| Tissue detection | qupath-core-processing, qupath-extension-processing | analysis | Partial | Threshold/morphology/components, retained masks and contour publication are implemented. R7-AREA1 [locally verifies calibrated component filters](benchmarks/tissue-area-r7/README.md), shared cell/tissue conversion, stale publication, frozen count-recipe replay and SDK/managed viewer/ImGui/batch/CLI. R7-FILTER1 [locally verifies pre-threshold masked Gaussian smoothing](benchmarks/tissue-smoothing-r7/README.md), QTM3/CSV2, owning SDK/managed/ImGui/batch/CLI and unchanged old QTM1/2 replay/quotas. R7-MEDIAN1 [locally verifies binary median cleanup](benchmarks/tissue-median-r7/README.md) with the pinned rectangular-mask primitive, native eligibility, QTM4/CSV3 and older metadata/replay limits. R7-GRID1 [locally verifies bounded area-RGB tissue grids](benchmarks/tissue-grid-r7/README.md#g4-installed-workflow-closeout--2026-11-05), exact/ROI context contracts, QTM5/CSV4 and installed SDK/managed/ImGui/batch/CLI with old quotas preserved. R7: other sampling/context contracts and remaining feature/stain/specimen comparisons; R12: large-slide seam/resource behavior. |
| Cell detection and features | qupath-core-processing, qupath-extension-processing | analysis | Partial | V2 original-source sampling and all current consumers are engineering accepted at308. The506-cell crop and13 held-out/nine prior workflows have exact scoped masks. [CONTEXT1](benchmarks/cell-context-r6/README.md) locally verifies exact-phase signed nominal support, QCL9/QCA9 identities, legacy replay and affected consumers at faa71c1f. [SAMPLING1](benchmarks/cell-pyramid-r6/README.md) adds installed lossless RGB TIFF pyramid/dense equivalence and linked SDK/CLI/ImGui replay evidence. [FEATURE1](benchmarks/cell-statistics-r6/README.md) locally verifies retained sampled RGB/transformed compartment statistics, owning SDK, versioned replay/classifier snapshots and opt-in CSV/CLI/ImGui with legacy quota guards. R6: broader context/vendor sampling, remaining feature-domain and representative split/merge/biological tolerances; R12: whole-slide seams/resources. Legacy quantitative xfails remain recipe-specific evidence. |
| Pixel and object classifiers | qupath-core-processing, qupath-gui-fx | analysis/gui | Partial | Native/raw/RGB training, model archives, prediction, stream/contour and consumer paths are delivered. R8: expanded feature schema, unmatched-grid semantics, independent models and applicable streamed/full seams; R12: measured memory/frame behavior. Reuse completed streamed-contour mechanisms. |
| Tissue microarrays | qupath-core, qupath-gui-fx | core/analysis/gui | Partial | QPTMA codec/project/UI, grid edits, proposal/dearraying, case pooling and batches are implemented. Current inherited engineering gates pass308. R5-TMASAVED1 [locally verifies native saved-grid import/use](benchmarks/saved-tma-r5/README.md) for recognized pinned layouts, with explicit inconsistent/unsupported refusals. R10: representative biological/vendor/skew/assignment comparisons and explicit specimen tolerances; R12: real resource behavior. |
| Batch workflows | qupath-core, qupath-gui-fx | jobs | Partial | Existing native/Python/CLI/ImGui captured-model processing, checkpoints and hash-only resume are delivered. R11: remaining partial-failure/source/model replacement, cleanup and long-run behavior; R16: broader/distributed contracts only for a named workload. |
| Typed raw pixel classification | qupath-core-processing | analysis/jobs/bindings | Partial | Typed features, training/prediction, persisted streams/labels, retained publication and consumers are implemented; inherited308 engineering gates pass. R8: feature/missing-data and unmatched-grid scientific completeness; R11/R12: lifecycle, seams and measured resources. Old arm64/observer failures are historical, not current tasks. |
| Python scripting | New API replacing Groovy workflow surface | bindings | Partial | Thin bindings, owning arrays, public stubs, native jobs and managed/document waits are delivered. R11: remaining lifecycle/backpressure/shutdown and versioned multi-stage replay. R13/R14: supported host/package compatibility. Avoid adding endpoints for workflows already exposed. |
| Bio-Formats and multiplex data | qupath-extension-bioformats | io | Partial | Openly licensed TIFF/OME typed channels/axes, explicit multi-file mappings and native/Python/CLI/batch/UI records are implemented. Explicit BinaryOnly XML/TIFF master metadata now has local native/Python/CLI/desktop/source-replay evidence. R2: additional eligible vendor/partial-OME/codec capabilities; JPEG scalar/RGB TIFF now has local R2-CODEC1 evidence and non-SI conversions have local R2-UNIT2 evidence; R3: fluorescence/color; R8/R12: missing features and large-window/process budgets. Proprietary readers remain excluded. |
| External model integrations | Extensions outside the core inventory | bindings/jobs | Deferred | Versioned Cellpose/StarDist results and publication contract |
| Java extensions and Groovy | qupath-extension-script-editor, qupath-gui-fx | compatibility | Deferred | No Java runtime compatibility promised in first release |

## Historical execution observations (source-specific)

At the earlier source `3fc3bcb`, the following
receipt commit changes documentation/evidence only. Windows GUI passes 50 native
suites and 3,256 installed cases; Windows/Linux headless pass 49 suites and
2,831/2,829 installed cases. Both Linux native configurations and both Mac native
architectures pass 49 suites each. All required engine, runtime, hierarchy,
scientific-grid, TMA, reader, archive and portable/fresh-consumer cohorts are
audited at that exact source. See the [qualification receipt](benchmarks/scientific-grid-169-qualification.json).
These results do not close scientific tolerances, hardware or release gates.
All M0-M7 remain Partial; the [active plan](project-plan.md) and
[API delivery paths](api-delivery-plan.md) retain the full remaining scope.

## Earlier evidence (source-specific history)

The observations below describe their named earlier sources. They do not replace
the current execution state or establish acceptance for later changed code.

The input loader rejects an observed oversized measurement file
before data reads and reserves admitted input storage. Its native quota test,
47 native suites and 77 installed TMA cases (29 file/CLI cases) pass. The
[paired pilot](benchmarks/tma-case-input-pilot-windows.json) retains all 32
observations: CSV hashes and independent statistics agree, and the largest
24 MB input's median process peak working set falls from 41.8 MB to 31.1 MB.
Wall-time differences remain unaccepted. The source-bound
[Linux-only audit](benchmarks/tma-case-input-linux-partial.json) accepts 46 native
suites in each Linux configuration and 2,015 installed headless Python cases,
including the exact TMA file-export cohort. The
[full installed Windows validation](benchmarks/tma-case-input-validation-windows.json)
passes 2,419 Python tests, 13 optional skips and three retained strict scientific
xfails, plus the 47 native suites, packaged stubs and typing checks. The saved
ImGui export capture verifies weighted mean 3.5 with zero UI overflow and has
been visually reviewed. [Both Mac native architectures](benchmarks/tma-case-input-macos.json)
now pass all 46 suites and the input-size check. Mac process read-transfer counts
are not measured by that platform test. Complete Windows/Linux and portable
builder/consumer engineering acceptance is now recorded above; wall-time,
hardware, biological and supported-release acceptance remain separate.

The accepted [multi-grid cohort workflow](tma-cohorts.md) implements captured
raw-row pooling, image/core namespaces, canonical source manifests, native/Python
file APIs, explicit project CLI selection and an ImGui project-pooling control.
The [fixed installed Windows build](benchmarks/tma-cohort-validation-windows.json)
passes 47 native suites, all 128 focused TMA cases and 2,470 full Python tests,
with 13 optional skips and the same three strict science xfails. The owner-thread
status defect is fixed; the saved-state two-grid capture is visually reviewed.
[Both Mac native audits](benchmarks/tma-cohort-macos.json) accept all 46 suites.
Complete Windows/Linux and corrected portable engineering acceptance now pass;
biological, hardware and supported-release acceptance remain open.

The accepted TMA case-summary engine adds native/Python/ImGui single-grid replicate
pooling, explicit weighting, missing-core exclusion, unassigned-core isolation,
revision checks and bounded cancellable CSV export. The
[Windows installed validation](benchmarks/tma-case-validation-windows.json)
passes 47 native suites, 48 focused cases and 2,390 full Python tests, with 13
optional skips and three strict legacy science xfails. Persistent input/output
path labels also pass the desktop checks and visual review. Source-bound CI
cohort guards reject missing, duplicate, failed, skipped and stale evidence.

The initial Mac compiler failure at floating `from_chars` is resolved by the
immutable C-locale parser. All 46 native suites pass on both Mac architectures;
this is native-engine acceptance, with Python/GUI/OpenSlide packages outside
that Mac scope. Cross-grid case pooling, biological/QuPath equivalence and
hardware acceptance remain open. The latest navigation qualification met frame
timing but failed the frozen ambient CPU eligibility limit in one trial.

The [real-slide cell comparison](qupath-ndpi-cell-comparisons.md) now retains
an immutable QuPath NDPI capture and exact source pixels. The legacy natural-grid LoG modes
detect299 cells versus506 upstream, with nucleus foreground IoU0.506.
Tile invariance and independent native RGB/count checks pass; real-tissue
segmentation parity remains unresolved. The source2afd Windows/Linux artifact
audit passes all five jobs and the42 body workflows; its portable and Mac
native audits are also complete, as recorded above.

Follow-up [stage controls](qupath-ndpi-cell-comparisons.md#isolated-topology-and-shape-stages)
identify background connectivity and shape splitting as concrete differences.
An internal topology candidate improves the real ROI without changing production
recipes and preserves all18 synthetic output maps. Native46/Python40 focused
checks pass. Source2afd's portable builders, fresh consumers and both Mac native
architectures are now audited. The latest hardware navigation set remains unaccepted
because one trial exceeded the fixed ambient CPU-load limit.

The independent [QuPath cell-plugin corpus](qupath-cell-comparisons.md) now
characterizes nine synthetic workflows, including IoU/counts, split/merge
relations and feature differences. Tile invariance and 36 shared-ROI compartment
checks pass on Windows/Linux. Isolated-cell acceptance and six resampled
boundary-membership comparisons retain explicit failures; the cell workflow
remains Partial and full M5 acceptance is open.

The image-server contract is recorded in [image-server-contract.md](image-server-contract.md).
Reader fixture identities and retrieval are committed; large binary fixtures
and SDK downloads stay under the ignored build directory.

M0 remains open: geometry and SQLite probes are recorded in geometry-spatial.md and project-storage.md;
choose the project license, obtain broader slide/damage fixtures, and freeze
benchmark hardware plus workflow tolerances. A bounded native read scheduler,
cancellation and decoded caching are implemented; see the
[scheduler contract](tile-scheduler-contract.md). A [complete bounded thumbnail
policy](thumbnails.md) provides native/Python/CLI access and an asynchronous
overview with explicit source budgets and desktop increase/decrease controls.
M1 remains open for broader reader validation. Tiled rendering and viewport integration
are implemented, including overview navigation and RGB display controls.
The [docked workspace](docked-workspace.md) adds separate layout preferences;
M2 remains open for fluorescence/ICC, OS/DPI interactions and
vendor stress benchmarks. Keep reader, scheduler, planner and OpenGL checks passing.

The [pinned runtime comparison corpus](qupath-comparisons.md) includes independent
QuPath-generated data and retained edge discrepancies. Its captured decoder gates now pass (exit 0) using the explicit matching
policy; default raw bounds and conversion notices remain visible. The previous
thin-ellipse expected failure is a passing regression. This evidence does not accept complete QuPath workflow parity.


The provisional [physical nucleus-area candidate](cell-area-limits.md) resolves
inclusive calibrated bounds into frozen sample cutoffs through native/Python,
CLI, project batches and ImGui. Windows/Linux full regressions and desktop
visual inspection pass; hosted acceptance remains pending. This narrows the physical-filter gap without accepting physical
radii, arbitrary resampling or cell segmentation parity.


Subsequent packet-105 native viewer validation exposed a packet-103 calibration
staleness gap: active physical requests could publish after units changed during
execution. Three direct regressions reproduce it, including enabled zero bounds.
The fix captures the calibration revision in active physical requests and rejects
stale publication; ordinary sampled requests and historical replay are preserved.
Rebuilt Windows/Linux validation is pending. Earlier full passes and hosted run
40022136685 predate this fix and cannot accept it. Main promotion remains withheld.


Calibration-stale fix revalidation: corrected Linux passes 42 native suites,
99 focused tests and the full 1,683-pass regression (394 optional skips, four
scientific expected failures). Corrected Windows passes 43 native suites and
120 focused cases plus installed/stub/typing checks; its completed frame is
visually inspected with one exported cell and zero UI overflow. Windows full
regression and hosted run 40024473049 at f8cb02a139d5c4b4651978425cb064472b3f1b81
remain pending. Main promotion and all other unfinished M0–M7 gates remain open.


Corrected packet-103 local validation is complete: Windows full regression passes
2,067 cases (10 optional skips, four scientific expected failures), alongside
the corrected Linux 1,683-pass result. Native, focused, installed runtime,
stubs/typing and completed-frame checks pass. Hosted run 40024473049 and its
artifact audit remain pending; superseded run 40022136685 is cancelled. Main
promotion and all other unfinished M0–M7 requirements remain open.


Packet 103 hosted acceptance is complete at source
f8cb02a139d5c4b4651978425cb064472b3f1b81, run 40024473049. All five jobs pass;
the artifact audit verifies 22 physical-area cases per Python cohort, including
three calibration-stale regressions, plus three desktop cases, native/installed
identities, runtime relocation, standalone payloads, full Aperio checks, stubs and
strict positive/negative typing. Windows desktop passes 2,066 Python/43 native,
Windows headless 1,683/42, Linux headless 1,681/42, and minimal/sanitizer each
pass 42 native suites. The completed desktop overview is visually inspected.
The defined engineering scope is accepted for main promotion; four scientific
expected failures remain per Python cohort. Physical radii, resampling, biology,
hardware, current-candidate portable/Mac qualification and every other open
M0–M7 requirement remain separate gates. Release readiness stays false.


The packet-105 managed-viewer candidate queues calibrated cell-area requests
through copied native data and owner-side resolution/publication. Linux full
regression, native lifecycle/staleness and typed queue checks pass; desktop and
hosted acceptance remain pending. See the [platform receipt](benchmarks/viewer-cell-area-limits-linux.json).


Packet-105 Windows focused validation now passes 43 native suites and 147
Python/desktop cases, matching installed stubs and strict positive/negative
typing. All seven new command cases pass, including producer-thread detection,
export/save/reopen and missing-unit failure without a new record. Both platforms
use source-content fingerprint 9dd1d11b7d6d8b4e51a481e734f52d46b72c9e345707bb3d4fdf33ff8c6d27d1.
The Linux full 1,688-pass result stands; Windows full regression is running.
Hosted run 40025778794 at 201bd8ed266d4803defd7321390b930cafe9dcb7 remains
pending. Main promotion, scientific/hardware and all other open gates remain open.


Packet-105 local validation is complete: Windows full regression passes 2,074
tests (10 optional skips and four scientific expected failures), alongside Linux
1,688 passes (396 optional skips and the same four failures). The Windows native,
installed, 147 focused desktop/Python, stubs and typing checks pass. Hosted
acceptance and its artifact audit remain pending at run 40025778794; main
promotion and all other open M0–M7 requirements remain separate gates.


Packet 105 hosted acceptance is complete at source
201bd8ed266d4803defd7321390b930cafe9dcb7, run 40025778794. All five jobs pass
and the artifact audit verifies seven new command cases on desktop (five headless
passes plus two explicit desktop skips), all 22 physical-area cases, three
physical-area desktop cases, native/installed/standalone identities, runtime
relocation, full reader workflows, stubs and strict positive/negative typing.
Windows desktop passes 2,073 Python/43 native, Windows headless 1,688/42, Linux
headless 1,686/42, and minimal/sanitizer each pass 42 native suites. The completed
desktop frame is visually inspected. The engineering scope is accepted for main
promotion. Four scientific expected failures remain per Python cohort; broader
commands/adapters, biology, hardware, portable/Mac and release gates remain open.


Packet120 adds the [explicit production LoG detector](log-watershed.md), with
context-aware native admission/digests and shared Python/CLI/desktop/viewer,
archive, classifier-source and table surfaces. Its initial Windows wheel passes
45 native suites and 50 focused installed cases. The unchanged isolated-nucleus
count/IoU/centroid gates pass for this mode, while the legacy detector keeps its
strict expected failures. Seven pinned nucleus cases are exact; ROI edges,
resampling and body expansion remain different. The rebuilt Windows wheel passes native, installed, focused, stub and typing
checks. The [repeated scaling profile](benchmarks/log-cell-scaling-windows.json)
retains all 24 observations and invariant labels/RGB features across tile sizes.
The complete Windows regression passes 2,157 cases, 13 optional skips and the
three unchanged legacy scientific xfails; the
[receipt](benchmarks/log-cell-windows-local.json) includes installed identities,
source projection, test evidence and the inspected desktop capture. Hosted
Linux headless passes 1,766 Python/44 native (47 LoG cases plus three desktop
skips), and minimal/sanitizer pass 44. Overall hosted, unified-source platform,
biology and supported release gates remain open. Cell detection stays Partial.


The unified packet121 source combines LoG, decimal parsing, Darwin width types
and the conservative winding cache. Its audited Linux headless result is
1,787 Python/45 native, with 47 LoG passes and three desktop skips; minimal and
sanitizer pass 45 suites. All 12 new OME and nine cache cases pass. The
[Linux-only receipt](benchmarks/unified-engine-linux.json) deliberately does not
accept either Windows or the overall matrix. Native Windows headless also
passes 45 suites separately. Unified hosted Windows, portable consumers, macOS,
research/hardware and release acceptance remain open; workflow states stay Partial.


Packet123 adds a [real nine-level NDPI corpus](ndpi-reader-validation.md) and
fixes omitted-source attachment of native OpenSlide/TIFF documents. Local Windows
headless passes 45 native suites, 134 focused cases and 1,812 full regressions;
all 23 headless NDPI cases pass and one desktop case skips. A separate source8160
renderer frame is inspected. These observations do not qualify the newer fix on
GUI or other platforms. Hosted/macOS, broader vendor and research/hardware/release
gates remain open. Main promotion awaits the required Mac architecture gates.


Packet123 NDPI/native-link engineering acceptance is complete at source1b90075:
Windows GUI2,206 Python/46 native, Windows headless1,814/45, Linux headless1,812/45,
minimal/sanitizer45. All reader/link cases and inherited gates are audited;
its source-matching desktop frame is inspected. Portable125 builders pass45
native/1,812 Python and fresh consumers1,773 Python, with the large-fixture
consumer skips preserved in the [receipt](benchmarks/portable-ndpi-build-consumer.json).
Mac124 and the remaining research/hardware/release gates stay open. One real
NDPI family narrows the reader gap without accepting the whole M1 inventory.


The explicit [watershed-body LoG recipe](cell-expansion.md) adds strict-radius
four-connected expansion with collision lines, preserved through QCL3/QCA3,
Python/CLI/batches/ImGui and managed viewer captures. Local source2afd passes
47 native and92 focused cases plus installed/stub/typing gates. Its independent
production corpus has seven exact nucleus maps and eight exact body maps;
ROI-edge nuclei and unmatched-grid resampling retain measured differences.
The full local GUI regression passes 2,248 cases with 13 optional skips and
the same three legacy scientific xfails. Hosted platform gates remain pending.
Cell detection remains Partial; expansion/kernel comparisons do not establish
broader biology or M5.

Nested annotation export is implemented in native/Python/file/ImGui APIs and
actual QuPath imports preserve the captured parents, nucleus and points.
The Windows recovery path failure is fixed through shared physical access while
logical paths, ownership, rescue and maintenance semantics remain unchanged.
The installed engine passes 50 native suites and 305 installed cases; three
host-capability skips are declared. Source-bound platform qualification remains
open for this integrated source. See the
[current receipt](benchmarks/annotation-migration-recovery-local-windows.json).
