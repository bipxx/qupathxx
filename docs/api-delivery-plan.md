# API and workflow delivery paths

This is the forward execution companion to [the full M0–M7 plan](project-plan.md).
Planning update: 2026-10-25. “API” here means a project entry point or workflow, not a credential. No service
API keys are required for the current native application. Names below are
existing entry points; proposed behavior is identified as remaining work. The
canonical signatures remain the public headers and generated Python stubs.

## Problems this revision addresses

| Observed inefficiency | Evidence | Replacement |
| --- | --- | --- |
| Historical packet log obscures the next action and contradicts current status | The previous active plan mixed thousands of lines of earlier packets with a strict M0→M7 order; inventory still named old main `82e159f` after public main reached `f2e8c2f` | Keep packet history in a separate file; one active queue and an updated workflow inventory |
| Horizontal implementation produces repeated “candidate complete” reports without finishing a researcher task | TMA progressed through reference, codec and project commands while desktop import/export remains missing | Accept complete vertical workflows; substeps remain visible but do not substitute for UI/Python/CLI/persistence completion |
| Repeated full qualifications and wheel rebuilds consume time before interfaces stabilize | Sources 152–154 used successive Mac candidates plus engine/portable qualifications; stubs required additional packaging passes | Local affected tests first; one installed integration pass; freeze a completed candidate and run the required full matrix once unless code changes/failures require another run |
| Passing suite counts can conceal missing behavior | QPTMA project native test initially followed a disabled compile-flag branch in a project-enabled build | Require runtime capability and semantic markers, exact relevant cases and fault outcomes; counts are supplementary |
| Repeated hand-written artifact checks create inconsistent rules and fragile suite counts | Separate engine/Mac/portable scripts embed suite counts and domain-specific imports; QPTMA/resampling each add source/cohort helpers | Share small evidence primitives incrementally and retain domain-specific contracts; test omission, disabled capability and mixed-source rejection |
| Scientific and release decisions remain open while engineering packets accumulate | M0 tolerances/rights/hardware and project-license choice persist in the plan; legacy scientific xfails remain | Maintain a decision/evidence lane with named prerequisites and unblock the next research workflow early |
| Worktree/build proliferation increases stale-source and wrong-environment errors | Many comparison/qualification worktrees; QPTMA reinstall once used a wrong relative path and tested old stubs | One active integration source, one frozen candidate and explicit optional comparisons; absolute artifact paths, fail-fast commands, compiled-source checks and retirement criteria |
| Local performance pilots can look like overall performance completion | Identity resampling pilot measures only the filter call; an earlier rendered hardware pilot was ineligible | Preserve component results and close separate decode, frame, memory and full-workflow budgets under frozen conditions |

These are process findings, not a claim that earlier evidence was worthless.
Preserve accepted receipts, raw captures, negative controls and failed attempts.

## Dependency lanes and immediate queue

There is one implementation queue. Independent decisions/reference preparation
can proceed alongside it; implementation was explicitly resumed on 2026-10-25.
This does not authorize autonomous worker delegation beyond the existing rules.

