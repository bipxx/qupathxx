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
| Horizontal implementation produces repeated “candidate complete” reports without finishing a researcher task | Earlier TMA stages progressed through reference, codec and project commands before desktop import/export was delivered | Accept complete vertical workflows; substeps remain visible but do not substitute for UI/Python/CLI/persistence completion |
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
| 1 | Project-enabled TMA build validation: engineering verified | Reuse the qualified installed artifacts and existing project contract; do not reinstall historical wheels merely to repeat completed checks | Source-bound engine/Mac/portable acceptance is recorded in the combined source169 receipt; biological and supported-release scope remain separate |
| 2 | TMA desktop workflow: engineering verified | Reuse `GuiTma`, `load_qptma`, `qptma_grid_edits`, `import_qptma`, `qptma_from_grid` and `save_qptma`; retain background IO plus owner-thread preview/apply | Import → preview → apply → edit → undo/redo → project save/restart → export and error/stale/cancel coverage are retained with the accepted desktop evidence |
| 3 | Integrated TMA source: accepted and promoted to main | Reuse its qualified workflow and inherited gates in subsequent integrated candidates | Matching source/artifacts and required configurations; no supported-release inference |
| 4 | Annotation/migration: engineering verified at source169 | Shared nested export, physical recovery and source-bound hierarchy contracts pass engine/Mac/portable audits | Combined source169 receipt; broader annotation/interchange and fault campaigns remain in M3/M4 |
| 5 | Close sampling and scientific analysis gaps | Reuse the implemented explicit-grid RGB pipeline, physical per-axis kernels and QCL5/QCA5 frontend workflow; next characterize the three unresolved scientific gates and remaining vendor/typed/tiled adapters | Source169 engineering qualification is complete; independent masks/features and split/merge results under predeclared tolerances remain, including anisotropic and unknown calibration |
| 6 | Complete research and batch workflow | Compose existing analysis/classifier jobs and checkpoint batches; build missing coverage/adapters only after native contracts are stable | Same selected workflow in UI/Python/CLI, interrupted-resumed batch correctness and bounded resources |
| 7 | Close release and remaining inventory requirements | Build on accepted packages; choose support matrix, license/notices policy, installers and clean-machine scripts | Every M0–M7 exit has scope-matched evidence; remaining scope is explicitly scheduled, not silently dropped |

Steps 5–7 do not prohibit fixing a concrete defect found in an earlier step.
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

