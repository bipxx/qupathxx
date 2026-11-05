# API and workflow delivery paths

Current status is maintained only in the [current task](current-task.md). Follow the [validation policy](api-delivery-plan.md#validation-and-build-policy); historical snapshots are preserved in [history](process-status-history-through308.md).

## Validation and build policy

Select checks from the changed behavior and dependency path. This section is
the single active validation policy and supersedes historical instructions to
run a complete qualification after every workflow or numbered packet.

| Change | Required check | Reuse / stop rule |
| --- | --- | --- |
| Documentation or status | Review changed links, consistency and diff | No native build, wheel, numerical suite, auditor campaign or platform dispatch |
| Native algorithm, reader, persistence or concurrency | Affected native regressions and independent expected results; include data-integrity, seam or cancellation cases where affected | Use existing build caches. Add sanitizers and other hosts when the changed code presents those risks; unchanged unrelated workflows retain their recorded baseline scope |
| Python API or bindings | Affected installed workflow, array ownership where relevant, typing and generated-stub consistency | One combined build/install after the native API settles; generate stubs once. No build for each intermediate substep |
| UI, CLI or batch consumer | Exercise the changed installed user flow, including save/reopen or resume if affected | Reuse the shared native result and installed product. Do not rerun the same numerical comparison separately for each frontend |
| Packaging, stubs or notices | Verify changed package members, RECORD, runtime loading and applicable clean-install smoke test | Repackage retained products when compiler inputs/configuration are unchanged; record the existing native identity. Do not rebuild to fix an install path or documentation |
| Release checkpoint or demonstrated cross-platform risk | One frozen campaign for the claimed platforms and affected dependencies | Full matrix only for an agreed release checkpoint or a concrete risk requiring it. A workflow's local completion alone does not trigger another full campaign |

Before running a check, name the changed input or unresolved risk it covers.
If an existing result covers the same inputs, configuration and assertions,
link it and proceed. Evidence reuse retains its tested source and scope; it
does not declare changed code qualified. Widen checks when dependencies, ABI,
compiler/runtime, fixtures or failures justify it, and record the reason.

For a changed result/snapshot layout that participates in stored quotas, inspect
the actual minimum/size and check a valid saved old boundary before packaging.
Preserve legacy bookkeeping explicitly and fully charge new measured storage.
Do not infer a valid quota case from an invalid tiny snapshot or guess row counts.

Use one concise evidence summary per delivered workflow. Record source,
configuration, commands and results once; link existing logs and receipts.
Avoid another receipt/publication cycle for every numerical substep or status
edit. Keep failures and existing historical records. Do not delete tests or
weaken numerical thresholds to reduce execution cost.

When configuring nanobind-owned list/dict properties, build the Python container
locally and assign it once. A returned STL container may be a copy; appending to
that copy does not change C++ options. Check actual APIs/ownership before
adjusting replay timing or writing a consumer around an assumed method.

A C++ observer edit participates in CMake's recorded configure dependencies and
can refresh provenance.cpp and the core archive. Do not assume an observer-target
build leaves that archive byte-identical. Keep the passing product frozen, compare
runtime inputs/configuration, and record the observer's distinct source/metadata
and tested scope. An observer repair alone does not justify another wheel or stub
generation, and its identity must never be relabeled as the original product.

Keep one short current-task file. Workflow coverage belongs in feature-matrix;
requirements belong in project-plan; this file owns scheduling and validation.
Historical records are references, never instructions to replay old campaigns.

Qualification308 is [engineering accepted](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md).
Its existing artifacts and complete original audits close R0; do not rebuild,
redispatch or repeat those audits without a specific changed input or concern.
Future full campaigns follow the checkpoint/risk rule above. Unsupported hosts,
deferred features and unclaimed speed advantages do not block a narrower declared
release, while the full long-term backlog remains recorded.

## Problems this revision addresses

| Observed inefficiency | Evidence | Replacement |
| --- | --- | --- |
| Historical packet log obscures the next action and contradicts current status | The previous active plan mixed thousands of lines of earlier packets with a strict M0→M7 order; inventory still named old main `82e159f` after public main reached `f2e8c2f` | Keep packet history in a separate file; one active queue and an updated workflow inventory |
| Horizontal implementation produces repeated “candidate complete” reports without finishing a researcher task | Earlier TMA stages progressed through reference, codec and project commands before desktop import/export was delivered | Accept complete vertical workflows; substeps remain visible but do not substitute for UI/Python/CLI/persistence completion |
| Repeated full qualifications and wheel rebuilds consume time before interfaces stabilize | Sources 152–154 used successive Mac candidates plus engine/portable qualifications; stubs required additional packaging passes | Local affected tests first; one installed integration pass when needed; full matrix at a release checkpoint or for a demonstrated platform risk. Reuse completed checks within their original scope |
| Passing suite counts can conceal missing behavior | QPTMA project native test initially followed a disabled compile-flag branch in a project-enabled build | Require runtime capability and semantic markers, exact relevant cases and fault outcomes; counts are supplementary |
| Repeated hand-written artifact checks create inconsistent rules and fragile suite counts | Separate engine/Mac/portable scripts embed suite counts and domain-specific imports; QPTMA/resampling each add source/cohort helpers | Share small evidence primitives incrementally and retain domain-specific contracts; test omission, disabled capability and mixed-source rejection |
| Scientific and release decisions remain open while engineering packets accumulate | M0 tolerances/rights/hardware and project-license choice persist in the plan; legacy scientific xfails remain | Maintain a decision/evidence lane with named prerequisites and unblock the next research workflow early |
| Worktree/build proliferation increases stale-source and wrong-environment errors | Many comparison/qualification worktrees; QPTMA reinstall once used a wrong relative path and tested old stubs | One active integration source, one frozen candidate and explicit optional comparisons; absolute artifact paths, fail-fast commands, compiled-source checks and retirement criteria |
| Local performance pilots can look like overall performance completion | Identity resampling pilot measures only the filter call; an earlier rendered hardware pilot was ineligible | Preserve component results and close separate decode, frame, memory and full-workflow budgets under frozen conditions |

These are process findings, not a claim that earlier evidence was worthless.
Preserve accepted receipts, raw captures, negative controls and failed attempts.

## Reader eligibility

Apply the [open-source reader policy](reader-license-policy.md) to every reader
API and workflow below. Exclude closed SDKs and their dependent adapters from
required parity; retain vendor-format work supported by eligible open-source
implementations. Copyleft readers remain eligible subject to distribution terms.

## Dependency lanes and immediate queue

This is the single remaining-work queue. Root owns implementation, integration
and task closeout. The repository owner decides the supported release matrix,
project license and publication; research decisions need an identified corpus,
metric and tolerance. Root prepares concrete proposals and continues work that
does not depend on an unanswered decision. Do not infer a narrower final scope
from the work already delivered.

R0 is complete. R1 inventory reconciliation is recorded in the workflow inventory;
R2-SI1, R2-META1, R2-UNIT2 and R2-CODEC1 are locally verified. R5 source verification, object properties/editing state and recovery reconciliation are closed within their recorded scopes. R5-PLANE1 is locally verified for its declared workflow. R5-SOURCEAXES1 is locally verified for retained-plane source adoption. R4-CLIP1 is locally verified. R5-ROOT1 is locally verified for historical image facts and shared fact ownership/storage repairs. R5-SETTINGS1 is locally verified for saved image analysis defaults. R5-DATAPROP1 is locally verified for portable typed ImageData properties. R5-TMASAVED1 is locally verified for recognized native grids. R5-ASSISTSTATE1 is locally verified for explicit native/GeoJSON assisted state. R5-CALIB1 is locally verified for saved physical Z/time calibration. R5-ENTRYMETA1 is locally verified for per-entry string maps. R5-ENTRYDETAILS1 is locally verified for tags/description retention. R5-ENTRYEDIT1 is locally verified for GUI authoring through those APIs. R5-PROJECTMETA1 is locally verified for project string maps and empty-project authoring. R4/R5-CLASS1 is locally verified for native/Python/ImGui ancestry/defaults. The one-time R4/R5 reconciliation is complete. R4-CLASSHISTORY1 is locally verified; R6-CONTEXT1 is locally verified for signed nominal support and versioned replay; R6-SAMPLING1 is locally verified for the declared lossless TIFF pyramid workflow; R6-FEATURE1 is locally verified for retained compartment statistics and legacy quota compatibility; R6-SPLIT1 trace reuses existing split/merge and feature-domain evidence; next implement R7-AREA1 while owner decisions remain open. Follow dependencies below;
within a ready task, finish one complete user workflow before the next. Existing
implementations are the starting point: inspect their evidence and implement
only the missing behavior. Use the detailed API routes below for entry points.
All M0-M7 remain Partial until their actual exit criteria are proved.

Current states: R0 is engineering accepted, R1 inventory reconciliation is complete
(with research/release decisions still open), R2-SI1, R2-META1, R2-UNIT2 and R2-CODEC1 are locally verified, R4-MERGE1 and R4-CLIP1 are locally verified; R5-QPPROJ1 is locally verified for manifest/assisted import and R5-QPDATA1 for pinned native data-file version3, R5-RELINK1 for verified-reader handoff, R5-PLANE1 for declared object planes, and R2-R12
contain partially implemented workflows, R13 is pending release closure and
R14-R16 retain later parity work. R1 replaces these broad states with actual
remaining gaps using existing evidence; no row implies starting from scratch.

Each row is a work family. Before execution, select one bounded missing user
outcome under that ID, its actual dependencies and decisive checks (for example,
R6 sampling equivalence). Perform the waste review after that task, including a
failed or stopped attempt, rather than waiting for an entire family to finish.
Keep its next action in the existing row/current card instead of creating a
second backlog or a campaign for each substep.

| ID / milestone | Remaining outcome and easiest implementation path | Dependency | Completion check and reuse rule |
| --- | --- | --- | --- |
| R0 / existing qualification | Completed: [308 closeout](benchmarks/qupath-body-boundary-qualification-308/terminal/README.md) accepts all three original complete audits and11 authenticated archives at frozen ae7a4148 | No remaining R0 dependency | Preserve its declared engineering scope and original source. Reuse this baseline; zero rebuilds/redispatches were needed. Mac Python/GUI/OpenSlide, broader science/resources/speed and release remain open |
| R1 / M0 scope and fixtures | Inventory task complete: all23 Partial rows now separate implemented mechanisms and missing work against the308 baseline. Retain Windows-first engineering direction; exact release support, project license, eligible labeled specimens/tolerances and hardware claims remain owner decisions | Existing corpus/receipts are reused; unresolved decisions block only their affected claims | Pinned CC0/vendor files and synthetic fixtures are available engineering references. They do not supply independent biological ground truth. R2-SI1, R2-META1, R2-UNIT2 and R2-CODEC1 are locally verified; R4-MERGE1 is locally verified; R5 direct-data work can proceed without those release/biology decisions; no extra qualification or repeated corpus capture is needed |
| R2 / M1 readers and quantitative input | Extend the shared ImageServer/OpenSlide/TIFF adapters only for missing eligible vendor, partial-OME, axis, calibration and damage cases; connect dense/typed sampling to the existing scientific grids | R1 identifies intended formats; independent adapter work need not wait for biological labels | Compare actual pixels/metadata, finite/alpha support, level-zero mapping, pending-close and bounded reads for the selected formats. All readers and required codecs must be openly licensed. Reuse existing decoders, fixtures and request validation; no separate reader model per frontend |
| R3 / M2 viewer and display | Finish dense-scene navigation, physical DPI/monitor behavior, international glyph delivery, fluorescence/channel presentation and explicit ICC/color policy using the current tile scheduler, font pipeline and display transforms | R1 defines supported display/host scope; R2 supplies missing inputs | Exercise the changed editor/navigation/channel flows on the relevant display configuration, including overlay alignment, resize/close and resource lifetime. Keep display transforms independent of measurements. Reuse the installed product where native inputs are unchanged; record unsupported shaping/color cases explicitly |
| R4 / M3 annotations and hierarchy | Close remaining QuPath editing comparisons through existing ROI commands, hierarchy, class and undo/redo authorities; add only demonstrated missing edit behavior | R1 geometry fixtures; current commands already provide the implementation base | Complete an overlapping/nested ROI edit, compound undo/redo and class operation with stable IDs and calibrated geometry. Compare the relevant independent/upstream fixtures; avoid rebuilding already accepted tools or creating a second UI object model |
| R5 / M4 projects and interchange | Finish source-fingerprint/replacement races, applicable orphan/obsolete-lock cleanup, recovery faults and broader nested geometry/property exchange using current SQLite snapshots, file guards and loss-aware GeoJSON routes | R1 capability contract; R4 where hierarchy semantics change | Save/restart/relink/missing-source, failed-write preservation and deterministic export retain IDs, calibration and results. Extend only uncovered fault/round-trip cases. Keep direct .qpproj/.qpdata migration as distinct format contracts under the broader parity scope; reuse QuPath-assisted exchange first |
| R6 / M5 cell sampling and scientific parity | Extend the delivered nominal/grid V2 request through missing context/halo, vendor/pyramid, feature-domain, split/merge and representative specimen cases; fix the common sampling/kernel authority when an actual discrepancy appears | R1 declares each comparison; R2 supplies relevant reader behavior | Capture exact inputs and recipe before comparison; compare nuclei/bodies, counts, split/merge and compartment features at predeclared tolerances. Preserve V1 replay, intentional recipe differences and strict unresolved failures. The existing506-cell crop and22 earlier workflows are reusable baselines, not universal biological acceptance |
| R7 / M5 stains, tissue and region measurements | Add missing stain-estimation/model-registry flows, physical tissue filters, smoothing/median or arbitrary-grid semantics and feature definitions through existing analysis requests, jobs, retained records and tables | R1 numerical/stain contract; R2 inputs; R6 only for shared sampling changes | Independent tiny-array/geometry checks plus the applicable real/upstream mask and feature comparison; save/replay/export preserve source, units and recipe identity. Reuse supplied-vector kernels, reservoirs, morphology and table schemas; add parameters/features only for an identified workflow gap |
| R8 / M5 classifiers | Finish quantitative feature schema/versioning, unmatched-grid semantics, streamed/full seam behavior and representative model comparisons through the current native forest, raw/RGB requests, streams and model archives | R1 model/corpus contract; R2 inputs; R7 where features are shared | Train/predict/save/reopen/apply through the shared model path; compare independent expected features/predictions and whole-image versus streamed boundaries. Test changed source/model and atomic cancellation. Preserve existing accepted stream/contour mechanisms; do not redeliver them as new work |
| R9 / M5 spatial workflows | Add neighborhoods, density, signed distances and tiled aggregation beside the delivered centroid-distance/index/table path | R4 geometry authority and R1 metric definitions | Match an independent small geometry oracle, then demonstrate the added workflow with retained table/export and correct physical units. Reuse the spatial index and existing native/Python/CLI/ImGui consumers; dense-scene scaling belongs in R12 |
| R10 / M5 tissue microarrays | Complete representative biological/vendor-slide dearraying, skew, missing-core and case-pooling comparisons through current proposal, QPTMA edit and analysis-batch APIs | R1 labeled/rights-cleared TMA references; R2/R7 only where inputs/detection change | Independent core/row/column/assignment and per-case measurements satisfy the declared comparison, including undo/reopen and absent/missing distinctions. Reuse the delivered codecs, axis edits, detector and all consumer paths; do not start another TMA-only full campaign |
| R11 / M6 scripting and batch completion | Define one versioned multi-stage workflow using current requests/commands; finish applicable source/model replacement, partial-failure, abandoned-work cleanup, viewer backpressure/shutdown and Python API/array-lifetime gaps | R5 persistence plus the analysis APIs used by the chosen workflow | One script and shared headless/UI path run open -> ROI -> analysis -> export -> save/reopen; interruption/resume, per-image errors and interpreter/owner shutdown preserve results. Existing numerical kernels are checked at their authority, not repeated per consumer. Add workflow dependencies/error policy before promising general pipeline replay |
| R12 / M1-M6 whole-slide resources and performance | Exercise representative large slides and dense objects across decode, tiled analysis, cache/GPU residency, long batches and UI latency; optimize only the measured bottleneck | R1 fixes workload/hardware/cache conditions; affected R2-R11 paths must exist | Independent full-versus-tiled seam results, no duplicated/lost detections, bounded process/GPU peaks, cancellation/stale publication and stable long-run memory. Report cold/warm/end-to-end timings separately. Require matched QuPath accuracy/settings before any speed claim; extend existing harnesses rather than starting microbenchmarks for every task |
| R13 / M7 supported release | Finish dependency/license/notices inventory, support matrix, sample workflows, known limits, installers/archive/uninstall and standalone/Python clean-machine walkthroughs | R1 owner decisions and applicable M0-M6 exits; inventory/preparation can proceed earlier | Every claimed host/package cell can install, open, annotate, analyze, save/reopen and export. One frozen release checkpoint runs the applicable platform campaign and final requirement-to-evidence audit. Mac-native alone cannot prove Mac Python/GUI/OpenSlide. Signing, main promotion or public release still needs the owner's existing or explicit authorization |
| R14 / later parity: additional supported hosts | Complete Linux/macOS GUI/Python/reader packaging and host-specific behavior for any support cells deferred from the first defined release | R13 support decision; stable portable engine and packaging | The same published user workflow passes on each newly claimed cell, including actual runtime/notices and clean-install evidence. Reuse qualified native components within their source/configuration scope; validate the new host surface instead of rerunning unrelated biology |
| R15 / later parity: integrations and extension APIs | Add versioned Cellpose/StarDist results, OMERO/ImageJ exchange and C++/Python extension interfaces after the native publication/lifecycle contract is stable | R5/R6/R8/R11 applicable data, mask and lifecycle contracts | A concrete adapter/plugin validates model/input/version, calibration/shape, failure/cancel/resource behavior and atomic publication. Broader QuPath project migration remains tracked by R5. Java binary plugins and Groovy execution remain outside the initial release; do not silently turn them into a new prerequisite |
| R16 / later parity: broader automation and workers | Extend the proven R11 workflow grammar to additional pipelines and, if included in the chosen parity/support scope, portable/distributed workers | R11 local replay/error contract and R12 resource behavior | Reproducible data references, checkpoint ownership, retry/idempotence and partial-failure rules are demonstrated by an actual multi-stage workload. Reuse existing batch/job authority; introduce distribution only for an identified user workload |

R1-R12 may expose a missing implementation or decision; update that task's
specific next action rather than adding an overlapping queue. Root records
scientific/release decisions with their owner and continues independent work.
The detailed routes and historical packets below supply implementation context,
not another task order. Deferred parity rows remain part of the long-term target;
they are not silently removed when the first defined release ships.

Reuse the completed workflows below; their detailed counts/source identities
belong in the linked evidence and feature-matrix, not another implementation task.
Each result retains its tested source/configuration; none transfers308 acceptance
to later code. Only the latest closeout stays in current-task.md.

| Existing authority | Reusable delivered workflow / evidence | Boundary to preserve |
| --- | --- | --- |
| R6 measurements | [Retained compartment statistics](benchmarks/cell-statistics-r6/README.md) | Counts/means stay intact; sampled RGB/transformed sum/extrema/sample SD, old/new replay and quota boundaries, selected classifier snapshots, owning SDK and opt-in CSV/CLI/ImGui; no new model preset or biology/speed claim |
| R6 lossless reader | [TIFF pyramid nominal integration](benchmarks/cell-pyramid-r6/README.md) | Dense/TIFF exact pixels, masks, retained features, calibrated area, native linked replay, CLI checkpoint/resume and selected ImGui; uint8 RGB/Deflate SubIFD scope only |
| R6 sampling | [Signed nominal support and replay](benchmarks/cell-context-r6/README.md) | New exact-phase support, old QCL6 replay, mixed classifier origins, installed SDK/CLI/batch and selected ImGui flows; broader R6/resource/biology remain open |
| R2 reader | [SI units](benchmarks/ome-calibration-units-r2/README.md), [BinaryOnly metadata](benchmarks/binaryonly-ome-r2/README.md), [non-SI units](benchmarks/ome-nonsi-r2/README.md), [JPEG TIFF](benchmarks/tiff-jpeg-r2/README.md) | Selected reader/metadata/codecs, not every vendor or whole-slide budget; all readers/codecs must remain openly licensed |
| R4 commands | [True object merge](benchmarks/annotation-merge-r4/README.md), [annotation clipboard](benchmarks/object-clipboard-r4/README.md) | Stable IDs/history, declared loss/refusal and shared native/Python/ImGui copy/paste; other demonstrated editor gaps remain open |
| R5 migration | [Manifest/assisted import](benchmarks/qupath-project-r5/README.md), [pinned native saved data](benchmarks/qupath-saved-data-r5/README.md), [verified-reader handoff](benchmarks/project-relink-r5/README.md) | Pinned format versions and explicit losses; do not rebuild the imports or reopen the same reader twice |
| R5 source identity | [Persisted reviewed verification](benchmarks/source-verification-r5/README.md) | Primary hash/companion metadata only; historical association is not past-decoder proof |
| R5 object facts/state | [Metadata/measurements](benchmarks/object-properties-r5/README.md), [locks/color/reported probability](benchmarks/object-edit-state-r5/README.md) | Historical values, interactive lock hints and foreign-extension limits; sparse history preserves the existing compact budget |
| R5 image facts | [Root metadata/measurements](benchmarks/root-facts-r5/README.md) | Native import/Python/ImGui/history/schema19 snapshot/reopen; common native shared ownership and borrowed BLOB lifetime fixes. ImageData defaults/properties remain separate |
| R5 saved defaults | [Image type/stain defaults](benchmarks/image-settings-r5/README.md) | Recognized pinned types/profiles, native values/history/schema20 snapshots/reopen and explicit Python/ImGui use; RGB8 application limits are explicit. Other ImageData properties remain separate |
| R5 ImageData properties | [Portable typed values](benchmarks/image-data-properties-r5/README.md) | Null/bool/exact signed64/binary64/text/list/map values, whole-property loss review, immutable ownership/history/schema21 snapshots/reopen and Python/ImGui inspection; unrecognized Java types remain explicit |
| R5 native saved TMA | [Recognized binary grids](benchmarks/saved-tma-r5/README.md) | UUID/slot/child retention, missing/case/legacy state and existing TMA history/snapshots/store/GUI; incompatible grids refuse. QPTMA text and assisted state are separate contracts |
| R5 assisted state | [Explicit native/GeoJSON state routes](benchmarks/assisted-state-r5/README.md) | Exchange1 compatibility, exchange2 native facts/defaults/properties/TMA and mixed routes, source guards and actual Python/desktop import/save; broader calibration/metadata/records remain open |
| R5 saved physical axes | [Z/time calibration](benchmarks/axis-calibration-r5/README.md) | Recognized native/direct-v2 Z spacing and explicit timestamp units/offsets/order/prefixes, owning history/snapshots/schema22, source compatibility and actual Python/desktop; acquisition metadata/historical recipes remain unchanged |
| R5 project-entry maps | [Owning metadata strings](benchmarks/entry-metadata-r5/README.md) | ProjectImage/Project replacement/getters, exact UTF8/bounds, source binding, actual saved-geometry routes, snapshot/schema23/SDK/inspector; tags/description/global metadata remain separate |
| R5 entry tags/description | [Nullable descriptions/ordered tags](benchmarks/entry-details-r5/README.md) | Atomic native/Python pair editing, exact null/empty/insertion order, snapshots/schema24, actual import geometry and read-only inspector; authoring and global metadata are separately covered below |
| R5 entry authoring | [Draft-based ImGui editing](benchmarks/entry-editor-r5/README.md) | Existing native setters, bounded drafts, stale/refusal guards, actual ImGui text/Tab and installed mouse/Apply/save/reopen; broader dense/display/host behavior stays R3 |
| R5 global metadata | [Project-wide string maps](benchmarks/project-metadata-r5/README.md) | Owning/atomic native-Python state, source-bound manifest/actual v1/v2 routes, schema25/snapshots/recovery, shared editor and zero-image workflow; class defaults remain separate |
| R5 planes | [Native plane workflow](benchmarks/object-planes-r5/README.md) | Selected single-series plane import/view/analysis/publication/storage/export; explicit channel/2D/QPTMA limits; source-axis adoption is covered separately below |
| R5 source-axis adoption | [Retained-plane compatibility](benchmarks/source-plane-replacement-r5/README.md) | Current plane/undo edits, atomic refusal, compatible reduction and no-op/move semantics; no new review/store or reader reopen |
| R5 recovery | [Covered fault assertions/results](project-storage.md#recovery-gap-reconciliation-2026-11-04) and [cross-session rescue](recovery-work-cleanup.md#evidence-and-remaining-scope) | Killed-writer/marked-debris scope is delivered; no new generic crash campaign. Power loss, network storage and wider pressure bounds are distinct claims |

Active bounded task: **R7-AREA1 — calibrated tissue component area limits** (ready for contract and implementation). R6-SPLIT1 trace found reusable engineering coverage; broader R6 specimen/vendor/context acceptance remains open.
[CLASS1](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05)
and [CLASSHISTORY1](benchmarks/class-history-r4/README.md) are locally verified.
Reuse their native/SDK/editor authorities. Continue the first ready analysis
slice; do not restart R4/R5 reconciliation or import/resource campaigns.

## Execution order for finishing the remaining work

The R0-R16 table above remains the only backlog. The slices below refine those
same rows; they are not additional milestones. Execute left to right within a
row, selecting one missing outcome at a time. Before coding, inspect the named
implementation and existing evidence: if a slice is already satisfied, link that
evidence and advance without reimplementing it. A newly discovered gap returns
to its existing R row, with its dependency and next action.

CLASS1, CLASSHISTORY1 and R6-CONTEXT1 are locally verified. R6-SPLIT1 trace
reuses existing engineering cases without a new campaign. Start R7-AREA1, then
remaining ready R6/R7, R8-R10, R11, R3 completion, R12 and R13. Pull an uncovered eligible R2
reader/axis/damage slice forward when its consumer needs it; reconcile remaining
R2 inventory before closing M1. Prepare R1 decisions and release
notices alongside the task that needs them. Move R3 earlier for an actual viewer
blocker, or R12 earlier for a measured resource failure. This order is scheduling,
not an instruction to finish every possible reader before doing analysis.
R14-R16 follow the applicable stable contracts and remain part of the full target.

| Existing row | Ordered remaining slices and easiest route | Stop condition / dependency handling |
| --- | --- | --- |
| R1 | For the next slice, identify its existing fixture, expected result and tolerance; propose only missing specimen labels, hardware or release decisions to the owner | Record the decision owner and affected claim. Continue independent engineering; synthetic/reference agreement cannot close biological accuracy |
| R2 | UNIT2 and CODEC1 are locally verified. Remaining work: compare the pinned eligible-format inventory with actual adapter capabilities and select one missing vendor/partial-OME representation; follow with its damaged-input and axis/mapping behavior | Finish each selected format through the existing reader and affected consumers. Record unsupported formats explicitly; do not add a decoder without confirming the reader and required codec licenses |
| R4 | MERGE1 and CLIP1 are locally verified. CLASSHISTORY1 locally verifies class-only rename field history/admission. Remaining overlapping/nested edit and hierarchy/class work needs a demonstrated missing behavior | Reuse the delivered clipboard contract, fresh-ID/hierarchy/state handling, native history and actual desktop evidence. Do not repeat clipboard implementation or checks without a changed input or concrete concern |
| R5 | ROOT1, SETTINGS1, DATAPROP1 and TMASAVED1 are locally verified for direct native import. ASSISTSTATE1 is locally verified for compatible assisted state exchange. CALIB1 is locally verified for saved Z/time state. ENTRYMETA1 is locally verified for short string maps. ENTRYDETAILS1 is locally verified for nullable description/ordered tags. ENTRYEDIT1 is locally verified for editable sidebar flows through existing atomic setters. PROJECTMETA1 is locally verified for global maps/schema25/zero-image import and GUI editing. CLASS1 is locally verified for shared ancestry and project-default retention/authoring. Remaining calibration/project properties, versions/builders/records, source races and geometry losses need named fixtures/fault contracts | Reuse stain transforms/model identity, requests, import review and transactional storage. Keep defaults distinct from historical root/ROI facts and explicit analysis recipes. Reuse delivered facts, snapshots and ownership/lifetime fixes; assess remaining formats from pinned fixtures |
| R6 | CONTEXT1 is locally verified: exact RGB-phase support, QCL9/QCA9 identities and retained legacy replay. SAMPLING1 locally verifies the declared lossless TIFF SubIFD workflow. FEATURE1 locally verifies [retained compartment statistics](benchmarks/cell-statistics-r6/README.md), owning SDK, QCL/QCA, CSV/CLI/ImGui and legacy quota compatibility. SPLIT1 trace reuses touching/clipped-domain, policy-edge and body-boundary cases. Remaining work is a named uncovered context/vendor/domain case or representative specimen acceptance; take R7-AREA1 while no such engineering case is ready | Reuse [CONTEXT1](benchmarks/cell-context-r6/README.md), [lossless pyramid integration](benchmarks/cell-pyramid-r6/README.md) and ordinary context/halo evidence. Inspect existing definitions and pinned reference before adding fields. Extend the shared authority; labels block only biological acceptance |
| R7 | First AREA1: optional inclusive tissue component area bounds in square micrometers through TissueRequest, nanobind, managed viewer, ImGui and batch/CLI. Resolve bounds against captured calibration and selected level into existing pixel-count filters. Then smoothing/median/grid behavior, missing measurement definitions, stain estimation/model registry and specimen comparisons | Reuse the existing labeling/morphology, jobs, records and tables; follow the cell physical-area factory pattern. Prove anisotropic/level conversion, inclusive/empty bounds, unknown calibration, stale publication and saved count-recipe replay. Do not add a second segmenter or persistence format unless the declared contract requires it |
| R8 | Feature schema/version and missing-data policy; unmatched-grid behavior; independent model comparisons and streamed/full seams | Train, archive, reopen and apply one model through existing native/Python/CLI/UI consumers. Reuse accepted stream publication; shared halo/resource work belongs to R6/R12 |
| R9 | Neighborhoods and density; signed distances; tiled aggregation | Extend the existing spatial index and measurement tables one metric at a time, with physical units and independent geometry expectations. Reuse the consumer routes; scaling belongs to R12 |
| R10 | Vendor-slide/skew/missing-core dearraying; assignment and biological comparisons; per-case pooling on the accepted core set | Reuse QPTMA proposals, edits and batches. Labels block the associated accuracy claim, not codec/editor engineering. No separate TMA qualification campaign |
| R11 | One versioned open/ROI/analysis/export/save workflow; source/model replacement and partial failures; interrupted/resumed and abandoned work; Python ownership, viewer backpressure and shutdown | Define stage inputs, dependencies and error policy before adding grammar. Keep native jobs/commands as authority. Verify orchestration failures here and numerical results at their existing kernel tests |
| R3 | Explicit ICC/fluorescence policy and channel behavior; monitor/DPI/resize behavior; uncovered international text and dense navigation | Reuse tile/display/font infrastructure and keep raw measurements unchanged. Test actual affected displays/hosts; route frame, GPU and process budgets to R12 |
| R12 | First full-versus-tiled correctness/seams; then long-batch process/GPU and whole-export bounds (the4096-property-column admission check is locally verified); then cold/warm decode and end-to-end latency; optimize only the measured bottleneck | Freeze workload, hardware, accuracy/settings and metric before timing. Reuse one representative workload across checks. A faster component alone cannot establish overall superiority |
| R13 | Prepare license/notices and package inventory; settle support cells; document the accepted sample workflow/limits; package/install/uninstall and clean-machine walkthrough; one frozen release checkpoint | Close applicable M0-M6 exits for each claimed workflow and host. Review the requirement-to-evidence map once. Release/promotion and project-license decisions remain with the owner |
| R14 | Add one declared Linux/macOS package/GUI/Python/reader support cell at a time through the existing portable targets | Validate the new host surface and affected native risk. Mac native acceptance does not substitute for Python/GUI/OpenSlide runtime evidence |
| R15 | First versioned external mask/model adapter; then required OMERO/ImageJ exchange and C++/Python extension contracts | Reuse stable R5/R6/R8/R11 lifecycle, shape, calibration and atomic publication rules. Implement a named adapter outcome before generalizing a plugin framework |
| R16 | Extend the proven local workflow to the next named pipeline; add portable/distributed execution only for an included workload | Keep checkpoint ownership, retries, idempotence and partial failures explicit. Reuse R11 jobs and R12 limits before introducing worker coordination |

For each selected slice, the current-task card records the user outcome, existing
entry point, prerequisite, decisive checks and stop condition before work starts.
Include affected API/consumer paths, the tested source or reusable product, and
what remains after that stop condition. Root owns implementation and closeout;
the repository owner owns the explicit research, license and release choices.
If a dependency blocks a slice, record it and take the first ready slice in this
order. Do not create speculative infrastructure to bypass a decision.
On completion, mark only the state supported by evidence: engineering verification,
scientific acceptance and release support remain separate. Close an R family only
when its remaining inventory entries are accepted or explicitly deferred with a
scope decision. Close M0-M7 against project-plan criteria, not task counts.

### Dispatch and finish the remaining queue

Use the ordered rows above until the full inventory is resolved. For each next
slice, write a concrete action and expected result in current-task.md; a family
name such as "classification" is not an executable task. Start with its existing
native request/command, then identify only the missing nanobind, ImGui, CLI or
batch plumbing needed for that user outcome. All affected consumers must use the
same native behavior. An API already exposed correctly needs no wrapper rewrite.

For R2/R4/R5, reopen only a named unsupported representation, edit or fault,
with its retained fixture and expected behavior. For R6-R10, establish sampling,
units, feature/model identity and a small independent expectation before broad
specimen comparison. For R11, retain one open-to-save pipeline and extend its
failure/resume behavior rather than adding a second job system. For R3, test the
specific display or input interaction; for R12, freeze the complete workload
before measuring and optimize the measured bottleneck. R13 uses the resulting
requirement-to-evidence map; R14-R16 reuse those stable contracts for each added
host, adapter or execution route. The detailed routes below supply existing API
entry points; historical packet checklists are not new execution requirements.

At each handoff, the selected slice must have:

- A user-visible result, prerequisites and explicit remaining scope.
- A native implementation path and affected consumers/persistence versions.
- A decisive correctness check and the compatible evidence it reuses.
- A close condition, expected build/install work and first next action.

No unmeasured calendar estimate or completion percentage substitutes for these
conditions. Close the full project only when every included inventory gap has
accepted evidence or an explicit owner scope decision, M0-M7 exit criteria are
met, and the declared release support is verified. A smaller supported release
does not erase later parity work. Do not claim speed superiority without matched
end-to-end workload, hardware, accuracy/settings and resource measurements.

### Task boundaries and cost controls

Before starting the selected slice, put its included outcome and explicitly
remaining gaps in current-task.md. Choose the smallest complete user workflow,
not an entire R family. New findings join the existing family; expand the active
slice only when they prevent its declared outcome or invalidate its evidence.
This prevents an open-ended migration or validation effort from delaying the
next research workflow.

State the expected expensive operations in that same card: which compiler
inputs change, whether a native build/install/stub generation is needed, and
which retained results can be reused. This is a planning estimate, not a quota
that can justify skipping a necessary check. Before repeating an expensive
operation, identify the changed input or unresolved failure and update the card.
Check a prior live session or retained result before launching a replacement.
Use directory/content searches with explicit globs and bounded output; avoid
Windows wildcard filenames and large unfiltered plan dumps.
Do not add a timer, reporting framework or separate cost ledger for this rule.

The one-time R4/R5 reconciliation is complete and selected project metadata,
then class ancestry/defaults. Continue the demonstrated gaps against existing
implementation and receipts; do not repeat that reconciliation, start another
general import audit or recapture accepted fields. When no independent R4/R5 implementation is ready, advance to
the next ready R6/R7 slice (currently R7-AREA1). Keep any blocked scientific acceptance or owner decision
in its existing row with a concrete re-entry condition.

### Completion gates and task handoff

Use the ordered slices above as the only backlog. Before implementation, the
current-task card names one missing user outcome, existing native authority,
actual prerequisites, affected consumers, minimum checks and stop condition.
Check the reuse index first; a delivered mechanism needs no new implementation.
Settle interfaces, casters, persistence and consumers before one combined cached
build. A card can be ready, in progress, awaiting verification, complete within
its evidence scope, or blocked by a named dependency; do not use task counts as
completion percentages.

Apply these gates to each selected slice, without creating a separate campaign:

1. **Choose the missing behavior.** Name the researcher action and observable
   result, the existing API authority and the gap in current evidence. Inspect
   existing implementation first. If it already satisfies the outcome, reuse
   its evidence and close the inventory gap without writing replacement code.
2. **Settle one contract.** Specify inputs, units/planes, ownership, errors,
   cancellation and retained state where affected. Identify the native command
   or request, thin nanobind exposure and necessary ImGui/CLI consumers. Reuse
   existing models, codecs and job publication; add an API only for a missing
   operation. Resolve interdependent interfaces before building.
3. **Implement and verify the complete route.** Use a small independent expected
   result or pinned upstream fixture at the shared authority. Exercise affected
   consumers and save/reopen when state changes. Select public test nodes and
   confirm the interpreter/product identity before a long run. Combine native
   changes into one cached candidate; subsequent builds need an actual changed
   compiler input or configuration. Preserve failed candidates and their logs.
4. **Close and replan.** Record the scoped outcome and waste decision below.
   Update the existing queue row only if scope, dependency or order changed;
   update coverage only when evidence changed. Set the next ready slice and its
   first concrete action before continuing. A failed attempt returns here before
   retry, with its diagnosis and changed input rather than another identical run.

After **every bounded task, including a failed or stopped attempt**, update the
latest review in current-task.md before the next task or retry. Keep it brief:

| Review field | Required decision |
| --- | --- |
| Outcome and evidence | What passed, failed or remains unknown; link existing source/configuration-specific evidence. A trace can close without new code or a new run. |
| Actual waste | Identify repeated builds/checks, duplicate implementation, wrong assumptions, oversized output, idle attempts or unnecessary artifacts observed in this task. If none was observed, say so; do not manufacture an improvement. |
| Action | Choose reuse, remove, merge, defer or retain with a reason. Preserve failure evidence, independent assertions and required compatibility checks. Retire only verified redundant temporary artifacts. |
| Plan adjustment | Update the affected existing queue row only when scope/order/dependency changes; update coverage only when evidence changes. Otherwise record that the queue is unchanged. |
| Next task or retry | Name its first concrete action, dependency, decisive checks and expected expensive operations. A retry must identify a changed input, corrected setup or diagnostic question; an identical unexplained rerun does not proceed. |

This is part of closeout, not a new audit, report, benchmark or approval round.
Use the existing card and queue; do not accumulate a parallel waste backlog.
Repeated waste must change the next task's procedure, rather than merely being
noted again. A check remains required when it covers a real unresolved risk.

R4/R5 reconciliation (2026-11-05): entry state, nested geometry transport and
covered recovery/source guards already have scoped receipts; do not redeliver
them. Nonempty project-level metadata is a demonstrated missing retained value
(cpp/qupath_project.cpp). Derived class-parent semantics remain a demonstrated
separate gap (classification_hierarchy_flattened in cpp/qupath_saved_data.cpp);
full names alone do not close that requirement. Additional server-builder forms,
file versions, Java-typed records and replacement/network/power-loss guarantees
need a named fixture or fault contract before implementation; existing explicit
refusals do not establish support. Keep these in their existing R4/R5 rows.

PROJECTMETA1 now retains the separate project map; its receipt and existing
entry/snapshot/store/editor evidence are reusable within their tested scopes.
Do not repeat that capture/build or infer class retention from string metadata.

CLASS1 is [locally verified](benchmarks/class-hierarchy-r4/README.md#installed-workflow-closeout-2026-11-05)
for explicit native class ancestry/raw colors, project palettes, native/GeoJSON/
manifest/v1/v2 routes, owning Python and actual ImGui editing, snapshots/recovery
and schema26 save/reopen, including zero images. Reuse its source-scoped results,
resolver, retained builder/ROI and settled APIs; do not repeat capture, foundation
implementation or the package campaign. All M0-M7 and broader R4/R5 stay Partial.

<a id="class1-remaining-execution-steps"></a>
### CLASS1 delivered workflow

These former execution steps are closed within their [recorded scope](benchmarks/class-hierarchy-r4/candidate-integrity.json).
They are evidence to reuse, not another task queue or approval cycle.

| Shared route | Accepted result / scope |
| --- | --- |
| Wire and project context | Existing binary decoder/common GeoJSON parser retain explicit paths, raw ARGB/absence, palette precedence, repeated order and zero-image registries. Native v1 flat RGB compatibility and v2 ancestry metadata remain distinct; incompatible identities refuse |
| SDK and editor | Owning ObjectClass/Document/Project surfaces and shared ImGui controls use native commands. Actual controls cover raw alpha/absence, explicit parent, defaults/order, no-op/stale/refusal, image application and rename/undo |
| Integrity and integration | Seven native roles/all25 older schemas, scoped319 earlier Python passes plus49 diagnostic/class repair passes, eight installed/stub checks, positive/three exact negative typing diagnostics and installed mouse/fresh-process save/reopen are recorded. No general biology/host/speed acceptance is inferred |
| Handoff | CLASS1 closes locally; existing R4/R5 gaps and dense/resource claims remain open. CLASSHISTORY1 now closes that history-admission concern; CONTEXT1 now closes signed-boundary replay; SAMPLING1 closes the declared lossless pyramid workflow; FEATURE1 now closes retained statistics; SPLIT1 reuses existing engineering evidence and AREA1 is next |

[CLASSHISTORY1](benchmarks/class-history-r4/README.md) now retains administrative
subtree rename in the existing field journal: the matched installed probe uses
1419 accounted bytes under16KiB, and the native subtree/property case uses3392.
Geometry remains immutable/shared; no process-memory or general speed result is
inferred. Reuse its source-scoped checks. The next ready task is R6-SPLIT1;
other R4/R5 gaps remain in their existing rows with fixture/fault re-entry rules.

Follow the ordered slices above through R6/R7,
R8-R10, R11, R3, R12/R13 and applicable R14-R16. A dependency blocks only its
affected slice. Keep owner decisions in R1/R13 and continue the first independent
ready outcome; do not silently drop unresolved parity work or claim full
completion from a narrower release.

## Task closeout and waste review

After **every bounded task and every failed or stopped attempt**, update
current-task.md before starting the next task or retry. Root owns this short
self-review; it needs no additional reviewer, build, benchmark or report.

| Record once in the current card | Required content |
| --- | --- |
| Outcome / evidence | What works, what failed or remains blocked, and the existing evidence link. Separate engineering verification, scientific acceptance and release support |
| Waste / decision | Observed duplicate builds, checks, decoding, waits, retries, fixture/report copies, unused output or scope growth. Choose **remove**, **merge**, **reuse**, **defer** or **retain**, with a reason. Use existing timings if available; otherwise cost is unknown. “No material waste; next action unchanged” is valid |
| Plan change / next action | Apply that decision now: name the next missing behavior, prerequisite, reusable source/product scope and only the checks still needed. Put larger necessary fixes in their existing R family once |

Fix a small local waste cause before advancing. A repeated check needs a changed
input or a concrete unresolved risk; reuse does not qualify changed binaries.
On failure, preserve the original evidence, diagnose the cause and record the
changed input before retrying. Inspect or resume a live tool session rather
than launching a replacement. Do not rebuild for a documentation, observer or
install-path repair when compatible native products can be reused.

Make the waste decision operational, not a recurring promise. For every observed
cost, name its cause and the action taken now: remove duplicate work; merge work
at its common authority; reuse evidence within its actual scope; defer work with
a dependency and re-entry trigger; or retain a necessary check with its risk.
Record elapsed time or build/install counts only when already available; do not
add instrumentation or a report merely to complete this review. If the same
waste recurs, change the execution step that allowed it before the next attempt.

Use this short handoff in the current card:

> Outcome/evidence: scoped result, unresolved failures and evidence location.
> Waste/action: observed cost, its cause and the remove/merge/reuse/defer/retain
> decision actually applied. State the remaining risk if a check is retained.
> Next: one ready outcome and first action, dependencies, reused product/source
> scope, minimum checks and stop condition.

The review changes execution before the next action, using these rules:

| Observed waste | Apply now | Re-entry or verification condition |
| --- | --- | --- |
| Changed layout invalidates old stored quotas | Check a valid saved old boundary before the first candidate; preserve old bookkeeping and charge new data explicitly | Use actual minimums/sizes and a pinned old artifact, not a guessed or invalid quota setup |
| Repeated build, install or test | Reuse the retained compatible result; combine pending changes at the shared authority | Repeat only for changed inputs/configuration or a named unresolved risk; record that reason |
| Failed command or uncertain live run | Inspect the existing output/session, diagnose and change the failing input | Retry only after the diagnosis and changed action are recorded; retain the first attempt |
| Duplicate model, decoder or frontend math | Merge into the existing native authority and keep consumers thin | Verify the shared change once plus the affected consumer plumbing |
| Repeated corpus capture or copied reports | Bind retained inputs/outputs and link their original evidence | Recapture only missing or invalidated cases, with the provenance difference stated |
| Blocked research, license or support choice | Defer only the dependent claim and select the first independent ready slice | Name the owner/dependency and the decision or input that resumes it |
| Unused build output or excessive logs | Keep required products, failures and provenance; stop generating unused output | Remove temporary artifacts only after verifying their paths and that the next task does not need them |

Keep this review in the existing card; it creates no separate approval, auditor,
benchmark or recurring review task. If a larger fix is necessary, put it once in
its existing R row with its dependency and trigger; finish the current bounded
outcome or record the block before switching scope.

No material waste is a valid finding. Do not manufacture optimization tasks,
weaken assertions, remove failure evidence or rerun passing checks to demonstrate
that a review occurred. The review is complete when the next action reflects the
decision, not when another document or audit has been produced.

Keep only the latest closeout in current-task.md. Update the affected inventory
row only when its evidence changes, and the same queue row only when its order,
dependency or remaining scope changes. Link durable evidence instead of copying
the review into several documents. Long-lived technical lessons may be appended
to the existing compilexx notes; raw failures stay at their original paths.

A slice closes when its declared user outcome, affected checks and this plan
handoff are recorded. Finish that outcome before taking new scope; additional
gaps return to their existing family. Shared sampling/halo fixes belong to R6,
orchestration to R11, resources to R12 and release support to R13; consumers reuse
the same fix. If blocked, record the dependency and take the first ready slice.
An unanswered decision blocks only work that actually depends on it.

At release closure, reconcile the remaining inventory against M0-M7 once.
R14-R16 stay in the full target unless the owner explicitly defers them. Close
families and milestones against their actual exit criteria; local passes and
faster components do not establish biological acceptance, release readiness or
overall speed. Owner decisions on license, research and release stay explicit.

Completed mechanisms stay out of the active implementation queue. The observations below are historical references, not pending work:

Historical qualification observation: original271 Mac-native passes its complete
native audit. Portable fails the schema13 descriptor benchmark and skips fresh
consumers; Windows/Linux headless show the same assertion. Engine observation
is now terminal with the same GUI/non-GUI descriptor assertion. [Observer272](benchmarks/schema-observer-272/receipt.json) repairs
the strict benchmark/auditor expectation to14, with actual process red/green,
10000-record GUI/headless and1007 audit controls, without rebuilding unchanged
native inputs. Exact installed admission passes307/one GUI skip and260/48
nonrendering skips on that GUI-capable package. [Terminal271](benchmarks/spatial-waits-271/terminal272/receipt.json)
preserves all nine available archives and original failures. Candidate273 now has
[complete engineering qualification](benchmarks/schema-observer-273/closeout275/receipt.json)
at5f1760a. A stale34-case engine observer rejects the correct35-case schema14
cohort; identified successor auditor432c36c corrects it and requires all13 named
upgrades and unique identities. All original full flags pass without another CI
run or native rebuild. Preserve the rejection and source/auditor distinction.
Snapshot274/native048cf25 remains locally verified; [candidate276](benchmarks/snapshot-waits-276/receipt.json) now runs its own-source qualification at9d92322.

| Baseline | Recorded evidence and remaining boundary |
| --- | --- |
| E1 / exact TMA cohort | [Accepted combined receipt](benchmarks/tma-dearray-202-qualification.json), artifact `19371b7` / successor auditor `cc2f524`. Preserve the original rejection and separate artifact/auditor identities |
| E2 / execution boundaries | 438 local auditor controls and 48 Linux stdlib witnesses at `cc2f524`; retain optimized-execution, authenticated-source and inherited-pytest admission protections |
| E3 / observer proof reuse | [Local receipt](benchmarks/source-proof-reuse-local.json), 579 controls and retained Linux successor audit. No persistent PASS cache, binary relabeling or overall CI speedup claim |
| E4 / independent installed diagnostics | [Local receipt](benchmarks/installed-diagnostics-local.json), 609 controls and real Windows/Linux child probes; all 58 command sites preserved. Campaign213 is the first hosted exercise of this later producer; campaign210 cannot prove it |
| MFO1 / full declared multi-file workflow and fault controls | [Campaign213 engineering accepted](benchmarks/multifile-fault-213-qualification.json) at `bfa416e`: all 11 archive sizes/digests/sources and full engine, both Mac-native and portable builder/consumer audits pass. The exact protocol requires 54 installed cases and the native cancellation marker. Earlier [normal campaign210](benchmarks/multifile-ome-210-qualification.json), partial observation and local fault evidence remain immutable. Later raw classifier sources are not covered |

Use MFO1's [A-E checkpoints](multifile-ome-tiff.md#delivery-checkpoints) as
historical implementation detail. Its original campaign is terminal and
accepted; do not redispatch it for later classifier work. B5 consumers are locally
verified in the historical snapshot; B6 is a historical checklist, not pending work. Never edit frozen sources or relabel compiled products. Rebuild only when consumed
compiler inputs/configuration or the required product evidence changes.

Resolve fixture rights/labels, scientific decisions and hardware eligibility
alongside the implementation that consumes them. A blocker affects its actual
consumers; it neither deletes scope nor stalls independent ready work. Preserve
all original milestone requirements except the user's explicit proprietary-reader
exclusion and the already documented release deferrals.

## Definition of done and work limits

Each queue item is a user workflow with a requirement ID and one accountable
implementation owner. Maintain a small row with: current state, next action,
code/fixture/dependency, acceptance artifact and blocker/decision owner. States
are **planned**, **implemented**, **engineering verified**, **scientifically
accepted where applicable**, and **release supported**. Store source/configuration
with the state. Never infer the next state from test volume or a green job.

Limit work in progress to one implementation slice and one frozen candidate;
reference/decision preparation is bounded by the next named workflow. Prefer
finishing a missing frontend/persistence step over adding a new public endpoint.
Fix correctness and source-preservation issues before optimizing the new path.
Stop expanding a slice when its agreed complete user outcome is met, then run
its final gate; additional breadth returns to the original backlog with an owner.
That controls intermediate scope without shrinking the overall M0–M7 objective.

The TMA and explicit-grid RGB-cell workflows now have exact-source hosted
engine/Mac-native/portable engineering acceptance. Reuse their shared native,
Python, CLI, batch and GUI paths for remaining work. The next scientific slice
must retain the recipe-specific strict failures and documented input/grid
differences while extending typed sampling and the relevant mask, feature and
split/merge requirements without weakening acceptance criteria.

## Prepared next workflow: SPAT1 centroid distances

Historical design reference: SPAT1 has been delivered. Do not reimplement it or
launch qualification from this section; use the current task and active queue.

Candidate262 now has complete declared engineering acceptance. [SPAT1](spatial-analysis.md)
has26 independent QuPath cases and17 validator controls. Foundation263 provides
centroids and nearest queries; [capture264](benchmarks/spatial-capture-264/receipt.json)
adds immutable requests/results and owner jobs at2b9c5d7. Four native suites pass
each Windows GEOS-ON/OFF profile. Corrected902 tooling controls pass at7bb8d78;
all395 compiler inputs are unchanged, so native products were reused.
[Persistence265](benchmarks/spatial-records-265/receipt.json) now adds retained
records, QSD1, project schema14 and bounded atomic CSV at1738022. Ten native suites
pass each GEOS-ON/OFF profile, with all13 legacy upgrades, injected rollback,
10000-record quota and history/export fault controls; final902 tooling controls
pass. All400 compiler inputs and503 raw sources authenticate. The15 updated Python
schema expectations await the new runtime.
[Installed Python266](benchmarks/spatial-python-266/receipt.json) now verifies those
bindings at9fa565a with stub observer2a16bd2:53 native suites,404 installed passes/
eight explicit skips across412 identities,902 observer controls and exact typing
diagnostics. The33 package members, loaded GEOS/notices and complete RECORD bind
the final wheel; only generated stub payloads/RECORD change after native compilation.
[CLI267](benchmarks/spatial-cli-267/receipt.json) now delivers native measure-to-new-project, historical CSV and fresh verification at9468ae3. Five suites and30 process cases pass each GEOS-ON/OFF profile; bounded ID files cover large cohorts. The installed266 package is only a fixture creator/reader, not a substitute for the new engine.
[Frontend268](benchmarks/spatial-frontends-268/receipt.json) now verifies shared ImGui/managed controls and the installed Windows workflow at d9bba6b, with stub observer c360ac9:55 native suites and132 installed passes, matching stubs/typing, exact wheel/RECORD/runtime checks and inspected desktop capture. Native stale/cancel/budget/export faults and managed history/save/reopen pass. [Fault/cohort269](benchmarks/spatial-faults-269/receipt.json) adds21 frontend controls,55 native suites and268 unique installed GUI-mode passes, with exact GUI/nonrendering cohort admission, native markers and typing. A separate headless/OFF source passes six affected native suites and30 CLI cases. [Wait270](benchmarks/document-waits-270/receipt.json) now verifies the scoped nine-controller safety audit below. Original271 is terminal/unaccepted. [Candidate273](benchmarks/schema-observer-273/closeout275/receipt.json) completes declared engineering qualification of the combined spatial/Document-wait source with the schema observer repair, using identified successor engine auditor432c36c. [Snapshot wait274](benchmarks/snapshot-waits-274/receipt.json) is locally verified at a later source and needs its own qualification after273 closes.
Use existing native ownership and atomic paths; no frontend CSV or second model.
Do not dispatch qualification until that complete workflow stabilizes. Biological,
representative resource/performance/hardware and full-platform acceptance remain.
The fixed reference tolerances and separate upstream FLOAT/DOUBLE storage cases
remain unchanged.
Reader/interchange/automation, broader spatial operations and every other M0-M7
requirement remain in scope; this preparation does not restart completed contours.

## Smallest delivery sequence for the next API workflow

After candidate262's required qualification, choose one unmet SCI1-B requirement
whose fixtures and acceptance criteria are ready. Reader/interchange/spatial/
automation breadth remains required; do not open all four implementations at once.
Record the exact missing behavior and current header/stub before assigning new
API names. Use the API-family table below to choose its existing native owner.

1. **Contract:** specify inputs, outputs, units/grids, ownership, errors and
   persistence compatibility. For a new reader, first admit the exact reader
   and every required codec license; unverified or closed dependencies block
   that adapter. Fixture rights are a separate prerequisite.
2. **Native workflow:** extend the existing request/kernel/job or codec. Exercise
   a small independent expected result and relevant failure/cancel/stale cases.
   Avoid a second document model, scheduler or frontend-specific algorithm.
3. **Python:** add only the missing nanobind surface; use owned results and the
   existing NumPy/lifetime conventions. Build one installed wheel after the
   native contract stabilizes; regenerate stubs only for public API changes.
4. **CLI, ImGui and managed viewer:** route the same immutable native request
   through existing commands/jobs. Verify the complete user action, cancellation,
   owner-thread publication and save/reopen/export rather than button presence.
5. **Integrated acceptance:** run affected installed cases, exact capability and
   typing checks, then freeze one coherent source for applicable platform gates.
   Use independent scientific references where required; do not substitute
   synthetic engineering evidence for biological or hardware acceptance.

For reader work, the smallest slice is one eligible format/axis behavior through
open, bounded region read and a real analysis workflow. For interchange, it is
one specified format's import/edit/export/reopen round trip through the existing
object model. For spatial work, it is one bounded calibrated query producing an
immutable table through the existing index/export layer. For automation, it is
one versioned multi-stage manifest over existing requests with interruption and
restart. Select one slice and record its decisive oracle before implementation;
these are delivery options, not four newly approved public API contracts.

## Detailed route for each API family

“Shortest path” means reuse the existing authoritative implementation and avoid
parallel models or unnecessary rewrites. It does not waive the required final
behavior or testing. These rows are implementation entry points, not a second queue or fresh gates.
Use the ordered slices and reuse index above for current state: historical
acceptance wording below does not reopen delivered work. No route declares full
parity or release acceptance.

| API/workflow and existing entry points | Shortest path and shared implementation | Remaining work and decisive acceptance |
| --- | --- | --- |
| Image open/metadata: `open_slide`, `open_tiff`, `ImageServer.metadata`, `read_region`, `RegionRequest` | Extend the reader adapter/capability declarations for an identified format; retain RAII, normalized metadata and shared region validation. Reuse pinned OpenSlide/quantitative TIFF paths before introducing a new decoder | Add vendor/axis/damage/calibration cases to the reader corpus; verify actual pixels and independent metadata, pending-close behavior and bounded reads. Broader eligible readers and partial-OME metadata stay tracked; the declared multi-file workflow has its own accepted baseline |
| Resampling/overview: `read_resampled_region`, `resample_area`, `plan_thumbnail`, `ThumbnailJob` | Use the accepted integer area filter and natural-level planner. Choose grids explicitly; do not implement another resize in Python or ImGui | Real-vendor grids and downstream scientific recipe integration; identity/alpha/padding and quota behavior must hold. Device-space filtering does not supply ICC/linear-light semantics |
| Raw quantitative data: `RawRegionRequest`, `RawPixelBuffer`, `TiffView`, `RawAnalysisRequest`, `RawAnalysisJob`, `read_scientific_raw`, `ScientificRawGrid` | Reuse native typed grid sampling, captured analysis/jobs, QRA2 retention/replay/CSV and shared Python/CLI/batch/ImGui/viewer adapters. Interleaved TIFF channels share decoded blocks even without LRU retention; existing count/work options now also drive bounded coarse materialization and hash-only resume. Keep raw data separate from RGBA8 display sampling; use the [current task](current-task.md) for source-specific validation and campaign status | Vendor/large-window/whole-slide and multi-file OME capabilities, raw classifier features, decoder/process/hardware budgets and independent scientific validation remain. Extend existing requests with versioned provenance rather than creating per-layout frontends |
| Scientific RGB/channels: `ScientificGridAxis`, `ScientificGrid`, `ScientificRgbGrid`, `read_scientific_rgb`, `ScientificChannels` | Reuse the exact grid, bounded level-zero float64 RGB sampler and OD/signed-stain transforms. Shared rasters/GEOS contours preserve both axes and one-read input identity. Reuse the [contract](scientific-grid-contract.md); keep typed raw channels separate | Native, installed Python and desktop cell integration pass locally through QCL5/QCA5/CSV, including captured calibration and metadata lifetime. The [integrated platform campaign](current-task.md) is accepted at source `3fc3bcb` within its declared scope. Pyramid/tiled sampling, typed quantitative sources and biological masks/features remain; a bounded source window does not complete large-slide sampling |
| Scheduling/cache: `TileScheduler`, `SchedulerOptions`, job `request/poll/cancel` | Add missing request classes/priority policy to existing scheduler and immutable completion paths. Reuse generation rejection and native handle ownership | Vendor stress, concurrent close/cancel, in-flight/decoder memory accounting, latency and long-session bounds. No new general scheduler unless a measured limitation requires it |
| Viewer/display: `show`, `GuiOptions`, `DisplaySettings`, `Viewport` | Reuse tiled renderer, cached overlays and owner/context-thread rules. Add ICC as a separately specified display stage with a candidate dependency/notice review | Physical monitor/DPI and GPU/OS matrix; advanced fluorescence, color-managed comparisons, dense visible scenes and eligible completed-frame timing. Raw measurements must stay unchanged |
| Objects/geometry/history: `Document`, `Roi`, `ObjectEdit`, `ObjectSelection`, `Document.execute`, `copy_objects`, `paste_objects` | Reuse the [delivered clipboard](annotation-clipboard.md); implement other missing gestures as compound existing commands; extend GEOS adapter and revision caches only where fixtures expose a gap | Hierarchy/interchange edge cases, multipart/holes and calibrated features, selection/edit interaction, rollback and undo/redo. Verify stable IDs and child/nucleus policy |
| Project persistence: `Project.open`, `save`, `save_new`, `ProjectSaveJob`, autosave/recovery | Finish and test new-file publication using the existing SQLite snapshot/transaction path; expose a clear distinction between update and fork/save-as. Reuse `open_without_journal_recovery` for source-preserving commands | Shared physical-path adaptation now fixes long work/snapshot/rescue paths, SQLite reopening, review/deletion and retention while preserving logical paths. Require source-bound platform evidence and retain hot-journal, destination-race, moved-link, missing-image and failure/cancel gates |
| Annotation interchange: `import_geojson`, `export_geojson`, file wrappers and `InterchangeReport` | Use the shared GeoJSON route and additive `nested_hierarchy=True` option for QuPath parent links; native file/Python/ImGui paths already share it. Extend the bounded loss/geometry corpus rather than starting direct Java serialization | The six-object nested corpus, object facts/state/planes and clipboard have scoped evidence in the reuse index. Manifest/assisted `.qpproj` and pinned version3 native `.qpdata` are delivered; root facts have local ROOT1 evidence; remaining image settings/properties, formats/records and demonstrated losses stay in R5. Apply the change-based validation policy rather than reopening delivered imports |
| QPTMA wire: `QptmaData`, `load_qptma`, `parse_qptma`, `qptma_text`, `save_qptma` | Keep the bounded native literal-TSV codec; Python passes owning values. Use actual pinned QuPath captures and native-output imports as the oracle | Finish file/UI lifecycle, byte/slot limits and malformed text cases across supported hosts. Preserve fractional importer behavior and expose upstream exporter quantization; never interpret the source hint as a file to open |
| QPTMA project/UI: `qptma_grid_edits`, `import_qptma`, `qptma_from_grid`, batch `--import-qptma`/`--export-qptma` | Use one atomic edit and new-project publication; CLI requires explicit image. Reuse the existing preview UI around these functions and preserve current project/view/image state | Final wheel + project save/restart, no source overwrite, stale/cancel preview, loss acknowledgement, owner-thread import and GUI replay. Do not create a second UI grid model |
| TMA management/case pooling: `TmaGridOptions`, `Document.create_tma_grid`, `TmaGrid`, `tma_case_summary_csv`, cohort APIs | Reuse the [locally verified row/column commands](tma-axis-editing.md), immutable snapshots and weighted raw-row pooling; keep present/missing/vacant semantics distinct | Axis platform acceptance and biological/upstream coverage remain. Axis cohorts are [registered](benchmarks/tma-axis-auditor-controls.json); source191 is qualified, but the [later completed campaign](benchmarks/tma-axis-196-audit-rejection.json) was rejected by strict engine/portable audits because of a stale QPTMA control manifest. Integration repairs it and independently checks actual pytest collection. Preserve the original failed audit and complete the next integrated workflow instead of dispatching another axis-only packet |
| Automatic TMA: `TmaDearrayOptions`, `detect_tma_grid`, `ViewerSession.detect_tma_grid`, `--detect-tma` | Reuse the [locally verified native/Python/ImGui/managed/CLI path](tma-dearraying.md): one bounded detector returns an immutable proposal, the current controller reviews it, and the same QPTMA edits apply atomically. Keep request capture, revision/cancel guards and source/output protection | Named evidence cohorts and 373 controls are registered; explicit per-axis physical conversion and a calibrated natural-level TIFF integration pass locally. Reuse its existing engineering evidence. Preregister representative real-slide/biological and large-slide fixtures; use a platform campaign only at the applicable release checkpoint or for a demonstrated platform risk. Preserve the fixed synthetic tolerance and upstream skew discrepancy |
| ROI/shape/OD measurements: `AnalysisRequest`, `AnalysisJob`, `analyze`, `RoiMeasurements` | Reuse immutable ROI/calibration capture, native kernels, revision caches and retained analysis records. Add a feature descriptor only for an identified missing quantity | Independent feature tolerances, edge/hole/alpha sampling and calibrated behavior; publish/replay/export must retain exact input/recipe identity. No per-frame recomputation |
| Stains: `StainTransform`, `optical_density`, `StainEstimateRequest`, `StainEstimateJob` | Reuse supplied-vector kernels and bounded estimation reservoir; add registry/persistence UI around existing stable model hashes | Additional stain/estimation reference workflows and accepted biological tolerances; freeze white reference/vector normalization before running comparisons |
| Tissue detection: `TissueRequest`, `TissueParameters`, `TissueJob`, `detect_tissue`, `trace_tissue` | Reuse threshold/morphology/components/contours and record publication. Add physical filters or smoothing only with an explicit sampling definition | Halo/edge connectivity, arbitrary grid recipe, physical filters and independent QuPath/biological masks; identical captured inputs across UI/Python/CLI and cancellation/stale tests |
| Cell detection/compartments: `CellRequest`, `GridCellSampling`, `GridCellParameters`, `CellParameters`, `CellJob`, `detect_cells` | For count-based explicit grids, use `CellRequest.for_grid_roi`; for exact origins/extents, pass `ScientificGrid` to the constructor. Choose typed pixel/micrometre units, preserve the ROI and reuse QCL5 retention/replay. Legacy recipes keep their existing parameter path. See the [workflow guide](grid-cell-workflows.md) | Physical sigma/radii/expansion, per-axis kernels, calibrated features and native/Python/CLI/batch/GUI integration now pass source169 engine/Mac-native/portable engineering qualification. Predeclared biological/QuPath mask, feature and split/merge tolerances remain. Unknown micrometre calibration rejects explicitly |
| Tables/provenance: `MeasurementTableRequest`, `CellTableRequest`, raw tables, export jobs, `build_environment` | Compose existing immutable table requests and incremental export jobs; extend the schema in one native table layer, bind it thinly | Historical/live metadata joins, revision selectors, reproducible schema/units, cancellation and row/byte budgets. Reuse evidence hashes; avoid new ad-hoc CSV format for each UI |
| Object classifiers: `ObjectFeatureRequest`, `extract_object_features`, forest training/classification and archives | Reuse frozen feature schema, training snapshot and portable model archive; expose missing workflow actions through existing jobs | Expanded feature schema/versioning and independent research/model comparison. Verify source links, absent labels, deterministic predictions and applying classes as an undoable command |
| Pixel classifiers/streaming: RGB `PixelTrainingRequest`, `PixelPredictionRequest`, `PixelPredictionStream`; native `RawPixelRecipe`, `RawPixelClassifier`, `RawPixelClassification`, `RawPixelTrainingRequest`, `RawPixelPredictionRequest` and their jobs; file/export/read jobs | Use windowed and block streaming paths to bound large slides; compose masks/contours and captured model identity. Preserve hash-only resume | Typed quantitative features follow SCI1-A1-A6; stitched contours and unmatched-grid feature semantics remain separate. Test seams against independent full-image references, IO cancellation, model/provenance persistence and research acceptance |
| Headless batches: `ProjectBatchRequest`, `ProjectBatchJob`, checkpoint APIs, `qupathxx_batch` | Adapt existing request/result/archive paths into the shared batch engine. For grid cells set `kind=CELLS`, `cell_grid` and `grid_cells`; each image resolves its own calibration. CLI and UI use the same native factory and options | Grid interruption/resume, per-image calibration, typed CSV and QCL5 checkpoints pass source169 engineering qualification. Remaining scientific/platform scope, partial failure, source/model replacement, abandoned-work cleanup and stable memory remain. Distributed workers require a later contract |
| Spatial analysis: `SpatialDistanceRequest`, `SpatialDistanceResult`, `SpatialDistanceJob`, retained records/CSV and native --measure/--export/--verify-spatial | [Python266](benchmarks/spatial-python-266/receipt.json) verifies bindings; [CLI267](benchmarks/spatial-cli-267/receipt.json) verifies real native measure/save/export/reverify commands and bounded ID files. Reuse owner capture/jobs, immutable table pages and project codecs for both UI hosts | [Frontend268](benchmarks/spatial-frontends-268/receipt.json) and [fault/cohort269](benchmarks/spatial-faults-269/receipt.json) locally verify ImGui/managed delivery and packaging. Qualified273 contains that spatial source; later wait/header repairs need own-source qualification. Neighborhood/density/signed-distance/tiled aggregation and representative science/resource evidence stay required |
| Recorded workflow automation: native commands, parameterized requests and retained records | Compose a versioned ordered workflow manifest from existing request/command identities; reuse batch execution and per-stage archive/restart checks | Define dependency/data references, error/cancel policy and reproducible replay before promising general pipeline automation; validate one multi-stage workflow across script, CLI and UI |
| Managed Python viewer: `ViewerSession.execute`, `grid_cells`, notifications and analysis endpoints | Submit `grid_cells` while the native owner pumps; submission copies the recipe and owner-stage capture resolves calibration. Reuse poll/wait/take, cancellation and retention; edits/publication stay on the owner | Grid producer/owner retention, export/save and stale/calibration delivery pass native/installed desktop checks locally. Broader lifecycle/backpressure/shutdown and platform qualification remain. Add endpoints only for missing real workflows; no Python callbacks owning native workers |
| Python API/NumPy: public `__all__`, generated stubs, `.numpy()`, `from_numpy` | Keep native authority and thin exports; validate mutable inputs before copy/GIL release. Generate stubs from the newly installed wheel, package them, then check typed examples | Additive API compatibility, array strides/dtype/lifetime, errors and shutdown. Record optional skips by capability. Avoid manually maintaining generated signatures |
| External models/plugins: deferred Cellpose/StarDist, OMERO/ImageJ exchange, versioned extension interfaces | After native publication contracts stabilize, use Python adapters for versioned masks/features and explicit image/data exchange. Keep remote access and credentials outside the native document authority | Version/model/input identity, shape/calibration, resource/lifecycle and publication validation; no model download/ABI/plugin host added as a shortcut to native scientific completion |
| Source identity/fingerprints: `ReaderSource`, `FileFingerprintJob`, `associate_source_file`, provenance JSON APIs | Reuse existing file-version and native content-hash jobs; capture identity once at the operation boundary and verify before publication | Define partial/full hash policy, changed/missing/aliased source behavior and IO budgets. Test source replacement races; a stable path or metadata-only check is not a content-equivalence claim |
| Packaging/distribution: CMake optional targets, wheel builders, runtime manifest and fresh consumers | Extend already accepted archive/wheel qualification to the selected supported platform matrix; reuse dependency/notice inventories and native host probes | Installers/uninstall, publisher signing, older-OS/ABI limits, Mac Python/GUI/OpenSlide, clean machines and sample workflow walkthrough. A Mac native builder result does not qualify a Mac Python package |

## SCI1-A: typed raw classifier delivery checkpoints

Historical implementation checkpoints follow. Their dated next-step/campaign
instructions are superseded by the current task and validation policy. Keep
their source-specific evidence and unresolved scientific requirements.

This is the integration route, not a completed public API. Current
`cpp/include/qupathxx/pixel_features.hpp` accepts `PixelBuffer`, exposes
RGB/OD/stain options, and retains `std::vector<float>` feature values.
`pixel_classifier.hpp` likewise classifies `PixelBuffer`. Raw reader support
therefore does not establish a typed classifier path. Preserve the RGB recipe
and archive behavior while adding an explicit quantitative recipe.

A1 has the [numeric contract and consumer trace](raw-pixel-classifier.md).
A2 features have [local Windows/Linux evidence](benchmarks/raw-pixel-features-local.json)
at `a13adef`. Source `ed2bf40` additionally has [native model/prediction evidence](benchmarks/raw-pixel-classifier-local.json):
six selected suites per host, preserved 12,600 reference feature slots, independent
QPR1 framing, exact forest vote/confidence cases, per-axis physical resolution,
model file replay and atomic replacement. That receipt covers the original
pre-release QPR1 raw model format. Candidate `220304d` now writes QRM1 and reads
legacy raw QPR1 explicitly while retaining original bytes/hash; RGB record QPR1
is a separate unchanged format. Project archives/retention now have the separate source440f610 local receipt;
stream prediction/training checkpoints have the separate local receipts below;
retained external raw stream references have the later local receipt below; installed workflows remain open.

The additive native `RawPixelRecipe`, `RawPixelClassifier` and
`RawPixelClassification` reuse `ForestModel` and the quantitative kernel while
retaining both axes. RGB QPC1/QPC2 remains unchanged. Bounded A3 owner-captured
annotation sampling and prediction jobs now have [local evidence](benchmarks/raw-pixel-workflows-local.json)
at `7beb80b`: six selected native suites on each Windows/Linux host. They reuse
raw-analysis source/ROI authority, object snapshots, native spatial indexing
and the RGB deterministic reservoir. Captured Document calibration overrides
file calibration for physical features. Cancellation, edits, metadata/compound
file changes and owner-thread delivery have distinguishing controls.

The earlier caches configure training OFF and prove backend-unavailable refusal
before pixel IO. A separate OpenCV 4.11.0 configuration now has [positive local
Windows evidence](benchmarks/raw-pixel-training-local.json) at `e7e4b66`: three
backend-ON suites and one backend-OFF workflow suite pass. Real TIFF direct/job
training, trained archives/model files and portable votes agree. Archived model
parameters include the effective admitted work budget; reuse those recorded
parameters for byte-identical historical retraining. Fresh table readmission
can admit a different budget while preserving training matrix/votes. Linux/Mac
and integrated platform backend training remain open. Candidate `220304d` adds QXS1 recipes,
QXT1 sampled/trained tables, QXP1 predictions and QXR1 retained records. Existing
SQL schema 13 stores the tagged raw record variant without another migration.
Decoded historical data has no live capture authority. Source-only provenance
uses explicit QPV4 opt-in; ordinary object provenance validation stays strict.

The [archive receipt](benchmarks/raw-pixel-archives-local.json) accepts the
affected native workflow suite on both hosts at `440f610`, with 336 frozen
compiler inputs and retained binaries. The cleanup-only change touches the
workflow helper; ten passing regressions keep their original source220304d.
All five failed development rounds remain raw evidence. Recovery's deliberate
lock/session sidecars are removed only by the joined, fixture-owned cleanup;
product recovery deletion remains unchanged.

The [replay/apply receipt](benchmarks/raw-pixel-replay-apply-local.json) binds
339 matching inputs at `92edba8`, four selected Windows GEOS-ON and four Linux
GEOS-OFF suites plus three Windows training-ON suites. Eleven final executables
are retained. Fresh owner requests reverify reopened TIFF records by content;
historical decoding confers no authority. The shared GEOS raster converter maps
both rational axes, borrowing labels/ROI. Apply checks the captured document
revision and commits one ordinary edit command, with undo/redo. Verification is
a fresh boolean observation; it does not cache or transfer a permission to apply.
Broader 15-suite development runs and initial failures keep their own sources.
The earlier Linux development XML reports were overwritten by driver path reuse;
their original text logs remain, and final source-bound XML was recovered intact.
This completes the bounded native replay/apply portion, not streamed/stitched
contours, installed frontends or integrated platform acceptance.

The [window receipt](benchmarks/raw-lattice-windows-local.json) accepts native
sampling/features at `ae0ba29`: five suites each Windows/Linux and two Windows
training regressions, 342 matching inputs, twelve retained executables. Exact
Fraction references and 9,072 bitwise tiled feature slots pass. Geometry is an
original global lattice plus a bounded sample-index range; it does not construct
or admit a whole-raster ScientificGrid. Window-only reduced weights use the same
integer accumulator with a checked 72-bit denominator mode. Legacy full-grid
limits/hash grammars and codec meanings remain unchanged. The primitive does
not supply completed streams.

The [window model/owner receipt](benchmarks/raw-window-classifier-local.json)
accepts source `642f728`: eight suites each Windows/Linux and three Windows
training suites with 345 matching inputs and nineteen retained executables.
`RawPixelRecipe` window overloads preserve QXS1/QRM1 meaning and bytes; the distinct
`RawPixelWindowClassification` shares feature admission and forest vote batching.
`RawPixelWindowPredictionRequest`/`Job` capture one bounded window, use original
global centers for ROI membership and enforce source/owner publication checks.
Metadata copy quotas and actual sampler workspace/series checks precede IO.
Sparse owner-captured tiled training now has [local acceptance](benchmarks/raw-tiled-training-local.json) at `1273c3e`: the affected workflow suite passes on Windows GEOS-ON, Linux GEOS-OFF and Windows training-ON. Sixteen passing regressions retain source `fb7ddb0`; the final source changes only the metadata-quota test fixture. Both sources authenticate 348 compiler inputs; 22 original/final executables are retained. Bounded prediction streams now have [local acceptance](benchmarks/raw-pixel-stream-local.json) at `8d544eb`: eight selected suites each Windows/Linux and three Windows training suites pass with 352 matching inputs and nineteen retained binaries. Prediction prefix files and hash-only resume now have [local acceptance](benchmarks/raw-stream-file-local.json) at `c843dfa`: three affected workflow suites pass; sixteen regressions retain source `125afbb`. Both sources authenticate 356 inputs and 22 original/final executables are retained. QTT1 sparse training archives, project retention and fresh owner replay now have [local acceptance](benchmarks/raw-tiled-archives-local.json) at `cc6fab4`: three affected workflows pass, sixteen regressions retain `30d0659`, both sources authenticate 360 inputs and 22 binaries are retained. Training prefix checkpoint/resume now has [local acceptance](benchmarks/raw-training-checkpoint-local.json) at `fb7ab73`: eight Windows and eight Linux suites plus three Windows training suites pass with 363 matching inputs and 19 retained binaries. Native owner file jobs have [local acceptance](benchmarks/raw-pixel-file-jobs-local.json) at `404f506`; external raw prediction links now have [local acceptance](benchmarks/raw-stream-links-local.json) at `a608a53`; frontend integration remains open. The following
packets refine A3-A6; they are dependencies within SCI1-A, not separate release
milestones or hosted campaigns. Root owns implementation and integration.

| Order / API or workflow | Easiest path and bounded scope | Exit evidence before advancing |
| --- | --- | --- |
| T1 / locally verified owner-captured tiled training: `ScientificRawLattice`, `RawPixelTiledTrainingRequest`, table and job | Describe global dimensions/axes directly, then create bounded `ScientificRawWindow` cores. Reuse `ObjectFeatureRequest`'s immutable annotation snapshot, source guards, native spatial index, one shared deterministic reservoir and `train_forest`. Plan only tiles intersecting selected annotation bounds. Keep the new table distinct from full-grid QXT1 | Exact values, global coordinates, annotation IDs, eligible/ambiguous counts and retained rows match full-grid training for equal coverage; repeat with different tile shapes. Actual backend training/model votes agree with fixed effective parameters. Sparse large-lattice, overlap, quotas, cancellation, stale source/document, retry and backend-OFF-before-IO controls pass |
| T2 / locally verified `RawPixelTiledPredictionRequest`, `RawPixelPredictionStream` and owner job | Compose existing window prediction/feature kernels over an explicitly bounded region or declared whole-slide traversal. Keep only a bounded number of windows and output blocks live; use global sample indices and existing owner delivery/cancellation rules. Avoid assembling a whole-slide result vector | Labels, support and votes match full-context references at fractional seams, edges, anisotropic spacing and ROI boundaries. Traversal covers each requested core once. Decode calls, peak live buffers and maximum in-flight work remain bounded as slide size grows; actual process-memory acceptance remains a separate measured gate |
| T3 / native persistence paths locally verified: QWS1 prediction files/resume, QTT1 training records, QTC1 training replay, owner file jobs and QWL1 external links; installed/integrated acceptance follows at T4-T5 | Use existing atomic block writers and checkpoint controllers with a distinct versioned tiled-table/stream representation. Bind lattice, coverage/tile policy, feature/model identity, selected annotations, effective training parameters and source/window commitments. Restore the global reservoir state exactly or deterministically replay admitted inputs | Uninterrupted and interrupted/resumed rows and prediction blocks agree; save/reopen/replay works on real TIFF. Truncation, tampering, source/model/selection/calibration changes and cancellation reject safely. Existing RGB and full-grid QXS1/QXT1/QXP1/QXR1 meanings remain unchanged |
| T4 / partial: Windows cp312 bindings/wheel/example locally verified; official CLI and raw project batches locally verified; ImGui/managed viewer and other installed profiles open | Reuse the installed native API, array ownership and job delivery below. Add shared raw recipe/model/prefix arguments to existing CLI/batch dispatch, then owner UI train/predict/retain/export/resume actions. Keep historical links separate from live apply authority; generate stubs from the installed module | One real-TIFF workflow trains, predicts, saves/reopens and resumes from Python and headless paths. Desktop preview and supported apply are undoable and refuse stale output; typing, array lifetime, cancellation and shutdown pass. Cross-tile contour apply must wait for its separate stitching acceptance |
| T5 / integrated engineering qualification | Preflight producer, fixture, manifest and auditor closure, freeze one complete T1-T4 source, then use the required platform profile. Preserve all inherited named cohorts and receipt rules | Scope-matched native/installed/desktop/persistence gates pass at the frozen source; platform/backend omissions stay explicit. Independent biological/model, hardware and release gates keep their own acceptance |

### T1 implementation decisions

The lattice, sparse request/table/job and shared full/tiled reservoir are native APIs at `1273c3e`, with the local receipt above. They now have the separate Windows installed Python acceptance below; official CLI now has the later Windows acceptance below; raw project batches have the later acceptance below; desktop and other installed profiles remain open. Tiled-table archives gained separate local acceptance at `cc6fab4`. T1/T2 pass their local native exit checks; T3 prediction persistence/resume is locally verified, with replay-based training checkpoints now locally verified and native external raw stream links now locally verified; installed frontends next. The initial three workflow failures came from a fixture retaining oversized string capacity after text restoration; restoring the original vector allocation corrected that test. Original failures, source identities and passing regressions remain separate evidence.

- **Sparse planning:** find global sample-center ranges intersecting each selected
  ROI's bounds using monotone binary search on the original rational axes. Union
  the resulting tile ranges before allocation or decoding; an interval sweep is
  sufficient. Reject scan/window-count limits from the union, rather than scan
  every global cell or materialize duplicate tile lists per annotation. Bounds
  choose candidate tiles; the existing exact ROI membership rule chooses rows.
- **Selection semantics:** visit each planned core cell once, exclude halo cells
  from training, and resolve same-class overlap by minimum annotation ID. Exclude
  conflicting-class cells. Use global indices for centers, reservoir priorities
  and stored coordinates. Maintain one reservoir per class across all windows;
  balance only after global selection. Share this policy with full-grid raw
  training so partitioning cannot create a second sampling algorithm.
- **Admission:** charge source/annotation snapshots, classes, spatial index,
  union plan, reservoirs and retained rows before allocating them. Check each
  window's actual sampler/feature workspace before that window's decoder IO;
  reserve fixed owner state from the same work budget. Do not imply every later
  tile was admitted before the first read. A failure returns no partial table or
  document publication. Backend-OFF training rejects before pixel IO.
- **Ownership and identity:** capture document identity/revision, calibration,
  source metadata and immutable annotations on the owner. Workers borrow the
  retained reader and access immutable captures; join before releasing it. Check
  actual primary/companion files at delivery. Global geometry and core shape
  remain separate, preserving QXS1/QRM1 bytes for equivalent recipes.
- **Training proof:** use explicit identical effective forest parameters when
  comparing models. A changed admitted work budget can change model identity
  even when selected rows and votes agree; preserve this recorded behavior.

### T2 implementation decisions

Use the [source-bound stream receipt](benchmarks/raw-pixel-stream-local.json)
for the exact acceptance scope. A `RawPixelStreamRegion` specifies global sample
indices; omitted coverage traverses the complete lattice. Row-major window
geometry is computed from an index, without a whole-raster admission or plan
vector. Capture one existing window authority for model, ROI, calibration,
source and document identity/revision; share its numeric/ROI result kernel for
all windows and admit actual workspace before each read. Existing window result
hashes and all legacy archive grammars remain unchanged.

The synchronous stream returns owning blocks. The owner job holds one future
and one pending slot; `take(document)` validates fresh authority/files, transfers
that block and starts the next. No next read occurs while output is pending.
Produced and delivered counters are distinct; only complete successful streams
and fully delivered jobs expose final ordered input/result commitments. Caller-
retained blocks are outside the producer budget. Tests prove consumed blocks
are released, decoder requests stay bounded, and a trillion-cell traversal uses
the same admission ceiling as small coverage. These are local engineering
resource controls, not process-RSS or hardware acceptance.

### T3 prediction persistence delta and remaining training work

The [QWS1 receipt](benchmarks/raw-stream-file-local.json) accepts native prediction
prefix files, historical block reading/verification and hash-only source replay
at `c843dfa`. `save_raw_pixel_stream` commits up to `max_new_windows`; passing a
prior file resumes from that explicit prefix. Existing atomic writers preserve
old checkpoints on cancellation/errors. Uninterrupted and resumed bytes agree;
real TIFF closes/reopens with a fresh reader and resumes unchanged source files.
Model/ROI/coverage/calibration, source generation/file-set changes, tampering,
source-output aliases and a concurrent writer at a previously absent destination
have distinguishing controls. The final race hardening changes only the file
implementation and its test; retain all initial 8/8/3 passing observations at
`125afbb` and final three workflow observations at their own source.

File metadata/one-historical-block limits are explicit and independent of the
live producer's admitted work budget. Prefix replay reuses sampler digests and
the feature-input hash grammar, without feature planes or inference. Historical
decode restores no live document authority. Successful explicit commits are the
resume points; automatic scheduling, abandoned temporary-file maintenance and
broader crash/power-loss qualification are not supplied by this local packet.

### T3 sparse training archive delta and next checkpoint

The [QTT1 receipt](benchmarks/raw-tiled-archives-local.json) accepts sparse sampled/
trained tables, existing record retention and fresh replay at `cc6fab4`. Add the
explicit QTT1 variant to `RawPixelArchive`; keep QXT1/QXP1 field grammars unchanged.
Use `tiled_table()` for sparse rows and `lattice()` for universal source geometry;
`grid()` deliberately refuses a tiled archive. The existing QXR1 wrapper and
SQLite schema 13 store mixed full/tiled records without a new migration.

Reuse shared bounded archive primitives, metadata/provenance, sparse candidate
planning, semantic table hashes and forest matrix checks. Rebuild derived plan/
index data on historical decode; clear its capture token. Native
`keep_raw_pixel_tiled_training`, `RawPixelTiledTrainingRequest.replay` and job/
document verification require fresh owner captures and actual source checks.
Project save/open, background snapshots and recovery preserve bytes. Every
selected annotation ID must fit a record's captured bounds, even when it supplied
no retained rows; both full and tiled consumers now use that shared check.

Training interruption/resume is locally verified through prefix replay below.
The alternative state-restoration route would preserve the complete per-class
reservoir and eligible/ambiguous counters exactly. A completed balanced QTT1 table cannot reconstruct
candidates discarded by balancing and must not be used as checkpoint state.
Keep original input/hash semantics and current source/model/annotation authority.
Then add native owner prediction-file export/read/retain adapters and T4 installed
frontends before the T5 integrated freeze. Automatic checkpoint maintenance and
broader crash/power-loss acceptance remain separate work.

### T3 training checkpoint acceptance and next consumer packet

The [QTC1 receipt](benchmarks/raw-training-checkpoint-local.json) accepts `fb7ab73`.
The implementation uses deterministic prefix replay through the existing tiled sampler and
shared reservoir. This is the simplest compatible route: it preserves the
current selection and input-hash grammar without serializing platform hash
state. Resume repeats prefix decoding and feature extraction; it is bounded in
memory, not constant in IO or elapsed time. Defer fast reservoir-state restore
until measured replay cost justifies its additional format and validation work.

1. **Native checkpoint contract (locally verified):**
   `RawPixelTrainingCheckpoint.from_bytes` and
   `load_raw_pixel_training_checkpoint` read distinct QTC1 historical metadata.
   `save_raw_pixel_training_checkpoint` reuses bounded codecs and atomic commits. Capture
   recipe/lattice, ordered sparse coverage, tile and sampling options, immutable
   annotation identities/geometry, source observations and completed-window
   prefix commitments. Checkpoint execution returns a table only at complete coverage; the caller
   supplies explicit forest parameters to the existing training function afterward.
   Interrupted forest fitting is not saved by QTC1.
2. **Resume execution (locally verified):** recapture the request on the document owner; reject
   changed selection, geometry, options or source observations before pixel IO.
   Replay completed windows into the same global reservoir and hash accumulator,
   validate their commitments, then continue the suffix. Recheck actual files before committing. Owner controllers must additionally
   check the current document before exposing a returned table. Apply normal per-window work admission during
   replay; cancellation or errors preserve the previous committed checkpoint.
3. **Decisive local checks (passed across the three configurations):** compare uninterrupted and multi-stage resumed
   tables bit-for-bit, including coordinates, owners, counts and input/result
   hashes. Cover balanced/unbalanced sampling and an actual trained backend with
   identical effective parameters. Use real TIFF close/reopen, changed decoded
   input/sampling-seed/calibration, malformed or truncated prefixes, cancellation,
   output aliases and concurrent destination changes. Preserve original failures;
   run only affected suites on the existing three configurations.
4. **Owner jobs and retained links (locally verified); next installed consumers:** native owner checkpoint/export/read jobs now reuse the synchronous codecs
   with one pending result and fresh owner/output checks. QWL1 now supplies retained prediction-file references with explicit external-file lifetime rules. Keep worker results private until fresh owner/document/file checks pass;
   cancel and join before releasing borrowed readers. Failures detected before
   commit preserve the prior checkpoint; late owner staleness suppresses delivery
   without rolling back completed historical output. Reuse one-slot delivery and existing QWS1
   verification instead of decoding an entire stream on the UI thread. Reuse the
   existing RGB `PixelStreamLink`/record ownership contract: records reference
   external output, saving rebases paths without copying file bytes, deleting a
   record leaves its file intact, and relinking requires fully verified matching
   content. QWL1 provides distinct bounded raw lattice/ROI/model/prefix metadata
   while preserving RGB QSL1 meanings. Project open restores historical metadata without
   scanning external files. Verify asynchronously before use, and test missing/
   moved/tampered files, save-as paths, quotas and snapshot/recovery behavior. Bind stable APIs once via nanobind, then reuse them in Python, CLI/batch
   and ImGui/viewer actions. Exercise one installed train/predict/save/reopen/resume
   workflow before freezing the integrated qualification source. Keep stitched
   contour apply behind its separate acceptance gate.

The [owner file-job receipt](benchmarks/raw-pixel-file-jobs-local.json) accepts
`404f506`: three affected workflows pass with 367 matching inputs. Sixteen
regressions retain `447d3ae`, where all 19 initial suites passed. The successor
changes only the job implementation to require full export-prefix verification
before summary delivery; 22 original/final executables are retained. Metadata
checks on poll/take do not rescan all blocks. Completed disk output is historical
even when a later owner edit suppresses delivery; no rollback deletion is implied.

The [QWL1 external-link receipt](benchmarks/raw-stream-links-local.json) accepts
`a608a53`: three affected workflows pass, 370 inputs match, and sixteen
regressions retain `d42fad4`. The final change fixes only the RGB coexistence
fixture's missing sampling resolution; original failures and 22 binaries remain
source-bound. Empty/partial/complete prefixes, independent record tamper,
trillion-cell geometry, mixed project records, missing-file metadata reopen,
save-as/snapshot/recovery and verified relocation pass. Links store bounded
metadata and a transient checked file observation, not the model/raster. Project
decode clears that observation; keep/relink requires verification again.

T1-T3 have local native exit evidence; T4 now has the installed Python receipt
below. Proceed to existing CLI/batch and owner UI actions through those bindings/
jobs, without another numeric implementation. Keep historical link reads distinct from live apply authority. Run one
installed real-TIFF train/predict/save/reopen/resume example with actual training,
typing, cancellation, shutdown and retained output. Generate final stubs from
that installed module, then freeze the complete T1-T4 source for T5. Automatic
checkpoint maintenance and abrupt-process/power-loss acceptance remain separate
open work; a local native receipt does not qualify installed consumers.

### T4 installed Python acceptance and next adapters

The [raw Python receipt](benchmarks/raw-python-bindings-local.json) accepts
`202b41a` on Windows CPython 3.12 with actual training/TIFF/project enabled and
GUI/GEOS/OpenSlide disabled. All 10 new raw workflows and 57 selected regressions
pass; the one optional legacy Aperio case remains skipped. Three native suites
pass. The imported noneditable wheel, package members, runtime source and all
374 compiler inputs authenticate; the pinned OpenCV SDK verifies 195 files.
Two positive typing files (including the runnable example) pass and four negative
cases reject. Require nonzero/exact analyzed-file counts, not a green empty run.

The new bindings expose lattice/window data, recipes/models, full/tiled sampling,
prediction/streams, archives/replay/contours, checkpoints/files/links and owner
jobs. Numeric kernels remain C++. Views retain their Python owner and are read-
only; explicit copies own mutable storage. Job waits inspect Document under the
GIL, release it only for joining, then recheck under the GIL. Python controller
destructors release it during native cancel/join so worker-held Python references
can be released, including when a foreign Python thread drops the last owner.

Run [the installed example](../examples/raw_classifier_workflow.py) with an
existing labelled TIFF project and a new output directory. It demonstrates
training/prediction checkpoints and resume, model/table retention, verified
external links and new-project reopen. Its identity-normalized intensity recipe
is an explicit example, not a biological comparison or stitched apply gate.

The official CLI now has the later acceptance below. Next extend multi-image
classifier batch and desktop consumers, sharing channel/normalization/lattice/
model/prefix validation. Reuse
existing project batch snapshots, file jobs and checkpoint admission; fail
unsupported reader/backend recipes before work. Then extend ImGui and managed
viewer owner actions, owning previews and retained-file browsing. Cancel/join
before reader shutdown and refuse stale preview/apply. Qualify one installed
train/predict/save/reopen/resume workflow per claimed frontend, including typing,
array lifetime, cancellation, shutdown and retained outputs; freeze T5 only when
those complete outcomes exist. This receipt does not qualify those frontends,
other Python/platform profiles, clean machines or scientific/resource/release
gates.

### T5 closeout sequence

Root owns the remaining arm64 failure. [Campaign236](benchmarks/raw-classifier-236-campaign/closeout/receipt.json)
is terminal at `530c6bb`: engine/portable audits pass and x86_64's Mac producer
passes, but arm64 fails one native replay suite. Preserve every original role.
The [Mac-only diagnostic](benchmarks/raw-classifier-236-campaign/macos-diagnostic-238/receipt.json)
is run `40377716087` at `c3c19a9`, with 719 authenticated inputs. It preserves
compiled products before CTest and labels unexpected replay failures. Required
operations/markers remain intact. Local Clang profiles do not reproduce the
failure, so they do not qualify arm64. The diagnostic arm64 archive now reports
`historical and foreign capture refusal`; x86_64 subsequently passed, making
238 terminal. Its inventory excludes test programs. [Diagnostic240](benchmarks/raw-replay-macos-240/receipt.json)
now isolates each call and its exception type, retains the actual failing test
and preserves every required rejection check. Run `40384720525` is frozen at
`4a72fd8` with 720 bound inputs. The 166 local observer controls and three Clang
probes pass; no Darwin repair is established. Diagnose first; do not dispatch another
complete engine/portable campaign merely for observer work.

| Step | Easiest path / existing owner | Exit or recovery |
| --- | --- | --- |
| Observe and retain | Query the exact original handles; collect completed-job artifacts while other jobs continue. Reuse campaign213 collector checks | Save original run/job metadata and failures. A polling/download timeout is uncertain; inspect the same attempt before retrying transport. Do not redispatch or cancel healthy jobs |
| Authenticate the complete set | Match run ID, source, artifact name, size and SHA-256; reject expired archives, unsafe paths and duplicate/colliding members | Exactly five engine, two Mac-native, two manylinux builder and two fresh-consumer archives. Partial collection remains partial |
| Audit the frozen candidate | Run `accept_unified_engine.py`, `accept_macos_native.py` and `accept_manylinux.py` from the frozen checkout with the complete existing campaign inputs | All three audits pass: inherited cohorts, 58 exact raw identities, declared skips, native enabled/OFF semantics, typing, runtime notices and final source checks |
| Diagnose at the failing layer | Preserve the first failure; distinguish build/runtime, fixture, observer and transport causes. Repair the smallest demonstrated issue in integration | Compatible observer-only repairs may re-audit retained artifacts with both identities explicit. Runtime changes need affected checks and a successor candidate. Never relabel binaries or silently patch frozen tools |
| Record engineering acceptance | Publish one combined source-bound receipt; update the execution card and affected inventory rows together | Close only the named T5 engineering scope. M0–M7, scientific, complete resource/hardware and release gates retain their own requirements |

No new evidence framework, universal cache or wholesale auditor refactor is a
prerequisite. Reuse maintained contracts and collect independent diagnostics
when their prerequisites passed. Documentation changes need link/diff and
compiler-input checks, without another native build or hosted matrix.

### SCI1-B next-workflow route

Selected SCI1-B workflow: streamed raw contours. Native, installed Python,
desktop and managed consumers have local verification. CLI has installed local verification at `4e91d1e`; project batches now have
installed local verification at `f5ec3eb`. [Historical-file delivery](benchmarks/raw-stream-boundaries-local/history-248/receipt.json) is locally verified at `3039f0c`.
The frozen Mac diagnostic is terminal; preserve it while diagnosing the failure.
Resolve SCI1-A's arm64 failure before claiming integrated T5 acceptance.

| Checkpoint / state | Easiest path and dependency | Exit evidence |
| --- | --- | --- |
| B1 reference / locally verified | Reuse the standard-library oracle and existing full-grid tracer; preserve four patterns, sixteen tile layouts, crop phase and both axes | Exact directed edges, parts, holes, class counts, areas and perimeters; original fixture failures retained |
| B2 boundary join / locally verified at `a3be74b` | Shared run sweep plus internal `RawStreamBoundaryBuilder`; consume the existing ordered blocks, reconcile class-tagged seams and release raster blocks | [Local receipt](benchmarks/raw-stream-boundaries-local.json): three affected Windows native suites, actual filtered streams, incomplete/overlap/order/cancel/quota refusals; two million samples with seven peak runs. No whole-slide RSS claim |
| B3 geometry / locally verified at `abeb20f` | [Connected GEOS geometry](benchmarks/raw-stream-boundaries-local/geometry-242/receipt.json) reuses the shared kernel, batch lookup, original axes and existing clipping/extraction. Four Windows ON and four Clang minimal OFF native suites pass; the complete workflow remains open | Exact B1 geometry across tilings, deterministic connectivity, work/output admission and GEOS cancellation. Avoid a boundary scan for every face and any full-mask allocation |
| B4 result and authority / C++ locally verified at `1fb2624` | [Captured result/direct/job/publication](benchmarks/raw-stream-boundaries-local/workflow-243/receipt.json) reuses the existing stream and owner-job pattern. Whole-result/edit/ID budget admission precedes the atomic edit. Four Windows ON and four Clang OFF suites pass; Python/desktop/managed delivery has the later B5 evidence | Model/source/image/revision mutation refusals, cancellation, no partial edits and apply/undo/redo. Historical QWS1 blocks confer no live authority |
| B5 consumers / Python, desktop and managed locally verified; CLI/batch/history locally verified | [Installed frontend receipt](benchmarks/raw-stream-boundaries-local/frontends-245/receipt.json) at `7c77367` verifies actual tiled desktop preview/apply/undo/redo, cropped viewer commands, pre-commit result/ID admission and project geometry save/reopen. [Installed CLI](benchmarks/raw-stream-boundaries-local/cli-246/receipt.json) at `4e91d1e` passes all 18 new cases and installed regressions. [Installed batches](benchmarks/raw-stream-boundaries-local/batch-247/receipt.json) at `f5ec3eb` verify sealed per-image geometry and input-hash reuse through Python/CLI/ImGui; [historical QWS1 conversion/fresh replay](benchmarks/raw-stream-boundaries-local/history-248/receipt.json) at `3039f0c` now has separate local installed verification | One installed train/predict/stream-contour/save/reopen/apply workflow; matching stubs/typing and named semantic cohorts |
| B6 qualification / open | Freeze only after B3-B5 settle; affected checks first, one installed integration pass, then applicable engine/Mac/portable audits | Exact source/configuration/artifact identities and all inherited gates. Scientific, resource/hardware and release acceptance remain separate |

<a id="b5-remaining-consumer-tasks"></a>

### B5 consumer closeout and B6 qualification tasks

Root owns this sequence. B5.1-B5.3 and B6 local candidate admission are verified
within their named scopes. Original257 is terminal; candidate258 repairs the
Windows path gaps and passes all three complete roles at its own source.

| Task / current state | Easiest route through existing APIs | Required outcome before advancing |
| --- | --- | --- |
| B5.1 CLI / locally engineering verified at `4e91d1e` | `cpp/raw_pixel_cli.cpp` and `qupathxx_batch --contour-raw-pixels`; reuse `RawPixelStreamContourJob`, exact capture, atomic publication and `Project::save_new`. Keep final source/file checks at the save boundary | [Installed receipt](benchmarks/raw-stream-boundaries-local/cli-246/receipt.json): all 18 new cases pass, with nine native suites and 240 Python passes/three skips overall; stubs/typing verify. Required outcomes exercised: native/Python geometry and metadata parity, cropped/no-clip behavior, collisions, invalid flags, quotas and interruption during live work. Prove no input overwrite or partial destination, worker join and reader release. Bind CLI and oracle runtime separately; finish with a coherent installed package and relevant regressions |
| B5.2 project batch / locally engineering verified at `f5ec3eb` | [Installed batch receipt](benchmarks/raw-stream-boundaries-local/batch-247/receipt.json): `ProjectBatchKind::RawPixelContours` and Python `RAW_PIXEL_CONTOURS` extend the existing `ProjectBatchRequest`/`ProjectBatchJob` and raw-classifier options. Reuse model-pinned captures, per-image document commands, directory lock, owned checkpoints and Stop/Continue policy. Expose the same native operation to Python/CLI/ImGui | Nine Windows/five Clang native suites and 274 Python passes/three skips, including all 15 new cases. Mixed compatible/incompatible images, exact axes, one atomic per-image result, cancel/restart and changed source/options checks pass. Reopen saved geometry; verify checkpoint output before reuse. No second job engine or unverified checkpoint skip |
| B5.3 historical QWS1 / locally engineering verified at `3039f0c` | [Installed historical receipt](benchmarks/raw-stream-boundaries-local/history-248/receipt.json): one verified block walk reuses the indexed reader and boundary/GEOS kernel for distinct immutable `RawPixelFileContours`. For live apply, capture current source/model/ROI/revision and use fresh native prediction/publication; begin with bounded re-execution rather than granting saved blocks authority | Nine Windows/five Clang native suites, 298 Python passes/three skips and all 23 new history cases pass; stubs/typing and actual file/retained-record UI/managed/CLI flows verify. Missing/moved/corrupt/partial files, changed source pixels/model/ROI/calibration/revision, quota/cancel, late file changes and cropped axes are exercised. Three explicit supplementary refusal cases on the unchanged wheel bring the combined new history scope to 26 cases (301 Python passes/three skips across 304 unique identities). Historical inspection remains available without live authority; valid fresh apply matches the independent oracle and supports atomic undo/redo |
| B6 Mac repair and integrated candidate / engineering accepted at258 | [Combined receipt](benchmarks/raw-path-fixture-258/closeout/receipt.json) accepts all required engine, Mac-native and portable profiles at a5b5394. Original257 Windows failures and repair probes remain preserved | All 11 archives authenticate; every complete inherited gate passes. Continue with M3-ALIAS1 and the remaining SCI1-B/decision/release lanes; scientific/hardware/UI/support/release boundaries remain |

#### B6 execution checklist

Root owns each step. This checklist refines B6 rather than adding another queue.
Candidate258 is engineering accepted at `a5b5394`. The complete five-profile
engine audit, both Mac-native architectures and portable builders/fresh Debian
consumers pass; all 11 artifacts authenticate under one source/ref. The original
Windows failures and local repair attempts remain preserved. B6 does not require
another dispatch. This acceptance does not close the separate explicit-root alias
reuse inefficiency, scientific/hardware/support/release gates or all M0-M7 work.

The [snapshot witness259](benchmarks/root-alias-analysis-259/receipt.json) isolated
relative/absolute locator selection under two spellings of one physical output
root. [Repair260](benchmarks/root-locator-fix-260/receipt.json) now uses one private
lexical helper for linked images and both QSL1/QWL1 stream serializers. Four
regressions fail on unchanged a5b5394 and pass on f9ad28f, including actual reuse;
paired whole-file snapshots now match. Missing-source serialization works without
filesystem lookup. Literal/device paths and incompatible drives/shares retain
absolute fallback. UNC controls establish lexical behavior, not network runtime.
No input-hash fields or model/source/no-overwrite guards were weakened.

Original260 is terminal: both Mac-native architectures and portable builders/
fresh consumers pass their complete source-bound audits; the engine fails its
sanitizer viewer_session assertion while four other profiles pass. Preserve all11
archives and the [scoped closeout](benchmarks/root-locator-fix-260/terminal/receipt.json).
The native owner-delivery defect is now locally repaired at e2282b8 with a
controlled before/after witness; [receipt262](benchmarks/owner-delivery-race-262/receipt.json)
records one changed compiler input and the completed installed validation.

The original262 campaign is engineering accepted under the
[combined closeout](benchmarks/owner-delivery-race-262/closeout/receipt.json):

| Role / original run | State at this planning update | Required action |
| --- | --- | --- |
| [Engine40646189840](https://github.com/bipxx/qupathxx/actions/runs/40646189840) | Accepted at e2282b8 | All five archives and the complete engine auditor pass, including sanitizer viewer_session |
| [Mac40646201004](https://github.com/bipxx/qupathxx/actions/runs/40646201004) | Accepted at e2282b8 | Both architectures, native, Mach-O and relocation gates pass |
| [Portable40646221175](https://github.com/bipxx/qupathxx/actions/runs/40646221175) | Accepted at e2282b8 | Both builders and both fresh consumers pass complete portable gates |

Preserve original handles and partial transfers. Quiet builds or observer timeouts
are not retry authorization. Audit from the immutable
`build/ci/owner-delivery-qualification-262-audit` checkout. Candidate258 and original260
acceptance cannot transfer to this repair. Mac-native does not qualify Mac Python/
GUI/OpenSlide. Continue the partially implemented SPAT1 workflow; independent
scientific/performance/hardware/support/release gates and all M0-M7 scope remain.

The following B6 steps are completed historical evidence, not a second queue.

| Step / state | Smallest next action and files | Exit evidence |
| --- | --- | --- |
| B6.1 retained-binary diagnosis / observed, binary mismatch located | [Observer252 receipt](benchmarks/raw-replay-arm64-252/receipt.json): native ABI probe passes; unchanged retained4a72fd8 exits1 both plainly and under LLDB. An invalid_argument escapes its typed helper into main. Both relevant helper addresses alias length_error specializations; final exception tables bind length_error | Retain authenticated inputs, probe, events, original failure and [offline interpretation](benchmarks/raw-replay-arm64-252/interpretation.md). This locates the binary mismatch without establishing its compiler/linker origin or a product repair |
| B6.2 consumer and source admission / locally verified at `20e0b07` | [Contract250 receipt](benchmarks/raw-classifier-contract-250/receipt.json): existing producer/auditor helper now requires 143 exact installed identities and 31 per-case GUI declarations; both subprocess children and seam-reference inputs join the 486-path source proof. New streamed/history/batch native markers are required. Eight capability-independent existing markers now belong to both native profiles | 183 controls pass. Actual unchanged installed3039f0c runs admit GUI 142 passes/one declared skip and headless 111 passes/32 declared skips. Retained Windows full/Clang minimal native outputs admit. Preserve the initial real-minimal contract rejection; full candidate producer/auditor execution still belongs to B6.4-B6.5 |
| B6.2 typing admission / locally verified at `20e0b07` | Same receipt: `raw_stream_contour_valid.py` and `raw_stream_contour_invalid.py` join the maintained cohort. Actual pinned mypy2.4.0 runs against installed3039f0c, with recorded commands/logs; local Pyright remains separate evidence | Five positive files pass; four negative files produce exactly 20 intended diagnostics at declared file/line identities. All 33 installed wheel members and original wheel hash still match; no native rebuild or supported-release claim |
| B6.3 link-stage cause and repaired Mac runtime / accepted at candidate257 | [Original-object255](benchmarks/raw-original-object-255/receipt.json) isolates folding of differing catch handlers. Policy87b9dac disables Darwin deduplication; the [complete Mac audit](benchmarks/raw-link-qualification-257/macos-terminal/macos-acceptance.json) accepts both architectures at d57df01 | Both authenticated archives, 50 suites per architecture, inherited domain/source gates, Mach-O inventory and relocated runtime probes pass. Preserve the negative tiny witness and original failed executable; no Mac Python/GUI/OpenSlide claim |
| B6.4 integrated candidate / candidate258 locally verified at `a5b5394` | [Receipt](benchmarks/raw-path-fixture-258/receipt.json): 392 compiler inputs (two changed), 51 native suites, 880 controls, 326 installed passes/three skips across 329 identities, matching stubs/typing/SDK/package identities. Two additional long-root configurations pass; 95 related passes/seven skips overlap the main cohort | Frozen feature/raw-path-qualification-258 and fresh audit checkout bind all 490 raw sources. Prior/intermediate/final products survive. OpenSlide OFF/shared dependencies, Unicode glyphs and explicit-root alias reuse remain stated boundaries |
| B6.5 required qualification / engineering accepted at candidate258 | Complete five-job engine, both Mac-native architectures and portable builders/fresh consumers pass at a5b5394; [combined receipt](benchmarks/raw-path-fixture-258/closeout/receipt.json) binds every archive and auditor | All 11 sizes/digests/sources and inherited reader/runtime/typing/rendering/semantic/source/cohort gates pass. Completed GUI overview inspected. Mac Python/GUI/OpenSlide, broader science/hardware/UI/support/release remain separate; original failures preserved |

Independent resource preparation now has [resource253](benchmarks/raw-stream-resources-253/receipt.json):
three fresh Windows processes, fixed native/cache budgets, exact authored geometry,
22 verifier controls and measured OS peaks. This closes the local synthetic
straight-boundary characterization only; it does not waive representative
workloads, eligible hardware or the complete process/resource acceptance lane.

Cost controls: do not rebuild SDKs for observer/documentation changes, introduce
a new runner or persistent PASS cache, or dispatch a second qualification for
unfinished next-workflow primitives. Bounded native preparation of the named
SPAT1 workflow may progress in the mutable integration checkout while the frozen
candidate qualifies; preserve prior products and never modify its frozen source.
Retain failed attempts and source-bound references; temporary build cleanup must
preserve those inputs and live job files.

[Broader local regression251](benchmarks/raw-integration-regression-251/receipt.json)
has now exercised all 4,420 installed test identities at runtime3039f0c.
The original 4,292-pass/one-failure run exposes a GUI test assuming every cancelled
batch writes a file. Kernel cancellation can precede output-directory creation;
test-only376a528 requires either an exact saved report or that specific early
cancellation with no output. Two affected reruns pass; combined unique scope is
4,293 passes/124 declared skips/three strict scientific xfails. Preserve both
observations and do not call this a second full green run or candidate qualification.

Preserve previous executables and their known identities before cache reuse.
Package once per completed consumer slice; regenerate stubs only if the public
Python contract changes. Use relative typing includes and require actual file
analysis. Enable the declared GUI cohort before the installed run; require exact
executed case identities and capability semantics, not a larger count constant.
Observer/fixture corrections reuse unchanged products where applicability holds.
After B5/B6, select the next remaining reader/interchange/spatial/automation
workflow from SCI1-B; scientific and release criteria remain unchanged.

For B4, extend the existing `RawPixelTiledPredictionRequest` capture and
`RawPixelPredictionStream` instead of introducing another sampler. The worker
feeds one borrowed block at a time into the builder, releases it, and records
only complete stream input/result commitments with the immutable geometry.
Use the existing contour job's owner/future/cancel/stale pattern. Admit the
captured producer work plus the larger of boundary and geometry phases before
IO; reserve the complete retained result and anticipated IDs before mutation.
A result must match its exact live capture and fresh source/model/image/revision
checks. Historical QWS1 conversion remains historical until verified through a
fresh request. Test direct/job parity, real TIFF, late source changes, cancellation,
wrong capture, copies, threads and atomic apply/undo/redo before thin bindings.

The assembler intentionally accepts the producer's ordered windows; arbitrary
reordered transport is not a new requirement. The oracle checks mathematical
order independence separately. Sparse ROI traversal remains a separate
optimization unless the chosen workload demonstrates it is a prerequisite.
Do not rebuild a wheel or dispatch a broad matrix for a private native kernel
or documentation update alone.

Keep one complete workflow active. Eligible reader expansion, annotation and
interchange, spatial analysis and recorded automation remain in SCI1-B. If a
concrete prerequisite blocks contours, record it and select the next ready
workflow from that queue. Preserve every M0–M7 requirement and the open-source
reader/codec policy; sibling-project agent policies and frameworks do not apply.

### T4 execution tasks: finish consumers before another qualification

Planning reconciliation through installed source `0dc916c` (2026-10-26). The tasks
below refine SCI1-A; they do not create additional milestones. Root owns the sequence, with no current implementation blocker. T4.1 now has
[local installed Windows acceptance](benchmarks/raw-pixel-cli-local.json) at
`f463110`; T4.2 has [installed local evidence](benchmarks/raw-pixel-batch-local.json) at `c2c0fa0`.
T4.3-T4.5 now have [installed local Windows evidence](benchmarks/raw-frontend-local.json)
at `0dc916c`: 195 Python passes, all 17 new raw frontend cases and seven native
suites, with separate geometry-OFF and training-OFF controls. Other platforms and
T5 integrated qualification remain open. See the development state below. `202b41a` retains its
earlier Python profile.
This is not a new Astra review or integrated platform qualification.

T4.1-T4.5 have named local Windows exits. T5 preflight is complete; campaign236 has engine/portable acceptance and an
arm64 failure. Finish the scoped diagnosis and qualify the resulting repair. Keep one
implementation task active. Each task has a complete observable outcome; reuse
its healthy evidence until a consumed input changes.

| Task / outcome | Existing authority and easiest path | Required exit evidence |
| --- | --- | --- |
| T4.1 / locally verified official headless train, predict/export and verify at `f463110`; [commands](raw-pixel-cli.md) | Reuse the delivered small raw adapter to `cpp/batch_main.cpp`, following `pixel_stream_cli` dispatch, JSONL and cancellation. Reuse `RawPixelTiledTrainingRequest`, `RawPixelTrainingCheckpointJob`, `train_raw_pixel_tiled_classifier`, model/archive writers, `RawPixelStreamSaveJob` and `RawPixelStreamFileReadJob`. Keep `qupathxx_region --raw` as quantitative region extraction | Installed native executable trains on a labelled TIFF project, resumes QTC1, writes QRM1/QTT1, exports/resumes QWS1 and verifies it. Compare rows, fixed-parameter model bytes, blocks and commitments with the installed Python workflow. Exercise Unicode paths, invalid arguments, backend OFF, cancellation and destination/source aliases |
| T4.2 / locally verified raw classification across project images at `c2c0fa0`; [guide](project-raw-pixel-batches.md) | Reuse the extension of existing `ProjectBatchKind`, `ProjectBatchOptions`, `ProjectBatchRequest` and kernel with an additive raw-classifier kind and typed model/lattice policy. Reuse selection snapshots, directory lock, owned bundle publication, checkpoint seal and Stop/Continue/resume reports. Add typed QWL1 extraction alongside RGB checkpoint records | A mixed project records success and incompatible-image failure correctly; Stop/Continue, exact reuse, changed inputs/model/policy and moved output are covered. Verify every reused stream, retain raw links in output projects and reopen them. Original project bytes and existing RGB/raw-measurement behavior remain intact |
| T4.3 / locally verified installed Windows at `0dc916c` | Reuse the delivered shared `RawPixelFrontend`, ImGui panel and managed-viewer ticket commands over existing owner jobs. Capture actions on the owner thread; poll bounded status; use an owning, bounded window preview and historical file/block browsing | Real interactions train, preview, cancel/retry, export/resume, retain and reopen. Edits, source changes, image switches and workspace replacement refuse stale delivery. Shutdown joins workers before readers release; installed GUI and managed viewer both exercise the path |
| T4.4 / desktop and managed bounded apply locally verified at `0dc916c` | Reuse `RawPixelPredictionRequest`, `RawPixelContourJob` and existing document commands for admitted full-grid results. Recompute a fresh owner-bound prediction when starting from historical output | Preview/apply/undo/redo is one atomic edit with rational axes preserved; stale capture, cancellation and oversized requests refuse without mutation. Label the supported bounds explicitly; a QWS1 stream does not provide stitched-contour authority |
| T4.5 / named Windows integration locally verified at `0dc916c`; T5 remains open | Freeze frontend contracts, regenerate stubs if public APIs changed, package once and run a named TIFF train/predict/save/reopen/resume cohort through Python, native CLI, batch and desktop. Reuse the pinned SDK and maintained producer/auditor profiles | Record exact installed origin, ABI, capabilities, source fingerprint, executed case names and packaged notices. Include backend-positive behavior and OFF refusal, legacy RGB/persistence regressions, typing, array lifetime and shutdown. Claim only profiles actually exercised; then schedule a T5 campaign only at a release checkpoint or for a demonstrated platform risk |

#### T4.3-T4.5 installed acceptance and next qualification

The [frontend receipt](benchmarks/raw-frontend-local.json) binds source `0dc916c`,
383 compiler inputs, 199 raw payloads and 26 retained final/development products.
Its Windows cp312 GUI/GEOS/training/TIFF/project-ON, OpenSlide-OFF wheel passes
195 Python cases, including all 17 raw desktop/managed cases, and seven native
suites. Three named skips cover OpenSlide and the separate training-OFF profile.
Four positive typing files and twelve negative errors across three files have
exact/nonempty analyzed-file guards; generated stubs match the installed module.

Separate geometry-OFF/training-ON and training-OFF native profiles bind the same
compiler inputs. OFF geometry is refusal evidence. Training-OFF native/CLI work
refuses training before files appear and saves/reuses an actual ON-trained model;
27 blocks match the installed ON consumer. Two further installed active-contour
cancel/close probes observe running work, prevent mutation, reclaim results and
release source readers. Initial scratch-budget fixture rejections are preserved;
the successful probes pre-admit their grid instead of bypassing memory limits.

Managed contour apply recomputes fresh owner authority. Returned geometry is
historical; optional expected revision is checked before capture, and the live
request rejects later changes. Reserve the conservative retained snapshot/model/
prediction/geometry payload and anticipated IDs before calling the existing
atomic edit. Desktop controls preserve rational axes/context, bounded class-
colored geometry preview, explicit limits and one-command undo/redo. Renaming
the annotation prefix alone does not require another prediction/trace.

Wheel membership, source/capabilities, loaded GEOS DLL paths, notices, 195 OpenCV
SDK files and 559 GEOS SDK files authenticate. The fresh package reuses workspace
test dependencies, so no clean-machine, other-platform or release claim follows.
This closes the named local T4 consumer outcomes, not T5 or the full milestones.

**Current execution:** [campaign236 closeout](benchmarks/raw-classifier-236-campaign/closeout/receipt.json)
accepts engine and portable roles at `530c6bb`. The remaining Mac arm64 replay
failure is under [diagnostic238](benchmarks/raw-classifier-236-campaign/macos-diagnostic-238/receipt.json),
run `40377716087` at `c3c19a9`. Keep both sources frozen and preserve every failed
attempt. Scope-matched Mac and final integrated acceptance remain open.

The
[preflight at `fb3dfe6`](benchmarks/raw-classifier-preflight-local.json) makes the
raw native/installed/GUI/typing/source contract mandatory in the producers and
all three auditors. It requires 58 exact raw cases, explicit headless and
training-OFF declarations, actual enabled native semantics, and late source
rechecks across 459 inputs. All 515 selected admission/auditor controls pass;
legacy diagnostic commands remain intact. Four strict-mypy positive files pass
and twelve intended errors reject. The example now transfers pending results
outside assertions and has real normal/`-O` resume/retention regressions.

Actual minimal native checks passed on Windows; GUI-enabled installed captures
exercise both desktop opt-in states. These are preflight observations, not a
new headless wheel, Mac/Linux/sanitizer or integrated acceptance. Preserve every
inherited cohort, fault contract, marker and receipt rule. Use full engine,
both-architecture Mac-native and both portable builder/fresh-consumer inputs;
track each original run to terminal state, then authenticate/audit all artifacts.
No project milestone or biological/resource/hardware/release gate closes here.

The development repair notes below are historical context, not an active queue.

#### T4.3 development state and repair order

Historical context: implementation `b5df7e1` and correction `a01b6bc` initially
passed native checks but failed three desktop interactions. Temporary trace
`c8331a4` identified missing prior-frame hover in replay. Actual move/down/up
fixed the interactions; tracing was removed. `10f44cc` passed 48 selected Python
cases and seven native suites; `a8e7eaf` added active cancel/retry/shutdown checks,
and `6a1bb0e` added actual-source-version refusal and desktop edit retirement.
All original failures and their available products remain in the receipt's
source-bound history, including the initial incorrect QWS footer comparison and
stale-epoch fixture. These are preserved observations, not tasks to repeat.

#### T4.4 bounded contour consumer wiring

The adapter began at `58f1241` and is wired/installed at `0dc916c`. It captures
`RawPixelPredictionRequest`, runs the existing full-grid prediction job and
`RawPixelContourJob`, then guards `publish_raw_pixel_contours` for one edit.
`ScientificGrid` retains both rational axes/context; excessive grids refuse
before reader IO. This is bounded full-grid apply, separate from indexed-window
preview and stitched streamed contours.

Desktop controls provide explicit inference/geometry limits, class-colored
preview and separate apply. Edits, axes/parameter changes and image/workspace
changes retire unpublished authority. The prefix alone can change without
retracing. Managed commands copy model/grid/options, reserve retained results
before mutation, and capture/recompute fresh authority for apply. Historical
geometry is data, never an apply token. Installed real interactions, source/
revision/cancellation/refusal controls, persistence and active shutdown now have
the local evidence above. The remaining work is T5 preflight/qualification and
the broader original scope, rather than another consumer implementation.

#### Shared input contract: settle in T4.1 and reuse in T4.2-T4.4

- **Command surface.** Native batch verbs `--train-raw-pixels`,
  `--export-raw-pixels` and `--verify-raw-pixels` are locally verified in the
  installed Windows profile. Project kind `raw-pixels` now has local installed Windows evidence at `c2c0fa0`. Keep current
  `--kind raw` measurement semantics and RGB `--kind pixels` unchanged. Use the
  existing exit convention: 0 success, 1 failure/partial, 2 arguments and 130
  cancellation. JSONL must distinguish durable partial output from completion.
- **Recipe and model.** Load the existing versioned recipe/model representation.
  Reuse the installed example's explicit intensity recipe as the first tested
  configuration; expose channel order, normalization and supported feature
  options without duplicating the feature kernel. Record effective forest
  parameters, including admitted work budget, for reproducible model identity.
- **Lattice and axes.** Carry both exact rational axes, global sample origin and
  coverage, channel order, level, Z/time/series and calibration policy. Validate
  each image against the recipe/model before decoding. A fixed samples-per-image
  count can change spacing across image sizes; it must not silently redefine a
  trained model. Reject incompatible images according to Stop/Continue rather
  than rounding, rebasing or using display controls as quantitative settings.
- **Budgets and progress.** Reuse tile/feature/forest/file/snapshot limits; keep
  one result slot and bounded previews, never collect all stream blocks. Report
  completed/total work, inferred versus replayed/verified work, output path,
  model/request identity and terminal state using the native statistics. Empty
  or partial training checkpoints produce no model until the table is complete.
- **Persistence and reuse.** QTC1 resumes by replaying prefix IO/features;
  QWS1 prediction resume hashes previous inputs and infers the suffix. Resume
  is not constant time. Reuse QTT1/QWL1 and the existing checkpoint seal/table;
  introduce no second Python batch loop, checkpoint database or file-ownership
  scheme. Check the existing bundle cleanup's owned extensions and exact paths
  when adding raw files. Missing outputs remain historical metadata, not PASS.
- **Publication and trust.** Use existing alias guards, conditional atomic file
  commits and owner freshness checks. A committed file can survive late cancel
  even when live delivery is refused. Full file verification confers no live
  document/apply authority. Keep historical file browsing separate from fresh
  prediction and document commands.
- **Licensing.** Every reader and required decoder must satisfy the open-source
  admission policy, including optional/process-isolated adapters. Unknown or
  restricted licenses block that adapter; eligible TIFF/OpenSlide workflows
  continue. Copyleft remains eligible; fixture rights and release obligations
  retain their separate gates.

#### Build and acceptance budget

For the next implementation, admit the
actual interpreter ABI, CMake capabilities, SDK manifest and pytest environment
before building; use the explicit CPython 3.12 environment for the current
Windows wheel. Retain the existing caches and build only affected targets first.
Join every live build before editing compiler inputs. Use unique result paths,
exact executed-case guards and nonempty typing checks. Preserve failed attempts.

Run focused native and installed command tests for T4.1/T4.2, then affected
GUI/managed-viewer controls for T4.3/T4.4. Keep the GUI/backend-enabled profile
explicit: the accepted Python wheel has GUI/GEOS/OpenSlide OFF and cannot prove
those capabilities. T4.5 packages the settled combined workflow; do not repeat
full platform campaigns for each command. T5 retains every required profile and
inherited cohort; no earlier multi-file campaign qualifies this newer source.

Remaining automatic maintenance, abrupt-process/power-loss behavior, streamed
contour stitching, independent scientific comparisons, hardware/resource budgets
and release support stay in their existing queues. This ordering does not remove
any M0-M7 requirement or select a project license.

### T4.1 installed CLI acceptance and next batch task

The [CLI receipt](benchmarks/raw-pixel-cli-local.json) binds `f463110`, 376
compiler inputs and eight retained binary/wheel products. The installed Windows
cp312 training/TIFF/project-enabled, GUI/GEOS/OpenSlide-disabled wheel passes
104 Python cases, including 14 new CLI cases, plus three native suites. The one
backend-OFF-only Python case skips in that ON package; a separate OFF native
executable refuses training before output and preserves its valid input project.
Source/runtime/wheel membership and the 195-file SDK manifest authenticate.

The initial budget coupling rejected valid small producer requests; keep producer,
checkpoint metadata, prediction-file metadata and forest limits independent.
Original collection, fixture and comparison failures remain source-bound evidence.
Resume preserves exact committed header/block bytes. Fresh process provenance
includes clocks/generations, so compare independent numeric blocks and input/result
chains rather than pretending independently captured whole files are identical.
Optional prediction project retention currently re-verifies the file; this extra
bounded IO is explicit and is not a speedup claim.

T4.2 now has the later acceptance below over the existing batch kernel.
Preserve raw measurement and RGB kinds, exact per-image model/lattice admission,
checkpoint seals, owned bundle publication and Stop/Continue semantics. Add typed
QWL1 reuse/readback and changed/missing-output controls. Then deliver desktop and
bounded apply before the installed integration/T5 freeze. Broader platforms,
clean-machine, scientific/resource/hardware and release gates remain open.

### T4.2 installed project batch acceptance and next desktop task

The [batch receipt](benchmarks/raw-pixel-batch-local.json) binds `c2c0fa0`, 377
inputs and twelve final/original products. The installed Windows cp312 profile
passes 136 Python cases (all 14 new raw-batch cases) and four native suites.
Five explicit profile skips cover OpenSlide, geometry, GUI and OFF-only behavior.
A separate backend-OFF native suite passes; its CLI saves and reuses an actual
trained model and matches 27 numeric blocks against the ON consumer. Stubs and
three valid/seven invalid typing checks pass with exact analyzed-file counts.

Reuse existing snapshots, owned bundle publication and checkpoint seals. Raw
and RGB catalogs share the same table with typed tag selection; charge raw
archive/link storage to materialization budgets. Completed-image reuse hashes
current raw inputs and fully verifies streams/descriptors. Native interruption,
missing/tampered outputs, current-source changes, Stop/Continue, quotas, no-resume
preservation, relocation and fractional async captures have distinguishing tests.
Each target still traverses its declared global lattice; sparse ROI prediction
planning and process-memory/performance acceptance remain open. Prefix resume is
separate from completed-image batch reuse.

T4.3 now has an unqualified development implementation through a shared owner
adapter, ImGui panel and managed-viewer commands. Follow the
[remaining order](#t43-development-state-and-repair-order): preserve passing real
interactions, finish mutation/source/lifetime coverage and retain the accepted batch baseline.
Keep historical browsing separate from fresh prediction/apply authority.
Then finish bounded apply and installed integration before T5. No new reader,
license, biological or supported-platform claim follows from this local result.

### Execution cost controls

Use affected local native suites on the existing Windows/Linux caches and the
pinned Windows backend-enabled configuration. Add actual backend-positive cases
as well as backend-OFF refusal; an OFF build cannot prove training. Keep failed
attempts with their source/configuration and freeze compiler inputs until all
live builds finish. Derive every JUnit path from its unique run directory and
preflight it before execution to prevent overwriting earlier evidence.

Advance after the packet's decisive checks pass; do not repeat healthy tests to
recreate logs or rebuild wheels for each numeric substep. Prepare the next
consumer contract while its producer is implemented, then perform one installed
vertical-workflow pass when the consumer changes. Defer integrated platform
campaigns to release checkpoints or demonstrated cross-platform risks. Reopen a
passed check only for changed inputs, failure or a specific unresolved concern.

Bounded whole-grid admission does not complete large-slide execution. Preserve
global rational phase; do not round fractional origins into scalar tiles.
Named installed Windows frontend outcomes are locally accepted. T5 broader
platform acceptance remains open. Stitched cross-tile contours retain their
separate workflow gate; do not rebuild delivered T4 consumers from older notes.

| Checkpoint / dependency | Smallest implementation and API path | Decisive verification / stop condition |
| --- | --- | --- |
| A1 / before public header changes | Trace raw sampler → feature schema → training table → forest → prediction → archive/checkpoint. Specify channel order, dtype conversion/normalization, finite/missing support, grid/halo/boundaries and sampled-pixel versus physical sigma units. Choose an additive versioned recipe with explicit rejection of unsupported combinations | A compact contract and independent tiny-array expected values. Include signed/unsigned 64-bit extremes, subnormals, nonfinite/channel-specific missing data, anisotropic and unknown calibration. Declare rounding/error bounds before implementing; never silently narrow raw values into RGB8 or float32 |
| A2 / A1 numeric contract | Implement the native quantitative feature kernel beside the existing pixel feature authority. Reuse `ScientificRawGrid` sampling, checked work admission, owned output, cancellation and current filter primitives only where semantics match. Start with the declared channel-intensity/smoothing/derivative set; keep OD/stain features tied to a valid explicit recipe | Compare each feature with an independent numeric oracle; prove channel/schema order, missing support and halo behavior. Test exact work limits before IO and bounded multi-block behavior. Finish these checks before binding/package work |
| A3 / A2 stable schema | For raw data reuse the implemented `RawPixelTrainingRequest`/`RawPixelPredictionRequest` and controllers; retain RGB entry points separately. Extend phase-preserving windows/stream execution and qualify the actual native forest training backend. Reuse native snapshots and model identity. Keep any needed raw-specific type local until the consumer trace proves where it belongs | Train/predict on the same captured source; compare streamed and full-context outputs at seams/edges. Cover mixed channel/grid/model mismatch, deterministic predictions, changed sources and cancel-before-publication without document mutation |
| A4 / A3 identity | Reuse locally verified archives, retention, replay and bounded apply; add stream/checkpoint support through the existing authorities. Include feature recipe/version, source dtype/channel/grid/calibration, missing-data policy and model/schema identity; reuse source guards and atomic writers | Save/reopen/replay and interrupted/resumed prediction match the declared result; tampered/mismatched recipes reject. Old RGB archives retain their original semantics. Hash-only resume avoids unnecessary feature-plane materialization |
| A5 / A3-A4 contracts stable | Bind the native request/result with nanobind and reuse immutable owning NumPy views. Add the workflow to existing CLI, batch, managed viewer and ImGui train/predict/apply actions. Generate stubs once from the final installed module | One example trains, predicts, saves/reopens and resumes through Python and headless paths; actual desktop preview/apply is undoable and rejects stale results. Verify dtype/strides/lifetime, typing and cancellation; frontend code owns no independent feature algorithm |
| A6 / complete A1-A5 outcome | Preflight producer → manifest → positive fixture → auditor closure and clean-checkout input availability. At the next release checkpoint or demonstrated platform risk, freeze once and run the applicable integrated platform profile. Attach biological/model comparisons only to eligible labeled references | Exact named native/installed/desktop/persistence cohorts and all inherited gates pass with source/configuration pins. Engineering verification, scientific acceptance and release support remain separate states |

A1-A6 are checkpoints within one train/predict workflow; do not dispatch six
qualification campaigns. Stitched contour extraction remains a separate next
workflow and is not silently claimed by a correct streamed probability grid.
Reference preparation is limited to the next checkpoint's inputs. An unavailable
biological corpus blocks its comparison, not the independent numeric kernel.

## Analysis baseline to preserve

`SampledLogWatershedBodies` is already a production entry point. The pinned NDPI
comparison records 503 nuclei with zero context or 478 with 32-pixel context,
against 506 in the upstream reference; these are source/ROI/recipe observations,
not universal performance or biological acceptance. The older 299/506 LoG
characterization remains historical evidence and must not be presented as the
current sampled-recipe result. See [sampled recipes](sampled-cell-recipes.md) and
[NDPI comparisons](qupath-ndpi-cell-comparisons.md).

The shared **scientific sampled-grid preparation** now supports bounded
level-zero RGB cells through one request/result/archive workflow. Reuse
`CellRequest.for_grid_roi` or the explicit `ScientificGrid` constructor, typed
`GridCellParameters`, per-axis raster edges and captured provenance. The native
request, archive/classifier/CSV consumers and Python/CLI/viewer/batch/ImGui paths
pass local engineering checks; the integrated platform campaign passes at source `3fc3bcb` within its declared scope.

For remaining tissue/classifier/raw workflows, extend this contract with an
explicit source dtype, natural-level/tiled sampling policy and channel semantics
before adding adapters. Preserve origin/extent, axis units, halo/context, ROI and
alpha/missing-data policy, pre-IO work limits, algorithm identity and replay.
Use independent numeric and biological references and preserve legacy defaults.

General RGBA8 region resampling remains a separate display-oriented operation.
It does not preserve typed quantitative TIFF values or establish ICC/linear-light
semantics. Bounded typed raw scientific sampling now passes source191
engineering qualification. Vendor/pyramid/tiled admission and independent
scientific acceptance remain explicit requirements; reuse filters only where
their contracts match.

Preserve the implemented typed raw workflow from `RawRegionRequest`/`RawPixelBuffer`
and compose through `RawAnalysisRequest`, its job, immutable result and record
path. It covers all ten exposed `SampleType` values. Preserve strict footprint support
and per-channel finite validity before adapting consumers: missing data is not
a measured zero, and one channel's NaN must not silently erase another channel.
Use an independent `Fraction`/binary64 oracle for weighted means, including
UInt64/Int64 extremes, subnormals and cancellation. Do not cast 64-bit integers
to double before their exact weighted sum or use an overflowing floating
numerator for a finite mean. Preserve source dtype/bits, series/Z/T/channel
selection, calibration and request authority in the versioned recipe; retain
legacy raw behavior. Retain the accepted retention/replay, CSV, resumed batch
and UI/Python/CLI delivery while extending reader/large-slide support. Require
new source-bound qualification for changed adapters.

## Implemented TMA desktop pattern to reuse

This pattern already powers the accepted TMA workflow. Reuse it for new imports;
do not schedule its existing controls/parser/model as unfinished implementation.

1. Add UI controls adjacent to the existing TMA panel; preserve numeric values
   of existing replay targets by appending new targets. Use one bounded path
   input and separate **Load preview**, **Apply**, **Export** and **Cancel** actions.
2. The owner captures document identity, image identity, revision and relevant
   grid state when loading starts. A worker performs only `load_qptma` and
   bounded preparation; it does not mutate documents or retain Python callbacks.
3. Poll completion without waiting on the frame thread. Show rows/columns,
   core/present/missing counts, bounds and the intended target image, with a
   clipped preview. Source hints are informational. Validate geometry against
   the current image and report unsupported/lost metadata concretely.
4. Apply only after an explicit user action. Recheck captured identity/revision
   and existing-grid condition; discard stale previews. Run one existing compound
   import command. Existing grids need an explicit replacement policy, never
   silent deletion. Initial path may require clearing explicitly as the API does.
5. For export, capture an immutable `TmaGrid` on the owner, compute its omission
   report and show the affected fields before publication. Worker IO uses
   `save_qptma`; a vacant slot is rejected with a fixable explanation. Preserve
   existing destinations and all image/project inputs.
6. Cancellation drops pending publication and preview; uninterruptible synchronous
   IO may finish privately. Bound retained previews, text buffers and retirement
   cost; close/image-switch while loading must be safe. Avoid creating a job
   framework just for this panel; reuse its current async conventions.
7. Replay the complete workflow, errors, stale target, close/cancel and undo/redo.
   Save/reopen the multi-image project, compare exported bytes/values and inspect
   a completed desktop capture. Apply the change-based validation policy; a full campaign is reserved for a release checkpoint or a demonstrated platform risk.

## Scientific and performance decision lane

| Decision/evidence | Owner to supply it | Work that can proceed now | Acceptance constraint |
| --- | --- | --- | --- |
| Intended research workflow and specimen/annotation corpus | User/research lead; implementer records identifiers and rights | Inventory existing public/synthetic fixtures and exact missing cases | Do not call engineering fixtures biological validation |
| Numerical tolerance and algorithm/grid contract | Research lead plus implementation review | Characterize current outputs, retain failures and define candidate metric before evaluating new output | Never weaken strict xfails or tolerances to obtain a pass; distinguish intentional algorithm differences |
| Physical calibration semantics | Implementer proposes, research lead reviews intended interpretation | Specify axes, grid origin/spacing, kernel units, halo and archive versioning; test unknown/anisotropic cases | No geometric-mean shortcut or unrecorded resampling recipe |
| Hardware/OS/cache workload | User supplies target or approves a concrete proposed matrix | Prepare repeatable workload/harness, capture machine and environment metadata | Prior ineligible pilot remains ineligible; component pilot is not a frame/RSS gate |
| Project license and distribution scope | Repository owner | Inventory dependencies/notices and existing package obligations | Do not select a project license or publish a supported release implicitly |
| Supported platform/package matrix and signing | Repository owner with platform evidence | Report native/wheel/GUI/reader capability separately | Finish clean-machine workflows and distribution requirements for every claimed support cell |

Track these as explicit unresolved decisions with an owner and the next evidence
artifact; don't repeatedly ask for a decision already answered in the session.
Missing decision input blocks only work that truly depends on it. Keep useful
independent implementation moving after the user resumes the goal.

## Evidence and source policy

Every slice has one record: requirement/workflow ID, source SHA, input/fixture
identity, platform/configuration, capability flags, command, artifact location,
semantic assertions, failures/skips, applicable performance/science limits and
remaining gates. Link existing raw evidence instead of copying huge JSON into
multiple status files. Source identity must distinguish code, fixture, harness,
compiled engine and documentation when they differ.

A normal development gate is the affected local workflow check. A frozen
full-matrix qualification is reserved for a release checkpoint or demonstrated
cross-platform risk, as defined by the validation policy above. Mode-aware concurrency keys now let engine/Mac/portable runs
share one frozen ref without cancelling one another; all three source169 modes
passed their full required audits. Portable qualification must enable extended
readers, as recorded by the corrected accepted run and retained under-scoped
attempt in the [combined receipt](benchmarks/scientific-grid-169-qualification.json).
Stop and inspect a live run after uncertainty; don't redispatch
or modify its source. New compiler/behavior changes invalidate affected evidence;
prove which configurations are unaffected rather than assuming. Never substitute
a future run's tools when auditing older artifacts. Publication of candidate
branches is not promotion and promotion is not supported-release acceptance.

Freeze scientific/reference inputs early and the integrated release candidate
late. An auditor-only fix first gets its own negative controls and a declared
compatible artifact schema/source contract; re-audit retained artifacts with both
engine and auditor identities recorded. This may avoid recompiling unchanged
binary inputs. It cannot let newer tools silently qualify an older incompatible
protocol or transfer source-bound evidence to changed binaries. Documentation-only
updates need documentation checks and input-identity proof, not a duplicate
11-job platform campaign. Keep the existing required final promotion gates.

When changing auditors, verify that omitted cases, disabled capabilities,
malformed outputs and mixed-source artifacts reject. Replace hard-coded suite
counts with semantic contracts in touched paths while preserving inherited gates.
Do not bulk-refactor all evidence tools before delivering the next user workflow.

Review and update this queue after every task using the task closeout and waste review, including failed or blocked attempts. Preserve M0–M7 criteria and
reference histories, update statuses at the defined fixture scope, and identify
the next blocking dependency. Do not claim full completion until the complete
requirement-to-evidence audit passes.

## Complete qualification campaign inputs

Use these commands only when a full campaign is justified under the validation
policy. They are not the next step for every delivered workflow. Existing308
runs need collection and audit, not another dispatch.

Use the frozen ref for every gate. Generate checked CLI input maps before
sending a workflow request; the helper performs no network action:

```text
python tools/ci/campaign_inputs.py engine | gh workflow run ci.yml --repo bipxx/qupathxx --ref FROZEN_REF --json
python tools/ci/campaign_inputs.py macos | gh workflow run ci.yml --repo bipxx/qupathxx --ref FROZEN_REF --json
python tools/ci/campaign_inputs.py portable | gh workflow run ci.yml --repo bipxx/qupathxx --ref FROZEN_REF --json
```

The complete engine profile enables live reference and extended readers. The
portable profile also enables extended readers: its native NDPI replay and
builder reader cohorts are required by the inherited auditor. A green portable
build without that input covers a smaller scope and cannot qualify the full
campaign. The Mac profile selects native-only builds. `gh --json` receives
string-valued maps; semantic values are validated as booleans before encoding.
Record the source, profile, attempts and exact resulting run IDs before polling.
Keep under-scoped attempts as evidence and repeat only the affected gate.


## Quantitative storage-layout integration after the raw-grid candidate

Reuse `read_scientific_raw` and `RawAnalysisRequest` for quantitative footprints;
no per-layout frontend or second sampler is needed. The
[local 30-case integration receipt](benchmarks/raw-grid-tiff-layout-local.json)
checks supported tiled/striped layouts, SubIFD level-zero authority, OME axes and
series selection against independent exact means. It extends the next named
installed scientific contract to 248 cases (243 local headless passes plus five
declared desktop skips). Existing source745662b campaign evidence stays bound
to its original contract. Next close the actual remaining large-window/vendor/
whole-slide and multi-file OME capabilities, preserving explicit limits and
source identity; these synthetic fixtures do not establish those capabilities.

### Scoped M6 Document-wait delivery / locally verified270

[Receipt270](benchmarks/document-waits-270/receipt.json) records native c949826 /
stub-and-contract observer f068594. Header inspection extended the initially
named six controllers to pixel training/prediction/contour waits whose arguments
were positional. All nine now use one shared binding ownership/join helper: live
Document checks/capture/delivery hold the GIL, only native readiness releases it,
and applicability is rechecked afterward. Python controller calls are owner-only
except cancel; destruction can retire on any thread without holding the GIL.
The native C++ readiness primitive does not inspect or publish Document state.

All55 native suites,72 new controls,240 separate legacy passes/five reader or
pyramid skips,305 exact GUI cohort passes/one declared skip and1000 auditor
controls pass. Seven positive typing files and five negative files with28 exact
diagnostics verify; only one stub payload/RECORD changes after compilation, with
all410 compiler inputs unchanged. Preserve native/observer identities and avoid
rebuilding for contract or documentation edits. Next freeze one combined source
and run every applicable complete platform role once. Full M6 compatibility and
thread-safety scope, scientific/resource/hardware/support/release gates remain.