| Order | Work item | Reuse and smallest complete step | Exit evidence |
| --- | --- | --- | --- |
| 1 | Project-enabled TMA build validation: locally verified | Install the already-built `qptma156-final-wheels` artifact in its isolated environment; inspect compiled source; run the 80-case QPTMA contract, existing project save/recovery tests, stub regeneration/check and positive/negative typing | No skipped project cohort; actual native project marker; final wheel identity and packaged stubs match; failures retained |
| 2 | TMA desktop workflow: locally verified | Reuse `GuiTma`, `load_qptma`, `qptma_grid_edits`, `import_qptma`, `qptma_from_grid` and `save_qptma`; background IO plus owner-thread preview/apply, not a second parser/model | Import → preview → apply → edit → undo/redo → project save/restart → export with error/stale/cancel coverage and visually inspected capture |
| 3 | Integrated TMA source: accepted and promoted to main | One clean candidate; full engine/Mac/portable evidence with project-enabled QPTMA contract and inherited gates | Matching source/artifacts, all required configurations and concrete workflow outputs; promotion only then |
| 4 | Annotation/migration: local engineering gates pass | Shared nested export and Windows physical recovery paths pass 50 native suites, 305 installed cases, typing and actual QuPath parent/nucleus/point comparisons; three local host-capability skips remain declared | Source-bound 101-case headless / 105-case GUI contracts and 43 auditor controls now pass locally; freeze this integration and require engine/Mac/portable acceptance |
| 5 | Close sampling and scientific analysis gaps | Specify physical grid semantics first; reuse region filter/requests and archive recipes only when equivalence is proved | Independent masks/features and split/merge results under predeclared tolerances, including anisotropic and unknown calibration |
| 6 | Complete research and batch workflow | Compose existing analysis/classifier jobs and checkpoint batches; build missing coverage/adapters only after native contracts are stable | Same selected workflow in UI/Python/CLI, interrupted-resumed batch correctness and bounded resources |
| 7 | Close release and remaining inventory requirements | Build on accepted packages; choose support matrix, license/notices policy, installers and clean-machine scripts | Every M0–M7 exit has scope-matched evidence; remaining scope is explicitly scheduled, not silently dropped |

Steps 4–7 do not prohibit fixing a concrete defect found in an earlier step.
A blocker in one lane is not permission to replace the original goal with a
smaller deliverable. Keep the original M0–M7 requirements and deferred boundaries.

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

For the current TMA slice, the outcome is a researcher importing a QuPath grid,
reviewing it on the selected image, editing it, saving/reopening the project and
exporting back with supported fields preserved and losses displayed. Native
codec, project CLI and GUI should share that one acceptance record. Its local installed project-enabled and desktop gates pass. Its next blocking
evidence is exact-source hosted engine/Mac/portable acceptance; do not start a
new parser or binary format while those runs remain live.

## Detailed route for each API family

“Shortest path” means reuse the existing authoritative implementation and avoid
parallel models or unnecessary rewrites. It does not waive the required final
behavior or testing. Each row describes what to do next, not a declaration of
full parity or release acceptance.