The TMA and explicit-grid RGB-cell workflows now have exact-source hosted
engine/Mac-native/portable engineering acceptance. Reuse their shared native,
Python, CLI, batch and GUI paths for remaining work. The next scientific slice
must characterize the retained strict failures and close the relevant mask,
feature and split/merge requirements without weakening acceptance criteria.

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
| Scientific RGB/channels: `ScientificGridAxis`, `ScientificGrid`, `ScientificRgbGrid`, `read_scientific_rgb`, `ScientificChannels` | Reuse the exact grid, bounded level-zero float64 RGB sampler and OD/signed-stain transforms. Shared rasters/GEOS contours preserve both axes and one-read input identity. Reuse the [contract](scientific-grid-contract.md); keep typed raw channels separate | Native, installed Python and desktop cell integration pass locally through QCL5/QCA5/CSV, including captured calibration and metadata lifetime. The [integrated platform campaign](current-task.md) is accepted at source `3fc3bcb` within its declared scope. Pyramid/tiled sampling, typed quantitative sources and biological masks/features remain; a bounded source window does not complete large-slide sampling |
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
| Cell detection/compartments: `CellRequest`, `GridCellSampling`, `GridCellParameters`, `CellParameters`, `CellJob`, `detect_cells` | For count-based explicit grids, use `CellRequest.for_grid_roi`; for exact origins/extents, pass `ScientificGrid` to the constructor. Choose typed pixel/micrometre units, preserve the ROI and reuse QCL5 retention/replay. Legacy recipes keep their existing parameter path. See the [workflow guide](grid-cell-workflows.md) | Physical sigma/radii/expansion, per-axis kernels, calibrated features and native/Python/CLI/batch/GUI integration now pass source169 engine/Mac-native/portable engineering qualification. Predeclared biological/QuPath mask, feature and split/merge tolerances remain. Unknown micrometre calibration rejects explicitly |
| Tables/provenance: `MeasurementTableRequest`, `CellTableRequest`, raw tables, export jobs, `build_environment` | Compose existing immutable table requests and incremental export jobs; extend the schema in one native table layer, bind it thinly | Historical/live metadata joins, revision selectors, reproducible schema/units, cancellation and row/byte budgets. Reuse evidence hashes; avoid new ad-hoc CSV format for each UI |
| Object classifiers: `ObjectFeatureRequest`, `extract_object_features`, forest training/classification and archives | Reuse frozen feature schema, training snapshot and portable model archive; expose missing workflow actions through existing jobs | Expanded feature schema/versioning and independent research/model comparison. Verify source links, absent labels, deterministic predictions and applying classes as an undoable command |
| Pixel classifiers/streaming: `PixelTrainingRequest`, `PixelPredictionRequest`, `PixelPredictionStream`, file/export/read jobs | Use windowed and block streaming paths to bound large slides; compose masks/contours and captured model identity. Preserve hash-only resume | Stitched contours and unmatched-grid feature semantics; test seams against independent full-image references, IO cancellation, model/provenance persistence and research acceptance |
| Headless batches: `ProjectBatchRequest`, `ProjectBatchJob`, checkpoint APIs, `qupathxx_batch` | Adapt existing request/result/archive paths into the shared batch engine. For grid cells set `kind=CELLS`, `cell_grid` and `grid_cells`; each image resolves its own calibration. CLI and UI use the same native factory and options | Grid interruption/resume, per-image calibration, typed CSV and QCL5 checkpoints pass source169 engineering qualification. Remaining scientific/platform scope, partial failure, source/model replacement, abandoned-work cleanup and stable memory remain. Distributed workers require a later contract |
| Spatial analysis: `Roi`, existing native spatial index and ROI/measurement queries | Build bounded neighborhood/distance/density queries on the current index and calibrated measurements; expose immutable tables through the established jobs/export layer | Specify units, metric, point/area semantics and anisotropic behavior, then independent nearest-neighbor/overlap/density fixtures and large-scene query/memory evidence. No full point-cloud copy per frame |
| Recorded workflow automation: native commands, parameterized requests and retained records | Compose a versioned ordered workflow manifest from existing request/command identities; reuse batch execution and per-stage archive/restart checks | Define dependency/data references, error/cancel policy and reproducible replay before promising general pipeline automation; validate one multi-stage workflow across script, CLI and UI |
| Managed Python viewer: `ViewerSession.execute`, `grid_cells`, notifications and analysis endpoints | Submit `grid_cells` while the native owner pumps; submission copies the recipe and owner-stage capture resolves calibration. Reuse poll/wait/take, cancellation and retention; edits/publication stay on the owner | Grid producer/owner retention, export/save and stale/calibration delivery pass native/installed desktop checks locally. Broader lifecycle/backpressure/shutdown and platform qualification remain. Add endpoints only for missing real workflows; no Python callbacks owning native workers |
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
semantics. Typed raw scientific sampling and vendor/pyramid/tiled cell admission
remain explicit requirements; reuse existing filters only where their contracts
match.

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
qualification. Mode-aware concurrency keys now let engine/Mac/portable runs
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

Review this queue after each accepted workflow. Preserve M0–M7 criteria and
reference histories, update statuses at the defined fixture scope, and identify
the next blocking dependency. Do not claim full completion until the complete
requirement-to-evidence audit passes.

## Complete qualification campaign inputs

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
