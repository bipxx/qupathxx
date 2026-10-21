# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, atomic
commands, bounded undo/redo, general immutable ROIs, cell/nucleus objects, a shared class registry, multi-selection and spatial queries. GEOS
provides polygon topology; ellipse metrics/masks use analytic geometry. Python
and ImGui share the native model. See geometry-spatial.md and document-commands.md.

Evidence: the final installed-wheel regression passes 837 Python cases with two file-symlink cases
skipped (this Windows account cannot create them; the junction passes).
Coverage includes readiness-gated GUI replay, settled captures, tissue cleanup,
immutable archives, analysis/provenance, stains, shapes, interchange, projects
and recovery; twenty-five native suites pass in the full GUI build and
twenty-four pass in core/minimal builds with optional geometry/readers/
interchange enabled and disabled. The installed GUI wheel includes its
packaged runtime libraries and licenses.
All 46 focused cell-archive/desktop cases pass, including eleven added after
the full run for shared IDs, malformed runs, source changes and save budgets.
The 100000-object document benchmark records approximately 230 ms batch creation,
116 ms undo, 126 ms redo and 815 bytes of history payload for a rename. The spatial
benchmark covers 1000 region queries and a cached narrow GUI view of 828 visible
objects. The selected-view benchmark additionally verifies all 100000 selected
with 828 visible overlays at about 16.9 ms p95 CPU frame work and 3 MiB GPU
tile payload. The recorded narrow view is below the cached-frame target; broader views and
broader complex-shape workloads remain unverified. The 4096-vertex editing
benchmark records roughly 11.9 ms p95 Boolean union, 8.2 ms validated edits
and 17.2 ms CPU frame work with vertex handles.
The existing large-pyramid navigation evidence remains scoped to synthetic pixels.