| API/workflow and existing entry points | Shortest path and shared implementation | Remaining work and decisive acceptance |
| --- | --- | --- |
| Image open/metadata: `open_slide`, `open_tiff`, `ImageServer.metadata`, `read_region`, `RegionRequest` | Extend the reader adapter/capability declarations for an identified format; retain RAII, normalized metadata and shared region validation. Reuse pinned OpenSlide/quantitative TIFF paths before introducing a new decoder | Add vendor/axis/damage/calibration cases to the reader corpus; verify actual pixels and independent metadata, pending-close behavior and bounded reads. Broader Bio-Formats/multifile OME stays tracked |
| Resampling/overview: `read_resampled_region`, `resample_area`, `plan_thumbnail`, `ThumbnailJob` | Use the accepted integer area filter and natural-level planner. Choose grids explicitly; do not implement another resize in Python or ImGui | Real-vendor grids and downstream scientific recipe integration; identity/alpha/padding and quota behavior must hold. Device-space filtering does not supply ICC/linear-light semantics |
| Raw quantitative data: `RawRegionRequest`, `RawPixelBuffer`, `TiffView`, `RawAnalysisRequest`, `RawAnalysisJob` | Keep typed raw data separate from display RGB; never route quantitative channels through the RGBA8 resampler. Adapt the existing TIFF/raw request and immutable record path for missing workflows | Validate multichannel/Z/time, axes, dtype and calibration; extend raw classifier features and file formats with independent numeric fixtures and persisted provenance |
| Scheduling/cache: `TileScheduler`, `SchedulerOptions`, job `request/poll/cancel` | Add missing request classes/priority policy to existing scheduler and immutable completion paths. Reuse generation rejection and native handle ownership | Vendor stress, concurrent close/cancel, in-flight/decoder memory accounting, latency and long-session bounds. No new general scheduler unless a measured limitation requires it |
| Viewer/display: `show`, `GuiOptions`, `DisplaySettings`, `Viewport` | Reuse tiled renderer, cached overlays and owner/context-thread rules. Add ICC as a separately specified display stage with a candidate dependency/notice review | Physical monitor/DPI and GPU/OS matrix; advanced fluorescence, color-managed comparisons, dense visible scenes and eligible completed-frame timing. Raw measurements must stay unchanged |
| Objects/geometry/history: `Document`, `Roi`, `ObjectEdit`, `ObjectSelection`, `Document.execute` | Implement missing gestures as compound existing commands; extend GEOS adapter and revision caches only where fixtures expose a gap | Hierarchy/interchange edge cases, multipart/holes and calibrated features, selection/edit interaction, rollback and undo/redo. Verify stable IDs and child/nucleus policy |
| Project persistence: `Project.open`, `save`, `save_new`, `ProjectSaveJob`, autosave/recovery | Finish and test new-file publication using the existing SQLite snapshot/transaction path; expose a clear distinction between update and fork/save-as. Reuse `open_without_journal_recovery` for source-preserving commands | Shared physical-path adaptation now fixes long work/snapshot/rescue paths, SQLite reopening, review/deletion and retention while preserving logical paths. Require source-bound platform evidence and retain hot-journal, destination-race, moved-link, missing-image and failure/cancel gates |
| Annotation interchange: `import_geojson`, `export_geojson`, file wrappers and `InterchangeReport` | Use the shared GeoJSON route and additive `nested_hierarchy=True` option for QuPath parent links; native file/Python/ImGui paths already share it. Extend the bounded loss/geometry corpus rather than starting direct Java serialization | The committed six-object nested corpus passes actual upstream parent/nucleus/point round trips. The recovery-path failure is fixed and the expanded local integration passes; require source-bound platform cohorts and broader edge/property contracts; direct `.qpproj`/`.qpdata` still need distinct contracts |
| QPTMA wire: `QptmaData`, `load_qptma`, `parse_qptma`, `qptma_text`, `save_qptma` | Keep the bounded native literal-TSV codec; Python passes owning values. Use actual pinned QuPath captures and native-output imports as the oracle | Finish file/UI lifecycle, byte/slot limits and malformed text cases across supported hosts. Preserve fractional importer behavior and expose upstream exporter quantization; never interpret the source hint as a file to open |
| QPTMA project/UI: `qptma_grid_edits`, `import_qptma`, `qptma_from_grid`, batch `--import-qptma`/`--export-qptma` | Use one atomic edit and new-project publication; CLI requires explicit image. Add the preview UI around these functions and preserve current project/view/image state | Final wheel + project save/restart, no source overwrite, stale/cancel preview, loss acknowledgement, owner-thread import and GUI replay. Do not create a second UI grid model |
| TMA management/case pooling: `TmaGridOptions`, `Document.create_tma_grid`, `TmaGrid`, `tma_case_summary_csv`, cohort APIs | Extend existing immutable snapshots and weighted raw-row pooling for missing workflows; keep present/missing/vacant semantics distinct | Automatic dearraying and biological/upstream coverage remain. First detector slice: supplied dimensions/core-size bounds → bounded low-resolution candidate detection → fitted grid with missing confidence → reviewed preview → same native edits, plus independent fit fixtures |
| ROI/shape/OD measurements: `AnalysisRequest`, `AnalysisJob`, `analyze`, `RoiMeasurements` | Reuse immutable ROI/calibration capture, native kernels, revision caches and retained analysis records. Add a feature descriptor only for an identified missing quantity | Independent feature tolerances, edge/hole/alpha sampling and calibrated behavior; publish/replay/export must retain exact input/recipe identity. No per-frame recomputation |
| Stains: `StainTransform`, `optical_density`, `StainEstimateRequest`, `StainEstimateJob` | Reuse supplied-vector kernels and bounded estimation reservoir; add registry/persistence UI around existing stable model hashes | Additional stain/estimation reference workflows and accepted biological tolerances; freeze white reference/vector normalization before running comparisons |
| Tissue detection: `TissueRequest`, `TissueParameters`, `TissueJob`, `detect_tissue`, `trace_tissue` | Reuse threshold/morphology/components/contours and record publication. Add physical filters or smoothing only with an explicit sampling definition | Halo/edge connectivity, arbitrary grid recipe, physical filters and independent QuPath/biological masks; identical captured inputs across UI/Python/CLI and cancellation/stale tests |
| Cell detection/compartments: `CellRequest`, `CellParameters`, `CellAlgorithm`, `CellJob`, `detect_cells` | Extend the existing versioned recipe/request/archive path; `SampledLogWatershedBodies` is already exposed across native/Python/CLI/UI/batches/archives. Reuse it rather than reimplementing it. First specify physical sigma/radii/expansion and anisotropic semantics, then change kernels and provenance together | Resolve existing strict science failures under predeclared masks/features/split–merge tolerances; physical radii and unmatched grids require calibrated fixtures. Reject unsupported calibration explicitly rather than silently treating pixels as micrometres |
| Tables/provenance: `MeasurementTableRequest`, `CellTableRequest`, raw tables, export jobs, `build_environment` | Compose existing immutable table requests and incremental export jobs; extend the schema in one native table layer, bind it thinly | Historical/live metadata joins, revision selectors, reproducible schema/units, cancellation and row/byte budgets. Reuse evidence hashes; avoid new ad-hoc CSV format for each UI |
| Object classifiers: `ObjectFeatureRequest`, `extract_object_features`, forest training/classification and archives | Reuse frozen feature schema, training snapshot and portable model archive; expose missing workflow actions through existing jobs | Expanded feature schema/versioning and independent research/model comparison. Verify source links, absent labels, deterministic predictions and applying classes as an undoable command |
| Pixel classifiers/streaming: `PixelTrainingRequest`, `PixelPredictionRequest`, `PixelPredictionStream`, file/export/read jobs | Use windowed and block streaming paths to bound large slides; compose masks/contours and captured model identity. Preserve hash-only resume | Stitched contours and unmatched-grid feature semantics; test seams against independent full-image references, IO cancellation, model/provenance persistence and research acceptance |
| Headless batches: `ProjectBatchRequest`, `ProjectBatchJob`, checkpoint APIs, `qupathxx_batch` | Add a workflow by adapting its existing request/result/archive path into the batch engine; keep UI launcher and CLI on the same job/options | Full interrupted/resumed scientific workflow, partial failure policy, stale source/model rejection, abandoned-work cleanup and stable memory. Distributed workers require their own later contract, not a second local batch engine |
| Spatial analysis: `Roi`, existing native spatial index and ROI/measurement queries | Build bounded neighborhood/distance/density queries on the current index and calibrated measurements; expose immutable tables through the established jobs/export layer | Specify units, metric, point/area semantics and anisotropic behavior, then independent nearest-neighbor/overlap/density fixtures and large-scene query/memory evidence. No full point-cloud copy per frame |
| Recorded workflow automation: native commands, parameterized requests and retained records | Compose a versioned ordered workflow manifest from existing request/command identities; reuse batch execution and per-stage archive/restart checks | Define dependency/data references, error/cancel policy and reproducible replay before promising general pipeline automation; validate one multi-stage workflow across script, CLI and UI |
| Managed Python viewer: `ViewerSession.execute`, notifications and existing analysis endpoints | Add a typed endpoint only when a missing real workflow needs it. Route edits/results to the native owner through the existing command queue | Correct close/interpreter shutdown, backpressure, notification lifetime and stale/revision handling. An unimplemented endpoint must be labelled proposed; no Python callbacks owning native workers |
| Python API/NumPy: public `__all__`, generated stubs, `.numpy()`, `from_numpy` | Keep native authority and thin exports; validate mutable inputs before copy/GIL release. Generate stubs from the newly installed wheel, package them, then check typed examples | Additive API compatibility, array strides/dtype/lifetime, errors and shutdown. Record optional skips by capability. Avoid manually maintaining generated signatures |
| External models/plugins: deferred Cellpose/StarDist, OMERO/ImageJ exchange, versioned extension interfaces | After native publication contracts stabilize, use Python adapters for versioned masks/features and explicit image/data exchange. Keep remote access and credentials outside the native document authority | Version/model/input identity, shape/calibration, resource/lifecycle and publication validation; no model download/ABI/plugin host added as a shortcut to native scientific completion |
| Source identity/fingerprints: `ReaderSource`, `FileFingerprintJob`, `associate_source_file`, provenance JSON APIs | Reuse existing file-version and native content-hash jobs; capture identity once at the operation boundary and verify before publication | Define partial/full hash policy, changed/missing/aliased source behavior and IO budgets. Test source replacement races; a stable path or metadata-only check is not a content-equivalence claim |
| Packaging/distribution: CMake optional targets, wheel builders, runtime manifest and fresh consumers | Extend already accepted archive/wheel qualification to the selected supported platform matrix; reuse dependency/notice inventories and native host probes | Installers/uninstall, publisher signing, older-OS/ABI limits, Mac Python/GUI/OpenSlide, clean machines and sample workflow walkthrough. A Mac native builder result does not qualify a Mac Python package |

