# QuPath workflow inventory

Baseline: [QuPath v0.7.0](https://github.com/qupath/qupath/tree/v0.7.0), revision
`04ccfa4fb7d43e9b566393e08e83690b72248d44`. Source areas below refer to modules
under that revision. Each row names an implementation responsibility, not a
claim of upstream parity. Acceptance applies only to the listed fixture scope.

| Workflow | QuPath source area | Native owner | State | Required evidence |
| --- | --- | --- | --- | --- |
| Dense image regions | qupath-core | io | Partial | Exact pixels, padding and lifetime pass; reader variety missing |
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
| Cell detection and features | qupath-core-processing, qupath-extension-processing | analysis | Partial | Native distance-marker watershed, normalized smoothing, nonoverlapping expansion, compartment features, pyramid grids, atomic cell/nucleus publication, ImGui preview and lossless run archives/rechecking and schema-6 published feature links pass independent synthetic references; native document/project-wide archived-run CSV export passes; capture-time source/calibration/build provenance passes; joined live-object metadata/shape tables pass; inclusive [physical nucleus-area filters](cell-area-limits.md) pass the defined Windows/Linux engineering scope; full file/binary/runtime manifests, physical radii, arbitrary resampling, split/merge and QuPath feature tolerances remain |
| Pixel and object classifiers | qupath-core-processing, qupath-gui-fx | analysis/gui | Partial | Native/Python forests, geometry/verified-cell snapshots, labelled-ROI pixel training, source label/vote masks, exact undoable contour publication, ImGui controls, saved cohort/mask/model reuse and schema-10 project/recovery/batch persistence implemented. Natural-grid [windowed prediction](pixel-prediction-windows.md) bounds output to the visible or requested region, and the [native/Python block producer](pixel-stream.md) iterates full targets without retaining the full mask. Atomic [QPS1 streamed files](pixel-stream-file.md) provide indexed historical blocks and complete verification. A shared native/Python [background export job](pixel-stream-export.md) also powers cancellable F5 file export without retaining a full mask. The [saved-file inspector](pixel-stream-browser.md) provides asynchronous indexed navigation, verification and a separate block preview. The [standalone CLI](pixel-stream-cli.md) exports one saved project target and verifies files. [Schema-10 external stream links](pixel-stream-records.md) preserve historical file descriptors and provide verified reopen/relink. [Hash-only scans](pixel-input-digests.md) compare feature/request/stream inputs without forest inference. [Project pixel batches](project-pixel-batches.md) provide captured-model exports and hash-only verified resume through native/Python/CLI/ImGui. Stitched contours, unmatched-grid resampling, expanded features, research/QuPath comparisons and hardware/frame budgets remain; see [object](object-classifiers.md) and [pixel](pixel-classifiers.md) workflows |
| Tissue microarrays | qupath-core, qupath-gui-fx | core/analysis/gui | Partial | Native/Python/ImGui ellipse grids, undoable core/label/geometry edits, missing flags, shared case IDs, vacancies, bounded immutable snapshots, managed viewer commands, schema-11 persistence, present/missing-aware ROI/tissue/cell/pixel batches and metadata/context CSV; see [TMA contract](tissue-microarrays.md). Automatic dearraying, direct QuPath TMA interchange, case aggregation, biological/upstream comparisons and hardware acceptance remain |
| Batch workflows | qupath-core, qupath-gui-fx | jobs | Partial | Native/Python/headless CLI ROI/tissue/cell processing, frozen capture, atomic per-image project/CSV checkpoints, pixel-verified restart, failure policies and cancellation; desktop launcher with progress, scope, presets and terminal reports; standalone single-target pixel-stream export/verification and project-wide captured-model pixel batches with verified hash-only resume are implemented; TMA core scope and CSV context are available; captured [raw-channel batches](raw-analysis-records.md) include immutable records and hash-only checkpoint reuse through native/Python/CLI/ImGui; distributed/portable workers and abandoned-work cleanup remain |
| Python scripting | New API replacing Groovy workflow surface | bindings | Partial | Shared document/region API and ROI analysis jobs pass; native batch execution is implemented; owned NumPy region/float/label views, strided imports, packaged public stubs and typed workflow checks pass; managed live viewer queues/notifications, owner-only publication and GIL/thread checks pass; broader commands/model adapters and beta compatibility gates remain |
| Bio-Formats and multiplex data | qupath-extension-bioformats | io | Partial | Optional [quantitative TIFF/OME reader](quantitative-tiff.md), typed raw arrays, channel/Z/time selections and display composites, schema-12 views, native worker UI and managed Python commands; schema-13 raw measurement records, explicit replay, historical CSV and raw batches; broader Bio-Formats readers, multi-file OME, raw classifier features and workflow validation remain |
| External model integrations | Extensions outside the core inventory | bindings/jobs | Deferred | Versioned Cellpose/StarDist results and publication contract |
| Java extensions and Groovy | qupath-extension-script-editor, qupath-gui-fx | compatibility | Deferred | No Java runtime compatibility promised in first release |

## Current packet and remaining decisions

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