Detection/cell kinds, optional nuclei and undoable class definitions/colors are
implemented; see object-kinds.md and classes.md. Multi-selection and atomic
group delete/classification are recorded in selection.md. Vertex/hole editing
and Boolean area operations are recorded in geometry-editing.md. Native multi-
image SQLite save/load, relative links, relinking and Ctrl+S are implemented;
see project-storage.md. The 100000-object persistence benchmark measures about
225 ms save, 380 ms full resave and 201 ms validated load on local storage.
Deterministic native GeoJSON/CSV interchange, atomic imports, loss reports and
GUI controls are implemented; see interchange.md. QuPath runtime-assisted
round trips remain unverified. In-window image browsing/add/rename/relink,
session views/display/selection retention and document isolation are implemented;
see project-browser.md. A 1000-image catalog benchmark with 100000 selected objects
records 20 switches, 0.63 ms maximum synchronous switch time, 17.0 ms p95 CPU frame
work and 3 MiB GPU tile payload on dense images. Persistent camera/display views
and transactional schema-1-to-2 migration are implemented; see project-workspace.md.
The schema-2 run records 217 ms save and 188 ms validated load for 100000 objects;
live view publication records 17.3 ms p95 CPU frame work on the browser workload.
Native asynchronous autosave, session-specific checkpoints, validated recovery,
generation conflicts and dirty-close choices are implemented; see project-recovery.md.
The 100000-object recovery run records 15.3 ms capture, 240 ms background write/
release, 0.027 ms maximum poll and 16.9 ms p95 CPU frame work in the narrow viewer.
Ordinary project-file opening now loads/validates on a worker with staged
Save/Discard/Cancel replacement; see project-opening.md. The 100000-object
load alongside a separate live 100000-object scene records 172 ms worker load,
17.0 ms p95 and 27.0 ms maximum CPU frame work in a narrow view.
Empty desktop workspaces, New project, `show(project)` and confirmed image
removal are implemented; see project-lifecycle.md. The 100000-object removal
from a 1000-image catalog records 14.1 ms binding/removal, 17.0 ms p95 and
17.5 ms maximum CPU frame work, including unreferenced model retirement.
Native background explicit saves, captured-baseline acknowledgment and GUI save/
close/replacement integration are implemented; see project-save.md. Hot-journal
recovery after a killed native writer is verified, including Windows read-only
permission restoration. The 100000-object run records 16.1 ms capture, 338 ms
worker write/release, 0.020 ms maximum poll and 16.9 ms p95 CPU frame work in the
selected viewer.
In-window background recovery loading and reviewed checkpoint deletion are
implemented; see recovery-maintenance.md. The 100000-object recovery run records
179 ms worker loading, 16.9 ms p95 and 38.4 ms maximum CPU frame work, including
old-graph retirement without an external owner. The separate checkpoint deletion
run records 19.6 ms standalone content review and 16.8 ms viewer frame p95.
Moved recovery association now discovers identities across primary/sidecar renames,
accepts a verified primary location and retains stale-writer protection; see
recovery-association.md. Native-factory graphs now retire on one worker while
external `add_document` graphs retain owner-thread/GIL release; see project-retirement.md.
Factory servers now preserve native shared ownership through Python round trips;
subprocess tests verify snapshot release without a GIL deadlock after live removal.
The 1001-checkpoint/100000-object run records 642 ms metadata discovery, 169 ms
worker loading, 14.6 ms worker retirement, 17.0 ms frame p95 and 26.7 ms maximum
frame work. The external-graph retirement pause remains an optimization target.
Marked abandoned autosave work is now reclaimed under the destination writer lock
after publication in batches of at most 32, with an explicit native/Python full-sweep API, separate failure
reporting and retry; see recovery-work-cleanup.md. Live writer locks, a killed
native process, Windows junctions and publication after a deletion failure are
covered. Crash tests verify the actual interpreter PID instead of killing only
the Windows virtualenv launcher.
The 1000-directory cleanup benchmark records checkpoint visibility after 256 ms,
330 ms worker completion including a 32-directory batch and 0.012 ms maximum poll.
The selected 100000-object desktop run records 16.9 ms p95, 37.0 ms maximum frame
work and a 34.5 ms capture pause; the latter remains an optimization target.
Opt-in age/count checkpoint retention and project/session ownership leases are
implemented; see recovery-retention.md. Metadata-only bounded candidate selection
preserves active owners, legacy/unknown files, recent copies and distinct base
branches. User-state lease handles keep scientific project folders movable on
Windows; closed markers travel with checkpoints. Collection follows publication
on the existing worker. Legacy cross-session orphan work and obsolete lock-sidecar
discovery/cleanup remain open. The distinct-session 1000-file headless benchmark
publishes after 255 ms but records 8.48 s of metadata/lease scanning and collection;
that recording motivated maintenance progress/cancellation and scan profiling.
The distinct-session desktop run records an 8.67 s background pass with 16.9 ms
frame p95, 17.3 ms maximum frame work, 100000 selected objects and 3 MiB GPU payload.
Publication/progress polling, safe partial-scan cancellation, Ctrl+F10 and stage
timings are now implemented; see recovery-maintenance-progress.md. Finite embedded
GUI runs acknowledge autosave before returning. Hard-link/base aliases, active
oldest sessions, blocked SQLite cancellation/retry and live desktop Undo are
regression coverage for the new native scan path. The recorded stop-after-100
header pass completes about 9.4 ms after the request with no removals; the full
1000-session scan still takes 8.90 s, dominated by ownership-marker access.
The viewer records 16.9 ms p95 and 17.7 ms maximum frame work during that scan.
Bounded ownership-marker prefetch is implemented; see recovery-marker-prefetch.md.
Four native readers overlap I/O with metadata inspection, while session leases,
ranking and final deletion stay serialized. Pending records are capped at four;
cancellation/exception teardown joins every reader before acknowledgement.
The 1000-session headless pass falls from 8.90 s to 1.92 s with the same 32
removals, complete header inspection and a four-record peak. Cancellation after
100 headers completes in 9.2 ms without removals. Viewer timing remains variable:
the final run records 7.67 s retention dominated by metadata, 13.5 ms frame p95,
55.8 ms maximum frame work and 51.1 ms snapshot capture. Those remain optimization
targets; no uniform viewer speedup is claimed. Unleased overwrites now invalidate
stale matching ownership tags, or preserve the previous checkpoint on failure.
Cross-session version-2 work cleanup now follows publication under each old
writer's exact lock, sharing the 32-directory budget. Canonical rows and recovery
metadata commit atomically; complete private snapshots are rescued without
replacement, while rolled-back empty databases are reclaimed. Legacy cross-
session data, unknown/corrupt files and different base branches remain protected.
See recovery-work-cleanup.md for the explicit API and compatibility boundaries.
The 1000-destination/100000-object run publishes after 521 ms and completes its
32-directory worker batch after 833 ms; the full standalone sweep takes 12.24 s.
The selected desktop records 16.83 ms frame p95, 38.90 ms maximum and 36.45 ms
capture. Incremental collection is verified; full-sweep and capture latency
remain targets. The installed wheel passes 357 tests with two skipped file-
symlink cases; all fourteen native suites pass across three configurations.
Initial project/recovery/image files now load after the first painted workspace
on a native worker, with staged counts, safe cancellation, image-ID validation
and frame-boundary adoption; see project-opening.md. SQLite startup lock waits
check cancellation between 5 ms retry sleeps. Missing/bad inputs preserve the
empty authority and permit another in-window opening. Startup smoke runs verify
settled rendering after the request completes. The 100000-object runs record
238 ms project and 241 ms recovery worker load, about 16.9 ms frame p95 and
3 MiB GPU payload. Including all startup frames exposes 203/275 ms maximum frame
work; a temporary trace locates the largest stall in rendering before adoption.
The final installed wheel passes 375 Python tests (two file-symlink cases
skipped), including eighteen initial startup/retry checks. All fourteen native
suites pass across full/core/minimal configurations. This initial rendering/
adoption latency remains open alongside metadata/capture latency. Validated document restoration now builds canonical maps directly and
memoizes hierarchy depths instead of creating edit patches. Nineteen added
storage tests retain both row orders, depth/cycle/containment/class/kind rules,
revisions, derived indexes, undo after loading and next-ID gaps. Worker startup
loading falls from about 238/241 ms to 106/114 ms for project/recovery workloads.
All 394 installed-wheel tests pass (two skipped file-symlink cases), and all
fourteen native suites pass across three configurations. Early frame maxima
remain 238/207 ms; even 1000 objects load in 3.3 ms with a 276 ms early spike.
State-query probes, startup-vsync changes and prestarted-worker experiments
isolated the limit but did not remove it; rejected code/probes are reverted.
See project-storage.md and startup-render-diagnostics-windows.json.
The corrected benchmark fixture also verifies its full 1000..1000000-object
range endpoints: one million objects load in 891 ms, all remain selected with
792 visible, 16.94 ms frame p95 and about 3.9 MiB GPU payload. Its 295 ms maximum
still exposes initial rendering/selection work.
Revision-keyed shape bundles and a bounded per-document LRU are now implemented
for C++/Python/ImGui, with body/nucleus pixel and calibrated area/length. Object
and calibration stamps govern reuse; snapshots/results have independent numeric
lifetimes. Physical length handles anisotropic axes, equal-axis length reuses
pixel length, and range handling avoids intermediate ellipse/area failures.
The inspector shows primary-object results, refreshes calibration drafts on the
calibration stamp and caches object IDs by membership revision. See
geometry-spatial.md for capacity, invalidation, lifetime and numeric contracts.
Seventeen Python measurement tests cover independent quadrature, unknown
calibration, Undo, eviction, source closure and storage; native checks cover
copy/move/assignment. All 411 installed-wheel
tests pass (two unavailable file-symlink cases skipped), and all fourteen native
suites pass in full/core/minimal builds. The anisotropic hot-set benchmark records
1240.7 ms disabled versus 21.25 ms warm for 65536 matching queries, retaining
4096 bundles after 100000-object pressure. The selected viewer records 16.79 ms
sampled frame p95, 17.12 ms maximum, 789 visible ellipses and 3 MiB GPU payload,
with 176 hits and no recomputations. Early rendering stalls remain unresolved.
Native optical density and fixed two/three-vector stain separation now start
M5; see stain-transforms.md. Immutable transforms precompute OD and concentration
contribution tables, release the GIL and return independent float32 values.
Transparent/partial-alpha pixels are NaN; negative concentrations remain intact.
Twenty-four Python tests compare the kernel with independent formulas/matrix
solves, exact tiled partitioning, lifetimes and concurrent reuse. Native checks
also cover cancellation, partial alpha, malformed buffers and the result memory
ceiling. All 435 installed-wheel tests pass (two unavailable file-symlink cases
skipped), and all fifteen native suites pass in full/core/minimal builds. The
1048576-pixel benchmark records 4.39 ms median OD and 5.49 ms separation,
including a 12 MiB allocation; Python byte copies cost 2.42/3.90 ms separately.
The initial kernel packet left analysis jobs, GUI publication, saved provenance,
stain estimation, tissue/cell detection and upstream runtime parity open.
The shared native request/job path now provides body/nucleus ROI OD/stain means,
bounded tile reads, progress, cancellation and owner-thread publication in
C++/Python/ImGui; see analysis-jobs.md. Runtime document/source/object keys reject
stale work, including relinks, restored/copied/moved graphs and selection edits.
Unitless results survive unrelated edits/calibration changes. Worker ownership
never releases a Python-owned source; built-in closed-state queries no longer
wait behind reads. Twenty-one headless and four GUI regressions cover independent
formulas, masks, partitioning, cancellation/reuse, relink/reopen, teardown and
GUI/native parity; native tests add a controlled blocked reader, graph identity
and transparent-source failure. All sixteen native suites pass in three builds.
The headless 4194304-pixel run records 0.0076 ms capture, 0.085 ms submission,
43.7 ms worker time and 0.012 ms maximum poll, with 1 MiB tile payload. The
100001-selected-object viewer records 25.4 ms analysis, 876 visible objects,
16 MiB GPU payload, 16.88 ms sampled frame p95 and 17.17 ms maximum. Runtime
source keys do not detect externally changed files or provide saved provenance.
Retained native analysis records now add versioned ROI input SHA-256, original
ROI/parameters/values, session-only verification and schema-3 project/recovery
storage; see analysis-provenance.md. Schemas 1/2 open without rewriting and
migrate transactionally on save. Copy/reopen trust stays cold. Native/Python/
ImGui Keep/Recheck paths preserve history and detect input or summary mismatches;
full-source/environment provenance remains open. Twenty-eight archive tests and a
new GUI keep/save/reopen/recheck regression pass, alongside all 489 installed-
wheel tests (two unavailable file-symlink cases skipped). All seventeen native
suites pass in full/core/minimal builds, including both SHA-256 implementations.
The 100000-record/100001-object workload appends in 76.3 ms, performs 10000 latest
lookups in 3.24 ms, saves on a worker in 484 ms and reopens in 244 ms. It verifies
the record cap and exact recheck. The selected viewer records 16.84 ms sampled
frame p95, 17.27 ms maximum, 910 visible objects and 3 MiB GPU payload. Snapshot
capture takes 50.1 ms and remains an optimization target. The archive benchmark
shares one captured ROI/result across records; varied large archives are unproven.
Archive snapshots now share immutable native record payloads in 256-slot blocks,
with permanent sealing and bounded copying before live slot edits. Owner-only
proof/index maps stay out of the worker. Free slots are reused and clearing the
last record releases live blocks independently of captured saves. Snapshot budgets
charge retained block storage, including sparse blocks. See
analysis-snapshot-sharing.md. Five Python regressions cover locked-writer slot
reuse, full clear/reuse, recovery capture, sparse-block budgets and subprocess
teardown; native checks mutate copied/assigned archives across block boundaries.
All 494 installed-wheel tests pass (two unavailable file-symlink cases skipped),
and all seventeen native suites pass in full/core/minimal builds. A fresh
100000-record/100001-object comparison drops capture from 52.56 ms to 17.17 ms.
The shared viewer records 16.86 ms sampled frame p95 and 18.93 ms maximum with
910 visible objects and 3 MiB GPU payload. A separate blocked-writer run verifies
baseline isolation while measuring 0.0217 ms removal and 0.0167 ms replacement
append after capture. Object/class copying and budget traversal remain linear;
capture is still near one display frame. These dense repeated-ROI recordings do
not establish broader geometry, hardware or vendor-reader latency guarantees.
Native bounded level-zero tissue threshold masks and connected components now
start the detection workflow; see tissue-detection.md. OD-sum or supplied-stain
thresholding shares C++/Python/ImGui jobs, cancellation and stale-result checks.
Global 4/8-connected labeling reconciles decode tile boundaries, filters pixel
sizes and reports counts, bounds and centroids. The viewer incrementally uploads
a separately bounded mask preview. Independent references verify every label,
including diagonal and cross-tile cases. The 4194304-pixel recording takes
58.66 ms worker time with 0.105 ms submission and 0.0224 ms maximum sampled poll.
The 100001-selected-object viewer records 17.34 ms sampled frame p95 and
18.88 ms maximum, with 16 MiB mask GPU payload. These are synthetic recordings.
Native pyramid sampling now selects the finest level fitting the work budget or
uses a strict explicit level. Sample-grid dimensions and nominal level-zero
component/preview geometry are separate; level-aware SHA-256 records exact native
reads and fractional-scale tile origins. Sixteen new Python cases cover real
1.5x/4x pyramids, budget boundaries, coarse body/nucleus geometry, input digests
and a framebuffer-scaled mask. The 205520896-pixel synthetic slide selects 8x
sampling and processes 3211264 samples in 153.79/134.65 ms measured worker runs,
with 33204224 estimated work bytes. Its 100001-selected-object viewer records
17.22 ms sampled frame p95 and 19.00 ms maximum with 12845056 mask GPU bytes.
Native levels do not provide arbitrary resampling or phase independence at
fractional read origins. Broader vendor/hardware behavior remains unproven.
Square opening/closing and bounded enclosed-hole filling now precede component
filtering, preserving ROI/opacity exclusions. Captured radii and hole limits use
sampled pixels; result statistics distinguish threshold, cleaned, filled and
retained pixels. The native cleanup phase reuses the existing queue and adds
one domain byte per sample to pre-decode budgets. Forty-eight Python cases
compare independent windows/flood filling, coarse coordinates, memory endpoints,
topology, cancellation and rendered filled-hole pixels. Native checks cover
transparent/partial-alpha domains and kernel cancellation. The 4194304-pixel
analytic-mask benchmark records 108.31/118.43/173.50 ms median cleanup at square
radii 1/8/64, with 45262848 estimated work bytes. The selected 100001-object
viewer records 17.33 ms sampled frame p95 and 20.05 ms maximum with 16 MiB mask
GPU payload. Timing variability and broader vendor/hardware behavior remain open.
The cleanup packet exposed fixed-frame Aperio, save-and-close and replacement
checks that failed intermittently, then passed in isolation. Native opt-in
readiness waits now pause the logical script clock until startup finishes or a
fresh visible/enabled named button is available. Physical rendering and worker
polling continue. Ordered cursor dispatch preserves exactly-once input and the
following release; steady-clock deadlines reject unavailable actions. Finite
tile settlement captures the completed current frame within existing budgets.
See gui-replay.md for semantics and limits. Fourteen new desktop cases and a
pure native clock suite cover missing/stale targets, timeout cleanup, exact
framebuffer bytes, startup adoption and legacy behavior. The full final wheel
passes 620 cases with two skipped; all nineteen native suites pass in three builds.
Three actual save-under-lock rounds complete at logical frame 13 while rendering
24/51/52 physical frames, including 11/38/39 wait frames, preserving both saved
and newer live states. Four-tile captures finish in 2/2/6 physical frames for a
one-frame logical budget. The 100001-selected-object viewer records 17.25 ms
sampled frame p95 and 19.51 ms maximum with 16 MiB each of image/mask GPU payload.
These fixtures establish readiness-aware workflow evidence, not broad latency
guarantees or complete OS/DPI acceptance.
Native tissue contours now preserve holes and diagonal multipart components,
clip nominal sample footprints, and publish one atomic annotation batch through
Python and ImGui. See tissue-contours.md. Worker failure permits retry; cancellation
and stale authority/selection prevent publication. Logical resource caps precede
backend allocation, and straight-run compression avoids per-pixel edge storage.
Tissue masks now persist losslessly with captured parameters and tile digests in
native projects and recovery snapshots. New native shared ownership prevents
Python-wrapper cleanup on save workers; sealed record blocks preserve snapshot
isolation during live removal/reuse. Reopened records require an explicit pixel
recheck. See tissue-archive.md. Sixty-two new Python/desktop checks include
checksum-preserving corruption, source changes, fractional/native-pyramid geometry,
archive migration rollback and Python-wrapper-free worker cleanup. All twenty-one
native suites pass in three builds. The 4194304-label rectangle mask encodes to
107548 bytes; the checkerboard uses raw labels. Eight shared records capture in
0.019 ms while charging 198014935 logical snapshot bytes. The 100001-object
recording keeps a mask in 0.043 ms, captures a save in 18.10 ms, and records
17.17 ms viewer frame p95 with 20.28 ms maximum. These are synthetic observations.
Object save/recovery capture now shares compact immutable blocks with cached
payload totals. Nine new Python checks and a native boundary/isolation suite
cover mixed edits, load-before-first-save, payload reclamation, copied authorities,
recovery counts and sparse unsigned IDs. The paired 100000-object capture records
16.50 ms before and 0.0246 ms after, with 0.0686 ms desktop capture and 17.26 ms
frame p95. Rename p95 changes from 0.0021 to 0.0052 ms; class/image/view traversal
and extra native object/string storage remain limits. See object-snapshots.md.
The fortieth packet adds native cell detection with nanobind and ImGui, bounded
distance-marker watershed, native pyramid sampling, nonoverlapping body expansion,
compartment features and atomic cell/nucleus publication. See cell-detection.md.
Independent SciPy/scikit-image comparisons cover complete labels and means;
native exhaustive distance/nearest-site checks include tie rules and opacity.
Source changes, cancellation, contour failures, Undo/Redo and published geometry
persistence pass. Cell masks, features and provenance archives remain unfinished.
The 1024² synthetic disk fixture detects 256 cells in 235–262 ms worker time,
reserving 162390016 logical work bytes. With 100001 selected objects and 911
visible, the viewer records 17.05 ms frame p95; the all-visible overview records
109.39 ms p95. This exposes the existing dense-overlay rendering limit and is
not a general throughput guarantee.

