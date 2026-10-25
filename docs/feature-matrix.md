# QuPath workflow inventory

The integrated raw-grid application source `745662b` passes the complete required
Windows/Linux engine, both Mac native architectures and both portable builders/
fresh consumers. The [combined receipt](benchmarks/raw-grid-186-qualification.json)
binds all11archive hashes and the frozen493-file source contract. Mac native
scope excludes Python/GUI/OpenSlide. Biological, hardware, rights/licensing and
supported-release requirements remain; every milestone is Partial.

The [current task](current-task.md) owns current source/count/gate status; older
receipts and observations below retain their original source-specific scope.
Newer feature-branch TIFF changes are outside this accepted candidate.

Baseline: [QuPath v0.7.0](https://github.com/qupath/qupath/tree/v0.7.0), revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`. Source areas below refer to modules
under that revision. Each row names an implementation responsibility, not a
claim of upstream parity. Acceptance applies only to the listed fixture scope.

| Workflow | QuPath source area | Native owner | State | Required evidence |
| --- | --- | --- | --- | --- |
| Dense image regions | qupath-core | io | Partial | Exact pixels, padding and lifetime pass; explicit alpha-aware [arbitrary-size resampling](region-resampling.md) implemented with independent overlap tests; vendor/reader variety and scientific grid integration remain |
| OpenSlide regions | qupath-extension-openslide | io | Partial | Synthetic pyramid and small Aperio comparisons; exact small/partial-tile levels with pinned Windows/Linux readers and relocated Linux runtime checks; opt-in [full three-level Aperio checks](extended-reader-validation.md) add fractional levels, partial alpha, native CLI, scheduling/close and damaged JPEG errors; other vendors and extended hosted acceptance remain |
| Metadata and calibration | qupath-core, qupath-extension-openslide | core/io | Partial | Unknown and anisotropic calibration; typed single-file OME axes and calibration are implemented; broader formats/ICC remain |
| Whole-slide navigation | qupath-gui-fx | render/gui | Partial | Real slide rendering, overview, readiness-gated ImGui replay, completed-frame captures, bounded exact-pixel canvas caching, large synthetic navigation and native resize/minimize/restore with overlay/view/owner/save checks pass; monitor-aware font/style scaling, responsive field/button/toolbar layout and live size overrides with cache/camera/input checks are implemented; dynamic dense scenes, physical OS/DPI delivery and vendor stress remain |
| Background tile reads and cache | qupath-core, qupath-gui-fx | jobs/io | Partial | Native scheduling, generation rejection, cache budgets and viewer integration pass; broader workloads remain |
| Image display and channels | qupath-gui-fx | render/gui | Partial | RGB/gray/channel LUT, window and gamma framebuffer checks pass; native TIFF channel composites and Z/time views are implemented; advanced fluorescence/ICC remain |
| Rectangle annotation | qupath-core, qupath-gui-fx | geometry/gui | Partial | Atomic inspector edits, sampling and drawing/history replay pass; pinned QuPath flat geometry/calibrated-feature/sampling fixtures pass; broader editing workflow comparisons remain |
| Polygon and ellipse tools | qupath-core, qupath-gui-fx | geometry/gui | Partial | Holes, multipart validity, analytic ellipses and drawing/history pass; vertex/hole editing and Boolean overlay pass; pinned QuPath flat geometry/calibrated-feature/sampling fixtures pass within their explicit precision policy; broader editing workflow comparisons remain |
| Object hierarchy and classes | qupath-core | core | Partial | General ROI parent containment, classes, cycles and depth pass; cell/nucleus snapshots, conversion and history pass; class definitions/colors/counts, rename and batch classification pass; multi-selection gestures and group delete pass; vertex editing passes; upstream workflow comparisons remain |
| Undo and redo | qupath-gui-fx | core/gui | Partial | Compound ROI/class/hierarchy/calibration restore and drawing/history replay pass; multi-selection gestures and group delete pass; vertex editing passes; upstream workflow comparisons remain |
| Projects and relinking | qupath-core, qupath-gui-fx | project | Partial | Multi-image SQLite with validated bulk restoration, rollback, stale writers, relative moves, browser/add/rename/relink, persistent views, v1/v2-to-v3 migration with retained analysis records, async project/recovery opening, explicit saves/autosave, save-baseline reconciliation, hot-journal recovery, staged replacement, empty workspaces, new/remove flows, reviewed checkpoint deletion, identity-based moved recovery, bounded native retirement, marked cross-session work cleanup with non-overwriting snapshot rescue, opt-in age/count retention with idle session protection, published/progress polling, safe maintenance cancellation, bounded marker prefetch, deferred initial file loading with counts/cancellation and dirty-close replay pass; legacy orphan/obsolete lock cleanup, source fingerprints and broader fault campaigns remain |
| Annotation interchange | qupath-core, qupath-gui-fx | project | Partial | Atomic GeoJSON, deterministic CSV, explicit losses, protocol fixtures and GUI replay; pinned QuPath v0.7.0 flat shape/class/cell/nucleus round trips and explicit ID/quantization losses pass in the defined corpus; explicit loss-aware QuPath ellipse precision matching resolves the captured thin case; hierarchy and broader workflows remain |
| Region measurements | qupath-core, qupath-core-processing | analysis | Partial | ROI-masked RGB mean, bounded revision-keyed shape bundles, anisotropic lengths, shared OD/stain jobs and retained schema-3 ROI records with input digests/rechecks pass; capture-time source/calibration/build provenance passes; full file/binary/runtime manifests, pinned QuPath public-API sampling masks, tiled ROI means and calibrated features pass in the synthetic corpus; [Raw TIFF records](raw-analysis-records.md) add exact input hashes, original object/TMA context, schema-13 persistence, explicit replay and snapshot CSV through native/Python/ImGui; broader feature/plugin and full runtime-manifest comparisons remain |
| Stain separation | qupath-core, qupath-core-processing | analysis | Partial | Native/Python supplied-vector kernels and C++/Python/GUI body/nucleus jobs with retained parameters/input provenance/rechecks pass; capture-time source/calibration/build provenance passes; bounded pyramid-aware OD-plane estimation, deterministic reservoirs, independent NumPy/known-mixture checks, exact model identities/reload and GUI apply pass; full file/binary/runtime manifests, model registries, pinned QuPath OD/supplied-vector kernel references pass; broader stains, estimation/plugin comparisons and model registry coverage remain |
| Tissue detection | qupath-core-processing, qupath-extension-processing | analysis | Partial | Bounded OD/stain masks at native pyramid levels, square opening/closing, domain-aware size-bounded hole filling, global 4/8-connected components, level-zero statistics, digests, GUI preview, mask archives/rechecking and atomic contour/annotation publication pass; capture-time source/calibration/build provenance passes; arbitrary resampling/kernels, physical filters, smoothing/median, complete file/binary/runtime manifests and QuPath runtime comparisons remain |
| Cell detection and features | qupath-core-processing, qupath-extension-processing | analysis | Partial | Native distance-marker watershed, normalized smoothing, nonoverlapping expansion, compartment features, pyramid grids, atomic cell/nucleus publication, ImGui preview and lossless run archives/rechecking and schema-6 published feature links pass independent synthetic references; native document/project-wide archived-run CSV export passes; capture-time source/calibration/build provenance passes; joined live-object metadata/shape tables pass; inclusive [physical nucleus-area filters](cell-area-limits.md) pass the defined Windows/Linux engineering scope; explicit-grid physical radii and per-axis sampling now pass local native/Python/desktop checks; complete platform/file/runtime qualification, vendor/pyramid/tiled scientific sampling, split/merge and QuPath/biological feature tolerances remain |
| Pixel and object classifiers | qupath-core-processing, qupath-gui-fx | analysis/gui | Partial | Native/Python forests, geometry/verified-cell snapshots, labelled-ROI pixel training, source label/vote masks, exact undoable contour publication, ImGui controls, saved cohort/mask/model reuse and schema-10 project/recovery/batch persistence implemented. Natural-grid [windowed prediction](pixel-prediction-windows.md) bounds output to the visible or requested region, and the [native/Python block producer](pixel-stream.md) iterates full targets without retaining the full mask. Atomic [QPS1 streamed files](pixel-stream-file.md) provide indexed historical blocks and complete verification. A shared native/Python [background export job](pixel-stream-export.md) also powers cancellable F5 file export without retaining a full mask. The [saved-file inspector](pixel-stream-browser.md) provides asynchronous indexed navigation, verification and a separate block preview. The [standalone CLI](pixel-stream-cli.md) exports one saved project target and verifies files. [Schema-10 external stream links](pixel-stream-records.md) preserve historical file descriptors and provide verified reopen/relink. [Hash-only scans](pixel-input-digests.md) compare feature/request/stream inputs without forest inference. [Project pixel batches](project-pixel-batches.md) provide captured-model exports and hash-only verified resume through native/Python/CLI/ImGui. Stitched contours, unmatched-grid resampling, expanded features, research/QuPath comparisons and hardware/frame budgets remain; see [object](object-classifiers.md) and [pixel](pixel-classifiers.md) workflows |
| Tissue microarrays | qupath-core, qupath-gui-fx | core/analysis/gui | Partial | Native/Python/ImGui ellipse grids, undoable core/label/geometry edits, missing flags, shared case IDs, vacancies, bounded immutable snapshots, managed viewer commands, schema-11 persistence, present/missing-aware ROI/tissue/cell/pixel batches, metadata/context CSV and single-grid/captured multi-grid case inventory and weighted measurement summaries through native/Python/CLI/ImGui; see [TMA contract](tissue-microarrays.md). Native/Python/project/CLI/UI `.qptma` workflows are retained in the accepted `745662b` baseline ([combined qualification](benchmarks/raw-grid-186-qualification.json)). Atomic row/column corrections pass [local native/installed/desktop validation](benchmarks/tma-axis-editing-local.json) at `96172f3`; their platform campaign remains open. Resource candidate191 has separate engine acceptance and pending Mac/portable gates. Automatic dearraying, broader biological/upstream comparisons and hardware acceptance remain |
| Batch workflows | qupath-core, qupath-gui-fx | jobs | Partial | Native/Python/headless CLI ROI/tissue/cell processing, frozen capture, atomic per-image project/CSV checkpoints, pixel-verified restart, failure policies and cancellation; desktop launcher with progress, scope, presets and terminal reports; standalone single-target pixel-stream export/verification and project-wide captured-model pixel batches with verified hash-only resume are implemented; TMA core scope and CSV context are available; captured [raw-channel batches](raw-analysis-records.md) include immutable records and hash-only checkpoint reuse through native/Python/CLI/ImGui; distributed/portable workers and abandoned-work cleanup remain |
| Python scripting | New API replacing Groovy workflow surface | bindings | Partial | Shared document/region API and ROI analysis jobs pass; native batch execution is implemented; owned NumPy region/float/label views, strided imports, packaged public stubs and typed workflow checks pass; managed live viewer queues/notifications, owner-only publication and GIL/thread checks pass; broader commands/model adapters and beta compatibility gates remain |
| Bio-Formats and multiplex data | qupath-extension-bioformats | io | Partial | Optional [quantitative TIFF/OME reader](quantitative-tiff.md), typed arrays/scientific grids, captured axes, QRA2 project/replay/CSV and Python/CLI/batch/ImGui/viewer workflows pass source745662b engineering qualification. See the [current task](current-task.md) for exact scope. Broader vendor/multi-file readers, large-window/whole-slide sampling, raw classifier features, independent biology and complete decoder/process/hardware/release budgets remain |
| External model integrations | Extensions outside the core inventory | bindings/jobs | Deferred | Versioned Cellpose/StarDist results and publication contract |
| Java extensions and Groovy | qupath-extension-script-editor, qupath-gui-fx | compatibility | Deferred | No Java runtime compatibility promised in first release |

## Historical execution observations (source-specific)

Public main's qualified application source is now `3fc3bcb`; its following
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