## Analysis baseline to preserve

`SampledLogWatershedBodies` is already a production entry point. The pinned NDPI
comparison records 503 nuclei with zero context or 478 with 32-pixel context,
against 506 in the upstream reference; these are source/ROI/recipe observations,
not universal performance or biological acceptance. The older 299/506 LoG
characterization remains historical evidence and must not be presented as the
current sampled-recipe result. See [sampled recipes](sampled-cell-recipes.md) and
[NDPI comparisons](qupath-ndpi-cell-comparisons.md).

Create one bounded **scientific sampled-grid preparation** task before adding
three separate resampling paths to tissue, cell and classifier code. Specify
origin, extent, axis spacing/units, level selection, halo/context, ROI membership,
alpha/missing-data policy, quantitative dtype, memory/work limits, algorithm
identity and archived replay together. First integrate a representative cell
request with an independent oracle; then reuse the validated contract for tissue
and classifiers. These details must preserve older recipe defaults and replay.

General RGBA8 region resampling is implemented and engineering-qualified. It
neither resamples typed quantitative TIFF values nor supplies the grid/halo/
provenance semantics of a scientific detector. Keep both remaining requirements
explicit while reusing the existing filter only where its contract is correct.

## TMA desktop implementation recipe

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
   a completed desktop capture. Then freeze and qualify one integrated source.

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

A normal gate is a locally completed workflow, then one frozen full-matrix
qualification. The current workflow concurrency key groups only by Git ref, so
engine/Mac/portable dispatches on the same ref cancel one another. Until a
separately reviewed workflow change keys concurrency by mode and candidate, use
mode-specific immutable refs, all pinned to the same source SHA. Plan that small
CI correction before attempting to collapse those refs. The resumed implementation now
includes that mode-aware concurrency-key change; its integrated qualification
is pending. Stop and inspect a live run after uncertainty; don't redispatch
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

Review this queue after each accepted workflow. Preserve M0–M7 criteria and
reference histories, update statuses at the defined fixture scope, and identify
the next blocking dependency. Do not claim full completion until the complete
requirement-to-evidence audit passes.