The forty-first packet adds an exact-pixel GPU canvas cache. A renderer callback
copies the opaque base canvas before transient tools; unchanged views use one
quad while native geometry still handles picking/editing. Revision and display,
tile/mask readiness, camera, clip, font and framebuffer keys prevent stale pixels.
Payload is capped before allocation, with immediate rendering on budget failure.
The paired 100000-visible-object, 2560-by-1440 recording reduces frame p95 from
99.72 to 16.91 ms using 12194160 GPU payload bytes and identical framebuffer
hashes. The view records four captures and 176 hits in 180 frames. Dense dynamic
views still render normally. See overlay-cache.md for scope and reproduction.

The forty-second packet retains lossless cell runs through schema-5 project and
recovery storage, including both masks, captured transforms/parameters, markers,
compartment features and input digests. Reopened values require an explicit
worker recheck; shared native snapshots preserve removed runs without Python
wrapper cleanup on workers. Strict transport/count/geometry validation, quotas,
transactional migration and ImGui retain/recheck/publication pass. Coarse curved
ROI tests also fix zero-area boundary remnants before cell publication, preserving
strict generic containment. See cell-archive.md.
The 1024², 256-cell run encodes both planes/features to 259620 bytes. Eight shared
runs capture in 0.016 ms while charging 78227751 snapshot bytes. The all-visible
100001-selected-object viewer records 16.90 ms p95 and 114.68 ms maximum; retaining
a run takes 0.0332 ms and save capture 0.0258 ms. Invalidated dense canvases still
require full rendering; these are warm synthetic observations on one PC.

Next complete source/environment provenance and cell feature-table export. Continue
numerical comparisons and improve initial
rendering and metadata/capture latency. Preserve all unfinished
M2 work: OS/DPI delivery, resize/minimize, docking, fluorescence/ICC, prolonged
cache pressure and vendor benchmarks. Persistence, interchange, analysis,
classifiers, scripting and packaging work under M4-M7 remains active.

Outstanding M0 decisions: project license choice
(asked asynchronously; no license selected yet), broader malformed-image
fixtures and numerical QuPath comparison tolerances. GEOS dependency selection
and packaging are now recorded with source identity and independent tests.
