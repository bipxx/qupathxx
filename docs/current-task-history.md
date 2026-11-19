# Earlier current-task snapshot

##2026-11-17 - A07 native COCO instance masks

Delivered selected polygon unions and compressed/uncompressed RLE through a
separate ABI5 QBM1 full-canvas transport, reusing existing native label/grid/
contours and preview/apply/undo/project/CLI/SDK services.32/65,24 pinned-reference
comparisons,19 workflow checks,50 legacy controls and21 mask trust-boundary cases
pass. Offline12/8 plus19 workflows and17 installed/coexistence checks pass with
five providers.194 directory/ZIP members pass admission and exact removal; nine
user files survive and two saved results reopen provider-free. Exact products,
component terms/source/notices and evidence are in the [A07 receipt](benchmarks/coco/README.md).

The new host reuses exact delivered label/contour objects after verifying all
12/55 recorded inputs; the frozen core/runtime and wheels stay exact. Failures
and corrections are retained: missing symbols in the older SDK, two new-consumer
profile/API/header spellings, a stale-test undo baseline, relocated control output
directories, duplicate mock format IDs and an old-source staging borrow rule.
No numeric/stored-identity contract was relaxed.49 initial snapshots/eight ZIPs,
1404 earlier products (four recorded archive relocations),43 prior inventory
groups,18 original dirty snapshots and QOI product sources are verified.
The full formats AND application/integration goal remains active; no commit,
push/publication, binding, GUI or wheel phase was selected.

This is a historical snapshot; its counts, outstanding items and timing scopes
do not describe the current source. See [the workflow inventory](feature-matrix.md).

# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, atomic
commands, bounded undo/redo, general immutable ROIs, cell/nucleus objects, a shared class registry, multi-selection and spatial queries. GEOS
provides polygon topology; ellipse metrics/masks use analytic geometry. Python
and ImGui share the native model. See geometry-spatial.md and document-commands.md.

Evidence: the final installed-native regression passes 1233 Python cases with
three expected skips: two Windows file-symlink permissions and one probe for the
GEOS-disabled configuration (tested separately by native builds).
Coverage includes readiness-gated GUI replay, settled captures, tissue cleanup,
immutable archives, analysis/provenance, stains, shapes, interchange, projects
and recovery; thirty-three native suites pass in the full GUI build and
thirty-two pass in core/minimal builds with optional geometry/readers/
interchange enabled and disabled. The installed GUI wheel includes its
packaged runtime libraries and licenses.
All cell archives and cell/ROI/tissue table cases are included in that full run.
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
99.72 to 16.89 ms using 12194160 GPU payload bytes and identical framebuffer
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

The forty-third packet adds native cell measurement CSV from immutable archived
run snapshots, with Python and ImGui export, a bounded background writer, quotas
and atomic replacement. CSV preserves exact unsigned IDs, capture-time status,
parameters and compartment features; missing cytoplasm remains blank. Run
metadata is formatted once per run. The 32768-row export improves from 78.32 to
38.17 ms with identical bytes; the all-visible 100001-object viewer records
16.93 ms p95 frame work during export. See cell-tables.md for validation and timings.

The forty-fourth packet preserves published cell feature identity through native
run/label references, object edits, history and schema-6 project/recovery storage.
Live publication retains/reuses an archive; verified saved runs publish directly.
The inspector shows captured measurements and explicit geometry/source/missing-run
status. The loader reconstructs bounded contours from validated historical labels;
numeric-only runs remain readable when geometry cannot fit saved limits.
GeoJSON reports dropped run links. Publishing 4096 cells takes 5.15 ms; the
104097-object selected/all-visible viewer records 16.93 ms p95 CPU frame work.
See cell-feature-links.md.

The forty-fifth packet adds immutable project-wide archived cell CSV with
project/image context, bounded aggregate capture and one native atomic worker.
Python accepts projects or selected image IDs; ImGui exports all images without
requiring an annotation or active image. The catalog capture performs no linked
source availability checks. The 32768-row export takes 45.63 ms; a 1000-image
catalog captures in 0.1417 ms p95 and its active 100001-object viewer records
17.18 ms p95 CPU frame work while exporting. Legacy document CSV retains its
original output hash. See project-cell-tables.md.

The forty-sixth packet captures immutable source metadata, reader/locator,
original/effective calibration, wall-clock time and native build/dependency facts
when ROI/tissue/cell requests are created. Schema 7 preserves these through
project/recovery and rechecks, with explicit legacy unknowns, bounded checksummed
transport and UTF-8 JSON export. The source fingerprint records actual C++/CMake/
Python wrapper bytes, including uncommitted changes. See analysis-capture-provenance.md.
The 100000-record synthetic shared-result archive saves in 878 ms and reopens in
632 ms; warm request capture is 0.0012 ms p95. The selected/all-visible 100001-object
viewer with 1000 saved records measures 16.89 ms p95 CPU frame work. These are
scoped measurements on one Windows PC, not independent whole-slide analyses.

The forty-seventh packet adds joined live cell-object tables for documents and
projects through Python and ImGui. Immutable captures retain current identity,
classes/colors, parents, body/nucleus geometry, effective calibration and explicit
object-feature status alongside historical detector columns. Manual cells and
removed archives remain visible with blank measurements. Duplicate publications
produce distinct object rows; strict mode requires both current sources and
unchanged geometry. One existing native worker computes shape metrics, streams
and atomically publishes the captured table. See cell-object-tables.md.
Immutable object blocks and per-run status capture reduce owner-thread work.
The 32768-row recording captures at 3.83 ms p95; the 1000-image/104097-object
selected viewer measures 16.87 ms p95 CPU frame work during export, with 5.68 ms
GUI capture. The initial 131072-row strict capture measured 18.68 ms p95. Historical document CSV retains its original byte digest.

The forty-eighth packet removes per-cell export-row/status allocation from joined
capture. Immutable block handles, referenced run/status maps and class colors
are sufficient to derive the same rows on the worker. Strict geometry checks
remain on the owner thread, and an emitted-row guard protects file publication.
Mixed-status document/filter/project CSVs match their previous bytes. Capturing
32768/131072 rows measures 0.49/2.24 ms p95; the 1000-image selected viewer records
16.85 ms p95 CPU frame work and 2.85 ms GUI capture. The 131072-row logical payload
falls by about 10.8 MB. See cell-object-tables.md for scope and raw evidence.

The forty-ninth packet adds explicit native file observations through Python and
ImGui: bounded streaming SHA-256, filesystem identity/write/change checks,
progress/cancellation, immutable results and optional atomic JSON. Input paths and
hardlink/renamed aliases are protected from JSON replacement. A 1 GiB warm file
hashes in 658 ms at 1555 MiB/s with 0.074 ms request and 0.091 ms maximum poll;
the 100000-object selected viewer records 16.88 ms p95 CPU frame work while hashing.
See file-fingerprints.md. This observes a chosen file at hashing time; it does not
retroactively bind archived reader bytes or enumerate a multi-file slide.

Next strengthen first-read source observations and decoder-read continuity, enumerate slide
file dependencies, complete native binary/Python/runtime inventories and portable
manifests, and extend measurement registries and annotation joins. Continue
numerical comparisons and improve initial
rendering and metadata/capture latency. Preserve all unfinished
M2 work: OS/DPI delivery, resize/minimize, docking, fluorescence/ICC, prolonged
cache pressure and vendor benchmarks. Persistence, interchange, analysis,
classifiers, scripting and packaging work under M4-M7 remains active.

Outstanding M0 decisions: project license choice
(asked asynchronously; no license selected yet), broader malformed-image
fixtures and numerical QuPath comparison tolerances. GEOS dependency selection
and packaging are now recorded with source identity and independent tests.


The fiftieth packet adds project/document ROI channel-mean and tissue-component
CSV tables, native immutable capture, historical calibration and source build
identity, bounded streaming/background atomic export, cancellation/progress and
Python/ImGui controls. Empty tissue runs retain explicit summary rows. See
measurement-tables.md. Full M0-M7 scope remains open, including portable
execution manifests and file observation association with reader generations.

The desktop controls are visually reviewed in a 1440x1000 capture.
The final benchmark with 1000 images and 100001 selected objects captures 8000 ROI runs
at 1.483 ms p95 and 8 tissue runs / 32768 component rows at 0.204 ms p95. Desktop
CPU frame p95 is 16.997 / 16.852 ms; background CSV writes take 20.397 / 38.031 ms
outside the viewer. See measurement-tables.md and its raw benchmark for exact
artifact fingerprints, workload bounds and desktop timings. Larger archives,
complex shapes, real slide catalogs and other platforms remain unverified.

The final installed GUI wheel passes 1011 Python cases with the two existing
Windows file-symlink permission skips in 238.56 s. All native suites pass in the
full/core/minimal builds (29/28/28). Source and binary fingerprints in the raw
benchmark identify this final dropdown-width build. The 40-case focused
workspace/export run also verifies the readiness-based same-file replay.


The fifty-first packet adds native reader generations and guarded opening
observations for PPM/OpenSlide, immutable analysis capture, QPV2 persistence with
QPV1 compatibility, native fingerprint associations and atomic JSON export in
Python/ImGui. Lazy reader descriptors publish independently of the decoder lock;
first-request unopened descriptors remain explicitly unknown. Comparisons report
matching metadata, changed versions, different files, incompatible schemes or
unobserved sources. They do not prove historical decoder bytes or enumerate
multi-file dependencies. See reader-source-association.md. The full M0-M7 goal
remains active, including complete manifests, reader continuity and all other
unfinished plan work.

The final installed GUI wheel passes 1028 Python cases with two existing Windows
file-symlink permission skips in 240.06 s; the focused source/fingerprint/
provenance set passes 62 cases. All native suites pass in full/core/minimal
builds (30/29/29), including reader generation, opening guards, legacy codecs,
foreign-host paths and native worker lifetimes. The sidebar is visually checked.
A warm synthetic 128 MiB associated observation takes 84.58 ms on the worker,
with 0.066 ms request and 0.048 ms maximum sampled poll. Cached descriptor and
analysis capture measure 0.3 / 1.8 microseconds p95. The 100001-selected-object
viewer records 16.87 ms p95 CPU frame work during associated hashing. See the
reader association documentation and raw benchmark for exact artifact hashes,
workload limits and remaining scope.


The fifty-second packet implements native OD-plane stain estimation, deterministic
bounded reservoirs, automatic existing-level selection under read/work limits,
quality rejection, model content identities, atomic JSON and exact normalized
model reload. Python jobs and ImGui estimate/apply/settings controls use the same
kernel and preserve exact fitted values in subsequent analyses. Independent
NumPy SVD/quantiles and reservoir checks, known mixtures, coarse pixels, masks,
GIL/lifetime/stale-result behavior and GUI analysis equivalence pass. See
stain-estimation.md. This adds the explicit M5 estimation workflow; full QuPath
numerical comparisons, model registries, batch/live scripting, portable manifests,
source continuity and all other unfinished M0-M7 scope remain active.

The final installed GUI wheel passes 1059 Python cases with two existing Windows
file-symlink permission skips in 244.13 s; the focused estimator suite passes 31.
All native full/core/minimal suites pass (31/30/30). The visually reviewed sidebar
shows fitted vectors and estimate/apply status. The synthetic 1048576-pixel
benchmark retains 200000 of 915836 eligible tuples in 6989824 logical work bytes;
estimation takes 25.48 ms, capture p95 0.0071 ms and maximum sampled poll 0.0128 ms.
Known-direction errors are 0.184 / 0.0038 degrees. The selected/all-visible
100001-object viewer records 26.79 ms estimation and 0.62 ms p95 CPU frame work;
that frame metric excludes presentation wait and does not assert a refresh rate.
Raw data records exact source/native hashes and the one-PC synthetic scope.


The fifty-third packet implements native headless project processing for ROI
OD/stain means, tissue and cell workflows. C++, nanobind snapshot/jobs and a
packaged standalone CLI share selection, bounded capture, per-image failure
policies and cancellation. Each completed image publishes an independent
SQLite project checkpoint containing archived results and generated-only CSV;
ordinary saves invalidate completion metadata. Verified restart recomputes
requested pixel-stream digests and checks canonical project/metadata/CSV
checksums before reuse. Linked pixel changes create new results, while edited
or damaged matching checkpoints fail explicitly. See [project batch
processing](project-batch.md).

All 22 focused installed-wheel cases pass, including direct-kernel/independent
CSV comparisons, snapshot lifetime, coarse-grid hashes, changed linked pixels,
corruption/invalidation, failure policies, budgets, output locking, Unicode CLI,
long-path repeated publication, optional cell publication and viewer reopening.
Native tests additionally verify cancellation after an image checkpoint and
restart of remaining images. Full/core/minimal native suites pass 32/31/31.

The 16-image synthetic tissue workload with 100000 background detections charges
51.34 MB of snapshot payload. Capture p95 is 3.129 ms, submit 0.0458 ms and maximum
sampled poll 0.0182 ms. First processing takes 1377.46 ms and verified restart
938.20 ms. With 100001 selected/visible objects, concurrent batch processing
records 16.85 ms p95 CPU frame work, excluding presentation wait. The raw Windows
benchmark records exact native/source hashes. These payload limits are not an
RSS cap; the CLI still loads the whole input model, and one writable image,
algorithm/table/codec buffers and SQLite can coexist with the snapshot.

This establishes the defined native headless workflows, not complete M6 parity.
Owned NumPy arrays, live scripting/stubs, a GUI batch launcher, abandoned-work
cleanup, portable/distributed workers and long-running real-slide memory and
throughput validation remain. All other unfinished M0-M7 scope remains active.


The final installed GUI wheel regression passes 1081 Python cases with two
existing Windows file-symlink permission skips in 247.04 s. Native full/core/
minimal suites pass 32/31/31; a separate storage-disabled build compiles the CLI
and passes the focused batch/digest/storage-rejection check. Source fingerprint
verification is included in the full installed-wheel run. The benchmark above
uses that same native artifact. The full M0-M7 goal remains active.


The fifty-fourth packet implements M6 NumPy exchange and public typed declarations.
Native RGB images share immutable backing; Document.image returns an owning Image
copy so wrappers and exported arrays survive reader replacement. RGB/RGBA imports
copy exact strided uint8 pixels into native buffers. Region, float and label views
are read-only, with explicit independent writable copies; masks materialize
independent uint8 arrays. The complete region example opens pixels, annotates,
analyzes, exports project measurements and saves/reopens in the viewer. See
[NumPy exchange](numpy-arrays.md) and [Python API declarations](python-api.md).

Wheels include generated native/public stubs and a PEP 561 marker. Generation
checks match the installed extension. Strict mypy accepts the declarations,
example and positive workflow and rejects five intended cases, including native-
only result construction. Installed tests cover public exports/class members,
ownership after relink/closure/eviction/archive removal, strided imports, exact
bytes/dtypes, read-only enforcement, concurrent native job completion, invalid
inputs, interpreter shutdown and example/UI reopening. All 41 focused cases pass
on NumPy 2.5.4 and 1.26.4. A clean environment without NumPy still imports and
reads native regions.

The full installed-native regression passes 1122 Python cases with two existing
Windows file-symlink permission skips in 272.65 s. Full/core/minimal native suites
pass 32/31/31; storage-disabled image-server and batch checks also pass. A final
static-only constructor annotation correction is packaged and the 41 focused
cases, stub regeneration and strict positive/negative type checks pass again;
the exact native binary remains unchanged.

The warm synthetic 1024x1024 benchmark records a 4 MiB region view and float32
view at about 0.0004 ms p95 (metadata/ownership work, near timer resolution).
Writable region copies take 1.905 ms p95 and the older Python-bytes copy route
1.429 ms. RGB import takes 1.309 ms p95 and a reversed/column-sliced import
2.538 ms. This establishes avoided payload copies for views, not universally
faster copies or a process memory cap. Raw data includes exact source/native
hashes. Live viewer queues, model adapters, beta API compatibility, broader
platform/long-running benchmarks and all unfinished M0-M7 work remain active.


The fifty-fifth packet implements managed live viewer scripting. A native-owned
workspace opens from a project filename through the background loader. Python
producer threads submit bounded typed commands while show(session) releases the
GIL on the main thread. Edits, selection, archival publication and save
reconciliation remain on the GUI owner; no mutable live Document/Project or
Python callback crosses into workers. Queries, region arrays, all three analysis
jobs, record-filtered tables, saves and normal/explicit-discard close produce
thread-safe ticket notifications. Workspace epochs, optional checked revisions
(including zero), captured requests and source-reader identity reject stale work.
See [live viewer sessions](live-viewer.md) and the complete live workflow example.

Twenty focused installed-wheel cases pass. Native tests cover independent owner
dispatch, simultaneous wait/take, cancellation/reclamation, source removal,
workspace epochs, stale results, resource limits and renderer revision semantics.
The full/core/minimal native suites pass 33/32/32; storage-disabled viewer and
reader probes also pass. The endpoint imports and handles queue cancellation in
a clean environment without NumPy. Public stubs regenerate exactly; strict mypy
accepts both examples/positive fixtures and rejects seven intended mistakes.
The script status/sidebar and scripted ROI are visually checked.

Performance investigation found avoidable canvas changes from variable-height
status and archival-only document revisions. The status row is now fixed-height,
geometry/class/calibration commands advance a distinct render revision, and the
loader prepares the active spatial index before native ownership handoff.
The same mixed 100001-selected-object workflow records 17.0061 ms p95 GUI CPU
work, 17.2125 ms p95 query round trips, 0.0068 ms p95 submission and 0.1032 ms maximum
dispatch/capture. The earlier run measured 49.838 ms p95 GUI work. Both raw runs
are retained with their exact binaries/sources and scope. No idle warm frames were
added to the mixed run; finite visual-capture runs use a distinct scope tag.

The maximum cold frame is still 313.73 ms and this mixed scene does not establish
the 16.7 ms target. Broader M2 rendering/startup optimization, live command
coverage, versioned external models, beta compatibility, real-slide/long-running
memory campaigns and all unfinished M0-M7 requirements remain active.


The final installed GUI wheel passes 1143 Python cases with two existing Windows
file-symlink permission skips in 279.33 s. The combined live-session/workspace
focused set passes 34 cases, including delayed save-and-replace at a finite frame
boundary. Full/core/minimal native suites pass 33/32/32; storage-disabled probes
pass. Stub regeneration matches the installed module, both examples and positive
fixtures pass strict mypy, and seven intended negative cases reject. The standalone
live CLI example saves/reopens its output, and visual inspection checks the fixed
status row and scripted ROI. The raw final performance record uses this exact
native artifact. The full project goal remains active.

The fifty-sixth packet adds opt-in, bounded per-frame CPU phase profiling and
removes redundant selection-tree searches during canvas rebuilds. The trace
separates driver submission from spatial queries, geometry and model/UI work;
4096 records are retained without frame-loop I/O. Dense selection membership
uses a sorted walk, full selection uses synchronized counts, and tiny views
of large selections keep sparse lookups. Existing geometry/order/labels/colors
are preserved. Exact canvas comparison covers dense versus sparse membership.

The initial trace isolates a 272.74 ms first OpenGL render before objects become
visible; a subsequent 100001-object geometry rebuild takes 30.57 ms. The new
mixed workload records 17.176 ms p95 and 276.13 ms maximum, versus 17.066 ms /
272.91 ms in the profiled baseline. These runs do not establish a speedup or meet
the cold-frame goal. Source/native-identified raw traces and matching workload
records are retained under docs/benchmarks; see live-viewer.md for timing scope.
The 16 overlay/profiling cases pass, including exact dense/sparse canvas pixels;
all 33 native tests pass. The broader project goal and M0-M7 gates remain active.

Final installed-wheel validation passes 1145 Python cases with two existing
Windows symlink permission skips in 309.94 s. All 33 native cases pass; public
stubs match the installed module. The dense/sparse fixture is visually inspected
in addition to its exact canvas comparison. Benchmark/trace records use this
same native artifact; the full project goal remains active.

The fifty-seventh packet implements the desktop launcher for the existing native
project batch engine. A dedicated ImGui window opens from the toolbar or
Ctrl+Alt+B. It supports ROI/tissue/cell workflows, whole-project matching area
objects or explicit active-image selection, current analysis settings or typed
presets, Stop/Continue, checkpoint reuse and optional output cell publication.
Its bounded summary shows progress and the first 20 image outcomes; the full
structured report retains every outcome. Output checkpoints are independent of
the live workspace, so switching images or replacing the workspace cannot cause
late batch publication into the current model. Closing/returning cancels
unfinished work and settles the worker, retaining durable completed checkpoints.

Managed viewer entry copies the optional batch preset before releasing the GIL.
GuiReport exposes a nullable native batch result and terminal state/error; optional
presets also accept None. Python stubs are regenerated and the typed fixture
covers these fields. Thirteen focused desktop cases pass, including all three
workflows, resume, cell publication, selected scope/image switch, empty-scope and
missing-directory rejection, Stop/Continue after a missing source, finite exit,
explicit cancellation, current white values, managed native-owned viewing and floating-window focus.
The full/core/minimal native suites pass 33/32/32. See project-batch.md for the
launcher, readiness contract and remaining broader batch/M6 requirements.


The final installed GUI wheel passes 1158 Python cases with two existing Windows
symlink permission skips in 268.18 s. All 33 native tests pass on the final build;
core/minimal suites pass 32/32. Public stubs match, and the typed fixture plus
both examples pass strict mypy. The final launcher is visually inspected; its
focus regression verifies that clicking the canvas cannot hide the batch window.
The 100001-selected-object desktop batch trace records 3.837 ms frozen capture,
39.52 MB logical snapshot charge, 777.31 ms batch processing, 17.049 ms p95 across
all 55 frames and a 220.86 ms cold maximum. Raw source/native-identified records
and every frame phase are retained in docs/benchmarks. Report-only warm timings
are distinguished from the all-frame trace. These observations do not close the
cold-render target, broader platform/slide performance or remaining M0-M7 gates.


The fifty-eighth packet adds reproducible comparisons against the official pinned
QuPath v0.7.0 Windows runtime. Bundle SHA/size and all 496 extracted file identities
are verified; a fresh public download/extraction is exercised. Independent Groovy
API output and a real native-export/QuPath-import/export round trip are retained
with generator/contract/data hashes. Ten primary objects cover flat classes/RGB,
Unicode, analytic and general shapes, cells/nuclei and UUID remapping. Exact
pixel-center masks and 108 tiled OD/stain mean jobs match in both directions;
OD/stain errors stay below 1.1e-7 for two white models and 260 RGB triples.

The corpus retains pre-export and decoded geometry/masks: QuPath serialization
changes a subpixel rectangle from 441 to 400 covered centers. Ellipse perimeter
methods differ explicitly, with a separate 0.05% engineering budget and independent
dense quadrature of the native result. A half-pixel hole agrees exactly. A very
thin ellipse has a genuine decoded-envelope disagreement of about 0.12 pixels;
it remains a named gate, strict expected failure and partial/exit-2 report.
No coordinate tolerance was widened to hide it. See qupath-comparisons.md and the
source/native-identified raw report for the exact scope, values and exclusions.

Sixty-five focused reference cases pass with one expected geometry failure,
including live regeneration from a freshly downloaded/verified runtime cache.
The combined reference/interchange/stain/analysis set passes 128 cases with that
one expected failure. Native application code is unchanged from the prior
1158-case installed-wheel validation. These checks introduce no new renderer or
whole-slide performance claim. Thin-ellipse decoder handling, broader workflow/
segmentation comparisons, CI/distribution, license/fixture decisions and all
unfinished M0-M7 requirements remain active.


The fifty-ninth packet resolves the captured thin-ellipse decoder gate with an
explicit native import policy. `qupath_ellipse_precision=True` runs GEOS valid
0.01-pixel precision reduction and then derives an envelope from separately
rounded float extrema, matching pinned QuPath v0.7.0. The default raw envelope
and native analytic hints remain exact. Changed matching bounds require loss
permission and emit a counted issue; collapsed/unrepresentable inputs reject
before objects/classes/IDs change. Undo/Redo and project persistence retain the
canonical matched geometry. Native, Python/file and ImGui import paths share it;
GUI choices and public stubs expose the policy without a Java dependency.

A fresh pinned runtime capture now reports passed with no unresolved captured
gates. Thin-ellipse bounds and area match exactly, its masks/counts/means agree,
and lengths satisfy the unchanged method-specific ellipse budget. The previous
strict expected failure is a passing regression. Raw-mode bounds remain in the
report rather than being erased. Full/core/minimal native suites pass 33/32/32;
a separate interchange-enabled, GEOS/project/Python/GUI-disabled native probe
also passes. Focused precision/interchange cases pass 27 with one backend-specific
skip; the combined precision/reference set passes 75 with that skip.

The final installed GUI wheel passes 1233 Python cases with three expected skips
(two Windows symlink permissions and the GEOS-disabled-only Python case) in
272.23 s, with no expected failures. Public stubs match and strict mypy accepts
the typed fixture and both workflow examples. The import controls and conversion
notice are visually inspected. A 1000-ellipse one-time import benchmark records
84.10 ms median raw / 342.22 ms matching (358.16 ms matching maximum), with exact
source/native identity and cost scope retained. This is import overhead, not a
new frame-rate claim. Broader interoperability, M2 cold rendering and all
unfinished M0-M7 requirements remain active.


## Archived REGION1 card before native verification / 2026-11-05

# Current implementation task

Follow the [single remaining queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. GRID1 is locally verified in its bounded area-grid scope.
Current: **R7-REGION1 — retained natural region intensity statistics**,
**contract and pre-layout witness complete; production implementation next**.

## Contract and captured compatibility evidence

[Contract](region-intensity-statistics.md), [trace/witness](benchmarks/region-statistics-r7/README.md):
reuse the existing cell compensated moments and analysis capture/loop/jobs/records/
tables. Add opt-in RGB/transformed sum/extrema/sample SD, preserving existing
float32 input values, old mean accumulation, source/ROI/alpha/plane/transform rules.
Use absent singleton SD and the existing empty-support error. No second source
read, reader adapter, detection wrapper or whole-plugin parity claim.

The [five mean-only records](../tests/fixtures/region-intensity-v1/manifest.json)
cover OD/stain, geometry/winding and singleton cases, signed channels and independent
moments. All old means reopen/replay/verify. [Native sizes](../tests/fixtures/region-intensity-v1/native-boundaries.json)
were captured against unchanged production headers: result256/request24824/record288,
channel stats104/compartment216 bytes. Old payload2141/2149, table67948/67956 and
batch111105/111137 bounds are pinned. Values are MSVC/x64; do not assume other ABIs.
Use lazy immutable statistics ownership instead of inlining216 bytes into all
mean-only records; charge enabled storage explicitly and preserve old defaults.

## Next bounded task — settle owners/storage and implement native route

First action: trace AnalysisResult/AnalysisRecord inline owners, snapshot/project
formats, classifier mean consumers and table/batch byte accounting using discovered
paths. Existing analysis SQL has fixed mean fields. Choose a versioned optional
statistics payload through current persistence; measured records must reject
missing/inconsistent payloads. Keep old-only replay and CSV unchanged.

Settle known request/result, GUI option/report and batch metadata layouts together
before the first production build. Then share the cell moments at a neutral
internal boundary, add explicit AnalysisRequest.with_intensity_statistics capture
and reduce accepted RGB/existing transformed samples in the same loop. Do not
change old mean sums/order or introduce another result/sampler/reader authority.
Use the contract's tiny expectations, alpha/nonrectangular eligibility, singleton/
empty/cancel/stale cases and the captured old bounds. Build only affected native
targets while debugging; one bindings/stub pass after all consumers settle.
Stop with the complete native retained/replay/export route verified before staged
thin-consumer closeout. Preserve failures and update this review before each retry.
No new wheel/install/stub generation for the native implementation step.

## Reusable product state

GRID1 installed f72c1992+dirty remains frozen (docs-only package stamp).485 native
inputs/65 installed members match, with staged evidence reused by explicit source/
configuration proof. Runtime: build/ci/project-metadata-r5-env; probe uses
build/tissue-grid-r7/stage. Root .venv is build/static only. The new observer target
used the existing cache and refreshed its own provenance; it did not replace the
installed wheel. All known handles are terminal. Openly licensed readers/codecs
only; wider research/resources/hosts/release and all M0-M7 remain open.

## Latest task review — pre-layout native witness (2026-11-05)

Outcome: actual C++ record/request/result/statistics sizes and payloads captured;
four header hashes equal the frozen production headers, all five table bounds
match, and the saved archive hash is unchanged. No production layout change.

Waste/action: first observer compile assumed pointer records, but the API returns
AnalysisRecord values. Read the signature, used explicit const AnalysisRecord&
and rebuilt only that target. Keep both build logs and the run. Earlier Python
capture's unbound-method failure and preserved/resumed archive remain documented.
Inspect actual C++ return ownership and Python bindings before observers; no
copying conventions from neighboring APIs. No binding/wheel/install/stub campaign.

Plan adjustment/next: the necessary C++ witness is complete. Use its216-byte DTO
measurement to choose lazy stats ownership and settle all known umbrella-header
fields before production compilation. Next action is the owner/storage trace
above. Review actual waste and update the plan before each next task/retry; the
full objective remains active.


Native implementation state: shared moment/compartment wire helpers retain cell
layout/order; opt-in region capture produces a lazy216-byte statistics value in
the existing pass. Known result/request/GUI option/report/batch/managed declarations
are settled before compilation. New algorithm markers require a QRI1 payload;
schema27 adds a keyed optional payload table through the existing project/snapshot
writer/loader. Replay/verification and ROI CSV2 are wired; old-only CSV1 and empty
statistics metadata preserve the old charges. Next complete native regression
cases and inspect coupled caller/accounting checks before one cached native build.
No binding/stub/wheel/install has run for REGION1. Consumer implementations remain
pending after the native retained route; no extra result/reader authority.


Pre-build owner review: analysis blocks/snapshots share immutable record pointers;
project SQL is the single retained writer/loader. The optional QRI1 sidecar is
bound to parent recipe/input/means by checksum; schema27 rejects orphan/missing
measured payloads. Classifier mean consumers use the retained means; the central
sampling-policy resolver admits measured algorithm markers. Viewer events charge
the enabled heap; default record charges subtract only the inactive pointer from
existing bookkeeping. New batch/managed native flags use the same factory. All
known public report/options declarations are already included. Native regressions
cover independent moments/domain, alias freezing, codec/replay/project/CSV and old
payload/table/batch bounds. Next one cached native candidate; no bindings/package.


---

# Snapshot2026-11-14: completed native format delivery

These cards were archived during the owner-requested cleanup. Their next actions
are historical. See [the current card](current-task.md) for live scope.

# Current implementation task

Owner scope update:100% QuPath parity is not required. Small cosmetic differences
are acceptable when results, interpretation and workflow usability are unaffected.
The [scope policy](api-delivery-plan.md#result-fidelity-and-scope-policy) and AGENTS.md
supersede earlier full-target instructions. Unselected inventory gaps are optional;
do not start work simply because a row remains Partial. Scientific correctness,
calibration, persistence/export/replay and meaningful overlay/display checks stay.

Owner format scope update2026-11-13: Java/Groovy and QuPath Java-extension
compatibility are excluded. Additional formats use native plugins, with each
plugin's actual license, dependencies, notices and source/distribution obligations
recorded under the [reader policy](reader-license-policy.md#format-plugins-and-commercial-use-eligibility).
No noncommercial/research-only implementation or required material is admitted.
The existing plugin framework is delivered; PNG is the first selected additional
native plugin. The separate UI workflow stays outside this thread.

Format coverage planning is complete in the
[coverage plan](format-plugin-coverage-plan.md). Retained installed catalog queries
pass (four image backends, three matrix and two annotation formats); a generated
RGB8 PNG is refused by the automatic route. Exact commands, binary/source hashes,
catalogs and bytes are retained in build/format-plugin-coverage/results.json.
An independent decoder confirms all12 authored PNG sample bytes; diff checks
and59 local documentation links pass. Stale H5AD and Java inventory labels were
reconciled with delivered scope.
Primary libpng/JPEG license evidence and candidate pins are recorded; dynamic
image plugins are distinguished from fixed family catalogs. That planning pass
introduced no decoder or product build. One probe-controller error-caption guess
was corrected against the native source; the expected unsupported-input result
is retained.

PNG is delivered locally: [receipt](benchmarks/png-reader-plugin/README.md),
`plugins/png` and build/png-reader/results.json. The independent module preserves
all declared static color/depth/Adam7 raw samples, separate RGBA8 display and
pHYs calibration. Final outside-checkout module098cb3a1 passes33 positive profiles,
10 exact refusals, decoded-memory/cancel/ABI/UTF-8 checks and8 native SDK/CLI routes.
Cross-location project replay, unchanged built-ins/families, bad-hash refusal,
88-member stage/ZIP admission and removal preserving user files pass. All54
frozen archives/objects/host/SDK inputs remain exact. The outside comparison is
retired into the task evidence directory; no host, binding, GUI or wheel build.

Corrections: zlib's required contrib controller and prefixed consumer headers;
three test-only coverage builds; staging path-key/diagnostic spelling and
package-relative guide/source-license fixes. Native assertions and exact
identities remain intact.25 admission/deployment tests pass with one existing
Windows symlink skip;90 repository/package document links pass. Failures, passing
logs and source contexts are retained. Stop at this selected plugin handoff.
No source commit or publication is selected automatically.

## Delivered - FP04 external matrix and annotation plugins

Owner-selected NRRD, ICS and external matrix/annotation workflows are delivered
locally. The [family guide](format-family-plugins.md),
[receipt](benchmarks/format-family-plugins/README.md) and
build/format-family-plugins/results.json own this final selected outcome.
External counts TSV and point TSV use separate typed C ABIs, hash-bound modules
and native services. Exact UInt64 counts/IDs, complete position joins, explicit
frames/classes/planes, atomic new-only QMX and oneundo annotation append persist
native receipts and source/native mappings. QMX schemas1–3 and old hashes stay.

Matching native source fb5289df, installed host/SDK/CLI and source snapshot are
retained separately from frozen earlier products. Python/GUI OFF; no binding,
wheel, UI or publication change.76 native/SDK/CLI commands,18 positive profiles,
50 exact refusals and23 selected native suites pass. Native source-disabled
branches compile; this is not a second linked-profile qualification. Both modules
rebuild offline outside the checkout against the installed C kit, without core/
Python/codec discovery.32-member source/kit/module stage and ZIP admission,
relocation, seven-image-provider exact display and removal preserving user outputs
pass.18 material/deployment tests pass with one Windows symlink skip. All68 frozen
artifacts and prior inventory groups remain exact; checked outside proof retired.

Corrections: controller absolute paths/binary test logs; native quantitative TIFF
fixture and exact native outside-image refusal; genuine historical schema fixtures;
valid directory metadata test path; platform profile/header line endings retained
with actual hashes. A probe isolated a pre-existing47-byte inactive directory
provenance charge. Native legacy bookkeeping now preserves the original
11489+path-adjusted exact/one-below witness while fully charging active directory
metadata. Only affected sources/test drivers and cached provenance relinks followed
the initial combined build. No scientific/identity/quota assertion was weakened;
failed evidence and intermediate products are retained.

Stop at the selected local handoff. Broader unreviewed/excluded/unselected format
variants, directory-store import, geometries, writers, remote IO and optional
FP03/FP05/FP06 work remain conditional on a concrete need. The full format roadmap
is not complete. No commit, push, public release, biological or speed claim.

## Delivered - FP02 ICS fluorescence file-set plugin

[ICS receipt](benchmarks/ics-reader-plugin/README.md), plugins/ics and
build/ics-reader/results.json bind final outside-checkout moduledf9af405.
66 exact profiles,60 refusals and11 ABI/SDK/CLI routes pass on the first combined
build and separately rebuilt installed module, including native libics1.7.2
reference comparison. Scalar/channel-first UInt8/UInt16/Int16/Float32 bits across
C/Z, all24 Float32 byte permutations, explicit calibration and saved channel/
window/default identity stay exact. Primary-only discovery and guarded adjacent
IDS/inline source-set changes refuse currentness without mutating history.

Independent parser/fixtures/kit retain AGPL-3.0-only. Maintainer-defined open ICS
and pinned dev-only libicsc1a1ae6f retain their actual distinct Library-GPL2-or-later
source grants and bundled Lesser-GPL2.1 text.19 unmodified reference source/header/
config/notice members and the reference executable are hash-bound; no reference
runtime/code/image/archive/zlib enters the product.22-member offline source/
stage/ZIP, seven-plugin coexistence, old catalogs/P6, bad hashes, cross-location
replay and removal preserving user files pass. All66 prior frozen inputs and old
inventory groups remain exact; outside proof retired after checked boundaries.
18 admission/deployment tests pass, with one existing Windows symlink skip;
package links, source/diff checks and census determinism pass.

Corrections before compilation: disabled-zlib include closure, a local C++
declaration, selected-sample cancel interval, canonical end-TAB and actual Windows
missing-companion control. Mutable SDK cases use fresh isolated source pairs,
since restoring bytes does not restore filesystem versions. One malformed doc
patch was corrected. No native failure/repair build or relaxed scientific/status/
identity assertion. General acquisition/axes/compressed/private-source/precision
forms remain outside the selected ICS profile. The full format goal stays open.

## Delivered - FP02 scalar NRRD plugin

[NRRD receipt](benchmarks/nrrd-reader-plugin/README.md), plugins/nrrd and
build/nrrd-reader/results.json bind final outside-checkout module141a85c4.
50 exact profiles,56 refusals and nine ABI/SDK/CLI routes pass on the first combined
build and independently rebuilt installed module. Scalar UInt8/UInt16/Int16/
Float32 raw bits and all Z planes remain exact; native display windows are persisted
separately, nonfinite display never changes raw validity, and calibration is
explicit or unknown. Window/default/source/module identity cases pass.

20-member offline source/stage/ZIP, six-plugin coexistence, old catalogs/P6,
bad hashes, cross-location replay and removal preserving user data pass. All64
prior frozen inputs and earlier inventory groups remain exact; no engine/SDK/
binding/GUI/wheel build. The outside proof is retired after checked boundaries.
18 admission/deployment regressions pass, with one existing Windows symlink skip;
source/package links, census determinism and diff checks pass.

Corrections were source/reference-only: bounded field views, axis/order and unit
range checks settled before compilation; pynrrd's case/type-spelling limitations
use explicit canonical same-payload seeds. Its documented exception class was
caught only for those variants. No native failure/rebuild or relaxed scientific/
identity assertion. General NRRD transforms/detached/compressed/time/vector forms
remain outside the declared profile. ICS and external family workflows are next;
the full format goal remains incomplete.

## Delivered - format roadmap FP00 and selected FP01 profiles

Owner selected implementing the [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14).
FP00's178-entry census and all three demonstrated ordinary-input gaps are now
delivered: PNM/PAM, BMP and static lossless WebP, alongside the earlier PNG/JPEG.
This closes the selected FP01 outcome, not the full roadmap or all format variants.

WebP is delivered locally: [receipt](benchmarks/webp-reader-plugin/README.md),
plugins/webp and build/webp-reader/results.json. The final outside-checkout module
2926cf41 independently passes19 exact profiles,28 refusals and eight ABI/SDK/CLI
routes. Simple single-VP8L version0 retains literal UInt8 RGBA, hidden RGB and
hint-independent alpha; unknown calibration stays absent.32KiB raster and96KiB
codec-scratch refusals have passing normal-budget controls. Codec heap/header/
conservative bookkeeping is charged, with synchronous call/allocator cancellation
and no fixed-latency promise inside a codec call.

libwebp1.6.0 commit4fa21912 retains BSD-3-Clause and full PATENTS/AUTHORS;
63 unmodified code/header/build-list/notice members,29 generic C sources, no
upstream archive/image/test assets or encoder/mux/demux. Adapter/control/fixture
sources remain AGPL-3.0-only.90-member stage/ZIP, independent offline source-kit
build, five-plugin coexistence, old catalogs/P6, bad hashes, cross-location replay
and removal preserving user data pass. All62 prior frozen inputs remain exact.
Only the standalone module/consumers were built; the outside proof was retired
after checked path/worktree/reparse/process boundaries.

Corrections: source-only allocation accounting and cancellation scope settled
before compilation; Pillow's cleared-alpha-hint RGB adapter behavior was recorded
instead of weakening literal RGBA expectations. All initial native checks passed;
a stale fixture basename in the runner's final PAM assertion was repaired, then
the same module validated without a C++ rebuild. Failed evidence remains.18
admission/deployment regressions pass with one existing Windows symlink skip;
source/package links, census determinism and diff checks pass.

Owner selected all three next workflows: NRRD scientific images/volumes, ICS
fluorescence images and external matrix/annotation plugins. Deliver them in that
order, one outcome at a time. Other unreviewed/excluded/unselected rows stay explicit.
Separate UI work and the frozen engine/SDK/bindings/wheels stay in their own scope.
No commit, push, public release, biological or speed qualification is claimed.
Goal tracking could not replace the older paused `close all milestones` goal;
this does not fabricate its completion or full-format-goal completion.

## Delivered - FP01 BMP plugin

BMP is delivered locally: [receipt](benchmarks/bmp-reader-plugin/README.md),
plugins/bmp and build/bmp-reader/results.json. Final outside-checkout module
d479d459 independently passes62 positive profiles,40 exact refusals and eight
ABI/SDK/CLI routes. Literal RGB/palette indices, top/down rows, X-is-unused and
per-axis ppm calibration preserve their contracts. Four-plugin coexistence,
unchanged catalogs/P6, bad hashes, cross-location replay,20-member stage/ZIP
admission and user-data-preserving removal pass. All60 frozen earlier inputs
remain exact; only the standalone module/tests were built. Outside comparison
was retired after verified path/worktree/process boundaries.

Correction: one intended truncated-header fixture instead declared unsupported
DIB size0. Only the fixture changed to a truncated declared40-byte header with
accurate file size; expected I/O4 stayed intact and no C++ rebuild was needed.
Read-only evidence filename guesses were corrected against actual generators.
18 admission/deployment tests pass with one existing Windows symlink skip;
package links, census byte determinism and changed-source diff checks pass.
WebP and later format packets remain outstanding; full goal completion is false.

## Delivered - FP00 census and FP01 PNM/PAM plugin

FP00 retains/classifies all165 Bio-Formats8.5.0 rows and13 OpenSlide entries,
282 aliases/195 distinct suffixes and57 roadmap groups in the [census](format-plugin-census.json).
Independent completeness/offline determinism and actual OpenSlide4.0.1 evidence
pass;14 linked source units are build facts, not new vendor qualification.45
runtime/SDK hashes match. Authored valid PGM/BMP/WebP controls expose three gaps.

PNM/PAM is delivered locally: [receipt](benchmarks/pnm-reader-plugin/README.md),
plugins/pnm and build/pnm-reader/results.json. The final outside-checkout module
22d33110 independently passes79 exact profiles,36 refusals and9 ABI/SDK/CLI routes.
Native UInt8/UInt16 source codes, PBM/PAM polarity, alpha and explicit display
transfer preserve their declared semantics. Saved explicit options remain stable
when provider defaults change; fresh choices get distinct input/result identities.
Three-plugin coexistence, old catalogs/P6, bad hashes, cross-location project replay,
20-member admitted stage/ZIP and user-data-preserving removal pass. All58 frozen
native/PNG/JPEG inputs remain exact; only the standalone module/tests were built.

Corrections: HTML optional-tag/link and literal-etc handling, explicit UTF-8 audit
reads; source-only descriptor/header/work review; one SDK consumer-only repair
requiring unknown calibration optionals absent instead of0. No scientific/status
assertion was relaxed. A receipt link to a repository-only census was made package
portable, then repackaged from the same module bytes.18 admission/deployment tests
pass with one Windows symlink skip; package guide/receipt links and diff checks
pass. The outside comparison was retired after verified boundaries and fail-closed
process-use checks. Existing source-archive aggregate labels remain distinct from
actual OpenSlide LGPL-2.1-only notices; FP00 introduced no material admission.
Remaining format packets are still outstanding; this is not full-goal completion.

## Delivered - plan for supportable format families

Owner-selected [full format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
extends the existing coverage authority:57 coverage entries and seven packets,
covering image/scanner/scientific arrays, matrices, annotations, projects, writers
and remote sources. Delivered/candidate/contract-first/unreviewed/excluded states
remain distinct; every new plugin retains actual schema/code/codec/fixture rights
and native scientific/source/publication contracts. No implementation/publication
is selected by this planning request; the separate UI workflow remains separate.

Primary upstream references and native v1/family source were checked.164 local
links and28 Markdown anchors pass;17 delivered PNG/JPEG product inputs match their
qualified source hashes, native root source is unchanged, and diff/whitespace
checks pass. No product build, material admission or corpus campaign. Evidence:
build/format-plugin-coverage/all-formats-plan-results.json.

Corrections: source inspection established v1's zero-selector RGBA and scalar
metadata limits; upstream SDK format lists are not runtime qualifications.
A broad anchor check found two historical UI headings renamed to Previous; only
their existing current-card link targets were repaired. One read command used a
PowerShell-only switch with rg; corrected native source searches resolved it.
Claude peer helper references are absent. Bare CLI could not access login; normal
read-only Fable/max then reached usage limit429. Both attempts are retained; no
independent peer endorsement or alternate-model review is claimed. Recommended
next packet is FP00's dated per-reader/runtime census and useful-gap ranking;
the census itself is not claimed complete. Stop at this planning handoff.

## Delivered - standalone JPEG native reader plugin

Owner-selected JPEG is delivered locally through `org.qupathxx.jpeg`:
[receipt](benchmarks/jpeg-reader-plugin/README.md), `plugins/jpeg` and
build/jpeg-reader/results.json. C/C++ owns canonical decoded8-bit Gray/RGB,
EXIF orientation1–8, physical calibration and strict profile/refusal rules.
The separate libjpeg-turbo3.1.3 source/notice selection preserves actual licenses;
all171 upstream members match the admitted archive. No upstream specimen/test
asset, NC/restricted material, Java or runtime Python dependency is introduced.

Final outside-checkout DLLe43f5750 independently passes40 exact profiles,
29 exact refusals, Unicode/budget/cancel/ABI and8 fresh native SDK/CLI routes.
Saved-source replay and exact implementation/source identities pass; equal
pixels do not authorize old implementation identity. PNG/JPEG coexistence,
unchanged catalogs, bad-hash refusal, cross-location replay and199-member
stage/ZIP admission/removal pass. User images/projects/results/notes survive.
All56 frozen native/PNG inputs remain exact; no engine/binding/GUI/wheel build.
The named outside comparison is retired and both paths have no active processes.

Corrections: pre-build marker-lifetime review and explicit codec `/MD`; initial
wrong-format error mapping; valid raw requests for the offline test; a cleanup
process-check quoting error corrected with fail-closed verification of original
and retired paths. One affected decoder/consumer rebuild; same-byte package/docs
refresh only afterward. Initial failures/logs remain; assertions stay intact.
18 existing material/deployment tests pass with one Windows symlink skip; the
initial unittest zero-discovery command was corrected to pytest. A shell listing
parse failure was corrected before execution. Final handoff passes306 local
repository/package document links, owned-source whitespace/diff checks, renewed
199-member archive/removal checks and preservation of all earlier inventory
groups. Results and corrected cleanup evidence are retained. Stop here: no next
format, commit or publication is automatically selected.
The separate UI workflow below remains outside this task.

## In progress - UI workflow safety in the isolated checkout

This thread's close-all-milestones goal is paused. UI implementation and checks
belong to the separate Claude/Codex workflow; this thread does not take over its
remaining milestones. Optional native Milestone N is not selected here.

The [safety plan](ui-workflow-safety-plan.md) belongs to sibling
checkout `qupathxx-ui-safety`, branch `feature/ui-workflow-safety`, based on
7c981bf9. Its separate Claude/Codex controller owns source, build and native
GUI sessions. Resume there; do not start a duplicate implementation here.

Milestone 1 is complete at immutable checkpoint
`refs/checkpoints/ui-safety/m1-review-fix1` (2145327f): 22 source contracts and
15 native commands passed inside the sandbox; the controller completed the
four remaining commands, including relative/absolute export feedback, Zarr
save/reopen and spatial publication. Evidence is that checkout's
`build/ui-workflow-safety/m1/` and `build/ui-workflow-safety/ledger.md`.
Its 14 native libraries and 38 binding objects are recorded unchanged.

Milestones 2 and 3 are complete at `m2-review-fix1` and `m3-review-fix2`
(6238b4e9) under the same refs namespace. Milestone 3's final receipt records
61 source contracts and 117 mapped native commands; real-path controller
verification records 104 native, 15 relative and five shorter-prefix checks.
The retained evidence also records unchanged native long-path limits; those
cases are not counted as passing. No optional native repair is selected.

Milestone 4 tasks 4.1/4.2 are complete at `m4-t42` (979c81ba). Task 4.3
(shared availability reasons, keyboard/button equivalence and generated Help)
is still validating in the existing worker. Its report explicitly says it is
not the handoff. The current `m4/t43-native-results.json` records all120 latest
native cases passing. Supplementary shortcut wiring passes absolute, relative
and long-path variants; command scope and the French presenter still belong to
the running dispatcher/presenter matrix. Finish that task and the milestone
review before starting Milestone 5. These changes are not merged or published.

Parallel integration preparation passes for checkpoint 979c81ba:
[preflight](../build/ui-workflow-safety-main/979c81ba/result.json), with its
retained source patch. All125 source/fixture/test paths apply cleanly to main;
public native headers are unchanged and the application.cpp additions are
GUI-guarded. Fourteen documentation files need reconciliation at handoff.
The helper now includes the inspected GUI entry-point changes and native test
inventories/fixtures; its scope remains an immutable source check. Source/index
hashes stay unchanged; no build or validation campaign was repeated here.

Next action: finish 4.3 and review in the existing checkout, retaining exact
result/identity/CSV assertions and diagnosed failure evidence. Then repeat this
cheap integration preflight against the final milestone checkpoint. Earlier
source checks and passing runtime evidence do not qualify later changes.

## Delivered integration - native workflows, SDK and format catalogs

Owner-selected seven-gap source integration is complete: GeneView QC controls;
A02 summaries/pseudobulk; A03 PCA/clustering; A04 replicate Welch/BH; native SDK/
material tooling; image reader plugins/saved sources; matrix/annotation catalogs.
Retained release/archive authorities join the current main UI without a new
algorithm, format, Python binding, optional workbench phase or release publication.
Root integrated; three agents restored/reviewed bounded native/reader/SDK lanes.

Decisive evidence: [integration receipt](benchmarks/native-format-closeout/README.md)
and workspace build/native-format-closeout/results.json.55 cheap checks pass
(one existing Windows symlink skip); five molecular suites,20 reader/family
commands (18loader/13saved-project cases,7H5AD oracles),11GUI routes and two
plugin startup smokes pass. Copied NuCLS Draw/Run/Keep/View/exact CSV/GUI Save/
reopen/Recheck preserves original identities/calibration/raw data. A fresh184-file
relocated SDK compiles119 headers and both consumers; new/historical plugin
projects and A01-A04 replay/CSV pass. Python-disabled root configure registers all
19 selected targets. No engine/binding build;14 libraries/38 objects stay exact.

181 core units/430 dependencies match frozen bc8 authorities (243raw exact,
187EOL-only). Engine identity remains bc8;859-input final consumer context is
f7f6693c. Native SDK metadata matches its actual engine; root configure metadata
is separate. Test-only placement/save-observation changes preserve all original
assertions, tolerances and the100-frame bound. Source/failed/passing evidence is
retained. Controller corrections covered notices/product/UTF-16/fixture paths and
Windows temporary-path limits; no scientific assertion was weakened. Optional
Claude planning timed out; three scoped source reviews and executable checks pass.

The scoped source commit/main merge is recorded in workspace
build/native-format-closeout/merge.json. Stop at that handoff. Preserve candidate8/
a10 products, recovery archives and local SDK/runtime. A02-A04 GUI/Python consumers
and broader platform/biological/parity claims remain separate. The owner's earlier
main-push authorization applies after the verified source merge; no new artifact
publication is selected.

## Delivered integration - workflow simplification on main

Owner requests merging the verified UI milestone. The main-based integration
carries the17 scoped presentation/test changes and UI-only layout/docking/
content-fit/catalog prerequisites, retaining grouped header/Help and twelve
parent tools plus existing child routes. Main's reader, annotation exchange,
clipboard, startup and gene handlers remain intact; unrelated native/SDK and
format-registry work stay excluded. All12 workflow findings retain their fixes.

Decisive evidence: build/ui-workflow-simplification-merge/results.json.20 source
contracts, one frontend-only cached build and nine native consumers/replays pass:
simplification, TMA/density, complex NuCLS Draw/Run/Keep/View/exact CSV/GUI Save/
reopen/Recheck, catalog, panels, protected exports, header/Help, target membership
and fluorescence Run/Add/Keep/replay/Undo. The91-input source snapshot owns this
main-based binary; earlier integration qualification is not relabeled. All14
cached libraries,38 binding objects and the original project remain exact.

All233 dirty old-integration files, its HEAD and separate index are unchanged.
One preparation text match was ambiguous for a standalone brace; positional
matching retained18 exact main native blocks before compilation. No product
build or test failed. Source and Git handoff are local; no kernel/binding/package
rebuild, push, interactive launch or optional phase4 expansion. The scoped commit
and resulting main merge are recorded in build/ui-workflow-simplification-merge/
merge.json. Stop at that handoff; the separately requested cleanup owns retiring
the verified old checkout after this task releases it.

## Delivered integration - grouped header and Help on main

Owner requests committing the completed header redesign and merging to main.
The main-based feature/grouped-header-help branch isolates the header/dialog/
caption/tests from concurrent uncommitted native and workflow changes. Reuse the
qualified private header and Help implementation plus its shared Analyze menu
catalog; bind the twelve tools to existing main controllers. Keep main's native
interchange availability and handlers. No unrelated dock/reader/science changes.

Changed integration inputs: main's gui.cpp and original organization harness,
shared locale/dialog headers, new private header/Help/menu helpers. Twenty source
contracts, one frontend-only cached build and the full --header-grouping replay
pass on this main-based consumer. Eight actual replays plus one placement probe
retain exact project/calibration/RGB state. All14 native-library and38 binding
object hashes remain unchanged. No failed build/run or correction in this pass.
Output: build/ui-header-merge; [receipt](benchmarks/ui-header-grouping/README.md).
Owner authorizes the scoped commit and local merge. Stop at that outcome; no
package, push, kernel rebuild or unrelated dirty-source admission is selected.

## Delivered UI - image inspector and workflow simplification

Owner approved real-project review, blocker fixes and a scoped local main merge.
Flattened the Image inspector; calibration uses a validated native modal with
Cancel/Apply/Clear/Undo. Read-only image/object information preserves literal
metadata and truthful RGB OD versus supplied-stain labels. Tables have one
kind/scope/destination form; saved-cell exports retain exact selected run identity.
Project settings, TMA and classifier sections expose the active workflow directly.
Dialog ownership, scrolling and translated primary captions follow the UI contract.

Actual copied-project review passed search/open, ROI draw/name/class, native
Run/Keep, one-row CSV, Save/reopen and explicit saved-result Recheck with matching
values. Fixed the observed result-link blocker: ROI/Tissue/Cells Go to tables
now selects that family and Current image, preserving cell-source preference.
Evidence: build/ui-workflow-merge; prior compatible receipts are
build/ui-panel-refinement/results.json and build/ui-analysis-details/results.json.
Exact scoped source passes 17 source contracts, 44 native workflow runs
(17 export/sampling and 27 panel/TMA), and actual ROI/Tissue/Cells result-link
navigation from a different prior table family/project scope. All three open the
correct table and Current image. The isolated frontend compiled against main's
native interfaces; frozen core archive hash stayed identical through linking.
Calibration and remaining unchanged flows reuse the prior source-scoped receipts.
Initial compiler setup quoting failed before compilation, then was corrected.

Validation uses scoped frontend consumers and the existing native authority;
no kernel/binding rebuild, science qualification, package or push. Reader, gene,
SDK and expression work are excluded from this commit. Stop at local merge.

## Delivered UI optimization - reviewed five-phase plan

Implemented the owner-requested five phases in the existing integration checkout:
guarded create-only GUI exports with registered replacement and independent import
paths; cached native sampling facts and honest units; workspace-only Eye visibility
with safe hidden input; shared native shape measurements/Keep/Table guidance; and
annotation-first virtualized rows with literal stable IDs and ordered selection.
Collapsed measurement exports poll independently and expose errors outside the
section. Existing CLI/API defaults, scientific authorities and bindings stay frozen.

Decisive evidence: build/ui-export-sampling/results.json. Four exporter suites,
tissue, sampling-only, ordered selection, shell schemas 1/2/3/coverage/locales,
replay contracts, six actual review flows and 17 export/sampling flows pass. All
14 source guards pass; 23 matching captures verify readable values and geometry,
including French/scaled confirmation and calibrated/unknown sampling. Standalone
starts with system-only PATH, no PYTHONPATH and unchanged project bytes; all
38 binding objects match. No broader biology/speed or observed-user claim.

Broader cell suite still refuses legacy-tight-grid.qpxproj at its work budget.
The preserved pre-change desktop reproduces it with identical fixture hashes;
baseline-only also fails while new sampling-only passes. Keep assertions and
quotas intact; the existing R6/R12 row tracks this separate issue.

Corrections: CRLF/15-column catalogue anchors and pinned CMake path; one forced
GUI-unit rebuild after snapshot reuse was finalized; translated capture runner
environment and legal replay coordinates; explicit resource-copy target because
references-disabled builds left old schema-2 assets; and non-repeating Delete.
Preserve failed logs/captures and the passing milestone-1 source/app. Reuse all
compatible scientific objects/results; no wheel, binding rebuild or new kernel.

Owner-authorized handoff: merge these UI changes and launch the qualified native
app. Preserve unrelated analysis/SDK changes. The merge/startup result is recorded
in build/ui-optimization-merge/results.json; frozen Python consumers and the
legacy quota issue remain separate scope.

## Delivered UI handoff - reviewed changes merged to main

This merge carries UI commit39823dee and the integration branch's completed
committed history into local main. The87-file UI commit contains project/browser
simplification, shared workbench styles, registered dialog/menu contracts and
their tests/resources/docs. Unfinished QC/normalization, aggregation/reduction and
A04 analysis edits remain separate in the integration checkout, including mixed
CMake, molecular GUI dispatch and task-card changes. The staged CMake delta is
only the organization-test GLFW dependency; the GUI retains its committed gene
controller signature. No unfinished analysis source was staged.

Nine source contracts pass on the exact staged UI tree. Shared native UI
components/tests match the qualified source snapshot; reuse the passing native
replay/workflow,47 captures, standalone startup and38 retained binding hashes in
build/ui-dialog-contract/results-final.json. The merge introduces no further
product changes, core/binding build, Python consumer or scientific campaign.
Source/ancestry/index/preservation checks are recorded in
build/ui-main-merge-20261109. The pre-merge138-file dirty-source snapshot is
verified. Concurrent analysis-file changes were observed and left untouched.

Correction: the temporary staged-tree source-test fixture first lacked assets/ui
(FileNotFoundError); exported the staged resources and all nine checks passed.
No product fix or repeated native build was needed. Preserve the active checkout,
cache, fixtures, qualified products and preview sessions. No push, release or
worktree removal was requested. Next: owner continues review from local main.

## Delivered UI task - enforce the dialog contract

Owner requests another audit and a proper contract. Findings: layout and drawing
duplicate action declarations; callers can bypass the frame; Escape ignores an
open child popup; title/actions and multiple notices can compete for viewport
space. Replace free-form composition with registered dialog kinds, typed actions,
one action definition for measurement/style/dispatch, mandatory owner/busy/submit
state, and one renderer owning header/body/notices/footer and popup lifetime.
Migrate all nine existing modal surfaces, including language, source review,
checkpoint deletion and import choice. Keep native handlers and data authorities.

Add enforceable source guards against raw/modal-frame bypass and unregistered
dialogs, plus native behavior contracts for role/order/identity, owner/busy locks,
single dispatch, nested Escape, text editing, long errors, resize/DPI and import
choice. Existing metadata and close save/recovery guards stay unchanged. Settle
types/callers/tests before one cached GUI/app/organization/replay build with
references disabled. Reuse unchanged science/rendering evidence; verify scoped
native flows, source contracts, standalone startup and38 binding objects. No
core/binding rebuild, Python consumer, wheel, commit or merge. Preserve products,
fixtures and live sessions.

Interfaces and nine callers are settled. Nine source guards pass. Added actual
native source review Cancel/Apply/Save-reopen, checkpoint Cancel/checked-delete,
and metadata blocked-Undo checks. Changed inputs are the contract, renderer,
controllers, menu wrappers and their focused test headers. Next decisive step:
one references-disabled cached GUI/app/organization/replay build, then typed
contract replay and scoped GL/native workflows. Source and38 binding-object
hashes are frozen under build/ui-dialog-contract before the build.

The first frontend build catches a shared_ptr-versus-observed-pointer comparison
in the added source ownership guard. Compare the locked pointer's get() to the
observed active document; native source identity validation is unchanged. The
checkpoint fixture also needs a real unsaved object because native autosave
correctly refuses clean projects. Both are corrected before native execution.
Reuse the compiled unchanged units; resume affected GUI/app/tests with references
disabled, preserving build.log as the failure receipt.

GUI and application now build. The organization test compile needs the public
source_identity.hpp definition, rather than its project.hpp forward declaration.
Add that test-only include and resume organization/replay targets; reuse the
passing production GUI/app. No source authority or core change is required.

Native contract replay passes, including owner/busy/refusal, nested/text Escape,
single dispatch, scoped metadata and non-writing file choice. Organization passes
through close Save/Cancel but fails the existing Discard recovery assertion.
Preserve native-artifacts-01 and add report details (frames/completed/path/error)
to isolate the failed term before changing behavior. Rebuild organization only;
reuse passing replay and production binaries.

Diagnostic receipt native-attempt-02.log shows frames28, completed0, nonempty
recovery path, no error and no surviving checkpoint. The fixed-frame test raced
publication: Discard is allowed to cancel the in-flight write. Correct the test
contract to require completed cleanup for either publication outcome, no error,
closed workspace and unchanged primary bytes. Add a separate deterministic
native request/wait/publish/recover/clear/wait check retaining completed==1 and
exact primary bytes, so published cleanup coverage is strengthened rather than
lost. No production behavior changes. Rebuild organization only.

Native organization, nine source contracts, standalone startup and38 retained
objects pass (results-before-contrast.json;49 PPM files includes fixture inputs).
Visual inspection catches metadata background inheriting the disabled workbench
alpha, even though controls re-enable. Move ModalInputScope before BeginDialog
so background and content share modal state; restore it after EndPopup. Add an
actual GL capture over contrasting blue and an opaque-surface assertion, alongside
the existing ambient-disabled interaction check. This is a form-readability guard,
not cosmetic screenshot equivalence. Rebuild the four affected frontend targets
and rerun their matched checks; retain attempt03 as diagnosed evidence.

Final matched result: build/ui-dialog-contract/results-final.json. Native replay
and organization pass, with nine source guards,47 actual UI captures (fixture
source PPMs excluded), standalone System32/Windows-only PATH startup and38
byte-identical binding objects. All nine modal controllers use one registered
renderer. Native checks cover stable localized IDs, invalid/busy owners, modal
input, nested/text Escape, single dispatch, refused/pending commands, scope-safe
metadata, file choice without writes, blocked Undo, source Cancel/Apply/Save-
reopen, checkpoint Cancel/checked deletion and close save/discard/recovery/error.
Reviewed narrow French, long errors, actual file picker and normal/scaled forms;
actual metadata/workbench and contrasting-blue captures confirm opaque readable
surfaces. Formal invariants and test mapping: docs/ui-dialog-contract.md.

Corrections were two compile-only integration errors, a diagnosed pre-existing
publication timing assumption in the Discard test, and the real inherited-alpha
readability defect caught by visual review. Passing unaffected binaries/evidence
were reused between corrections. No core/binding build, Python consumer, wheel,
commit or merge. Frozen review inputs and matched source are retained in this
receipt's task directory; earlier products and live sessions are preserved.
Next: owner reviews the updated preview; no further workflow expansion selected.

## Delivered UI task - shared dialog and popup-menu design system

Owner finds close-project and metadata dialogs disorganized. Extend the existing
native design system with shared bounded dialog surfaces, contextual headers,
scrollable bodies, labelled full-width fields and measured footer action rows.
Keep one primary action at the trailing edge, Cancel separate from destructive
choices, and secondary recovery controls out of the main closing decision.
Apply the pattern to metadata, close, workspace replacement and image removal.
Standardize popup menu row spacing through shared menu components.

Keep native save/recovery state machines, draft validation, metadata versus
description/tag publication scopes, stable command IDs and project/selection
guards. No core/binding changes or new combined persistence authority. Existing
metadata text/Unicode/duplicate/stale/cancel/save-reopen checks must still pass.
Add native close save/discard/cancel/recovery and bounded dialog captures, plus
metadata/description views at normal/scaled/narrow sizes. Settle inputs before
one cached GUI/app/organization/replay build with references disabled. Run those
focused native checks, source contracts, standalone startup and38 retained binding
object hashes. Reuse unchanged rendering/science evidence; no Python consumer,
wheel, commit or merge. Preserve live user sessions and frozen review inputs.
Next: implement the common components and migrate these project dialogs.

Initial cached build and native replay pass, including actual metadata/Unicode/
duplicate/stale/cancel/save-reopen and class/font checks. Organization interactions
pass through close Save/Discard/Cancel, then Keep recovery waits because replay
target marking only accepted the modal root, excluding its new legitimate body
child. Accept that modal's RootWindow descendants while retaining visibility,
disabled and underlying-window guards. The close captures also reveal an initial
header-focus tooltip; default focus to footer Cancel and use the compact shared
confirmation height. Preserve the initial evidence, rebuild affected frontends
and rerun the matched native checks/captures with these diagnosed corrections.

Corrected native replay and organization suites pass: close save/discard/cancel/
recovery, failed-save cancellation, metadata drafts and viewport/footer guards.
Four source contracts,38 captures, standalone startup and38 retained binding
hashes pass. German metadata is rendered correctly. Visual review finds that the
French close fixture still inherits the harness's forced English environment;
correct only that test's environment override, rebuild organization only and
repeat its matched checks/captures. Reuse the unchanged passing replay binary/log
and production GUI/app. Preserve before-i18n evidence under its own names.

Source review also identifies a context edge case: the close dialog shares a
pending Save As path buffer. If that destination differs from the saved project,
show its editable value instead of claiming Save targets the current project.
Add a narrow/high-DPI first-save capture. This changes only Close composition,
so rebuild GUI/app/organization; reuse the unchanged replay binary/log. No new
save authority or persistence behavior is introduced.

`build/ui-dialog-system/results-final.json` records passing native replay and
organization evidence, four source contracts and39 captures. Actual metadata
input covers Unicode/tab/newline preservation, duplicate/stale refusal, nullable
description, ordered tags, clear/cancel, project/image ownership and save/reopen.
Close covers Cancel/blocked Undo, scaled Save, Discard/owned recovery cleanup,
Keep recovery/publication and failed-save Cancel. Dialog captures prove bounded
normal/light/narrow layouts, independently scrolling bodies, visible footers,
first-save destination, real French closing and German metadata. Popup menu
shortcuts align in their trailing column with stable original command IDs.

All38 binding objects match, and System32 startup with Windows-only PATH/no
Python path exits0. The first targeting failure and test-language correction are
preserved with their evidence; final GUI/app/organization cover the destination
context fix while the unchanged passing replay binary/log is reused. No core or
binding rebuild, scientific changes, Python consumer, wheel, commit or merge.
Updated app: `build/ui-dialog-system/qupathxx/qupathxx_ui_dialogs.exe`, opened with
its own review copy while preserving older live sessions. The shared
[recipe](design-system.md#dialog-and-popup-recipes), [metadata form](images/dialog-metadata.png)
and [close dialog](images/dialog-close.png) document the result. Next: owner review.

## Delivered UI task - source repair belongs to image rows

Owner wants Relink in the image list rather than the metadata inspector. Remove
the inspector's source-action button. Missing linked images retain direct Locate
file in their row and row/context menu. Available linked images expose Relink in
that same menu; embedded images have no source action. Keep F9, selected-entry
targeting, native relinking and source metadata unchanged.

Changed inputs are Project composition and one native linked-row menu capture/
identity guard. Settle them before the cached GUI/app/organization build with
references disabled. Run matched organization open/menu/remove/missing-file repair,
camera/byte guards and normal/light/scaled/narrow captures; inspect the linked row
menu and absence of an inspector action. Reuse compatible shell/design evidence;
check source contracts, standalone startup and38 retained binding-object hashes.
Preserve old products, frozen review inputs and live user state. No core/binding
rebuild, Python consumer, science campaign, wheel, commit or merge. Next: deliver
the action in the row menu and keep metadata focused on inspection/editing.

`build/ui-row-source-action/results-final.json` records passing native organization
interactions, four source contracts and26 captures. The linked-image menu shows
Rename/Relink/Remove while its inspector has only metadata and Edit. Opening the
menu preserves active image, generation and saved bytes; missing-row Locate and
exact-RGB relink still pass alongside selection/camera/project-switch guards.
All38 binding objects match; Windows-only-PATH/no-Python System32 startup exits0.
One cached frontend build/check pass, no failed retries, scientific/core/binding
changes, wheel, commit or merge. Updated app:
`build/ui-row-source-action/qupathxx/qupathxx_ui_row_actions.exe`, opened separately
with its own review copy, preserving all old sessions. See the
[linked row actions](images/project-linked-actions.png). Next action: owner review.

## Delivered UI task - simplify metadata, footer and file checks

Owner finds Source and files redundant, wants the image count in the bottom
status bar, and requests an audit for similar simplification. Review the Project,
Image, Annotations and seven Workflow tasks from native captures and source.
Apply three high-confidence presentation fixes: flatten selected-image source
details into metadata rows; move filtered/total image count into the global
status bar; expose the dedicated File checksum task without a second collapsed
section or idle-status line. Keep missing-source Locate/F9 repair, visible errors,
calibration semantics, confirmations and native jobs/polling unchanged.

Changed inputs are GUI composition and metadata presentation only. Settle these
before one cached GUI/app/organization build with references disabled. Use matched
native organization interactions and normal/light/scaled/narrow captures to check
footer fit, flat metadata and direct checksum controls; reuse compatible shell/
design evidence. Check source contracts, standalone startup and all38 retained
binding-object hashes. No new cosmetic tests, core/binding rebuild, Python
consumer, science campaign, wheel, commit or merge. Preserve existing live user
state and frozen qualification inputs. Record other audit findings as a small
backlog in ui-ux-review.md. Next: implement and validate the three scoped fixes.

Initial native organization suite, four source contracts,25 views, binding hashes
and standalone startup pass. Visual review finds Source: Embedded RGB / Reader:
embedded repeats the same fact. Omit that embedded-reader row; also account for
both separator gaps in the split allocation. Rebuild affected frontends only,
then rerun the matched native checks/captures under final names. Preserve the
initial passing evidence; no scientific or binding changes are required.

`build/ui-simplification/results-final.json` records the final passing native
organization interactions, four source contracts and25 populated native views.
The metadata inspector has no source disclosure or repeated embedded-reader row;
filtered/total image counts occupy the bottom status bar, including narrow/scaled
views; the Files task exposes its controls directly without an idle-status line.
Existing camera, selected/unopened entry, remove, relink/exact RGB, thumbnail and
project-switch/cancel guards pass. All38 binding objects match; standalone
System32 startup with Windows-only PATH/no Python path exits0. Initial passing
artifacts remain under initial names; the one extra incremental frontend build
resolved the duplicate observed during visual review. No failed build/test
retries, scientific/binding changes, wheel, commit or merge. Updated native app:
`build/ui-simplification/qupathxx/qupathxx_ui_simplified.exe`, opened separately
with its own review copy; old live sessions and subsequent user state preserved.
The [audit](ui-ux-review.md#simplification-audit-2026-11-09) records the three fixes,
bounded coverage and follow-up candidates. Next action: owner review.

## Delivered UI task - selected image metadata in Project

Owner wants the selected image's metadata in the lower Project pane, like an
editor inspector, with simpler context/More menus. Keep single-click opening and
active-image highlighting. Add an independently scrolling lower inspector and a
resizable divider saved only in workspace preferences. Show selected-entry
metadata, description/tags, dimensions/calibration and secondary source details.
Use the existing native metadata editor and rename/relink/remove authorities;
menus retain Rename, missing-file Locate and Remove. Remove the floating details
window and redundant Open/Edit/Relink menu entries.

Changed inputs are Project composition, shared divider/inspector presentation and
workspace split preference. Before the cached GUI/app/organization build, settle
these inputs and native checks for selected versus active entry, inspector edit,
rename, divider persistence and existing open/relink/remove/camera/byte guards.
Capture the populated normal/light/scaled/narrow/menu layouts and check standalone
startup. Reuse compatible shell/design evidence; no binding/core rebuild, Python
consumer, science campaign, wheel, commit or merge. Preserve source projects,
previous products and live sessions.

`build/ui-project-inspector/results-final.json` records the passing matched native
organization suite, four source contracts and25 native views. Selected literal
metadata/description/tags, replacement of the inspected snapshot, divider drag
and bounded preference round-trip pass. Inspecting an unopened entry preserves
the viewed image, camera and project bytes; existing single-click, removal,
relink, project-switch/cancel and thumbnail guards pass. Normal/light/scaled/
narrow/context captures show the lower inspector, wrapped values and reduced
menus. The metadata editor was also observed in the live preview. All38 binding
objects match; standalone System32 startup with Windows-only PATH/no Python path
exits0. Native app: `build/ui-project-inspector/qupathxx/qupathxx_ui_inspector.exe`,
opened separately on the six-image review. Preserve its live session and any
subsequent user changes; frozen qualification inputs remain separate. See
[inspector](images/project-metadata.png) and [menu](images/project-metadata-menu.png).
One cached frontend build/check pass, no failed retries, binding/core rebuild,
scientific campaign, wheel, commit or merge. Next action: owner review.

## Delivered UI task - shared chrome and spacing refinement

Owner finds the ellipsis and overall UI rough, with uneven padding. Apply one
shared recipe:24px icon canvases inside28px square targets, compact vertical More
glyphs, square row actions centered in40px rows,4px row edge insets and8px text
gaps. Keep full-row active highlighting and contextual action visibility. Unify
native combo arrow surfaces with fields and use4px popup corners. Balance the
slide toolbar's top/bottom insets and remove the extra layout gap below it.
Typography, palettes, native commands, data and project ownership stay scoped
to their existing authorities.

Inputs settled in shared tokens/components/theme, native/RmlUi glyph placement,
toolbar layout and Project row composition. Build GUI/app/organization/shell/
design frontends in the existing cache with references disabled. Run matched
native interaction/camera/persistence/relink/project-switch checks, actual icon
coverage at normal/fractional/enlarged DPI and populated light/dark/narrow/menu
captures. Reuse compatible source evidence; add no cosmetic-constant tests,
binding/core rebuild, scientific campaign, wheel, commit or merge. Preserve old
products, source projects and live sessions. An unrelated napachan build was
observed in its own workspace; it does not own these GUI targets.

`build/ui-chrome-refinement/results-final.json` records passing organization,
RmlUi shell and design suites, four source contracts and24 populated native views.
The shell suite checks icon ink and intermediate ellipse edge coverage at100/
125/150/200% in both themes. Existing menu/open/relink, current-row Enter,
project-switch/cancel, thumbnail RGB, camera and project-byte guards pass.
All38 binding objects match. No Python consumer or scientific source was rebuilt.
System32 startup with Windows-only PATH and no Python path exits0. Final preview:
`build/ui-chrome-refinement/qupathxx/qupathxx_ui_chrome.exe`, opened separately
on the six-image review with both sample projects in its selector. Updated
[catalog](images/project-thumbnails.png) and [menu](images/project-row-actions.png)
show the shared treatment. No failed build/check retries, commit or merge. Next
action is owner review of the shared visual hierarchy and spacing.

## Delivered UI task - full-row active image highlight

Owner wants the Viewing text removed and highlight extended through the row's
More action area. Use the active image as the persistent highlight; opening a
different entry's menu must keep that active-image indication. The shared catalog
component paints selection/hover across the full row while retaining independent
entry/action hit targets and keyboard focus. Keep Missing file status and repair.

Changed inputs are only the catalog recipe and Project composition. Build cached
GUI/app/organization targets with references disabled, then run the existing
matched native organization flow and populated normal/scaled/narrow/menu views.
Inspect continuous background through the action slot. Reuse the previous source
and runtime qualification; no new tests for cosmetic constants, bindings, core
changes, scientific campaign, wheel, commit or merge. No active compiler process
was observed. Preserve previous products, source projects and live sessions;
open a separate native preview when the focused checks pass.

`build/ui-project-highlight/results-final.json`: matched native organization
checks, four source contracts and24 native views pass. Dark, light/scaled and
other-row-menu captures confirm equal active background at the caption gap and
trailing action slot, distinct from an inactive row. The viewed image retains
that highlight while another row's menu is open. Review bytes and all38 binding
objects match their snapshots. Windows-only-PATH/no-Python System32 startup
exits0. Updated native preview:
`build/ui-project-highlight/qupathxx/qupathxx_ui_highlight.exe`, opened separately
with both sample projects in its selector. No new scientific, binding or format
work, commit or merge. Next action is owner review of the full-row selection.

## Delivered UI task - project switcher, collapsible groups and thumbnails

Owner accepts single-click rows, rejects repeated ellipses, chooses a project
switcher plus image groups, and requests thumbnails. Use a named project selector
with eight bounded recent paths in workspace preferences, reusing native loading
and dirty-project Save/Discard/Cancel. Project administration/grouping moves into
that selector; row More actions appear on hover, keyboard focus or an open menu.
Use real metadata groups as collapsible rows with counts. Search reveals matches
inside folded groups without changing the stored fold choice. No disk moves,
project merges or simultaneous-project runtime are selected.

Visible image rows use the existing native thumbnail reader/display transform,
one job at a time, 64 cached 64px textures, and an8M source-pixel bound. Native
decoder memory is additional; failed/missing previews show an image placeholder,
never fabricated content. Cache ownership/invalidation follows source/display
and workspace replacement. UI rows are40px with40x30 thumbnails; constant pitch
keeps clipping reliable. Recent/group preferences live in the bounded .ini only.

Inputs settled in the GUI projection/components, workspace preferences and a
small native preview cache. Extend native organization checks for MRU round-trip,
group collapse/search, real project-selector switching and unsaved cancellation,
thumbnail RGB/cache replacement, and existing single-click/relink/camera/data
guards. Include native populated/light/scaled/narrow/grouped/folded/hover/menu
captures and Windows-only-PATH startup. Build GUI/app/organization once in the
existing cache with references disabled; no bindings, core edits, scientific
campaign, wheel, commit or merge. Preserve prior products and analysis sessions.

Reference: current official QuPath projects/structure docs describe a managed
project folder with image references, per-entry .qpdata/thumbnails, scripts and
classifiers, plus recent-project reopening and metadata organization. QuPathGUI
holds one current project. Adapt the UI distinction while retaining native
.qpxproj storage; no folder-format migration or QuPath parity claim is selected.

Initial native suite, four source contracts,24 views and standalone startup pass.
Visual/source review calls for disclosure chevrons rather than movement arrows
on groups, and retaining each tiny raw thumbnail so display adjustments re-upload
its pixels without decoding the slide again. Apply those two final UI changes,
rebuild affected frontends only, then rerun the matched native checks/captures.
The cache is bounded to1MiB raw thumbnails plus1MiB GL textures, with one bounded
native source read. Preserve the initial passing evidence under its own names.

`build/ui-project-browser/results-final.json` records the passing matched native
organization suite, four source contracts and24 native views. These exercise
MRU persistence, real selector switching, unsaved cancellation, thumbnail RGB
and cache replacement, fold/search behavior, normal/light/scaled/narrow/grouped/
menu layouts and the retained single-click/Enter/relink/camera/project guards.
All38 binding objects match; no Python consumer was built or linked. Both review
source copies are retained, and the six-image project is byte-identical after
checks. Standalone System32 startup with Windows-only PATH and no Python path
exits0. Final app: `build/ui-project-browser/qupathxx/qupathxx_ui_browser.exe`.
The separately opened preview offers Histology design review and NuCLS annotated
fields in its project selector. See [thumbnails](images/project-thumbnails.png)
and [collapsible groups](images/project-groups.png). Initial passing artifacts
and products are retained; no core changes, wheel, commit or merge. Next action
is owner review of the project selector, grouping and image-preview workflow.

## Delivered UI task - direct image opening and inline entry actions

Owner wants the separate Selected image block simplified into the list entry,
and confirms a click should open the image. Available row clicks now activate
through the existing pending-image service; Enter remains available. Keep row
menus/context actions independent so editing/removing an unopened entry does not
switch slides. Missing rows have a direct Locate file action. Keep status/actions
in fixed-height rows, remove the duplicate footer name/source/Open controls and
160px reservation, and let the virtualized catalog fill the panel above its count.

Inputs settled in Project composition, shared CatalogEntry trailing action space,
and removal of the unused inspector token. Adapt native navigation checks for
single-click camera restore and separate row actions; exercise missing-row Locate
and Enter relink with exact RGB. Use the matched native organization executable
for normal/scaled/narrow and populated six-image review captures. Existing Python
test input follows the new click contract, but its runtime ABI refresh remains
separate. No Python relink/build, binding compilation, core edits, science campaign,
wheel, commit or merge. Build cached GUI/app/organization targets only with
references disabled. Only an idle MSBuild worker was observed. Preserve source
projects, old products and live sessions; launch a separate updated native preview.

Post-build source review found that skipping activation of the already-viewed
image must still consume Enter in its row, to avoid completing a canvas draft.
Keep that keyboard route, constrain mouse activation to its hovered row, and add
a native draft-polygon/current-row Enter regression. Rebuild only gui.cpp and
the affected native consumer/test, then run the settled checks once.

`build/ui-project-inline/results.json` records the passing matched native
organization suite and four source contracts. Native checks prove single-click
opening/saved camera restore, independent row menus and unopened-entry removal,
focused Enter retaining an unfinished canvas draft, and inline Locate/Enter
relink with exact RGB. Twenty native views include the existing pane workflows
and five populated default/light/scaled/narrow/click/menu review states. The
source review copy is byte-identical after checks; all38 binding-object hashes
match. No Python consumer was built or linked. Standalone startup from System32
with Windows-only PATH and no Python path exits0. The fresh native preview is
`build/ui-project-inline/qupathxx/qupathxx_ui_inline.exe`, opened separately on
the six-image review project. See the [inline catalog](images/project-inline.png).
No core changes, wheel, commit or merge. Next action is owner review of the
single-click Project flow. The earlier Python/native ABI task remains separate.

## Delivered UI task - contextual project organization and missing files

Owner questions the permanent Missing files selector and asks for clearer UX.
Keep search/catalog in the primary flow. Move optional metadata grouping into
Project actions; show a removable row only when grouping is active. Show the
missing-files count/action only when sources are absent, with a close action
while filtered. A catalog refresh after the last repair restores all images.
Browsing/filtering must retain the viewed slide, selected entry and project data.

Inputs settled: native ProjectBrowser projection, two small presentation
components, Project pane composition and translated grouping caption. Extend
the existing organization suite with actual filter/clear clicks and last-repair
state; use existing Project navigation/relink and responsive layout checks.
One cached GUI/app/organization build with references disabled, then link retained
binding objects only into a fresh UI consumer. Check populated/default/missing/
grouped states at normal/enlarged size; reopen the existing complex review copy
in a separate preview. No science campaign, core rebuild, wheel, commit or merge.
No competing compiler process was observed. Preserve existing products/sessions.

Initial native control harness clicked during ImGui's first auto-fit frame;
actual desktop replay already filters/clears correctly. Settle the test window
before input, as in the existing docking harness, then rebuild that test only.
Retain the failed log. No product-code retry is needed for this harness issue.

The retained Python consumer passes26 GUI cases but fails the source-relink case
with a directory-scope validation error. Its38 objects predate the Nov9 native
ReaderSource layout/ImageServer virtual-interface changes; the prior matched
toolbar consumer passes that same case. Do not qualify or ship this relinked
Python consumer, rebuild all bindings, or change source validation for a UI task.
Add the decisive relink/RGB flow to the matched native organization executable
and run it there; standalone native remains the delivery route. Preserve both
Python logs and finish with native interaction/standalone evidence.

Delivered standalone preview: `build/ui-project-context/qupathxx/
qupathxx_ui_context.exe`, opened on a separate six-image/1,093-object review copy.
`build/ui-project-context/results.json` records the passing matched native
organization suite, including real filter/group clear clicks, last-source repair,
navigation/persistence and desktop relink with exact RGB. Four source contracts
pass. Eight observed UI views cover default/missing/active/cleared/menu states,
dark/light and enlarged/localized text; three additional captures come from the
matched native suite. System32 startup with Windows-only PATH and no Python path
exits0. All38 binding objects and original review-project bytes remain unchanged.
The unqualified Python consumer is preserved under `consumer-evidence`, outside
the launch package; its26 passes/one failure are retained without a Python support
claim. No scientific validators/assertions were weakened and no core source was
edited. No wheel, commit or merge. Next action is owner review of the calmer
Project panel; Python/native ABI refresh is a separate runtime-consumer task.

## Delivered UI task - toolbar group separation

Owner wants proper separation in the toolbar. Use three coherent groups:
drawing/editing, undo/redo, and view/panel controls, with noninteractive vertical
rules and shared spacing. Groups wrap together; the drawing group can wrap
internally if the viewport is narrower than that group. Mirror the grouping in
the compact native toolbar and reserve its actual grouped row height. Preserve
command IDs, shortcuts, hit targets and the repaired coverage/docking behavior.

Inputs settled in RML/RCSS/token expansion, native toolbar adapter/composition
and existing shell visibility checks. Those checks now use measured command
bounds instead of fixed dense-row coordinates and check wrapped targets remain
inside the toolbar. Build only affected GUI/app/shell/organization frontends in
the existing cache with references disabled; retain38 binding objects and link
only a fresh consumer. Run existing shell/organization and affected layout/
activity/design checks plus normal/scaled/narrow/light/dark captures. No new
cosmetic-constant tests, science campaign, binding compilation, wheel or merge.
Only idle MSBuild workers were observed; preserve existing source/products and
the user's current analysis windows.

Workspace-root `build/ui-toolbar-groups/results.json`: 30 focused UI/source
cases, two native suites and eight normal/scaled/narrow/light/dark/compact views
pass. Bounds and actual command clicks cover 220px/700px toolbar widths at both
DPI scales; measured icon visibility and existing coverage/docking/scientific
invariants pass. Source-project bytes and camera are retained. All38 binding
objects match; only the fresh consumer was linked. Standalone startup from
System32 with Windows-only PATH and no Python path exits0. Matching preview:
`build/ui-toolbar-groups/qupathxx/qupathxx_ui_toolbar.exe`.

Corrections: put separator spacing inside the group's border-box to prevent
narrow-width overflow; rebuild only its stylesheet consumer/test afterward.
MSBuild accepts one project per invocation. The capture helper uses layout_path
only for docked mode, content bounds for compact geometry and supported replay
frame2. Retain the first six passing views and rerun only compact captures.
Concurrent core-reader checks and previous products/live analysis sessions are
preserved. No binding compilation, wheel, science campaign, commit or merge.
Next action is owner review of the separately opened grouped-toolbar preview.

Owner follow-up delivered: a separate `build/ui-toolbar-groups/complex-review/
Histology design review.qpxproj` is open in the tested toolbar product with its
own layout. It contains six retained density/neighborhood, NuCLS, classifier and
whole-slide samples with1,093 objects. Native Project APIs copy existing documents
and preserve their calibration, object IDs/classes and saved result counts across
save/reopen. Explicit sample labels and provenance distinguish the examples;
this is not a single research cohort. `complex-review/review.json` records counts,
source hashes and the passing initial-view check (visible canvas, settled tiles,
no errors/overflow). Five original project hashes are unchanged. No scientific
processing, build or product changes. Next action is the owner's interactive
review of Project, Annotations and Workflow with populated examples.

## Delivered UI repair - icon coverage and docked tool controls

Owner still saw pixelated icons and could not close a docked tool. The RmlUi
bridge discarded the UV texture coverage used by ImGui1.93 line antialiasing;
stroke/style changes could not recover it. The bridge now retains UVs and owns
a compact premultiplied white-alpha strip containing only used atlas rows. It
invalidates on size/color/atlas replacement, without borrowing the font GPU
texture or changing scientific image filtering.

Only primary workbench nodes hide tabs. Ordinary, mixed and detached tool groups
retain title/tab close controls and their selected tab. Six dockable consumers
use FirstUseEver placement so reopening no longer discards saved docks/sizes.
Classifier cancellation is labeled while busy, distinct from panel closure.
The activity rail renders independently when a tool covers Slide; the hidden
slide toolbar cannot render/intercept input and stays hidden through reload.
The owner's live Object classifiers panel was dismissed with F4; its analysis
session remains open. See the [review](ui-ux-review.md#current-renderer-and-docking-repairs)
and [shared rules](design-system.md#tokens-and-components).

Workspace-root `build/ui-render-docking/results.json`: 73 final GUI/source cases,
two native suites and 35 checked views pass. Native checks measure intermediate
ellipse side-edge coverage at 100/125/150/200% in dark/light and exercise legacy
dock-title close/reopen/undock/redock. Six actual saved Object/Pixel classifier
views prove dock preservation, title-X close, F4 reopen and rail visibility while
Slide is covered. Existing RGB, camera/generation, source bytes, real classifier
train/predict/apply/Undo/save and project persistence remain covered. All 38
retained binding-object hashes match. Standalone startup from System32 with
Windows-only PATH and no Python path exits 0. Fresh repaired product:
`build/ui-render-docking/qupathxx/qupathxx_ui_render_fixed.exe`.

Corrections: use the public read-only atlas Pixels field instead of its non-const
getter and include the design header in the expanded shell test. Stage the normal
app executable for the launcher route; the documentation helper explicitly uses
UTF-8 on Windows. Actual saved-layout/visual checks exposed
the Appearing placement overrides and rail callback ownership; preserve those
failure captures/logs and retain their new assertions. Final suites/captures pass
against the matching product. Earlier products, source projects and the user's
live session remain. No binding compilation, wheel, science campaign or merge.
Next action is owner review of the separately opened repaired preview.

## Delivered UI task - type, spacing and lighter icons

Owner rejected the small text/padding balance and heavy, uneven icon strokes.
Shared text roles now use 16px body/headings, 14px secondary captions and 18px
modal titles, within the existing 28px controls. Field/checkbox gaps are 4px and
text-action padding is 8px. Icons use a lighter 1.35-unit stroke with antialiased
ends; filled nodes/dots match that weight. Font-role scopes preserve the outer
workspace geometry scale. Navigation targets, command IDs and native authorities
remain unchanged. See the [review](ui-ux-review.md#previous-type-spacing-and-icon-refinement)
and [shared typography/geometry recipe](design-system.md#tokens-and-components).

Workspace-root `build/ui-type-spacing/results.json`: 47 final GUI/source checks,
4 native suites and 27 checked views pass. These include Image/Project/Annotations/
Workflow, normal/125%/150%/200%, dark/light, Chinese/German, confirmation, hover and
pressed states. The component check proves caption/title scopes retain action
target sizes and workspace scale. Existing font-atlas, entry/class authoring,
native RGB, camera/generation, saved project bytes and save/reopen checks pass.
All 38 retained binding-object hashes match. Standalone startup from System32
with Windows-only PATH and no Python path exits 0. Fresh app:
`build/ui-type-spacing/qupathxx/qupathxx_ui_refined.exe`.

Correction: the first scale check showed that ImGui PushFont also mutates
Style.FontSizeBase, shrinking a caption-scoped action to 24.5px. Retain the outer
scale for the role scope and keep the new regression assertion. Final consumers,
installed checks and captures pass with that fix. Earlier products/source data
and concurrent work are preserved. No binding compilation, wheel, scientific
campaign or merge. Next action is owner review of the opened refined preview.

## Delivered UI task - shared button polish

Owner found the buttons rough. The shared native/RmlUi action recipes now use
4px button corners, balanced10px text padding, restrained outlined secondary
actions and consistent hover/pressed/selected/disabled states. Ghost glyphs
brighten with interaction; keyboard focus has a separate ring. The vector family
uses a consistent1.75-unit stroke, smoother curves and cleaner pointer/save/
slider shapes. Explicit rectangle strokes preserve hollow icon interiors in the
RmlUi geometry-only bridge at200% scale. Compact28px icon targets, input geometry,
command IDs, shortcuts and native actions remain. Navigation stays unboxed.
See the [review and captures](ui-ux-review.md#previous-button-polish) and shared
[button recipe](design-system.md#tokens-and-components).

Workspace-root `build/ui-button-polish/results.json`:47 final GUI/source checks,
4 native suites (replay/shell/organization/design) and20 checked dark/light,
normal/200%, Chinese/German and confirmation/hover/pressed views pass. Checks
retain exact native RGB, camera/generation, project bytes, save/reopen and
entry/class authoring assertions. All38 retained binding-object hashes match.
Standalone startup from System32 with Windows-only PATH and no Python path
exits0. Updated app is open at
`build/ui-button-polish/qupathxx/qupathxx_ui_buttons.exe`.

Corrections: update older entry/class replay checkbox targets to the existing
scoped value ID without relaxing assertions. Enlarged captures exposed an
ImGui rounded-rectangle optimization carrying coverage in UVs; explicit paths
fix that geometry-bridge mismatch. Concurrent fluorescence edits needed their
owning workflow.hpp include and a matching core archive for five new symbols.
After an archive collision left no output, serialize the2s archive-only build
using retained objects. An early native check locked a not-yet-finished shell
test link; relink it and run the final suites serially. Failure logs are retained.
Final GUI checks were repeated against the matching runtime. These are UI checks,
not scientific acceptance of the concurrent features. Preserve earlier products
and live windows. No binding compilation, wheel, science campaign or merge.
Next action is owner review; no additional feature scope is selected.

## Delivered UI task - reference-informed Project explorer

Owner requested comparison with similar/related software and implementation.
The [reference review](ui-ux-review.md#reference-review-project-explorer-implementation)
compares official QuPath, OMERO, Cytomine and Adobe Bridge designs, with VS Code/
Photoshop references. The native Project pane now fills available space with a
clipped catalog, searches names/metadata/tags, groups only by chosen actual
metadata, and separates selected entry from viewed image. Enter/double-click/Open
activate explicitly. Rename/metadata/relink/remove also target unopened entries.
The bottom inspector identifies selection, source availability, chosen grouping
value and current viewer. Add/Save/More owns the header; administration/recovery
and detailed entry editing use bounded windows. No inferred case/review state.

Workspace-root `build/ui-project-explorer/results.json` records66 distinct latest-
passing GUI/source checks,5 native suites and14 checked normal/scaled/localized
views plus the project menu capture. Exact project bytes, native RGB, calibration,
camera/generation, save/recovery and unopened-entry removal remain covered.
Computer Use typed CONTROL (8/24 tag matches), selected S02 while S01 stayed
viewed, and grouped by Case; the isolated window closed without saving. Final
scale/inspector deltas have separate passing replay/capture evidence. Standalone
startup from System32 with Windows-only PATH and no Python path exits0. All38
retained binding-object hashes match. Fresh product:
`build/ui-project-explorer/qupathxx/qupathxx_ui_project_explorer.exe`.

Corrections: an initial nonexistent test filename ran no tests; use discovered
recovery files. Six scale/layout failures came from sending an already measured
ImGui width through the logical-size scaling helper; final affected checks all
pass. Compatible earlier56 GUI passes remain valid. The capture helper now uses
the retained binding's existing active_image property. Receipt aggregation was
corrected to match the actual CTest summary text; no native retest was needed.
Preserve concurrent native work, prior products and source fixtures. No binding
compilation, science campaign, wheel or merge. Next action is owner review of the
delivered explorer; thumbnail caching and batch-selection are separate workflows.

## Previous Project explorer design review - proposal

Owner still finds Project rough/confusing and asks for a pathologist/researcher
perspective with VS Code/Photoshop references. The current eight-row collection,
immediate selection/open coupling, active-image-only context actions and inline
administration remain concrete browsing problems. Propose a filling slide list,
explicit selected versus viewed slide, metadata search/optional grouping, a small
selected-slide inspector and one project menu for administration. Case/stain
groups must use explicit metadata; no inferred clinical review status.

Interactive example: active-checkout `build/ux-project-proposal/project-explorer.html`.
This is a design proposal with example data, not a native implementation. Browser
checks cover selection/open separation, metadata search, grouping, missing-source
repair entry, double-click/Enter and narrow layout. Preserve prior UI/native work;
no native build, binding compilation, wheel or merge belongs to this review.
All seven interaction checks pass at736px/320px with zero browser errors; results
are in that proposal directory. Local-file preview was blocked, so browser QA
rendered the task-owned fragment directly without a background HTTP service.
Next implementation priority is filling list geometry, separate browser selection
and viewer activation, and the contextual inspector; grouping requires an explicit
metadata mapping. This proposal does not mark the native Project pane accepted.

## Delivered UI follow-up - simpler panel workflows

Owner accepts the style direction and asks for simpler, more intuitive panels.
Fix three evidenced presentation issues: flat everyday/advanced hierarchy,
selection-insensitive annotation clutter, and the ungrouped wall of tool launchers.
Keep the four panel owners and existing native actions/IDs. Project retains its
explorer with contextual image actions; Image leads with display and summarizes
calibration; Annotations leads with selection identity and folds specialist edits;
Workflow keeps common inputs/run/results visible and discloses advanced recipes.
Group specialist tools in one searchable launcher. Preserve scientific settings,
validation, history, polling/cancellation and all existing shortcut routes.

Implemented those four panel recipes, full-width task/method choices, selection
context in place of repeated task headings, and result actions shown with their
results. Unchanged object Apply disables. New captions cover all16 locales.
Polling, cancellation, validation and saved-result recheck stay outside settings
disclosures. Compact API inspection keeps its continuous controls. The
[review](ui-ux-review.md#current-panel-simplification) records the three findings;
[design rules](design-system.md#panel-workflow-recipes) describe the reusable recipe.

Evidence under workspace-root `build/ui-panel-workflows/results.json`:191 distinct
latest-passing checks (187 GUI plus4 source contracts),5 native UI suites and42
distinct captures, including main panes/tasks at1×/2×, empty/selected states,
Chinese light panels, expanded settings and contextual/scaled tool menus.
Final affected workflows/captures supersede compatible earlier cases. Custom
white-reference run/keep, native RGB, calibration, object lock/properties, Undo,
project bytes and camera checks remain exact. All38 binding object hashes match.
Standalone final app starts from System32 with Windows-only PATH (exit0), staged
separately at `build/ui-panel-workflows/qupathxx/qupathxx_ui_panels.exe`.

Corrections: serialize after a concurrent dark-flat build locked gui.obj; retain
that job/cache. Visual review removed remaining cell sampling/result clutter and
truncated method values. Two legacy annotation replays now start on Annotations
before clicking their controls; all value/history assertions remain. Computer Use
rejected inputs twice because of active user input: leave that live first preview
alone; manual search typing is not claimed. Actual chooser selection passes replay.
No binding compilation, science campaign, wheel, architecture migration or merge.
Prior products/live windows and concurrent native feature work are preserved.
Next action is owner review of this simpler main-panel flow; specialist dialogs
retain their existing recipes.

## Delivered UI follow-up - shared workbench style system

Owner asks for a VS Code-like activity bar and consistent styling across the UI.
The existing split RCSS/native dimensions and bypassed component helpers are the
concrete gap. Use full48px navigation rows, an outer-edge active indicator,
unboxed glyphs and bottom workspace settings. Keep toolbar buttons distinct from
navigation. Export native semantic tokens into RCSS at load/reload, and apply
shared control, label-column, checkbox and flat-section recipes to native forms.
Preserve the neutral Save/dirty behavior and contextual Project workflows.

Implemented shared semantic metrics/palettes and RCSS token expansion, schema2
activity views/footer settings (schema1 stays readable), native label columns,
full-row vectors, flat headers and consistent28px inline /32px modal actions.
Eleven native modules route their literal buttons/checkboxes/headers through
the adapters. Pointer selection stays unboxed; keyboard navigation shows focus.
See [design rules](design-system.md) and [current review](ui-ux-review.md#current-workbench-style-system).

Evidence: workspace-root `build/ui-style-system/results.json` records163 distinct
latest-passing GUI/source cases (159 workflows plus4 source contracts),5 native
suites,26 pane/task/scaled/translated/dialog captures and38 unchanged binding
object hashes. Final focus-only inputs have their own shell/native pass and
pointer/settings captures; compatible earlier workflow/capture evidence is
retained. Exact native RGB, saved bytes, camera and generation checks pass.
Standalone startup from System32 with Windows-only PATH exits0. Fresh matching
app/resources live at `build/ui-style-system/qupathxx/qupathxx_app.exe`.
No binding compilation, science qualification, wheel or merge. Earlier products,
live windows and concurrent native feature work remain separate and preserved.

Corrections: stage RCSS explicitly when skipping project references; fix grouped
field cursor offsets; remove disclosure borders and unequal inline heights.
Refresh links after the concurrent native archive acquired its marker symbol.
The broad run's one stale physical launcher coordinate now targets the observed
28px row and passes without weakening workflow/data assertions. A misspelled
capture-helper enum was corrected without changing the product API. Next action:
owner visual review; passing checks do not imply design acceptance.

## Previous UI delivery - Project explorer and Save semantics

Owner rejected the standing highlighted Save and the remaining permanent forms.
The earlier alignment pass had passing checks, not design acceptance. Project
now leads with its filename and a neutral New/Open/Save command bar, followed by
the image explorer and Add/Refresh. Save disables for an unchanged saved project;
a filename asterisk indicates unsaved changes. Open/Save As/Add/Relink reveal one
contextual form. First Save requests a destination; abandoning Save As restores
the current native path. Successful save reconciliation adopts the canonical
path. Native job/review/import authorities and configured shortcuts remain.

Changed inputs: private pane/navigation/save presentation, one16-language Save As
caption and focused GUI regressions. Reused the integration cache, retained
dependencies and38 binding objects (all final SHA256s unchanged). No scientific
API, binding compilation, wheel, science qualification or merge belongs to this
UI slice; concurrent registration work and earlier desktop products are intact.

Evidence under workspace-root `build/ui-organization/project-explorer`:5/5 native
UI suites pass (`ctest.log`);54 distinct GUI cases have passing latest results
across the broad pass and focused Open repair (`gui-results.json`). Coverage
includes clean/dirty Save, first destination, save/close/concurrent-edit ordering,
workspace replacement, failed-open Undo, QuPath review/import and scaled/localized
commands.11 default/scaled/translated/dirty/contextual captures pass without
source writes or camera/generation changes (`captures/results.json`).

Computer Use exercised Save As cancellation followed by Save, and an actual Save
As/Enter/reopen on a disposable native project. The cancelled file is absent;
identity, objects, available image and camera persist; original CMU-1 project
bytes are unchanged (`interaction-results.json`). These file handlers are
unchanged by the final focus-only repair. The final standalone app passes from
System32 with Windows-only PATH (exit0), retained separately as
`build/ui-organization/python/qupathxx/qupathxx_ui_project.exe` and opened on
the Project pane. [Current layout](ui-ux-review.md#current-project-pane-explorer-and-contextual-file-actions).

Corrections: the first broad replay found3 failed-open Undo regressions because
preconfigured Open stole text focus; focus only when requesting a path. The20
affected Open/organization/QuPath retries pass, preserving all history/value
assertions. A raw copied interaction fixture lacked relative image references;
create it through native Save instead. No assertions or identities were relaxed.

## Previous UI pass - consistent sidebar forms

Owner's Project-pane screenshot exposed competing fixed-width fields/icons,
uneven heights, an unaligned filter and nested collection boxes. Shared
`ActionRow` now grows the field around fixed trailing commands with equal heights
and a narrow-row fallback. Save/Open have explicit labels; New belongs to the
heading; filter/source/rename actions align. Primary image/object lists use the
pane surface and fit their small row counts. Count/Remove share the list footer.
[Updated screenshot and component rules](ui-ux-review.md#screenshot-follow-up-sidebar-forms).

Changed inputs: private layout helpers and Project/Annotations presentation, plus
the existing two-row list click (now targets the second of two fitted rows).
Path editing, commands/IDs, native validation, project values and scientific RGB
assertions remain. Reused the integration cache for affected frontend targets;
replay links the38 retained binding objects only. No binding compilation, wheel,
new numerical qualification or unrelated native source edits belong to this UI
slice. Concurrent registration/import work and retained products are preserved.

Decisive results under workspace-root `build/ui-organization/sidebar-polish`:
5/5 native UI suites (`ctest.log`),34/34 affected GUI cases (`gui.log`, `gui.xml`)
and7 real/empty pane captures pass (`capture-final.log`, `captures/results.json`).
Coverage includes scaled/translated Save, second-row switching and exact source
RGB, add/relink/save/reopen, QuPath review/import, expanded source details,
annotation selection and1×/1.5×/2× English/Chinese/German views. Exact saved CMU-1
bytes/generation/camera are unchanged. All38 binding SHA256s match; the retained
standalone executable runs from System32 with Windows-only PATH (exit0).

Corrections: the first build caught a QuPath refresh snapshot scoped too narrowly;
fixed its parent scope. Initial captures showed excess single-row space and an
orphan Remove action; tightened those before final checks. No numeric or stored
identity assertions were relaxed. The revised native desktop is retained as
`build/ui-organization/python/qupathxx/qupathxx_ui_polish.exe`, separate from the
shared link output and prior desktop product, and opened on the Project pane.

## Delivered UI task - desktop layout and research workflows

Owner requested three independent UI/UX reviews followed by improvements.
The hierarchy, workflow and interaction reviewers independently covered all major
desktop surfaces. [Findings, coverage and evidence](ui-ux-review.md).

Project exposes Add; Image leads with Display/Calibration; Annotations leads with
Name/Class and owns object exchange. Workflow has seven persistent task choices
above its scrolling body; shortcuts reveal the corresponding task. Tools can
inspect raw files without an ROI. Batch keeps progress/Cancel and Run reachable.
Collections scale with readable rows; tool windows stay inside the viewport;
icons show keyboard-focus hints. Failed overviews no longer intercept canvas
input; retry controls stay stationary. New captions cover all16 languages.
Native jobs, scientific results and the RmlUi shell retain their existing owners.

Changed inputs were private frontend layout/navigation/helpers/captions and their
replay tests. Reused the integration cache and staged one candidate runtime under
workspace-root `build/ui-organization`. No scientific API, binding compilation,
wheel or new numerical qualification belongs to this UI slice. The optional
replay module links the38 retained binding objects; all38 final SHA256s match.

Decisive checks: all5 native UI suites pass;145 distinct Python-native replay
cases have a passing latest result. Coverage includes10 pane/task views across
1×/2×, native region run/keep, failed-map selection/retry, batch visibility,
classifier/file routes, F5 ownership and F6 export from another task. Exact
project bytes/generation, object counts, camera and scientific RGB assertions
remain intact. Real CMU-1 dark/Chinese-light captures are readable and unchanged
in project/camera state. Standalone project startup from System32 with a
Windows-only PATH exits0. No unresolved selected-check failure remains.

Corrections: capture review caught a Tables action outside its task guard and
excess task-child inset; fixed. Broader replay found moving overview retry
controls; stabilized. Updated obsolete batch pixel/location assertions to actual
window visibility while retaining exact science RGB checks; made saved-stain
replay start explicitly in Image. Missing app staging and an initially skipped
link-only target were repaired without compiling bindings. Initial structural
edit stopped before writing on an exact-match assertion and was corrected.

Outcome: requested organization changes and affected checks are complete in the
active checkout. Detailed logs/captures remain in `build/ui-organization`.
Advanced forms remain ImGui; no browser/release/performance claim is part of this
task. Concurrent native expression/registration work is preserved and retains
its separate qualification scope.

Local main fast-forwarded through UI commite76d9677 and the completed integration
history at the owner's request. Unfinished spatial-import edits remain separate
and intact in the active checkout. Reused the passing checks above; merge changes
no product inputs. The active checkout/cache, fixtures and unfinished native work
are retained. No push, release or worktree removal was performed.

## Delivered UI task - RmlUi workspace shell

Owner selected RmlUi. The default activity rail and slide toolbar now use
reloadable packaged RML/RCSS, shared vector icons, themes and all16 languages.
An owned snapshot and typed command queue separate presentation from native
state/actions. Scientific rendering and workflow authorities stay in C++;
advanced inspectors/menus/dialogs and the compact API layout retain ImGui.
[Architecture](rmlui.md), [checks/corrections](benchmarks/rmlui-shell/README.md).

Native-only CMake build/install and all4 selected native CTest cases pass, plus
3 source design contracts. Real CMU-1 dark/light Chinese views render; native
camera, object count, exact project bytes and scientific RGB checks pass.
Installed app runs with Windows-only PATH from System32. All38 binding objects
are byte-identical; optional SDK flags restored without compiling bindings.

Owner requested merging this delivery into main. Promote the reviewed source,
resources and evidence together using the existing passing checks; retain the
active integration checkout/cache/runtime and unrelated local fixtures.

Reused the active checkout/cache. A locked running demo required a separate
runtime output directory in main build/rmlui-shell; old windows/products remain.
Corrections addressed Windows resource URLs/UTF-8, border-box reservation, icon
draw density and obsolete cosmetic font/contrast test assumptions. No scientific
thresholds or stored identity checks changed. No wheel, binary release, web
runtime, complete inspector migration or further feature phase is claimed.

## Delivered task - real fluorescence and spatial-omics viewers

Opened real openly licensed OME tubhiswt-2D and 10x V1_Adult_Mouse_Brain data.
[Source/license/data-preparation receipt](benchmarks/real-data-demo/README.md).
Both CC BY4.0; large input files/projects stay in main build/real-data-demo.
Fluorescence has two512×512 uint8 channels, displayed with explicit7–48 and0–255
windows without changing raw samples. Mouse-brain H&E has2,702 real tissue spots
and six markers (Gad1, Slc17a7, Pcp4, Aqp4, Mbp, Plp1),14,550 selected nonzero
counts. Counts remain unchanged; positions use the published0.17011142 hires scale.
Both native windows render; whole-tissue alignment was inspected. Gene values
are available through each point's existing imported-properties inspector.

Waste review: checked licenses before admission (noncommercial samples excluded),
downloaded only processed open files, and streamed the source matrix in C++ to a
small selected-gene profile. Existing PNG/core/importer services perform all data
interpretation. Lossless download/archive controls may use Python; the optional
fluorescence script only forwards typed native options/statistics/save calls.
One temporary data-preparation target used the existing cache; its project-include
setting was cleared afterward. No product source/binding/release changes or full
campaign. Native display defaults were too dark for the lower-range channel;
explicit presentation windows fixed that without touching original values.

## Delivered integration - native workflows merged into main

Owner requested merging feature/scientific-grid-159 into main. Mergec2823c2b
preserves main62e35c8f activity sidebar/icons/design system plus feature3d715c84
native/optional-Python/fluorescence/spatial-count work. Only this task-history
document conflicted; both histories below are retained. GUI source is byte-identical
to the main parent. [Integration evidence](benchmarks/main-integration-20261108/README.md).

Existing active checkout/cache synced to the merge; combined native app/entry/
fluorescence/spatial targets build. All four selected native cases pass, and the
merged GUI opens/renders the saved spatial-count project and exits0. All38 binding
objects preserved. No new checkout, binding compilation, full science campaign,
binary release or cleanup. Source main handoff follows these passing checks.

Waste review: resolve only the one documentation conflict, preserve both UI and
native changes, and verify the combined frontend/workflow link in the existing
cache. Prior scientific/UI results retain their source scope; no duplicate campaign.

## Delivered task - open-format fluorescence and spatial counts

[Plan/checklist](fluorescence-omics-plan.md), [usage](fluorescence-omics.md) and
[evidence](benchmarks/fluorescence-omics/README.md). FO00-FO05 complete for the
selected finite workflows; local and hosted native-only checks pass.

Reused existing --kind raw/explicit-axis batch, TIFF/raw measurements and
retention instead of adding a fluorescence pipeline. Independent uint16/float32
OME fixtures pass axes/names/calibration/high/zero/nonfinite values, display-
independent hashes, CSV, save/replay, batch and invalid/stale/no-overwrite checks.
New C++ Matrix Market/TSV reader keeps bounded sparse input and only densifies the
selected projection. Existing point detections/ObjectProperties/save_new retain
selected genes, exact counts≤2^53, IDs/coordinates and input digests. No new project
schema, HDF5/Zarr dependency or Python implementation. CLI imported projects and
saved fluorescence records open in the existing viewer/properties inspector.

Three focused native tests pass; installed CLI/no-overwrite and explicit-channel
fluorescence batch pass with PATH=Windows System32. Both saved-project viewer
smokes pass. Python discovery was disabled; no binding targets, all38 binding
objects preserved. New executable admission in Windows/Linux package dependency
manifests passes109 focused archive/runtime regressions, including refusal of a
Python-dependent spatial CLI. This does not publish/qualify a new wheel.

[Linux native-only run42516464079](https://github.com/bipxx/qupathxx/actions/runs/42516464079)
passes configure/build/tests/install/runtime-dependency and installed-consumer
checks on native source99e75706. Optional-interface cache flags restored without
binding rebuild. Usage/support documentation and source handoff complete.
No further biological/parity feature, full matrix, new binary release or dataset
download is selected. H5AD/Zarr, unmixing/segmentation/normalization/clustering and
vendor-native formats remain outside this slice.

Waste/corrections: direct ObjectProperties include missing on first compile;
test fixture used tile2 below native minimum16; bad-channel rejection belongs at
analyze_raw's actual worker boundary, not necessarily snapshot construction.
Fixed inputs/includes without weakening production checks/assertions/tolerances.
Reused passing spatial evidence during fluorescence repairs. One cache/component
install, no binding compilation. Existing viewer/schema reused. New native CLI
must be admitted as an explicit package entry point so Python-independence checks
also cover it; synthetic PE/ELF archive tests avoid a packaging rebuild.

## Delivered task - native C++ usability and CI guard

Owner approved one C++ example, native build/backend documentation and a
Python-disabled CI configuration. No new binary release or scientific migration.

- [x] U01: small project open/analyze/retain/save/reopen/replay example using
  shared APIs, with a native synthetic input and no-overwrite check.
- [x] U02: native build defaults/optional SDK requirements documented.
- [x] U03: bounded headless Linux CI guard; default Python OFF, discovery disabled,
  no binding targets/products/runtime imports; installed C++ consumer exercised.

[Guide](native-build.md) and [evidence](benchmarks/native-usability/README.md).
Two selected native CTest cases plus installed example pass with Python discovery
disabled. No binding targets; all38 binding objects preserved. Existing Windows
GUI/SDK flags retained, optional Python configuration restored. New example source
is included in the compiler-input manifest. Shell syntax and local links pass.
Initial Linux run42513372855 exposed ambiguous checkpoint lookup under GCC.
Four calls now name the intended helper; algorithm/cancellation behavior unchanged.
Affected local cells/example plus installed replay pass. Corrected Linux
[run42513681737](https://github.com/bipxx/qupathxx/actions/runs/42513681737) passes
all configure/build/tests/install/runtime-import/installed-consumer checks on
source673caa69. This closes U01-U03 for the selected native usability slice.

Waste review: one cache, example component install only; no binding rebuild,
wheel, scientific campaign or release. GCC failure diagnosed before retry;
keep the original failed run as evidence. Stop this usability
slice after source handoff and CI status; add further consumers for a named need.

## Delivered task - native-first application and optional Python

[Boundary audit](consolidation-python-boundary.md) and [follow-up checklist](consolidation-coverage.md#native-first-optional-interface-follow-up):
7/7 complete. Reviewed all3 package modules/11 supported examples. Scientific,
cohort/import/reuse/persistence/export authority remains native; independent
Python tools/oracles are development code, not a product dependency.

Moved the duplicated desktop startup into shared C++ run_application, used by
native executable and optional argv-only Python launcher. Moved raw-classifier
lattice/tile/feature recipe, checkpoint/train/predict/retention/no-overwrite and
native lifetime into run_raw_classifier_workflow, also exposed through the C++
CLI. Python example now forwards arguments and displays native JSON. Normal
complete runs avoid the old forced one-window/resume IO; explicit native resume
APIs remain. No scientific algorithm or data schema changed.

Plain CMake now defaults Python OFF; actual wheel backend settings explicitly
opt in. Existing cache configured with Python/Python3/nanobind discovery disabled:
no binding targets or installed Python files. Native GUI/CLI train/save/stream
verify/project reopen run with interpreter directories absent from PATH; all6 PE
closures have no Python DLL. Original Python, optional native interface, native
CLI and Python-OFF install preserve identical36-row/four-window model/stream
hashes. Default headless application entry compiles/links/runs against core alone.
[Evidence](benchmarks/native-optional/README.md).

Checks:11 controller/workflow cases,8 actual launcher cases,15 public-stub/API
cases; native workflow contract plus GUI/headless entry checks pass.36/38 binding
objects reused for new APIs (large bindings.cpp preserved); all38 unchanged
through native-only/headless/restoration checks. Optional development cache and
stage restored. Final source receipt owns current identity; earlier observations
retain their tested source/configuration. AGENTS now enforces native-default,
no Python process fallback, and delivered migration status.

Waste/corrections: froze one old-workflow baseline and reused its input. First
GUI selection skipped the opt-in tests; explicitly enabled the established flag
and obtained actual coverage. Checked the newly introduced headless source branch
without recompiling optional bindings. One cache, no duplicate checkout/wheel,
full scientific campaign or new binary release. Candidate7 stays frozen. Stop the
selected audit; future app-owned recipes go into shared native services.

## Delivered task - all five selected gaps closed

[G01-G05 checklist](remaining-gaps-checklist.md): **5/5 complete** for the finite
Windows research acceptance targets selected before execution. [Candidate7 is
public](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.7):
all14 asset sizes/SHA256 digests match before/after publication; source tag0ff8c5c3
matches the571-input raw source kit and native tree
b633f9924589299be1a4900f3f4afc37aa12d270251822f0bcaffe04c67869fc.

G01 comparison remains source-scoped, with overlapping native305.6ms /QuPath313.1ms
ranges and no general speed claim. All scientific inputs match that measured
source; only private importer/tests/CMake registration changed. G02:2,768 identical
coarse tissue calls/600.08s,1.07MiB private range within32MiB bound,100,001-object/
600-frame viewer with actual OS GPU process counters, exact tiles/oracle/cancel
and8 publication guards. G03: fixed independent CMU-2 crops give6,732 exact cells;
20,196 quantitative compartment domains pass. G05: native vertex-touch admission
and unnamed-object retention preserve all558 cells/559 objects and22,880 bit-exact
stored values plus masks/names/parents/calibration/reopen. Native controls and5
Python importer regressions pass. [Evidence](benchmarks/all-five-gaps/README.md).

G04:69 immutable installed wheel members/DLL origins, static wheel70/7 and native
59/6 closures, installed density/import/bidirectional reuse and full558-cell
preservation, standalone app/batch and removal with33 output hashes unchanged.
Matching source/notices/12 open external dependency archives plus vendored
LodePNG are published. All38 binding objects reused; new logic is C++ only.
Candidate6 and earlier releases remain frozen. Native's historical f110c183+dirty
label is disclosed; raw compiler-input/source/runtime receipts own provenance.

Waste review: fixed a wrong float64-per-pixel OD oracle by using the declared
float32 domain, retaining1e-10 tolerance and all cases; recovered its successful
saved archive without redetection. Strict comparison found/fixed null-name loss.
Robust orientation review required a narrow correction and superseded one local
preparation, not another scientific campaign. Both preparations/failures remain
local; only final bytes published. Corrected Windows text decoding/field-scoped
release lookup without a duplicate upload. No full binding/platform/corpus rebuild.

Stop this selected batch. Remaining unselected capability/support boundaries are
in the checklist/known limits; do not convert them into another automatic parity
phase. No universal speed, biological, lifetime/hardware or additional-platform
claim is made. Future work needs a real workflow and finite acceptance target.
## Delivered task - activity sidebar and compact workspace chrome

Owner requests a VS Code-style left activity rail instead of tabs, drawing tools
pinned above the slide, no side-panel dock dropdown arrows and less excess
padding. Use shared native icons with translated hints, selected indicators and
Ctrl+1..4 navigation; save the selected sidebar separately from project data.
Keep existing pane identities, split sizing, legacy saved arrangements, scientific
camera/pixels, disabled/modal routing and reset protections. Remove outer gutters,
trim panel/form spacing and keep readable fonts and icon/action hit areas.

Continue the named UI-worktree exception from main36199638: the active integration
checkout has new dirty native-build/CI work. Do not merge over it or write its
cache. Settle layout and tests before one GUI/font build (the shared metric
header affects both), reusing retained native/entry and compatible binding
objects. Check activity selection,
persistence, toolbar alignment after resize/splits, old layouts, real saves,
localized/scaled controls and exact image/camera/project invariants. No kernel,
binding rebuild, new wheel or scientific campaign is selected.

Initial checks: all activity clicks/persistence, toolbar ownership, legacy split
geometry, saves and pixel/camera contracts pass. Eleven checks still assumed a
tab-order suffix in ImGui DockId settings or the former sparse language-row
spacing. Adapt those interaction helpers to hidden-tab serialization and the
denser selector; retain the exact state/pixel assertions. No product rebuild is
needed for these test-only corrections; preserve the first log/fixtures.

Outcome:44px activity rail replaces sidebar tabs, with four shared vector icons,
translated hints, selected indicator, Ctrl+1..4 and saved activity. Drawing tools
belong to the slide pane and wrap there. Dock-tab/menu arrows are disabled in
both default and retained layouts. Outer/slide gutters are zero; panel insets are
12px/8px, toolbar8px/4px and field spacing8px/4px. Readable fonts and action hit
areas stay intact. All101 focused cases pass, including real activity clicks,
resize/split geometry, legacy layouts, hidden panes, language switching,200%
controls, save/load/raw export and exact camera/pixel/project invariants. Updated
dark/light/Chinese slide captures are visually inspected. See
[validation](benchmarks/design-system/activity-sidebar/README.md).
One GUI/font compile and relink reused all38 binding objects unchanged, the
retained entry and native libraries. No scientific/cache/release changes.
Preview and all passing/failed outputs remain in main build/activity-sidebar.
UI commit3f9a92d6 is merged into local main3d869792. Nine canonical source hashes
and all three capture hashes match after promotion. The temporary UI worktree
and merged branch are removed; the independently advancing native integration
checkout is preserved. No push or publication.

## Delivered task - consistent command icons

Owner requested more consistent icon buttons. Reuse the native design system:
one vector stroke family and IconButton recipe for project/image/file commands,
refresh/reset, clipboard, list controls, dismiss and metadata/class utilities.
Keep translated hover/focus captions, native disabled/selected behavior, stable
English widget IDs and existing replay targets. Analysis commands and explicit
destructive confirmations retain text when an icon alone is ambiguous.

Continue the named UI-worktree exception from main96319e4f; the separate native
Python-boundary migration is active in scientific-grid-159 and is untouched.
Before compiling, settle all frontend/icon changes; then compile gui.cpp and the
unchanged standalone entry (to avoid a moving cache's launcher object),
reuse retained font/native/binding inputs and relink a separate icon preview.
Decisive checks: existing routed save/display/overview/clipboard/class/metadata
flows, translated and200% layouts, disabled controls, camera/project invariants
and rendered dark/light icon screens. No kernel/full binding rebuild or publication.

Build correction: the GUI and unchanged standalone entry compile/link successfully.
The separate migration replaced viewer_bindings/native_workflow_bindings in the
shared cache with calls absent from the retained native libraries. Preserve that
failed link; compile only those two unchanged binding sources from this UI branch
into private compatibility objects, reusing the other36 and the passing GUI object.
Do not recompile gui.cpp or touch the migrating cache to fix this source mismatch.

UI verification found a new default-focus tooltip and exposed existing clipped
model-load/display-reset targets (confirmed on the retained baseline), plus a
clipped raw export. Use ImGui's native hover/navigation tooltip policy, place
model file icons before the long form and constrain the raw panel to the viewport
with more usable initial height. The per-image display test now selects the
visible Image tab and samples its canvas center, retaining exact expected RGB.
Recompile only gui.cpp for these corrections; retain first checks/screens/logs.
The first correction made file Load visible but Apply still needed scrolling at
125%; fold feature options on short windows while preserving their values and
validation. Correct the test's tab caption to its stable Image and display ID.

Outcome: one shared vector family and IconButton now covers drawing, project/
image/file, refresh/reset, clipboard, class/default, metadata/tag, model-file and
record-navigation utilities. Translated hover/navigation hints, keyboard focus,
disabled/selected styling, stable IDs and existing native command targets remain.
Complex analysis commands and destructive confirmations retain explicit text.
All80 focused GUI/source-contract checks pass, including real save/load/export,
clipboard persistence, exact RGB, all16 locales and200% layouts. Updated dark,
light and Chinese slide screenshots are visually inspected. See
[validation and corrections](benchmarks/design-system/icons/README.md).
The preview is main build/icon-consistency/qupathxx_app.exe. Native libraries and
font object were retained;36 binding objects are hash-unchanged and two original
sources were privately compiled for compatibility. Scientific cache/products,
the concurrent migration, fixtures and failed attempts are preserved. No release
or full scientific/binding rebuild was made.
UI commit69f83b12 is merged into local main6bd123da. All17 canonical source and
three screenshot hashes match after promotion. The temporary UI checkout and
merged branch are removed; the preview, build helpers, failures and fixtures
remain under main build/icon-consistency. The separate scientific checkout has
advanced independently and is preserved. No push or publication.

## Delivered task - COLMAP WebView design system

Owner selected `../colmap_webview/colmap-webview` as the new UI reference.
Use its current `docs/design-system.md`, CSS tokens and shared component recipes
at121bd73a: neutral dark default, matching light palette, IBM Plex Sans14px,
regular/semibold roles,4/8/12/16/24/32 spacing,8px panels, fine borders and32px
actions. Implement reusable native tokens/components and appearance preferences;
keep scientific pixels, overlays, calibration and result state independent.

Work is isolated on `feature/colmap-design-system`, from local main605bba55,
because the active integration checkout has ongoing native import/resource work.
This named UI worktree is the exception for this owner-selected UI outcome.
Do not touch that checkout or regenerate its cache/runtime. GUI-only preview
compilation reuses retained native libraries and binding objects.

Before compiling: validate token agreement with the local reference, font bytes/
OFL notices and theme/locale preference contracts. Then compile the two changed
GUI/font objects, relink a separate preview, inspect dark/light/translated/scaled
screens and exercise changed user flows. Reuse scientific acceptance. New sans
metrics deliberately supersede the earlier Proggy bitmap appearance; preserve
international fallback fonts, atlas budgets, per-module asset loading and IDs.

Outcome: shared semantic palettes, spacing, typography and action recipes now
drive native panels. IBM Plex Sans regular/semibold matches the reference font;
the complete OFL and authenticated font resources ship with the GUI. View >
Appearance offers saved Dark/Light/System choices. Compact vector commands and
collapsible Project tools keep localized controls usable at200% scale. See the
[design-system guide](design-system.md) and
[source-scoped validation](benchmarks/design-system/README.md).

Checks:90 real-GL/source-contract cases pass; five unchanged installed-subprocess
font/launcher cases are deselected. Native design-token/contrast/target-size and
131-entry/16-language checks pass. Dark/light/Chinese screenshots use the retained
CMU-1 ROI, with exact synthetic image pixels, camera, save/reopen and project-byte
invariants checked separately. The preview reuses native libraries and38 binding
objects; no scientific qualification, wheel or release is replaced. Live desktop
inspection was stopped by physical Escape; no further desktop input was issued.

Waste/correction: proportional text invalidated the old fixed overview-width
assertion and a language-menu coordinate. Keep exact native thumbnail data and
test the displayed aspect/complete area; isolate image rows from antialiased gray
text. Use the native language shortcut. A Russian200% save control was obscured
by long utility labels; shared icon controls resolve it. Development fontTools
initially selected Python3.14; reinstall only the private tools for Python3.12.
Local preview inputs, failures and scripts stay under main build/design-system-preview.
UI commitb4aeb25f is merged into local main37cfc747. All23 canonical changed
source/test/asset hashes match after promotion. The temporary checkout's files,
Git registration and merged branch are retired;549 build/tool files (16,445,383
bytes) are preserved in preview/dev-tools/source-worktree-build. Windows initially
refused final directory removal; automatic approval review blocked the subsequent
empty-folder removal with no reason beyond "blocked by policy". That empty folder
remains. The separate scientific integration checkout and its newer native work
stay intact. No push or publication.

## Delivered task - desktop UI merged to main and worktree retired

Owner requested merge and cleanup. UI commit87745eee and integration merge
e26d773e are included in local main merge6a8b3274. The latest committed native
integration state0ddf944f and remote main ancestry are retained. Reviewed tree
identity and all six tested GUI C++/catalogue hashes match after promotion.

[Durable UI validation](benchmarks/desktop-ui/README.md) is committed. All594
temporary build files (1,259,877,543 bytes) are SHA256-verified in a118,125,317-byte
ZIP under main `build/ui-design-handoff-20261108`. Passing products, both recovery
snapshots, fixtures and failures remain recoverable. The UI worktree and merged
branch are removed; disposable Python/pytest caches are retired. About1.14GB net
was reclaimed. The prior untracked main router is preserved there before the
canonical tracked AGENTS.md was merged. Main build, published products and
storage-cleanup-20261105 were not removed.

The active integration checkout/cache/runtime and its untracked density fixture
remain. Native import work appeared there during this merge; its tracked and
untracked edits were neither stashed nor overwritten. A fast-forward attempt
stopped before changing files because CMake/current-task had pending edits.
Synchronizing that checkout to main is deferred until that work can be reconciled.

Checks: existing80-case GUI/native localization evidence, unchanged GUI hashes,
merge ancestry/tree identity, per-file archive verification and no running preview
process at retirement. Attribute/status merge conflicts retained both sides.
An initial whole-history whitespace check encountered preserved old captures;
the actual merged tree is exactly the reviewed integration tree. No historical
capture was rewritten, numerical campaign rerun or native binary rebuilt.

## Delivered task - multilingual desktop UI

Owner requested popular languages with a proper selection system in the UI
worktree. Deliver16 built-in languages for desktop chrome/common controls,
native and English language names, search, Automatic system-language selection,
immediate switching and saved per-workspace choice. Preserve English fallback
for advanced messages without a translation; document the coverage boundary.
Do not offer complex-script languages before the renderer can shape them.

Changed inputs: GUI translation catalogue/helpers, workspace preferences and
GUI call sites. Keep numerical locale, scientific state, user text and widget/
dock/popup identities unchanged. Validate catalogue/format/ID and locale mapping
cheaply first, then one GUI-only cached compile/relink, language-specific real-GL
save/reopen/camera/DPI and selector checks, plus affected English UI regressions.
Reuse native libraries and binding objects; no core rebuild, wheel or release.

Outcome:16 languages and126 shared translation entries are delivered with a
searchable native/English/code chooser, case-insensitive Latin/Cyrillic search,
immediate switching, system detection and `.ini` preference persistence. Reset
layout preserves the choice. Stable English widget/popup/dock IDs and session
restoration prevent preference changes from touching scientific/user state.
The chooser blocks background editing. [Guide and language coverage](docked-workspace.md).
Arabic/Indic shaping and full advanced-dialog translation remain outside this
initial catalogue; missing entries use the declared English fallback.

Checks:80 real-GL GUI cases pass in `build/ui-language/pytest-combined.log`;
five unrelated installed-subprocess font/launcher cases are deselected. Native
catalogue/format/locale/search/identity/preference/session checks pass in
`build/ui-language/native-tests.log`. Both live chooser paths,16 saved-language
roundtrips, localized save at200%, hidden controls, exact viewer pixels, camera
and unchanged stored user text/project bytes are covered. Two rendered previews
are linked in the guide. Scripts/build/receipt remain under `build/ui-language`;
`qupathxx_app.exe` there is the new local preview. The UI-only cache rebuild reused
native libraries and38 binding objects; there is no new scientific qualification.

Waste/correction: the first preference test used a nonexistent update_annotation
method; use the existing rename_annotation API and rerun those cases. Quote/run
Windows batch paths natively rather than passing slash-separated relative paths
to cmd. Native replay proved the chooser flow after desktop click timing was
ambiguous. Include the catalogue `.inc` in source manifests and use UTF-8 on MSVC.
No additional feature phase or publication is selected.

## Delivered task - QuPath-style desktop UI

Owner requested a separate UI worktree. Branch `feature/qupath-ui-design` starts
at integration commit4546b6e6; uncommitted detector optimization remains untouched
in `build/ci/scientific-grid-159`. This explicit scope supersedes the previous
single-checkout restriction for this UI task. Preserve its cache and runtime.

Deliver light desktop chrome, a menu bar, vector drawing toolbar and a left
Project/Image/Annotations/Workflow tab group around the existing image viewer.
Keep native scientific services, pixels, result colors and project semantics.
Existing layouts retain their splits; Reset layout selects the new default.

Changed inputs are `gui.cpp`, `gui_workspace.hpp` and `gui_theme.hpp`. Before
building, decisive checks are a GUI-only compile/relink using retained native
libraries and binding objects into this worktree's `build/ui-design`, then
changed desktop flows, tab navigation, saved layout, DPI/resize, image input and
save/reopen. No native kernel rebuild, wheel, release or numerical campaign.

Outcome: the light theme, native menus, vector drawing tools, left analysis tabs,
viewer overview and status bar are delivered. [Guide and preview](docked-workspace.md).
The GUI-only development overlay is `build/ui-design/qupathxx_app.exe`; the
test-only Python runtime is under `build/ui-design/runtime`. Retained core
libraries and all38 binding objects were reused; the integration cache and
installed runtime were not rebuilt or installed by this UI task.

Validation:56 Windows real-GL GUI cases pass in `build/ui-design/pytest-final-verified.log`
(docking, layout, overview and scaling). Five unrelated font-package/launcher
cases were deselected because isolated Python subprocesses use the installed
runtime. Checks include save/reopen, exact cached viewer pixels,100%/200% tab
contents, camera/project invariants, hidden-control readiness, legacy split
preservation and Reset layout. Manual desktop checks confirm menu tab navigation
and rectangle drawing. Local build/receipt/scripts remain in `build/ui-design`.
This is a development UI preview, not a newly qualified scientific release.

Waste/correction: an open task-owned demo initially locked the preview executable;
close it and relink existing objects. The first viewer overview restored a cursor
outside the content boundary; remove that restoration rather than suppress ImGui
diagnostics. Persisted tab selection uses each window's seeded `#TAB` ID. A later
cache comparison differed in34 fractional-DPI chrome glyph pixels; its exact
comparison now covers the viewer surface it is intended to qualify. Scientific
pixel, geometry and project assertions stay intact. Separate Annotations bounds
from Project bounds when users move tabs. No further feature phase is selected.

## Delivered task - candidate6 handoff and remaining gap status

Owner selected G01-G05 closure; [checklist](remaining-gaps-checklist.md) records
actual progress without converting broad unqualified claims into passes.
G01 comparison is complete: native305.6ms /QuPath313.1ms, exact2,237-cell RGB/
partitions; overlapping ranges/high load prevent general superiority.
G02 selected coarse whole-slide case/repeats/viewer pass; wider level-zero cells,
dense scenes, long-duration/GPU residency remain open. G03's new558-cell NDPI
crop matches masks/RGB/calibration/independent arithmetic; broader biological/
context domains remain open. G05 supported saved-data controls pass, but actual
upstream crop data has an invalid self-intersecting ring and is refused before
publication. Preserve strict refusal until explicit repair/admission semantics
and decisive result-preservation checks are designed.

G04 candidate6 freezes the exact tested tree2875ca45 without a native rebuild.
Wheel69 immutable members/DLL origins, static wheel70/7 and native59/6 closures,
installed native/Python density/import/bidirectional reuse, standalone app/batch,
and uninstall with29 output hashes preserved pass. Source includes all568 raw
compiler inputs,12 authenticated external dependency archives plus vendored
LodePNG source/notices. No new binary qualification is borrowed from candidate5.
[Candidate6 is public](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.6):
all14 asset sizes/server SHA256 digests match before/after publication; tag targets
f1bb522c, whose source archive contains the repaired test and identical568 compiler
inputs. Matching wheel/native bytes were never rebuilt. G01/G04 closed; G02/G03/
G05 remain partial. Next concrete task: native explicit-loss handling for the
retained invalid-ring import case, with all558 masks and retained measurements
compared against upstream; do not silently repair/drop geometry or accept
arbitrary crossings. Broader domains/platforms need finite acceptance targets.
[Evidence and limits](benchmarks/gap-closure/README.md).

Waste/corrections: no compile, binding rebuild or corpus campaign. Removed unused
SciPy assignment import and reused successful captures. Corrected one selected
test name and used development pytest against the explicitly installed package.
Two controls pass; the sole failing assertion expected obsolete class flattening.
Replaced it with retained-parent/reopen checks; focused repair passes. Final source
archive includes that test correction through a source-only refresh; wheel/native
assets stayed unchanged. Draft lookup by tag returned404; fetched/verified and
published the existing draft by release ID, without duplicate upload. No further timing search or intermediate
binary packaging. Optional O01-O05 stay deferred, not parity obligations.

## Delivered task - remaining gaps checklist

[Remaining gaps checklist](remaining-gaps-checklist.md) separates the completed
28/28 consolidation and contour optimization from five open evidence/delivery
categories and five optional expansion categories. These are scope categories,
not a count of defects or mandatory implementation tasks. No new feature,
benchmark campaign or binary release is selected by this planning task.

Suggested next for the speed question: G01, one bounded current-source/QuPath
comparison using existing fixtures/harnesses. Latest native-versus-native timing
does not establish a QuPath win. G02-G05 apply to selected workload/support claims;
optional O01-O05 require a concrete user need.

Check: local checklist links and git diff whitespace; documentation only.
Waste review: reused existing acceptance records instead of creating another
per-API ledger or rerunning builds. Corrected stale consolidation introduction
and labeled the older direct QuPath comparison as historical. Next: select one
outcome from the checklist; keep completed workflows closed.

## Delivered task - exact nominal contour-cycle optimization

[Contour evidence](benchmarks/contour-optimization/README.md): observed detector
median341.3 ->302.9ms (11.2%) in six alternating baseline/candidate pairs on the
selected512x512/2,237-cell ROI. Twelve serial contour pairs show nuclei113.7 ->100.0ms
(12.0%) and bodies135.9 ->111.3ms (18.1%). This is a local matched result, not general
QuPath superiority or an overall workflow/archive ratio.

A single unambiguous closed cycle in the noded raster boundary graph proves a
simple shell. Carry that certificate and known label to avoid duplicate pre-clip
validity/interior queries; move standalone polygons into ownership. Ambiguous,
multi-ring/hole/disconnected and non-nominal/general faces retain GEOS validity.
All final clipped ROIs still pass full validity/nonempty/positive-area checks.
Robust clipping, mapping, order, labels/area, limits and cancellation stay intact.
No public trusted-validity parameter, schema, extra thread or new decoder.

Three affected native suites pass2.62s. All596 nominal/general topology patterns
compare exact WKT and admit exact work while refusing one byte less. Every measured
pair has identical ordered geometry, edge/vertex/work/retained counts. Final
installed replay preserves all2,237 cells' masks, ordered geometry and features.
Final tree2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e
(568 inputs) matches the source manifest/generated provenance/installed runtime.
All38 binding objects reused; only changed private core/test/provenance inputs
compiled and existing frontends relinked. Same cache/runtime and retained fixture;
no new release, broad numerical campaign or repeated archive publication.

Waste review: the duplicate-query/clone-only trial showed~0.6% median change within
noise and was not accepted as a gain. Initial serial A/B/A drift likewise gave
no usable speed claim. One bounded interleaved comparison resolved host-time drift;
then the measured repeated validity cost justified the exact cycle proof. Four
warmups/12 contour pairs and four warmups/six detector pairs retain all samples,
never timing workers concurrently. Profiling source was restored with fresh
timestamps; accepted workers emit no diagnostics. Trials/failures remain preserved,
and durable notes are in compilexx.

Stop this bounded batch after matching source handoff. Existing consolidation
remains28/28 complete; remaining intersection/nucleus work requires a new measured
opportunity. Do not add percentages across batches or claim general speed parity.

## Delivered task - paired native raster identity optimization

Owner-selected [native detector optimization](benchmarks/native-detector-optimization/README.md)
removes duplicate source/domain hashing for nominal nucleus/body masks. In the
isolated same-process alternating probe, raster construction21.28 ->15.49ms:
27.2% stage reduction, about5.79ms saved. Both masks retain separate label/domain/
counter/component/budget/expected-digest admission; only an internally produced
identity is reused for the same immutable source/input. General rational-grid
construction remains unchanged. No public trusted-hash API/cache/new worker.

Full detector A/B/A medians329.8/334.0/330.0ms under31-36% other-process CPU load
do not demonstrate an overall gain. Preserve those samples; do not rerun to chase
a favorable median or claim QuPath parity. Diagnostic profile identifies contour
trace~122ms and nuclei~86ms as larger costs; contour clipping/order is unchanged.
The selected duplicate-work reduction is evidenced at its own stage only.

Three affected native suites pass2.23s, plus the final pair test after opt-in probe
edits. Pair-versus-single nominal/fractional/context/general values and exact
per-mask budget/identity/label/cancel refusals pass. Full outputs preserve exact
RGB, both masks, ordered geometry, stored features and density CSV for all2,237
Cells. Fresh archive replay verifies in the final installed runtime. Final tree
a3cbffa4ae17136523f40464a09acfc5504a8132231694cfcd4d82efa8b3f6e7 has568 inputs;
source manifest/generated provenance/installed module agree. All38 binding objects
remain byte-identical. Existing cache/runtime refreshed; frozen products/fixtures/
archives and diagnosed failures remain intact. No new release or schema.

Waste review: copy2 restored old timestamps and MSBuild reused a diagnostic object;
retain that invalid candidate run, touch restored source and rebuild only the
changed object/relink. Fix the probe's stain array/vector conversion and respect
request tickets when verifying replay. Compare stored geometry directly; recompute
one replay through the native request. The isolated alternating probe resolves
duplicate-hash benefit without another full pipeline/QuPath campaign. Temporary
profile instrumentation is removed. Durable lesson added to compilexx.

Stop this bounded batch after matching source handoff. Further contour/nucleus
changes need a specific measured optimization and exact identity evidence; broader
speed superiority remains unproven. Consolidation remains28/28 complete.

## Delivered task - focused post-consolidation speed comparison

Owner-selected fresh comparison is [complete](benchmarks/post-consolidation-speed/README.md):
current native cell pipeline median324.8ms versus314.1ms QuPath, an observed10.7ms/
3.4% native overhead on the same512x512 ROI. Native range320.4-330.3ms overlaps
reference294.9-346.3ms; other-process CPU averages23.5%/26.8% of logical capacity
over complete engine lifetimes. This one block establishes neither a reliable
remaining wall-time gap nor speed parity/superiority. No product change is justified
by this small noisy difference alone.

Exact RGB and canonical nucleus/body partitions agree across engines for all2,237
Cells. Native ordered mask hashes match the retained accepted result; calibration,
analysis area and RGB sums agree. Six native/24 JVM warmups and six measured
samples each use the existing recipe. Native cell detection dominates at320.5ms;
publication4.3ms, capture0.06ms, read7.8ms and density9.4ms are separate. CPU is
reported for all engine threads and includes possible JVM JIT/GC effects.

CSV/archive/reopen ran once each, retaining about13.6MiB total evidence instead
of per-iteration large archives. The formats/retained work differ, so their single
observations are descriptive, not a save/reopen or whole-workflow speed ratio.
Native versus scripted reference statistics also have no equivalent-work ratio.
Source/runtime remains15f6a9baf4ce114189cdbdcb00ccc565062e84a670fdb1348eeb393eb7856011;
no C++ rebuild, new runtime, wheel, release or broad test campaign occurred.

Waste review: reuse accepted scientific qualification, capture one final oracle
pair and keep every timing sample. Do not repeat blocks merely for a favorable
median or change scientific safeguards to chase an overlapping10ms observation.
Stop this measurement. If further latency/CPU reduction is selected, the useful
next task is a bounded internal profile of the current native detector before
choosing a specific optimization. Consolidation remains28/28 complete; broader
QuPath parity and reliable general speed superiority are unclaimed.

## Delivered phase - native consolidation NC01-NC28

The [single consolidation checklist](consolidation-coverage.md) records **28 complete,
0 in progress,0 planned**. Shared C++ density/manifest/reuse/export and NuCLS import
services, native training-cohort selection, focused bindings, thin controls,
shared frontends, boundary cleanup, API/stubs/guides and source handoff are complete.
Stop this selected phase; do not automatically start another release, optional
QuPath feature or speed campaign.

[Final boundary receipt](benchmarks/consolidation/boundary-receipt.json) records
raw native source tree15f6a9baf4ce114189cdbdcb00ccc565062e84a670fdb1348eeb393eb7856011
(567 inputs), independently matched to the generated manifest and installed module.
Compiler and control/stub/tool manifests identify tested files. All11 shipped
examples and three package modules are reviewed; native preview owns pixels,
calibration and reader dispatch. Pixel controls take supplied projects/ROI IDs/
models; independent synthetic/NumPy/CSV checks remain runnable in tests/tools.

[Boundary evidence](benchmarks/consolidation/README.md#shipped-python-boundary-closeout):
new native preview suite passes;10 installed changed-example cases pass with one
optional GUI interaction skipped;15 public stub/signature checks and10 focused
binding ownership/range cases pass. Negative/overflowing numeric conversions now
raise ValueError before altering options. The final installed stub check matches.
Only two focused binding objects changed;36 others, including the expensive main
binding, were reused. No scientific algorithm, tolerance or codec dependency changed.

[Consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair)
retains source-scoped native/installed authority checks. Native and thin Python
routes agree on8 density rows/CSV, bidirectional2-image resume with zero computation/
project copies and checkpoint replay. Both NuCLS routes preserve the32/28-object
subset's exact legacy pixels, geometry/nuclei, calibration, IDs/revisions, classes
and metadata. Source inputs stay unchanged. The archive golden roundtrips exactly
and refuses resealed off-by-one charges. Three training examples and standalone
app startup smoke pass. Unchanged authority inputs are explicitly matched against
the final tree; these accepted scientific checks are reused, not repeated.

Waste review: preserve exact historical eager/current WKT admission layouts,
correct native output-error precedence and test preview versus manifest guards
according to their actual contracts. Move independent checks to tools without
erasing them. Resume completed fixture outputs after a checker API mistake.
Catch binding range-conversion errors at the language boundary. Details/failures
and new durable lessons are retained in evidence and compilexx's canonical notebook.

Matching source is handed off on origin feature/scientific-grid-159. The one
cache and active runtime (main build/consolidation/development/qupathxx), frozen
products, fixtures, archives and failure evidence remain preserved. There is no
new wheel/binary release,100% parity or reliable general speed-superiority claim;
those broader scopes remain separate from this completed consolidation.

## Delivered phase - C++ cell kernel optimization L1/L2/L4

[Evidence](benchmarks/cell-kernel-optimization/README.md): direct interior
watershed neighbors and same-byte Windows SHA buffering preserve exact outputs.
Native median328 ms versus348 ms retained old module (~5.9% faster). Extended
QuPath median347 ms spans305-403 ms; native has a lower median in this run, but
no reliable general speed win/strict parity is confirmed. The seven affected
native suites pass;28 pre-change thin cases and26 ImageJ cases plus mixed/hash
boundary/finalization tests pass. Four full RGB/mask/CSV comparisons and four
reopened detector checks retain all2,237 cells and ordered geometry exactly.

Raw tree ef7338dd16763fc5c62b5aeb501a4b1eeb6540acb0f6bb5a17c343e175727584 has530
inputs. Only private native inputs/provenance rebuilt; all37 binding objects are
unchanged. Same cache/runtime and shared DLLs, one small retained control module;
no new wheel, install, binary release or language-boundary change.

Waste review: six JVM warmups did not settle the first reference (412 ->318 ms).
Expose explicit counts and repeat only that reference with24 warmups/six measured;
retain both runs and completed native results. Extended variability remains, so
do not chase favorable medians or compare historical absolute times across host
windows. No extra predicate workers, geometry shortcut or identity-token framework.

Next planned phase is L5's shared headless C++ density/selection/resume/export
service, then L6 importer and L7 thin controls. Conditional L3 sharing is deferred
until profiling justifies its ownership complexity. The scientific/performance
claim remains scoped to this ROI; stop this batch after source/evidence push.

## Storage maintenance -2026-11-07 complete

Owner interrupted optimization to recover disk space. No L1 implementation had
started; source was clean. [Receipt](benchmarks/storage-cleanup-20261107/receipt.json):
standard NTFS compression of144 inactive artifact folders saves1.51 GiB;
all10,369 files retain their original SHA256. C: had18.46 GiB free at verification;
other activity also freed space, so only the measured1.51 GiB is attributed here.

Active source/cache,37 binding objects, selected module/runtime bytes, published
products, fixtures, original ZIPs and storage-cleanup-20261105 are preserved.
Recursive removal of verified ZIP duplicates was rejected by automatic approval
review (blocked by policy); no folders were deleted or deletion retry attempted.
Normal NTFS compression preserves regular-file behavior; no WOF/reparse encoding.

Waste correction: Linux SDK link entries cannot be read as regular Windows files;
exclude those SDKs and preserve partial hash progress rather than repeat an
all-tree scan. No product build/test campaign or cache regeneration. Next planned
work remains L1's C++ watershed optimization; preserve the expensive binding cache.

## Delivered prerequisite - untangle codebase before optimization

Owner-invoked untangle-codebase with Comments+Tests completes U1-U6 in the
[decision log](untangle-decision-log.md): neutral JSON/dependencies, self-contained
watershed modules, shared contour/archive authority, density reuse, replay
vocabulary and explicit panel target membership. Independent repeat reviews
found no further worthwhile issue in these addressed areas. Meaningful science,
archive, compatibility and quota tests remain; redundant/stale comments and a
superseded replay acceptance subset are removed.

[Evidence](benchmarks/untangle-codebase/README.md): ten standalone header checks,
16 distinct affected native suites across two source-scoped steps,66 preserved
enum members/order, four real-slide runs with all2,237 masks and ordered geometry
exact. Final raw tree d1bee927656c1ada8b05774f9cb6eb1216fe9238d4e0144f32fdad9b5ded9910
has530 inputs; development runtime is the existing cache Release folder. No new
binary release, speed claim or completed Python workflow migration follows.

Waste review: an incorrect save-test target name stopped the first command;
verified qupathxx_save_tests and reused the completed core/targets. The repeat
scan found a writer-embedded encoder missed by helper-name searches; final small
core relink reused all37 binding objects byte-for-byte. The monolithic binding
compile dominated elapsed time; do not repeat it for documentation/stub changes.
Preserve logs/failures/products. No cleanup or full qualification was attempted.

Next planned task is L1's measured interior-neighbor optimization. L2-L4 retain
exact hashes/identity and one final paired result; L5-L7 remain the explicit
native workflow migration queue. Stop this structural prerequisite after source
push; no algorithm change has started and candidate5 stays frozen.

## Prior phase - optimize C++ logic and thin Python control (plan ready)

Delivered S11's next optimization batch: implicit nominal segments, bounded
predicate fallback and an exact dense integer-grid geometry/containment proof.
[Follow-up report](benchmarks/speed-gap-followup/README.md) and
[shared coverage](speed-gap-coverage.md) own source scope and checks.
Final settled medians are320 ms native versus292 ms QuPath (~9.5% overhead,
about28 ms). Strict speed parity remains open; no general speed win is claimed.
All2,237 cell partitions, RGB/calibration/area, CSV and ordered geometry stay
exact. Six affected suites pass; new adversarial/inclusion/hole/cancel/archive
checks validate the proof. Sparse/fractional/curved inputs retain GEOS checks.
Raw source tree d9f6b3c2348f473367358ec726515b07756f8af20eb8050f4d64d96646acfdbe
has523 inputs and its own runtime-delivery receipt. Candidate5 stays frozen.

Waste review: precise profiling ruled out context pooling (~1 ms/mask). Eight
predicate workers gave no clear benefit and were discarded. One-warmup reference
samples were unsettled; longer warmup without intersample oracles corrected a
premature parity interpretation. Extra fixtures overflowed a large Windows test
stack; isolate value-heavy tests up front, preserving checks and stack limits.
Use narrow probes during development; no broad qualification/binding recompile.
Closeout diff checking flagged a required blank context marker in the retained
profile patch; a file-specific whitespace attribute preserves that evidence.

Logic diagnosis: [source comparison and isolated probes](benchmarks/speed-gap-followup/logic-analysis.md)
show repeated general neighbor access in shape growth costs~23 ms; an equivalent
interior-offset path takes~8 ms, preserving all2,237 nucleus labels in36 total
original/variant/control runs. Nucleus subtotal falls105 ->87 ms in the probe.
Small scalar SHA updates cost3.48 versus0.92 ms for the same buffered bytes;
input hashing repeats for both masks. Neither is yet an integrated speed result.
Production source/binaries stay unchanged; strict parity remains open.

Waste review: reused raw RGB, masks, headers and MSVC Release settings in tiny
standalone probes instead of rebuilding bindings/full workflows. Box counters
rule out explosive basin filling on this ROI. Corrected one summary script's
shadowed builtin using retained output; no benchmark rerun. Early guessed/glob
source reads were unhelpful on PowerShell; use rg file inventory and exact paths.

Owner direction: all shipped scientific and workflow logic belongs in C++;
Python only parses/options/submits/polls/displays. The [ordered L1-L7 plan](api-delivery-plan.md#c-logic-optimization-and-thin-python-control-plan)
and [live checklist](speed-gap-coverage.md) cover native optimization, shared
density/manifest/resume services, native NuCLS import and thin consumers.
AGENTS.md now enforces that boundary. Existing kernels are already native;
Python density reuse/selection, NuCLS centroid/import and raw training cohort
selection are migration gaps. Independent test oracles remain in test/tool code.

Plan review/waste: reuse the native density panel's existing headless workflow,
JSON, guards and atomic exports rather than creating another runner. Preserve
both existing receipt formats through native adapters. Separate the measured
speed batch from workflow migration; do not claim that moving controls speeds
up the cell kernel. No native build/test is needed for this plan-only change.

Next: L1 exact interior-neighbor access and affected edge/numeric checks; L2
buffer identical hash bytes, L3 share immutable work if justified, then L4 one
final integrated/source-scoped comparison. L5-L7 consolidate workflow authority
and bindings after settling interfaces, with one combined consumer build. Keep
scientific/persisted identity exact. No new reader expansion, generic GPU/thread
framework or binary release is selected.

## Delivered phase - label-local contour performance

Owner-selected C1-C4 are complete: phase profile, bounded label-local GEOS
polygonization, affected tests/exact-output checks and source handoff.
[Report](benchmarks/contour-performance/README.md) and
[8-row checklist](contour-performance-coverage.md) own scope and evidence.
All four workflows retain exact RGB/masks/CSV and 2,237 QuPath-equivalent cell
partitions; ordered WKT/labels/pixels remain exact. New 84-pattern topology
regression and five affected suites pass. Raw source tree
08f5967e9e7de1109a00f9985c9c98344d1c7ab9e6523413d9f7d76557d6c756
has its own module/runtime receipt. Cached bindings were reused.

Native full-workflow medians are 3,215/2,988/3,564 ms (before/candidate/later
original control), an observed 7-16% reduction. Host drift is substantial;
no new QuPath timing ratio, RAM, whole-slide or universal speed claim follows.
Published candidate5 remains unchanged; these are development sources only.

Waste review: phase-level timings selected the largest measured cost; reused
GEOS topology handling, existing boundary labels and benchmark/reference outputs.
The added regression initially gave only the reference grid a context pixel;
corrected the setup and preserved the failure. Avoid guessed-path/oversized
source reads and always specify UTF-8 on this host. Temporary profiling is absent
from the candidate. Prior cleanup rejection was not retried; comparisons remain.
Next action: stop after source push. Clipping, geometry creation and eager reopen
reconstruction remain measured opportunities, without automatic follow-up work.

## Delivered phase - focused candidate5 workflow performance

Owner-selected B1-B4 are complete: fixed workload, paired reference, bounded
fix and source/evidence handoff. [Report](benchmarks/candidate5-performance/README.md)
and [12-row coverage](candidate5-performance-coverage.md) own details.
All four paired ROI workflows agree on 2,237 cell partitions and RGB/calibration.
Native cell pipeline is 1,079 ms versus QuPath 486 ms (about 2.2 times slower).
Resource-only process peaks are 217/1,262 MiB including runtime startup. These
are local bounded-ROI observations, not whole-slide or overall superiority.

Fixed a demonstrated compatibility grid-cell archive reopen bug and removed
polygon WKT reparsing while retaining exact persisted identity. Five affected
native suites pass; full workflow improved about 11% against the later original
control, with inconsistent isolated contour probes disclosed. Development raw
source tree dddf5bb3412a58e2ade9317f46e21d06d2857ca387ba40db37d0fd5fbedeaa41
has its own runtime/source receipt. Published candidate5 remains immutable and
still has the newly discovered archive limitation; no candidate6 is selected.

Waste review: capacity refusals exposed linked raw-basin/contour/statistics
allowances; establish a coherent admitted recipe before timing. Trace actual
reader requests rather than guessing context. Separate oracle allocations from
resource measurements. Keep failures and contrary controls visible. Private CPP
changes reused cached bindings and affected suites; no broad qualification.
Temporary comparison runtime deletion was rejected by automatic approval review
("blocked by policy"); copies remain local. UTF-8 is explicit after a host GBK
document-read failure. Next action: stop this selected phase after source push. Further work requires a
selected user outcome; contour/reopen cost is the measured performance opportunity,
and any future binary handoff must package the archive repair under a new identity.

## Delivered phase — Windows candidate5 packaging and publication

Owner accepted packaging the latest signed-distance workflow as candidate5.
Ship one CPython3.12 Windows x64 wheel (build tag5), native ZIP, matching source,
unchanged pinned dependency sources, example project/CSV and concise install guide.
Include the delivered ImGui project-density/resume code; older-engine receipts
recompute rather than implying cross-upgrade cache reuse. Keep candidate4 frozen.
This is local engineering support, not fresh-host/platform, signing, speed or
biological acceptance. No new feature/kernel/reader or broad test campaign.

| Task | Existing path / decisive check | State |
| --- | --- | --- |
| P1 freeze and package | One cached wheel build, only provenance compiled; explicit native targets relinked; source3155df80 retains raw522-input tree2ddb06c5da4d | Complete |
| P2 isolated installed route | All67 immutable members and mapped DLL origins match; signed jobs/CSV/save/reopen/replay/managed ImGui pass; native ZIP opens installed project | Complete |
| P3 distribution and removal | Wheel/native dependency closures complete, AGPL/notices and12-source inventory verify; uninstall preserves all10 output files | Complete |
| P4 publish | Candidate5 public with19 assets; GitHub sizes/SHA256 digests match before/after publication and tag targets3155df80 | Complete |

Before build: native behavior and Python interface are unchanged from the signed
delivery. Packaging changes need source identity, installed-file/dependency closure
and missing install/removal checks, not repeated numeric/native qualification.
Waste review: reused release scripts/cache/source archives; no binding recompile,
new decoder, new download or broad numeric/fresh-host campaign. An unpublished
draft's tag endpoint returned404 after successful uploads; retrieved releaseID
from the authenticated list, verified existing assets by ID, then published.
No rebuild/re-upload. Static manifests intentionally make no signing/fresh-host
or exhaustive dynamic-loading claim. No novel engineering/biology claim added.
[12/12 coverage](windows-candidate5-coverage.md), [release/install guide](benchmarks/windows-candidate5/RUN.md)
and [delivery receipt](benchmarks/windows-candidate5/README.md) close the outcome.
Local artifacts/native app:main build/windows-candidate5; test wheel was removed
from its isolated environment. Candidate4 and prior products remain frozen.
Publish closeout docs and stop; no next feature or validation phase selected.

## Delivered phase — signed Cell-to-annotation boundary distances

Owner accepted the original R9 signed-distance gap. Extend the existing distance
request/job/record/export path with an explicit metric; preserve centroid behavior
and old QSD1 records. No new reader, orchestration framework or full-parity target.

Contract: selected Cell centroids (nucleus preferred, body fallback) to the nearest
individual selected Annotation area boundary, including holes and multipart rings.
Negative inside that annotation, positive outside, canonical zero on its boundary;
minimum absolute distance wins, exact ties use lower target ID. Regions are not
unioned. Pixels or positive finite independent X/Y micrometre calibration. Curves
require explicit 512-segment polygonization. One Z/time plane, no pixel reads.
Bound capture/work, vertices and segment tests; cooperative cancellation and stale
guards remain native. Preserve exact saved float64 identities and historical CSV.

| Task | Easiest existing path / decisive check | State |
| --- | --- | --- |
| T1 contract and native implementation | Existing metric/request/job with immutable bounded boundary capture; independent geometry/calibration oracles and failures | Complete |
| T2 retain/replay/export | QSD2 boundary geometry; QSD1 unchanged; exact saved hash/CSV and native replay | Complete |
| T3 consumers | Thin nanobind fields, CLI operation and existing ImGui panel; same frontend lifecycle | Complete |
| T4 delivery | Native suites9/9 plus final archive suites2/2 in6.89s; Python/CLI/managed ImGui, legacy identity, types and exact cross-interface CSV pass | Complete |

Build risk/check: native headers and payload change; settle all consumer contracts
before compiling cheap affected targets, then one binding/staging pass. Published
candidates and prior app bundles remain frozen. Do not replay broad qualification.
Waste review: reuse the distance pipeline instead of another record/job/frontend
family; preserve centroid capture layout so old archive admission stays valid.
First native run passed. Review added close-to-edge and vertex-limit checks, then
only affected checks. Initial exploration produced truncated reads; narrow reads
avoid repeating that. Reuse the native app/CLI while the binding TU compiles.
Review found QSD2 loading repeated the full nearest search. Like QSD1, load now
checks the chosen distance/geometry and identities; exact segment totals are
validated arithmetically. Explicit Verify owns full recomputation. Final signed/archive suites2/2 pass; cached consumer relink and staged checks
pass, preserving the compiled binding objects.
The large binding TU was the actual critical-path cost; it compiled once while
standalone checks proceeded. Staged source522-input tree 2ddb06c5da4d matches the
raw compiler manifest and runtime. [14/14 coverage](signed-distances-coverage.md),
[guide](signed-distances.md) and [source/runtime evidence](benchmarks/signed-distances/README.md)
close this slice. Local app/Python runtime:main build/signed-distances/runtime.
This is a separate development runtime; candidate4 wheel/SDK and prior apps remain
frozen. No new binary release, indexed-search speed, biology or full-parity claim.
Compilexx records durable lessons. Publish source and stop; no next phase selected.

## Delivered phase — ImGui density resume/reuse

Owner accepted a previous-run folder selector and reuse/recompute indicators.
Delivered private controller/GUI receipts over existing native replay/capture,
records/CSV and the CLI's exact engine/geometry/revision/calibration/policy/limit
rules. Each saved image publishes a bounded checksum receipt before combined
CSV; no final report is required to resume. Eligible unchanged images reference
original checkpoints without kernel work or QPX/pixel copies. Changed/missing
results compute; eligible corruption fails explicitly without replacing data.

T1/T2 complete: both affected native suites pass,2/2 in1.64s. Tests prove0 kernel
calls for reuse/chains, partial receipt recovery, changed geometry/calibration,
unrelated metadata, smaller quotas, engine changes, duplicate JSON/checksum/
replay errors and separated folders. Actual desktop first run computed2; second
run computed0/reused2 with0 new QPX copies. All old run files and source remain
byte-identical;31/27 counts and all20 native CSV columns agree. [10/10 coverage](gui-density-resume-coverage.md),
[guide](gui-project-density.md) and [desktop/source evidence](benchmarks/gui-density-resume/README.md)
close this scope. Local app:main build/gui-density-resume/application;56 members,
raw520-input tree e83ae913ef42. Older pre-receipt/different-engine runs compute.
Published candidate4 and Python consumers remain unchanged; no new binary release.

Waste review: first tests passed; one boundary review added required-field
admission and Windows case-alias nesting checks, then only affected suites and
one cached app build. Reused strict JSON, atomic publication, downloaded fixtures
and numeric oracles. No bindings, wheel, new dependency version, decoder, generic
cache framework, broad qualification or fresh biology/speed claim. Compilexx
records durable lessons; preserve prior apps/outputs. Publish sources and stop;
no further feature or validation phase is selected.

## Delivered phase — ImGui project-wide density panel

Owner continued image/class selection, native cohort/calibration preview,
background progress/cancel and combined CSV/error reporting. Delivered as a new
standalone app with a private controller over existing native density/records/
CSV/save_new; no new SDK fields, batch enums, bindings or wheel. Saved-source
SHA/review gates, new directories and independently owned worker Documents
preserve the displayed workspace and input files. Per-image failures continue;
completed whole-image outputs survive cancellation and CSV quota failures.

T1/T2 complete: affected controller suite passes in0.82s, including active cancel,
stale source, quoted CSV, quota and a collision followed by a successful third
image. Actual final desktop preview/run shows32/28 selected Cells,31/27 in-FOV
counts,2 saved images/2 rows and0 failures. Both saved and original source bytes
remain unchanged; unchanged candidate4 Python reopens/verifies both checkpoints.
Selector changes clear reviewed rows; short image IDs make both rows readable.
Folder opener works; CSV opener reached the Windows chooser without changing
associations. [Guide](gui-project-density.md), [14/14 coverage](gui-project-density-coverage.md)
and [desktop/source receipt](benchmarks/gui-project-density/desktop.json) close
this scope. Final local app:main build/gui-project-density/application,56 files,
about40.6MB; its519 raw compiler inputs match recorded tree23111b162413. Reader
DLLs/notices match candidate4; published candidates and Python consumers stay
frozen. This is local Windows desktop evidence, not a new binary release or
fresh-host, biology, segmentation, platform or speed acceptance.

Waste review: a fixture conflated historical/current revision-bearing SHA;
retain exact stored SHA/CSV and use native replay verification. Long names hid
preview rows; private GUI repairs reused controller/kernel evidence and cached
objects. WindowsPath sorting differed from CMake's string sort in the receipt
observer; corrected only that script, without rebuilding product code. Original
failures and intermediate products remain in main build/gui-project-density.
Compilexx records durable lessons. No full kernel/binding/package campaign.
The selected outcome is closed; publish sources and stop. No next phase selected.

## Delivered phase — reusable NuCLS PNG/CSV import

Owner accepted generalizing the pilot into a standalone Python importer. Match
PNG/CSV stems across two directories, optionally select named stems; require
explicit X/Y calibration and outlines versus centroid-marker representation.
Preview missing files, malformed rows, classes and coordinate problems. No save
unless every pair passes; write a new QPX only. Preserve selected class plus
source classification/row provenance in native metadata. Do not infer tissue,
windows, calibration, clip coordinates, repair topology or resample images.
Reuse native ROI validation, Project.save_new and delivered file guards; keep
candidate4 and delivered density consumers unchanged. No C++/wheel/UI work.
T1/T2 complete: first focused run28/28 passes. Native outlines/marker centroids,
classes/provenance, malformed/missing pairs, budgets, changed input and no-replace
output have evidence. Added Pillow header-size-refusal coverage after review;
final affected module29/29 passes in1.57s. Installed CLI demonstration passes:
2 public cases/60 nuclei, all classes and original RGB pixels exact on reopen;
outlines correctly reject the known out-of-image box; explicit markers reproduce
the prior pilot's counts31/27 and areas104006/101124px2. No new downloads/build.
[Guide](nucls-import.md), [14/14 coverage](nucls-import-coverage.md), source/test
receipt and installed workflow evidence close the selected outcome. Push and
stop; no further phase or broader native test campaign selected.
Waste review: settle this small format contract before coding; no query language,
general adapter registry, new download, segmentation or native qualification.
Execution waste review: no failed test/implementation run. Public outline
rejection is expected and recorded; reused downloaded assets and prior independent
FOV/count-area oracles. Native ROI validation and save_new own topology/publication.
One final module pass covered the added real Pillow size-refusal case; no native
build, wheel, install or unrelated tests. Compilexx records the reusable lesson.

## Delivered follow-up — public multi-image research pilot

Owner accepted running the delivered selector/runner on a real research project.
Owner has no local project. Select a tiny public NuCLS corrected single-rater
subset:2 FOVs from different TCGA cases, author-published CC0 and0.2um/px.
Use annotation FOV support for the denominator, not a claimed tissue ground truth;
represent annotated nuclei as native Cell objects, without claiming whole-cell
segmentation. Preserve source CSVs and rectangle/polygon centroid semantics. No new reader.
Decisive check: generated class manifest -> existing installed runner ->2 CSV
rows matching independent rectangle-area and annotation-centroid counts.
Local read-only metadata inventory found test fixtures and the duplicated-crop
demonstration, not an identified research cohort.
Inventory is retained in main build/research-density-pilot/local-project-inventory.json;
reuse it instead of rescanning. No biological acceptance claimed.
Completed:7 directly downloaded public assets total396720 bytes;2 distinct cases,
60 annotated nuclei represented by centroid markers. Actual preview/generation/
batch CLI produces2 rows:31 and27 in-FOV nuclei,0.00416024 and0.00404496mm2,
7451.493183 and6674.973300 nuclei/mm2 of annotated FOV. Counts and areas match
independent arithmetic; saved project bytes unchanged. [Pilot coverage, sources
and evidence](benchmarks/research-density-pilot/README.md) close all6 selected checks.
No native build, tests/package reinstall, model training or feature expansion.
Waste review: initial inventory stopped on an unavailable historical artifact;
the corrected read-only pass records unavailable/schema-ineligible entries and
preserves all files. No repeated test campaign or fixture substitution for the pilot.
First public import exposed a one-pixel out-of-image annotation box, correctly
rejected by native Document. Preserve the failed analysis.log and partial output;
use1px native Cell/nucleus markers at all original annotation centroids for this
centroid-only pilot, rather than clipping source annotations or resampling pixels.
Record source shape types, mask/RGB extent differences and representation limits.
The corrected centroid-only workflow passes. Exact source hashes, failed first
import and source annotations remain preserved. Durable compilexx note records
bounded public-data selection and explicit representation/denominator semantics.
Next: no further implementation or validation phase selected; report this pilot's
scope rather than treating public annotations as whole-cell/clinical acceptance.

## Delivered phase — class-based density manifests

Owner accepted a thin Python manifest generator. Select images by ID (all images
by default), resolve exact class names into explicit window/tissue/exclusion/Cell
IDs, and preview counts, calibration and native request errors before publication.
Require explicit Cell classes or all-Cells selection. Write only a new manifest
when every selected image passes. Reuse the delivered runner's file guards and
atomic JSON writer; keep candidate4 and the batch runner unchanged.

T1/T2 complete: standalone generator,22 focused tests passing against installed
candidate4, and actual isolated CLI generated-manifest to runner demonstration:
2 images/8 rows with unchanged counts289/121/168/0 and areas40576/17536/23040/0
per image. Reused the preserved duplicate-crop project; original bytes unchanged.
[Guide](density-manifest.md), [12/12 coverage](density-manifest-coverage.md) and
[receipt](benchmarks/density-manifest/receipt.json) record scope. Native candidate4
and batch runner unchanged; no native build, wheel, install, GUI or segmentation.
Waste review: exact-name selection avoids a new query language or hierarchy
policy; one object scan per image, one project open, no kernel work in preview.
Fixture repairs: Document.set_class takes RGB; ellipses use add_roi(Roi.ellipse).
The curve case passed alone, then final22 passed including repeated selectors
and native policy options. Nondefault planes require quantitative TIFF, so the
invalid synthetic plane fixture was removed; native plane capture stays unchanged
with no new plane-coverage claim. Raw failures are preserved in main
build/density-manifest. Lesson in compilexx: inspect installed stub semantics
before fixture authoring; no second validation framework. No further phase is
selected; publish this outcome and stop.

## Delivered phase — project-wide density runner

Owner accepted the proposed thin Python runner. Delivered over candidate4 unchanged:
explicit per-image windows/tissue/exclusions/Cells, sequential native density
jobs, per-image QPX checkpoints, combined CSV, failure reporting, cancellation
and reuse of unchanged saved results. No native/binding/UI changes or new wheel.
[Project-density coverage](project-density-coverage.md) tracks this outcome.

T1 complete:standalone examples/project_density.py with a bounded versioned JSON manifest.
Every run writes a new directory. --resume-from reads earlier immutable image
checkpoints; it does not overwrite or duplicate earlier projects. Optional
record_id selects an existing native saved result. Reuse requires native replay/
input identity and exact native source-tree identity; it is labelled reused,
not a numerical re-verification. Changed inputs compute anew; corrupt caches
fail explicitly. Native geometry, jobs, serialization and CSV stay authoritative.

T2 complete:focused multi-image tests cover native agreement, CSV identities/quoting,
per-image failures, cancellation, resume without kernel work, changed-input
recomputation, corrupt checkpoints, byte limits and input/output protection.
Cancellation polls the native job; synchronous project open/save must return
before cancellation can finish. Partial/cancelled runs publish density.partial.csv.

T3 complete:standalone installed-candidate4 CLI demonstration computed2 image
entries/8 ROI rows, then reused both checkpoints with0 new QPX copies. Each
image has the accepted506-cell crop counts289/121/168/0 and areas40576/17536/
23040/0px2. The same crop is deliberately used twice to test namespaces, not
independent specimens. Source bytes remain unchanged and checkpoints reopen/
replay. [Guide](project-density.md), [source/test receipt](benchmarks/project-density-runner/receipt.json)
and [CLI evidence](benchmarks/project-density-runner/workflow.json) cover all16
selected requirements. Native source remains cbf4102f; the standalone runner has
its own source hash. No reader/segmentation/platform/resource campaign, GUI work,
native build or wheel/install was needed. Native headers/bindings stay unchanged.

Waste control:open the source project once; hash at setup/final and stat between images;
one single-image checkpoint per computed image, never a whole-project save per
image. Resume references existing checkpoints instead of copying their pixels.

Waste review:29 focused tests pass against unchanged installed candidate4.
Initial23 passed and one test used Project.dirty as a method; corrected its
property accessor and ran that test alone. Final code review added reduced-budget
native readmission, SQLite-sidecar protection and bounded checkpoint identities;
the affected module then passes all29 cases. Native values/replay, source-record/
checkpoint reuse without jobs, unrelated/relevant edits, failures, active cancel/
SIGINT, rollback/byte caps and preserved inputs/outputs have evidence. No build,
wheel or install. The standalone orchestration reuses native authorities rather
than adding C++ batch enums/UI/stubs. Canonical compilexx notes record the reusable
lessons. Inputs, previous outputs/checkpoints and failed logs are preserved.
Closeout normalized doubled Windows line endings in the archived failure-log
view after diff-check flagged them; original raw log remains preserved. No product
code changed and no behavioral checks were repeated for that documentation fix.
The selected outcome is closed; no further feature or validation phase is selected.

## Delivered phase — tissue-aware ROI cell density

Owner continued the recommended density phase. Delivered selected cell centroid
counts per mm2 of valid tissue for each selected ROI, with explicit tissue and
exclusion cohorts, calibrated area, CSV, retention/replay and shared Python/CLI/
ImGui consumers. Candidate3 stays frozen. This is per-ROI tissue density; radius
maps, smoothing, segmentation, new readers and additional platforms are outside
this selected phase. The previous50/50 neighborhood checklist remains closed;
[density coverage](density-coverage.md) tracks this phase separately.

Contract:valid footprint=(ROI intersect image bounds intersect union(tissue))
minus union(exclusions). Count each explicitly selected Cell ID once per ROI if
its chosen centroid covers that footprint and does not cover an explicit
exclusion (including its boundary). Use existing closed geometry coverage;
hole interiors are out and hole boundaries follow that established coverage.
Overlapping ROIs produce independent rows, never an implied pooled total. Area
is pixel area*calibration_x*calibration_y/1e6. Positive finite calibration is
mandatory; empty area yields count0 and undefined density. A zero count with
positive area has density0. Rectangles/polygons/multipart are exact linear
inputs; ellipses require explicit polygonize_curves acceptance and use the
existing512-segment polygon for BOTH membership and area, captured in identity.

T1-T3 complete locally (2026-11-06):density capture/kernel/jobs, retained QDN1
records/schema29/CSV and thin shared frontend/Python/CLI/ImGui. All9 affected
native checks pass, including closed-edge regressions, nearest/neighborhood and
legacy migration/snapshot budgets. Python/API/stubs:25 passed;16 existing opt-in
GUI cases skipped. New actual density cancellation/retry passes separately;
positive typing passes and negative typing has3 expected errors.
[Native receipt](benchmarks/density-workflow/native.json),
[consumer receipt](benchmarks/density-workflow/consumers.json) and
[506-cell staged workflow](benchmarks/density-workflow/staged-workflow.json)
record source scope. The4 ROI rows match independent area/count oracles:
40576px2/289 cells,17536/121,23040/168 and empty0/undefined. SDK/CLI/GUI CSV bytes,
keep/verify/save/reopen and standalone reopening agree. Inspected desktop shows
readable values/units/status and registered overlays,0 UI overflow.

One bounded observation:20000 coincident Cells in500 overlapping ROIs,10M
predicates/1000 Boolean operations,27.46MB planned work,69.10MB process peak and
119 Python observer ticks during released-GIL wait. This is diagnostic evidence,
not speed superiority or an RSS guarantee. Geometry calls remain synchronous.

T4/T5 complete:one frozen wheel build (tag4) compiled only provenance/relinks,
then one isolated install passed the506-cell SDK/CLI/GUI route, including actual
Inspect, CSV/save/reopen and standalone reopening. All67 immutable installed wheel
members match; mapped GEOS/OpenSlide DLLs are inside the package. Static runtime
closures pass;515 raw compiler inputs match the source ZIP and12 unchanged
dependency-source archives verify. [Candidate4](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.4)
is public with19 assets, every remote size/SHA256 checked before and after
publication. Native source cbf4102f2cffe6ed8f71549087ae5bf74cdcd8e2, tree
5a98347c5447c5b8f414d299a3411e9554c72188f92e1ca3ecd7c8c69aeee96e.
All40 selected requirements are closed; see the
[delivery receipt](benchmarks/density-workflow/delivery.md) and
[completion audit](benchmarks/density-workflow/completion-audit.json).
Support remains local Windows x64/CPython3.12 engineering delivery. Signing,
fresh-host/additional-platform, exhaustive parity, biological and speed acceptance
are not claimed. No further phase or validation campaign is selected.

Final products/runtime are main build/density-workflow/delivery and installed.
The tools interpreter resolves the final installed runtime. Preserve active
source/cache, products, reference scenes, failures and archived
build/storage-cleanup-20261105. The stage is superseded; no recursive cleanup
was attempted. Candidates2/3 remain frozen and PRESET1 stays parked.

Waste review:first fixture was outside allowed image bounds; translated it
without changing its oracle and retained the failure. Removed quadratic payload
rescans. Grouped Document fields into one shared-header rebuild. The large main
binding unit used>16 CPU minutes; order cheap changed CLI/frontend targets first
in future combined builds. Its pass was preserved when a later MSVC CLI lambda
failed; one cached repair reused all binding objects. That repair also fixed
GUI target-array/mode guards and true closed-edge index bounds, and charged
candidate capacity/growth. All follow-up native/Python/desktop checks pass.
Durable lessons are appended to compilexx's canonical notebook. No intermediate
wheel/install, broad corpus/platform campaign or additional feature is selected.
The bundled closeout command was rejected by automatic review with only
"blocked by policy"; explicit file diffs/copies completed the same handoff safely.
No binary inputs changed and no rebuild/reinstall was needed for documentation.

## Delivered phase — radius-based cell neighborhoods

Implementation requested after candidate2. Selected addition: for each
selected source cell/object, count selected target centroids within an explicit
radius, inspect the table, export CSV and save/reopen/replay. This adds the missing
R9 neighborhood operation; existing cell detection/measurements and nearest
spatial distances are reused. All five planned tasks are delivered. Detailed contract,
API paths and five ordered tasks are in the [next-phase plan](api-delivery-plan.md#next-phase-radius-based-spatial-neighborhoods).

Coverage is maintained in the [detailed checklist](neighborhood-coverage.md):
requirements and source-scoped evidence, with the execution queue kept here.
T1-T5 complete; coverage50/50 selected-phase requirements verified.
[Windows candidate3](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.3)
is published with18 assets, each remote size/SHA256 matched before publication.
Frozen native source618b0dd0a6f39cc062941c6ea524ac22cb863bc5, tree
b30bafcb4c982c55d7069276e4c82b9119d731ef7a69d519e50a19599dcbcd95.
One cached wheel build (tag3) refreshed only provenance/linking and one isolated
installation passes the506-cell Python/CLI/ImGui/CSV/verify/save/reopen route and
standalone reopening. All67 immutable wheel members match the install; mapped
GEOS/OpenSlide DLLs come from its package. Wheel/native runtime inspections pass;
all506 raw compiler inputs match the source archive;12 unchanged dependency
archives verify. The source tree matches the tested stage, so native oracle,
quota/cancel/stale/legacy and dense/GIL evidence is reused within that scope.
The20000-cell coincident observation has399980000 comparisons and68.60MB process
peak, without a dense matrix or speed/RSS guarantee. Both inspected GUI captures
show readable counts/radius/units and registered cell overlays,0 overflow.

The selected outcome is closed. No further build/API/segmentation/parity/platform
campaign is selected. The [delivery receipt](benchmarks/neighborhood-workflow/delivery.md)
records public state, support and disposable-state disposition. Final packages/
runtime live in main build/neighborhood-workflow/delivery and
build/neighborhood-workflow/installed. The native cache and all previous
candidates remain preserved; the tools interpreter resolves the final installed
candidate rather than the superseded dirty stage.

Waste review:T2's schema generator omitted27/28 tables; only that test target
was corrected/rerun. T3's missing CLI-test header and hidden GUI target flags
needed only test/provenance/GUI compilation and relinks, reusing binding objects.
The genuine TypeAlias required a stricter identity/canonical-member check, not a
native rebuild. Package assembly's blanket embedded-build-identity assertion
incorrectly covered the region-only utility, which does not link that authority;
its exact cache/wheel/installed bytes are checked, while the app/batch/module
embed the clean revision/tree. The partial draft/log is preserved; no binary
changed and the final wheel was not rebuilt/reinstalled. Native/package/compiler
and corresponding-source checks pass; publication and final requirement-to-evidence
closeout are complete.
Keep candidate2 frozen and
PRESET1 parked. After each task/failure, record the result, decisive check, waste
and next task here; do not create another ledger or recurring review campaign.

Planning waste review:the earlier suggestion to add a generic cell-analysis
workflow duplicated existing capabilities. Reading the actual spatial headers
identified radius counts as a real missing operation. Reuse owner capture,
calibration, indexed centroids, jobs and native persistence/export patterns;
settle all known SDK/CLI/ImGui fields before one combined consumer build.

## Delivered outcome — representative large-slide tissue workflow

R12 continuation delivers whole-slide tissue at an explicit native pyramid
level while high-resolution ROI statistics remain bounded. The new opt-in
`canonical_pyramid` reads the selected level once at origin zero; this fixes
OpenSlide decoder-phase differences across processing tile sizes without
changing legacy recipes. SDK, CLI and ImGui share the native implementation.
QTM6 retains the new recipe; QTM1-5 and inactive legacy memory charges remain.

Local changed-source evidence passes:7 affected native suites;120 focused
Python cases initially passed,8 skipped, and the sole failing case passed after
a test correction. Snapshot accounting includes project-path bytes: both old
and new engines charge11512 at the same relocated fixture path. The test now
accounts for relocation and retains exact-budget admission and budget-minus-one
rejection. No native compatibility change was needed.

CMU-1.svs level2 (2875x2057) produces identical labels/input hashes at tile256
and512 and zero differences against the independent threshold oracle. There
are5913875 supported samples,1083798 foreground samples and688 components.
Cancellation leaves no result/retained record. Native estimate102136112 bytes
at tile256; measured observer peak271642624 bytes includes Python oracle and
reader caches. This is pyramid tissue, not level-zero cell analysis or a speed
superiority claim. Python ROI statistics/CSV/QTM6 saved replay and ImGui tissue
CSV/save/replay pass; standalone opens the combined Python project. The captured
overlay aligns with tissue, scale is visible and the UI reports zero overflow.
See [evidence and commands](benchmarks/large-slide-workflow/README.md).

Delivery complete: [Windows candidate2](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.2)
is published with16 assets, each remote size/SHA256 verified. Native source is
`bee4736f2ad0aeffc9907542c873f06efd335cc7`, tree
`b52fa9b85799b7ce0b68780a84f2c95ff993c69b848ce7b24a9b74d54215e8f7`.
The cached wheel build refreshed provenance/linking, reused settled bindings and
produced build tag2 with AGPL SPDX metadata/license. One isolated installation
passes Python and ImGui large-slide/CSV/QTM6 replay, all3 focused installed tests,
and standalone reopen. All63 installed package members match the wheel; wheel
and native runtime inspections pass; all497 raw compiler inputs match the source
archive, and12 unchanged dependency archives authenticate. No fresh-host result
is claimed for candidate2. Final packages:main build/large-slide-workflow/delivery;
installed runtime:main build/large-slide-workflow/installed. The selected outcome
is closed; no new parity extension is selected. Automatic approval review blocked
retirement of superseded stage/test directories; they remain intact outside the
selected delivery. See the [delivery receipt](benchmarks/large-slide-workflow/delivery.md).

Waste review:the first native test used a Python alias; it was corrected and only
that target retried. The snapshot test confused fixture relocation with native
layout change; a same-path old/new comparison resolved it before touching code.
One combined bindings/UI/CLI build was reused for all consumers. The restored
build environment lacked pip; invoking the installed backend directly avoided
an unnecessary installation. An identical duplicate license and the native
archive basename were corrected before publication, without changing binaries. Required build
tools were restored for the demonstrated decoder defect. Optional PRESET1 source
is preserved byte-exact in build/stain-presets-r7/parked-source and Git stash
88f1eba9fe33025dd696ef3fbf6e8c7e55c6363f; keep it parked.

## Delivered baseline — Windows candidate handoff

The selected [Windows workflow](windows-workflow.md) and its targeted fresh-host
handoff pass. [One evidence summary](benchmarks/windows-workflow-checkpoint/README.md)
links the local and hosted receipts. The declared engineering
[prerelease is published](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.1)
with16 assets, sources, guide and deployment evidence. Do not merge the parked native PRESET1 work or start another parity
extension. Unselected M0-M7/R0-R16 features are optional; the declared candidate scope remains unchanged.

Native code is frozen MODEL1 source6219a08f, tree
432eab13b81702d423091d8bdd2113d75be9ada3ab2b9a05804128b80cccc892.
Owner-selected project license is AGPL-3.0-only. The license overlay preserves63
original members and changes only metadata/RECORD/notices; all65 seed shipping
members matched before native extraction. No native rebuild or stub generation.
The application source archive verifies all495 compiler inputs. A12-archive
source bundle provides dependencies, including OpenSlide's16 SDK source projects
and build patches, and nanobind/robin-map. Fonts retain unmodified assets/notices;
NumPy and Windows/MSVC/graphics are separate prerequisites.

Fresh Windows run42099947943 at harness commitc0aeba86 passes installation,
real-slide Python/ImGui ROI/tissue/statistics/CSV/save/replay, standalone reopen,
actual wheel/native removal and preservation of7 user files. Image20261101.353.1
uses declared MSVC runtimes and pinned test-only Mesa; PATH excludes development
SDK directories and mapped GEOS/OpenSlide come from the installed package.
Local Windows11 Pro10.0.26200 also passed the original workflow. Do not infer
arbitrary GPU/Windows support or biological/performance parity from these results.

Published candidate tag: v0.1.0-windows-candidate.1 on bipxx/qupathxx;
release targetf92f0659, deployment harnessc0aeba86, native source6219a08f.
Final local files: build/windows-workflow-checkpoint/delivery-agpl. Hosted receipt
and source inventory are durable under docs/benchmarks/windows-workflow-checkpoint.
The seed/runtime/cache and candidate data remain at the previously recorded
main-workspace paths. Only main and this active source checkout remain; source
archives from cleanup are preserved. The old probe is a different dirty stage
and is ineligible for acceptance.

## Parked scope

PRESET1 P1 passed focused native checks locally. Its native sources and CMake
changes remain uncommitted and were excluded from the handoff commits/binaries.
P2/P3 and exact import identity remain optional, with the existing normalization
observation retained. R2-R16 and the23 Partial rows form a reference catalogue,
not a count of mandatory remaining jobs.

## Latest closeout and waste review

Fresh handoff initially stopped on draft-token visibility, Windows environment
case handling and external fixture DNS, before numerical acceptance. Scoped
fixes changed only the harness and authenticated fixture access. Failed runs and
logs remain linked in the evidence summary; all candidate hashes stayed fixed.
A duplicate permission retry did not change the cause: inspect the failed phase
and its actual authorization/environment before redispatch. Source collection
reused cached pinned archives; no compiler, full platform/corpus qualification,
registry or performance campaign was added. After publication, stop at this
handoff and report the supported scope rather than pulling in new features.

The superseded pre-license archive in build/windows-workflow-checkpoint/delivery
remains after automatic cleanup review rejected deletion (blocked by policy).
It is not the selected candidate; use only delivery-agpl or the prerelease assets.


## Planned follow-up - eight-gap native expansion and material separation

Owner requested a plan for all eight remaining feature areas. The
[canonical roadmap](feature-expansion-plan.md) defines22 implementation items
(7 complete,15 pending); the completed FO00-FO05 slice stays closed. Gene display/selector and
native import preview come first, then registration/fluorescence, backed matrices/
H5AD/OME-Zarr, molecular methods and SDK/release. Current active UI work is preserved.

Proprietary and NC material have distinct metadata-only registers and separate
external-pool rules; neither is an open dependency, CI fixture, trained-model
source or release acceptance asset. Unknown terms remain unreviewed. No restricted
material was downloaded. Primary format/license sources and task dependencies
reviewed; unique-ID/link/diff checks only, no build or scientific campaign.

Waste review: one roadmap/checklist, existing material inventories extended at
L01, no second algorithm/state machine or duplicated validation ledger. Shared
native providers precede new readers; full-matrix normalization cannot use only a
selected-gene projection, and single-study spots cannot supply biological replicates.

### Active native expansion: S01/V01 foundations

The active closure goal selects the22-item roadmap for implementation. Current
UI layout changes continue independently and are preserved. Native changes add
owned expression snapshots over the admitted point projection and shared gene
display requests/fields/cache; no Python logic or new dependency is introduced.
Raw uint64 counts and missingness remain separate from display-domain doubles.

Changed inputs: two native services/public headers, focused expression test and
their CMake registrations. Unresolved risks: captured source/cohort identity,
missing versus measured zero, retained-memory accounting and palette/range
interpretation. Decisive check: build only qupathxx_expression_tests in the
existing gridfrontends167-package cache, run its analytical/rejection cases and
the admitted mouse-brain project. Do not rebuild bindings or GUI for this check.
Existing MSBuild node-reuse workers are idle (no cmake/cl or active project
command); recheck before starting the cached build.

S01/V01 focused native checks and all six real mouse-brain marker fields pass;
the extra save/reopen and retained-capacity cases also pass (expression CTest,
2026-11-08). No GUI/binding rebuild. A second incremental test build was justified
by newly added persistence/extreme-value/budget cases, not a repeated campaign.

Next I01 changes: shared owned SpatialImportPreview and cancellable job; legacy
CLI imports delegate to the same preview/publication service. Reuse native
WorkflowInput versions and atomic save_new, with no second parser or per-object
Python work. Decisive check: affected spatial_counts/CLI tests exercise read-only
preview, exact imported values, saved recipe, stale source/project refusal,
collision and cancellation; rerun expression once against the changed library.

I01 native/CLI tests pass. Final review adds all source locators/resource limits
to the stored recipe and retains earlier recipes by hash on repeated imports;
one affected incremental rebuild/check covers these changed persistence inputs.
L01 offline guard now reads material annotations in existing inventories and
rejects absent records, restricted/unreviewed inputs, restricted ancestry and
derivatives. Existing font/CC0 fixture records are annotated; two focused tests
pass, and the link-creation case is unavailable on this Windows host. Complete
package/dependency membership admission stays open through P02; this is not an
audit of every historical archive. No restricted material was downloaded.

Checkpoint: S01/V01/I01 complete locally. Final expression/spatial_counts/CLI
CTest run passes all3 cases (0.71s); the real2,702-spot/six-marker check passes.
See [the single progress receipt](benchmarks/feature-expansion/README.md).
Native-only CI includes the new expression case; hosted execution is not claimed.
Waste review: no repeated scientific campaign, bindings/UI compilation or new
cache. One manual test wrote a311KiB project into the source checkout; that exact
disposable file was removed and later checks ran from the existing cache.
Next: V02/I02 on the settled desktop shell; independent R01 can proceed while UI
work continues. L01 final membership and release-stage integration remain open.

Final cache review found that separately admitted cohort capacity and work could
exceed the shared request budget when combined. Charge their sum and add the
near-limit retained-capacity case; rebuild only the affected native targets.
Partial staging initially hit Windows text-mode newline translation; applying
the same selected hunks as raw bytes preserved concurrent UI changes.

### Active R01: explicit native coordinate frames and affine registration

The previous turn made progress: S01/V01/I01 and the first L01 guard foundation
are committed at3e0d7082. The desktop layout work remains uncommitted and is
preserved. R01 proceeds independently without modifying its UI files.

Changed inputs: native CoordinateFrame/Affine2D/RegistrationRequest service,
typed ImageDataProperties persistence, affected test/CMake/CI registration.
Original coordinates/counts stay unchanged; native acceptance recaptures source
cohort and target identity before the existing undoable properties command.
Linked sources reuse verified file versions/series; embedded RGB uses exact
bytes, without process generation or display-window identity. Physical conversion
requires explicit known pixel calibration. No new dependency or Python logic.

Decisive check: analytical translation/scale/rotation/reflection, inverse and
composition; anisotropic unit conversion/unknown calibration, stale count/geometry/
image/calibration refusal, cancellation, retained outside positions, undo/redo and
save/reopen. Build only registration/expression targets in the existing cache and
run the real admitted mouse-brain registration check. No live compiler project
command was found before this build; idle MSBuild node-reuse workers are retained.

The first build command stopped before compilation: Visual Studio cannot build a
new target until its project file exists. Explicitly regenerate the existing
cache with unchanged options, then build the named targets; no new cache/install.

R01 complete locally: registration/expression CTest passes (2 cases,0.21s) and
the real2,702-spot project passes native transform/acceptance without changing
counts/coordinates. Analytical affine/inverse/unit tests, linked/embedded identity,
unknown calibration, stale geometry/count/image/calibration refusal, cancellation,
outside-position retention, undo/redo and save/reopen all pass. Native-only CI is
extended; no hosted result is inferred. No binding or GUI target rebuilt.
Waste correction: explicitly regenerate the existing Visual Studio cache before
the first invocation of a newly introduced target. Next connect V02 to the current
desktop shell, preserving its existing layout changes and scientific authorities.

Before closeout, add the admitted companion OME-TIFF check for the claimed
display-window-independent image identity. This changes only the native test;
build that target and run both real examples, without another science campaign.

Both admitted real examples pass: mouse-brain original coordinates/counts stay
exact after native acceptance, and fluorescence display-window changes preserve
frame identity. No proprietary/NC data, new dependency, binding/GUI compilation or
package was introduced. R01 is closed; R02 interactive landmarks remains open.

### Active V02: gene expression display and synchronized comparison

The previous turn made progress: R01 is committed at5eb899dd. Preserve ongoing
desktop layout edits. New integration adds a separate gene tool and small hooks
to the current shell, rather than replacing those edits. RmlUi's existing Workflow
route and Analyze menu lead to the native controls.

Changed inputs: shared native GeneViewSession/paged ID+symbol search, pooled
comparison range and preferences; separate GuiGeneDisplay panel/overlays and
workspace preference field; narrow app/CMake/CI integrations and affected tests.
Workers own immutable snapshots and coalesce rapid changes; stale generations
cannot publish. Typed native display fields supply all colors/ranges; UI does not
parse counts or duplicate transforms. Original projects/counts/positions stay
unchanged. Local workspace preferences persist presentation; two comparison
canvases share one camera and tile cache. Main overlays bypass the geometry cache
so a new field cannot leave old colors on a cache hit.

Decisive check: native pooled percentile/minmax/constant/missing cases, coalescing/
cancel/ownership/search/persistence. One combined cached native/frontend build,
then GUI capture verifies native colors at exact scientific spot positions and
zero/missing presentation, shared comparison/prefs and unchanged project bytes.
Run the admitted real mouse-brain comparison. No bindings or package build. A new
Visual Studio test target needs explicit regeneration of the existing cache first.

Shared-cache check found the other frontend build live. The initial inspection
and configure were separate shell statements, so configuration still ran; it
finished without launching our compiler. The other build is now terminal (its
process is absent). Correct subsequent build commands to refuse a live project
process before starting, and preserve its existing objects/results.

Native gene-view/expression tests pass (2 cases,0.18s). First GUI capture visibly
renders the expected zero/missing/min/max colors, but the pixel oracle used
Viewport::origin as image coordinates; the API stores screen translation.
Reuse its public image_to_screen conversion in the oracle and correct the same
assumption in comparison drag panning. Retain the failed capture, rebuild only
affected frontend/test targets, and retry in a new evidence directory. Numeric
color/data assertions remain unchanged.

Corrected GUI replay passes, including the real2,702-spot comparison and exact
spot-color/data/project checks. Final review extends workspace preferences to
retain the explicit source lineage when multiple imported sources exist. Add one
comparison-drag replay that measures both native-color marker displacements and
checks the main camera/project stay unchanged. These changed inputs justify one
final affected native/frontend build; no binding/package/requalification work.

V02 complete locally. Final native expression/gene_view CTest passes (2 cases,
0.55s); GUI replay passes exact point colors, measured zero versus missing,
workspace/source persistence across two lineages, matching40-screen-pixel drag
in both comparison canvases and unchanged main camera/project bytes/counts.
Real Aqp4/Gad1 shared-log1p comparison over2,702 spots is readable; source counts/
coordinates retain exact identity. Captures and failed oracle run remain under
workspace-root build/feature-expansion; see the single progress receipt.

Reuse the completed UI layout's CollectionHeight/SetNextToolWindow helpers as
dependencies of this tool; preserve its other source edits. No public GuiOptions
or binding layout changed, no binding compilation, new cache, package or restricted
material. Waste review: one viewport-origin mistake was fixed in both oracle and
drag logic; source persistence and drag checks closed concrete final-review gaps.
Next I02: a shell-free import wizard over the existing native preview/job, without
a second parser or Python orchestration.

### Active I02: guided spatial import

The previous turn made progress: V02 is committed atfc1d0016. I02 adds a separate
wizard over SpatialImportJob and extends its shared native authority for feature/
image catalogues, explicit three-column mappings, calibrated units and image
inputs. The same feature parser serves catalogue and import; shared native search
pages catalogue IDs. Image imports admit native RGB/PPM/TIFF only. Original source
coordinates/units and the full mapping recipe remain separate from mapped point
positions. Preview owns its image/points and never mutates the live study.

Changed inputs: spatial_counts/import headers/services, narrow catalogue-search
overload, standalone picker/wizard, app hooks and affected tests/CMake. No public
GuiOptions/binding layout or new dependency. Decisive checks: existing sparse/CLI
contracts plus native catalogue identities, header permutation, calibrated
mapping/raw-coordinate persistence, direct PPM import and unknown-calibration
refusal. GUI test clicks the actual import action, verifies prior file/live state
and exact retained counts/positions, and imports the admitted real demo without
shell commands. One cached native/frontend build after interfaces settle; preserve
other completed UI edits. Explicitly configure the new Visual Studio test target,
with a conditional live-process guard before using the shared cache.

Native spatial_counts/CLI tests pass (2 cases,0.66s), including catalogue identity,
calibrated/permuted coordinates, exact original-coordinate persistence, direct
image import and unknown calibration. GUI test compilation stopped because the
new target omitted MSVC /utf-8; add the existing frontend UTF-8 setting, without
editing translated source. Final review also invalidates the saved-image choice
when changing bases and requires reading its catalogue before preview; folder
navigation now stops rendering the old page after selecting another directory.
Retry only affected frontend/test targets using retained native objects.

GUI publication passes, including the real2,702-spot import and unchanged prior
study. Final review found repeated imports of the same input could collapse
source cohorts and duplicate observation IDs in the gene viewer. New imports get
an explicit recipe-scoped source identity; legacy objects without that optional
scope retain their exact prior identity. Add repeated-import cohort checks and
click the GUI Open action to verify its existing-loader handoff. Rebuild only
affected native/frontend tests and app links for those changed inputs.

I02 complete locally. Final expression/spatial_counts/CLI CTest passes3 cases
(1.37s); shared catalogue search compatibility passes gene_view separately.
GUI replay clicks Import and Open, retains exact data/coordinates and the prior
study, and imports all2,702 spots/six markers from the admitted real demo without
shell commands. Preview image completion/errors gate the Import button. Selected-
gene storage is delivered here; full backed storage remains D01 scope.

New recipe scopes distinguish repeated imported cohorts while retaining original
raw-source hashes; they are not biological sample/replicate identities. Legacy
objects without the optional scope retain their old identity. GUI image selection
is invalidated when changing bases. The current app loader handles the returned
new path and existing unsaved-work protection. Evidence is under workspace-root
build/feature-expansion/gui-i02-final; no original demo was overwritten.

Waste review: reused native parser, affine/calibration, jobs, tile preview, source
guards and atomic save. One missing UTF-8 target option caused the compile failure;
fixed it without editing translation data. Final checks closed actual stale-source/
cohort and loader-handoff gaps. No binding compilation, dependency download, new
cache, restricted material or package. Next R02: native landmark fit plus interactive
preview/accept/undo through the delivered registration contract.

### Planning update: remaining expansion packets and material separation

Updated the canonical feature-expansion-plan.md with an implementation path,
prerequisites, shared-service/consumer integration and decisive completion check
for all16 remaining tasks. The22-item checklist remains the only progress ledger:
6 complete,16 pending. Preserve unfinished R02 and concurrent desktop source;
this documentation update claims no new implemented feature.

Proprietary, NC and unreviewed records/pools remain separate and excluded from
open dependencies, fixtures, training, acceptance inputs and distribution. Added
explicit source-kit/package and derivative-lineage acceptance, AGPL compatibility
review, synthetic exclusion checks and handling when no open real fixture exists.
Next native packet remains R02; complete L01 admission as actual inputs settle.

Validation: documentation diff, checklist count and local-link checks only; no
build, binding regeneration, download or cleanup. Waste review: reused existing
22 tasks, licensing register and staging guard rather than another roadmap or
inventory. An initial task-note patch missed an exact line; appended this scoped
note instead, preserving existing concurrent edits.

### Active R02: native landmark fit and atomic registration

The existing draft now has an affected native test target. Changed inputs are
landmark fitting/evidence persistence, the shared current-registration guard and
CMake test/source wiring. Decisive first check: independent known translation,
similarity/reflection, affine/noisy least squares, held-out isolation, degenerate/
budget/cancel refusal, atomic undo and exact save/reopen. Reuse the real admitted
mouse-brain project for the same native contract; preserve original files.
Configure/build only landmark and existing registration tests in the shared cache;
no binding, package or broad campaign. Interactive UI and its proof remain open.

R02 interfaces are settled for the first affected build: compiled private panel,
native preview/atomic accept/load and active gene-overlay mapping. GUI replay
will exercise target dragging, invalidation, actual Fit/Accept, blocked publish,
undo/load/save/reopen and known displacement of an in-memory real open demo.
Source coordinates/counts are checked against the prepared baseline, and the
original fixture remains untouched. Earlier guards refused two competing desktop
builds; no competing compiler was started. Those processes are now absent.

First native R01 check passes. New landmark test crashed because its undo oracle
assumed default image-data properties were non-null; Document legitimately has
no metadata initially. Compare optional properties without dereferencing absence.
The first frontend compile also caught a stored transform field called as a
function; corrected. Gene replay's new input-clear used the singular field name;
corrected before its compilation. Retry the affected targets only, preserving
native library/test objects and the failing CTest log. No numeric assertion relaxed.

Native registration/landmark tests pass2 cases (0.25s), including the admitted real
mouse-brain native save/reopen check. GUI registration replay passes actual drag,
preview, accept, blocked publication, undo/load/save/reopen and all2702 known-shift
positions on the real in-memory test copy. Original fixture is untouched.
The new gene-overlay oracle omitted explicit source selection after its earlier
multi-source fixture and retained the comparison log display while expecting
linear colors. Correct only that setup: select the recorded lineage and linear
policy. Keep the failed capture; rebuild the gene test target only.

The corrected gene setup still failed. Binary/object inspection shows the new
registration caption absent from gui.obj: the competing compiler read gui.cpp/
gui_genes.hpp before edits, then finished afterward, defeating timestamp freshness.
Do not mutate shared translation units during another build; prepare isolated
files and defer wiring until it finishes. Touch only gui.cpp after all compiler
processes are absent to force its affected compilation. Also strengthen GUI replay
so save/reopen reloads landmarks through the actual panel using one persistent
project/image identity. Preserve both failed gene captures and native evidence.

R02 complete locally. Final native registration/landmark CTest passes2 cases
(0.25s) and the real native persistence contract passes. Final GUI replay proves
actual drag/invalidation, Fit/Accept, disabled publication, atomic undo, saved-fit
load and reload after save/reopen with persistent image identity. The controlled
real-data shift is recovered for all2702 spots; raw geometry/count identity stays
exact relative to the prepared baseline. Actual app gene colors pass at mapped
positions; final captures are readable. See the single feature-expansion receipt.

Updated the canonical checklist to7 complete/15 pending. No new dependency,
restricted material, binding compilation or package. Preserved concurrent sidebar
edits and all failed captures. Moved only the two known native test projects from
the source root into r02-native evidence; nothing was deleted. Waste review:
compiled controller isolates subsequent UI changes; a shared-source mutation race
required one forced gui.cpp compilation. Compilexx records the durable correction.
Next F01: reuse native raw TIFF/channel snapshots and existing segmentation kernels
for nucleus/cell measurements, with explicit compartment/invalid/seam semantics.

### Active F01: raw fluorescence cell service

Changed inputs: additive fluorescence_cells native header/service and one focused
native test target. Existing RGB/OD cell routes and binding ABI stay intact.
Reused physical Gaussian, exact metric distance/watershed, calibrated expansion
and compensated intensity moments. New raw reducer supports arbitrary channels,
body/nucleus/cytoplasm, finite/nonfinite/invalid/zero distinctions and explicit
binary64 integer admission. Raw source and segmentation/display values stay
separate. Local ROI/raster/support truncation is retained and flagged.

Decisive first check: independently defined masks/expansion, analytical four-channel
compartments, empty/missing data, Gaussian impulse/original-value measurement,
Otsu/constant cases, anisotropic area/radius, boundary flags, bounded/cancel and
malformed input refusal. Run the admitted real TIFF only as a native geometry
smoke; it has no admitted annotation and cannot close biological validation.
F01 remains open for source-bound requests/jobs, object/recipe persistence,
consumer integration, context/seam handling and an open annotated panel.
No new dependency, restricted download, GUI/binding build or package. Confirm no
live compiler before one existing-cache configure/build of the affected target.

Initial F01 native checks pass (1 case,0.12s); admitted OME raw-source smoke runs
and reports candidates without an annotation/accuracy claim. Final kernel review
found the reused watershed's regional intensity-mean gate could threshold the
already-thresholded foreground again. Pass the binary foreground to that gate,
so nucleus membership has one strict intensity threshold and never depends on a
rounded regional mean. Add a nextafter-threshold oracle. Rebuild only the changed
native source/test; no frontend, binding or package. BBBC039 is a possible open
annotated panel: the official page explicitly declares CC0 for v1, unlike the
excluded NC examples. No archive/data from it has been downloaded/admitted yet.

F01 native foundation passes its final affected check (0.15s), plus a native smoke
on the preselected CC0 BBBC039 image (130 candidates, not annotation acceptance).
The original TIFF/mask/official validation list are pinned in the existing fixture
inventory and pass offline staging admission. Range acquisition transfers about
511KiB including metadata, avoiding the78MB archive. Preserve its local recipe and
hash/range receipts; no proprietary/NC material was acquired.

F01 stays open and the checklist remains7 complete/15 pending: live source/ROI/
series/Z/T requests and jobs, contour/object publication, recipe persistence,
UI/CLI, context/seam behavior and declared annotated comparison are still needed.
Waste review: shared-cache guard deferred one competing desktop build; isolated
new native files avoided stale shared-header consumers. A reviewed binary-mask
handoff removed the reused splitter's second intensity gate. No bindings, new
library, full qualification, GUI build or release. Next continue the source-bound
native workflow using this tested service and the existing raw currentness guard.

### F01 continuation: source-bound request/run/job

Changed inputs: additive native fluorescence_cell_workflow header/service/job,
shared reserved raster preflight and one source-workflow test target. Reuse native
RawAnalysisRequest ROI/calibration/document authority and existing raw source
snapshot/metadata/file guards. Workers capture no live Document, decode the
selected raw region once, and preserve original samples. Context must cover the
Gaussian radius (or available image edge); source-only crops stay explicit.
Content/result identity is separate from capture-bound publication authority.

Decisive checks before this affected build: independent raw-read counter and
analytical channel/Z/T/ROI results, calibrated expansion, same-core context
agreement, metadata/ROI/calibration/files/capture mismatch refusal, budget before
pixel IO, no display fallback, and deterministic read barriers proving native
cancel/join/stale delivery. Also rerun the existing pure raster case because its
admission now reserves request overhead. Native-only CI includes the new case;
no binding/frontend/package target. Preserve concurrent desktop edits. F01 remains
open for native contour/object publication, persistence, consumers and annotated
comparison; this source job is not completion of that full contract.

The first pure-raster case passes. Source-workflow checks caught source-only
provenance on an object-scoped ROI: the reused lattice capture helper intentionally
captures source-only provenance for classifier lattices. Retain the existing
object-specific capture_provenance for ROI requests using the RawAnalysisRequest
revision/context, while preserving source-only crops. Correct the new request
only; do not alter existing classifier helpers or their source identities.
Retry the affected native source/test, preserving the first failed CTest log.

The ROI provenance repair passes. The next source test correctly delivers stale
for a reused request after ROI Undo: native Undo restores geometry while advancing
its object revision. Recapture after Undo for subsequent generations. Make the
metadata-change oracle start from a demonstrably current fresh capture too, so
its refusal proves metadata currentness rather than an already-stale ROI.
No production revision check is weakened. Rebuild only the changed source test.

Source/pure tests now pass2 cases (0.25s), and the admitted real TIFF completes the
source-bound native job. Final review caught terminal duration freezing at the
last running poll if that poll had already filled elapsed_ms. Update duration
when consuming the future and freeze it afterward; add a read-barrier test proving
terminal elapsed advances after a running poll. Retry source-workflow only; reuse
passing pure-kernel evidence. No scientific parameter or assertion is relaxed.
The pinned annotation is RGBA8, so future comparison must use its declared channel
semantics; do not weaken the existing NuCLS RGB8 decoder to ingest it implicitly.

F01 source-bound request/run/job is implemented and verified. Pure/source native
checks pass2 cases (0.25s); the final source-only timing check passes (0.26s total).
The admitted real TIFF runs through the native job (130 candidates, not annotation
acceptance). Native read counters, original axes/values, context agreement, actual
file guards, capture/currentness and deterministic stale/cancel/join checks pass.
See the single feature-expansion receipt and fluorescence-omics native API guide.

F01 remains unchecked; total remains7/22. Next native contour/object publication,
recipe persistence, consumers and annotated comparison. Reuse the existing typed
label contour adapter rather than fabricating RGB CellResult/TissueResult input.
Waste review: reused raw source/ROI/file/moment authorities and one reserved work
admission. Corrected source-only ROI provenance, one stale test capture after Undo,
and terminal duration. Only affected cached native targets compiled; no bindings,
new dependency/data download, frontend/package or broad replay. Preserve all failed
logs and the independent desktop edits.

Moved the one known standalone native fixture directory from the source root to
f01-source-final evidence after verifying paths and terminal test use. Nothing
was deleted. Run later standalone file-producing checks from the evidence/cache
working directory to keep generated fixtures out of the integration source root.

### F01 continuation: typed contours and native cell observations

Changed inputs: raw fluorescence mask adapter in the existing contour engine,
additive contour/publication/replay service and test target, and run context/
retained-size accessors. No Document/GUI/binding layout change. Preserve all
concurrent desktop edits. Reuse native GEOS tracing, ObjectEdit creation and
ObjectProperties persistence; no RGB mask/source construction or new store.

Decisive checks: analytical raw nucleus/body areas and original channel sums;
label/containment and fractional ROI clipping; single-command cell/properties
publication, duplicate refusal, undo/redo, exact recipe/source replay after
save/reopen, real Z/T/source-only planes and absent cytoplasm. Capture/file/
calibration/geometry and budget/cancel errors must leave objects intact. Run the
admitted real TIFF through tracing/publication as a geometry/identity check, not
annotation acceptance. Rebuild affected native publication/source targets once
interfaces settle. Native-only CI without GEOS checks explicit capability refusal;
this local existing cache tests the enabled geometry path. F01 remains open for
UI/CLI, annotation comparison and complete workflow acceptance.

The first compile caught a test calling nonexistent Document::add_ellipse; use the
existing add_roi(Roi::ellipse(...)) API. No product API shim is needed. Final API
review adds complete-cohort verification so a consumer checks source/file metadata
once for all cells rather than repeating it per object; also require every label
exactly once. Preserve raw scalar inspection for individual cells. Missing/cleared
publication facts return false without dereferencing absent properties. Retry only
the new objects/source native targets after these settled inputs; no GUI/bindings.

The fractional-ROI test reproduces GEOS zero-area boundary remnants already
handled by RGB cell contours. Extract that existing Difference/Union/Covers
normalization into the private shared cell-contour helper and reuse it for raw
contours. Positive-area escape still refuses and final Covers remains exact.
Include the affected existing cells test because this is shared native code.

Also complete compact recipe/result-identity retention through existing typed
image properties, including zero-cell outcomes, with native list/replay/verify/
remove and undo. No fake cell or duplicate source raster is stored. Add empty/
nonempty keep/save/reopen, malformed record and unrelated-metadata checks.
Read-only record lookup uses the immutable map by reference; mutations copy it
once. Build affected objects/source/cells targets after this combined correction.

Native objects/source/legacy cells tests pass3 cases (1.96s). The admitted real
TIFF traces, publishes and batch-verifies all130 labels with one source-currentness
check. Atomic native cells/properties, exact recipe replay and save/reopen,
fractional ROI containment, captured Z/T, zero-cell record keep/remove/undo and
unrelated metadata preservation pass. No annotation/biological acceptance claim.
F01 remains open for UI/CLI, annotated comparison and final workflow acceptance.

Waste review: reused typed contour tracing and extracted the existing RGB boundary
normalization only after the fractional-ROI failure demonstrated that need; exact
Covers/Difference checks remain. A test used a nonexistent convenience method;
replaced it with existing add_roi. Added batch verification to remove per-cell
file checks, and retained compact zero-cell recipe records without fake objects
or duplicate rasters. Only affected native targets and the shared cells check ran;
no bindings/frontend/package build or new dependency/data download. Real standalone
fixture output stayed under f01-objects-final evidence, outside source. Preserve
failed logs and concurrent desktop edits. Next wire these shared services into
native UI/CLI, then qualify the admitted RGBA annotation panel with declared metrics.

### Planning update: finish the eight feature areas

Owner requested implementation planning with proprietary and NC material kept
separate. Refined the existing22-item plan rather than creating another backlog.
F01 next steps now name the native CLI, desktop job integration, predeclared
annotation comparison and affected acceptance checks; tested native backend work
is explicitly partial. Shared request/job/publication/replay boundaries keep
frontends thin. Remaining packets retain prerequisites and one decisive exit.
Proprietary, NC and unreviewed references/pools remain distinct and excluded from
open staging; no restricted downloads or adapters are selected.

Waste review: reused existing checkboxes, source-scoped tests and licensing
registers; no new engine, duplicate numerical campaign or build/package work for
a planning edit. Next implementation is F01 consumer integration.

### F01 continuation: native project CLI

Changed inputs: additive fluorescence CLI dispatcher, batch command/help, focused
native consumer target and CI registration. Commands detect into a new project,
list retained records and verify exact native recipe replay. Record-only runs
need no geometry; publishing cells requires existing GEOS. The project retains
source series; channel block/Z/T/crop or ROI are explicit recipe inputs. Reuse
native detection/publication/records and project save_new with input/companion/
journal alias protection and source-currentness at final commit. No Python or
desktop layout changes. Decisive checks: exact synthetic raw facts after saved
CLI run, zero-cell outcome, list/verify, no overwrite/protected paths, argument
refusal, cancelled work and bounded-save failure without output. Build only batch
and the new consumer test in the existing cache after settling these inputs.

Initial CLI test passes (0.38s total), and the actual batch executable detects/
saves832 candidates from the admitted two-channel OME demo, lists its record and
replays the exact run digest; original project SHA256 unchanged. This is consumer
evidence, not biological accuracy. Before closing this slice, add the missing
nonzero-first-channel plus explicit-crop mapping case: original channel1 is local
segmentation channel0, with independently known nucleus sum2941. Also assert
explicit zero dimensions/channel count refuse before opening. Rebuild only the
changed CLI test; scientific/backend source and passing batch executable stay.

Final native CLI test passes1/1 (0.30s total), including the added independent
channel1/local-channel0 crop oracle. Real executable help/detect/list/verify passes
on the admitted OME demo:832 cells,26 flagged at boundaries, exact run replay and
unchanged input project bytes. See the single feature-expansion receipt for logs.
F01 still needs desktop integration and the frozen annotated comparison.

Waste review: reused native authorities and existing project atomic-save/path
protection rather than adding a second workflow/store; source currentness is
rechecked at commit. The first passing consumer check lacked nonzero-channel/crop
coverage; one affected test rebuild closes that explicit risk. No bindings, wheel,
GUI build, new download or broad scientific rerun. A multi-file documentation
patch used an incorrect heading and was rejected without changes; exact text
replacement corrected it. Preserve existing linker warnings and concurrent UI
source, and leave F01 unchecked until its remaining acceptance is demonstrated.

Selective current-card hunk staging refused mixed line endings; stage the native
tail against the exact HEAD document instead, preserving the unrelated UI top
section unstaged. No source or evidence changed during that correction.

### F01 continuation: fluorescence desktop tool

Changed inputs: compiled private fluorescence tool and additive menu/tool/overlay
hooks, preserving concurrent desktop edits and public Document/GuiOptions/binding
layouts. Use existing immutable detection job, publication and run-record APIs;
trace contours on an owned cancellable worker before one owner-thread publication.
Defer publication during save/load/edit blocking. Draw a bounded sampled label
preview after the existing canvas cache, never changing masks or source samples;
hide it for a different display Z/T. Expose channel/block/plane/ROI/crop controls,
measurements with absent cytoplasm, keep/remove/replay and native error messages.

Decisive checks: actual native ImGui action clicks on an analytical TIFF, preview
registration, original compartment values, blocked Keep, atomic publication/undo,
run retention/replay/save/reopen, stale ROI and switched-document refusal, empty
outcomes and cancellation. Build only the compiled panel test and standalone app
in the existing cache after interfaces settle; no bindings or package rebuild.

Initial desktop compile caught private analysis_identity_ access and a poll hook
before editing_blocked is defined. Use the document/source observation plus the
existing request.matches authority (which still detects a replaced Document at
the same address); no friend/API/layout change. Early poll always defers contour
publication; actual draw delivers only after edit/save/load blocking is known.
Retry only affected native GUI/test/app targets after these diagnosed repairs.

The repaired build passes, but the first native ImGui click test does not produce
a ready result. Preserve f01-gui-ctest.log; add native error/busy/result diagnostics
and a failure framebuffer capture before changing product logic or assertions.
Rebuild only the changed test to distinguish UI input delivery from job failure.

Diagnostic retry reaches native Keep and fails later at Replay, while Run and
raw preview registration pass; the first Run failure is input-timing dependent.
For the hidden GLFW test, disable ImGui event trickling and add a hover frame
before button-down so queued backend cursor-loss events cannot shift synthetic
clicks across frames. Preserve all assertions, add Replay failure capture/details,
and rebuild only this consumer test. No product behavior is changed by this fix.

The next replay failure isolates the real race: native job.busy() becomes false
when its future is ready, before the panel has delivered it. The test can finish
its wait with pending delivery and no result, at different steps depending on
scheduling. Include pending owner delivery in the panel busy state; do not change
native job semantics or weaken result assertions. Retain hidden-input hover/
queue control for deterministic synthetic interaction. Add a ready-contour/save-
blocked delivery assertion and same-address Document replacement case. Cache
boundary count once per immutable run and use centered sampling for reduced
preview pixels. Retry the affected panel/test/app targets; main gui.cpp is settled.

With pending delivery included in busy, Run/Keep and save/reopen Replay pass.
The next failure is the test shrinking a parent ROI beneath already-published
child cells, correctly refused by Document. Enlarge the ROI within the source
instead: it still changes captured geometry but preserves child containment.
Rebuild only the test; retain the product parent/child invariant unchanged.

Desktop checks pass1/1 (0.60s total), including deferred publication, exact
reopened replay, stale ROI, zero cells and same-address Document replacement.
Final error-path review finds contour/publication exceptions leave the caption
tracing after the worker is terminal. Set failed/cancelled terminal captions and
add duplicate async Keep/refusal/replay recovery. Rebuild the affected panel/test
and relink the standalone app; no main GUI source or binding changes.

Final desktop CTest passes1/1 (0.53s total); actual ImGui Run/Keep/record/replay on
the admitted CC0 TIFF delivers all130 labels with unchanged original bytes. The
standalone app smoke passes from System32 with Windows-only PATH; admitted OME
project bytes are unchanged. Source-scoped working-tree snapshot/hashes and logs
are in f01-gui-real; preserve concurrent UI edits, not a frozen-release claim.

Waste review: reused native jobs/publication/records and isolated the panel in a
compiled module. Private-token and early-block-scope mistakes caused one compile
repair. Worker-ready vs owner-delivered caused intermittent missing UI results;
count pending delivery in busy instead of rerunning or weakening assertions.
A stale-ROI test violated parent containment; enlarge it, keeping that invariant.
Late private header diagnostics required another large gui.cpp compile; freeze
that interface before future cached builds and keep extra diagnostics in the
implementation. Only affected targets/app smoke ran; no bindings/package build.
The single receipt/plan now mark desktop and CLI consumers implemented. F01 remains
open: next freeze annotation semantics/matching/tolerances before evaluating the
admitted RGBA panel, then finish its declared context/boundary acceptance.

### F01 continuation: predeclared annotation and boundaries

Changed inputs: existing fixture manifest protocol, independent native test-only
RGBA8 annotation decoder/metrics/reducer and affected finite-context checks. No
new public API, GUI/binding source, package, reader profile relaxation or material
download. Freeze criteria in the manifest/receipt before annotation pixels/results:
untuned Otsu recipe, equal-value8-connected first-channel instances, all borders,
maximum-cardinality IoU0.5 matching, Dice/F1/split/merge and raw mean tolerances.
Decisive checks: hand-derived metrics/decoder examples, exact admitted input hashes,
single preselected panel against frozen gates and independent original-sample
compartment statistics; context/ROI/source boundary checks remain separate from
D02's whole-slide tiling. Preserve failures; no post-hoc gate relaxation or tuning
on the annotation. Build only new evaluator and changed source-workflow test.

The evaluator compile catches Windows legacy near macro expansion through the
existing file-guard header. Rename the local numeric test helper expect_close;
no Windows macro/API, product code, protocol or numeric tolerance changes. Retry
the evaluator and still-pending boundary target, not GUI/bindings or broad tests.

Evaluator analytical/decoder tests pass1/1 (0.15s total). The separate boundary
test compile omitted RoiSamplingPolicy on contains_sample; pass the existing
GeometryCoverage policy explicitly, matching native request eligibility. The
evaluator target already built, so reuse it for the first frozen real-panel run
while rebuilding only the still-unbuilt boundary target. Preserve compile logs.

Native analytical evaluation passes1/1 (0.15s); expanded finite boundary/source
checks pass1/1 (0.25s). First frozen real pilot fails F1:82TP/48FP/28FN,110 reference
and130 prediction instances; F1=0.6833333333333333 below0.70. Dice0.93555, raw moment
oracle, split/merge and matched intensity gates pass. Preserve exit1/full first
report and exact original manifest in f01-evaluation-v1; F01 remains unchecked.

The first official training filename was selected before reading its pixels or
results. Its CC0 image/mask/partition are admitted separately (offline guard3/3),
reusing bounded range extraction and existing metadata;476,499 bytes transferred.
Existing manifest freezes a9-recipe sigma/prominence grid and training selection
rule. No training pixels/metrics have been examined or validation tuning run yet.
Next implement/run that small native selection, freeze its choice, then compare
on the original validation panel with unchanged gates and disclosed validation
reuse. Do not replace the failed v1 report or call the reused panel unseen.

Waste review: used one existing decoder/library, fixture inventory and receipt;
no new product API, raw reader relaxation or binding/GUI/package build. A Windows
near macro collided with a test helper; rename it. A test omitted the existing
explicit ROI sample policy; add GeometryCoverage rather than a default API shim.
Use already-built evaluator evidence while only the pending boundary target
rebuilds. Raw measurements are independently reduced, not inferred from contour
publication or high foreground Dice. Preserve scientific failure rather than
weakening the predeclared object-F1 gate or launching a broad corpus campaign.

### F01 continuation: train-selected recipe and fixed-gate follow-up

Changed inputs: native evaluator prepares admitted data/annotation/raw oracle once,
then runs only the nine previously declared sigma/prominence candidates. Native
selection excludes failed non-F1 gates, maximizes training F1 and applies fixed
ties (Dice, lower sigma, lower prominence). Admission explicitly separates the
training versus validation file lists. Add independent selection/no-candidate/tie
oracles; follow-up mode reads a separately frozen protocol with unchanged gates.
No scientific kernel, default/preset, UI/binding or package changes. Decisive
check: evaluator self-test, one complete training grid, freeze its selected recipe,
then one original validation comparison disclosing panel reuse. Preserve v1 failure.
Build only evaluator target in the existing cache; no new fixture download.

Native training selection/self-tests pass1/1 (0.13s). Exactly nine declared
candidates evaluated; index6 (sigma2px/prominence0.5px) wins training F1=0.85333
with all other gates/raw moments passing. Preserve complete training report and
manifest. Freeze followup_protocol/2 with unchanged v1 gates and report hash before
the next validation invocation. No validation search, default/kernel change or
new data download occurred; one annotation/raw oracle is reused across candidates.

Freeze728aae2a precedes the follow-up call. The selected recipe passes unchanged
validation gates:118 predictions,82TP/36FP/28FN, F1=0.7192982456140351, Dice0.92252,
split17/110, merge4/118, matched mean relative error median0.03056 and independent
raw moments passing. Preserve v1 failure and v2 first report; explicitly reused
validation, no unseen-data claim. Before closing F01, check contour publication/
all-cell save/reopen plus actual CLI sigma mapping against this selected run.
Extend the existing native objects test to accept a test-only sigma and preserve
seed/published real projects. Build evaluator (removing duplicate final file-hash
verification in its training wrapper) and this changed native object target once;
reuse existing batch executable for the consumer check. No GUI/bindings/package.

Selected real integration passes: all118 cells trace/publish/batch-verify, keep
native records and exactly replay their geometry/facts after project save/reopen.
The actual existing batch executable gives v2's exact run digest, publishes118 and
verifies saved replay; original TIFF and seed hashes remain unchanged. Final
evaluator self-test passes1/1 (0.12s). Logs/commands are in f01-selected-integration;
original v1 failure, training grid and v2 first report remain separate.

F01 is complete under its bounded declared contract, not general biological,
unseen-held-out or whole-slide tile acceptance. Plan is8/22 complete,14 pending.
Next F02: typed marker gates/AND-OR rules over original-domain compartment facts,
Unknown for missing values, native preview/publication and exact stored replay.

Waste review: native training selection prepares image/annotation/raw oracle once
for nine fixed candidates and follows the predeclared rule. No validation search,
gate relaxation, new default/model/kernel or new material download. Removed a
duplicate final file-hash verification from the training wrapper, retaining the
single owner-boundary check. Reused the passing frontend job/UI evidence and
existing batch binary; only evaluator/native object tests rebuilt for the missing
selected-shape persistence/CLI mapping check. Preserve all prior receipts and
concurrent desktop edits; no bindings, GUI rebuild or packaging campaign.

### F02 continuation: native marker rules and saved observations

Changed inputs: additive typed marker rule/DAG/value evaluator, scalar-only
request/result snapshot, rule persistence through existing image properties,
and one native test target. Reuse the F01 observation-integrity hash and raw
capture identity, without private Document access/layout changes or pixel reads.
AND/OR use full three-state logic; missing/empty/nonfinite/insufficient samples
remain Unknown. Statistic/units/domain are explicit, graph and recipe bounded,
and result gate states are flat batched buffers. Native class publication is
one existing apply_classifications command, with its normal probability/color
policy and Undo; no geometry/raw-fact rewrite or per-object recipe duplication.

Decisive checks: complete analytical truth tables/threshold edges and malformed
DAG/units/schema; original multi-channel blob statistics and NaN/empty/unmeasured
Unknown states; exact cohorts/saved rules, owner/capture/stale/cancel guards,
no pixel reads, one Undo, and save/reopen replay. Include maximal declared graph
and native work limits. One cached core/new-test build after interfaces settle;
no bindings, GUI, package or new dependency/material download. F02 remains open
for consumer integration and full workflow acceptance after this backend slice.

Initial native marker build/test passes1/1 (0.22s total). Final review adds two
missing named trust-boundary cases: changed body geometry must refuse capture
of historical raw facts, and a valid-size100000-ID/64-gate request must hit its
4MiB work bound before allocating the observation table or looking up rows.
Rebuild only the changed native test; no API/kernel/GUI/binding changes.

Final native marker test passes1/1 (0.20s), including edited-geometry refusal and
preallocation work admission. Actual cache fixtures classified.qpx/rules.qpx prove
the enabled save/reopen branches ran. Root/gate counts/cohorts, exact native rule
replay, complete three-state logic, one class Undo and zero added pixel reads pass.
Plan remains8/22 with F02 unchecked until CLI/desktop workflow integration.

Waste review: reused existing observation hash, raw identity guard, typed image
properties and compound class command. Workers use scalar snapshots, one node
workspace and flat gate states; schema validation/recipe storage do not repeat
per object. All native/consumer interfaces settled before the first core build;
only one test-only rebuild added two concrete trust-boundary checks. No new
forest/framework, binding/GUI/package build or dependency/data download. Next add
thin native marker rule CLI/desktop controls over this same service, with native
cohort selection and exact saved-rule replay. Preserve concurrent UI edits.

### F02 continuation: native rule files and CLI

Changed inputs: shared native human JSON IO/report formatting (existing optional
project/JSON capability, no dependency fetch), native cohort selection and
post-classification publication check, plus batch preview/apply/list commands.
Recipes explicitly declare all channels/compartments/statistics/units/thresholds
and topological nodes. Reject duplicate/unknown keys, ambiguous enums, noninteger
indices, bad graphs and excessive size/depth; exact native recipe identity must
survive JSON/file/project round trips. No Python data interpretation.

Preview is read-only. Apply keeps a rule once, uses the existing class transaction
and saves a new project with protected input/rule/image/companion/journal paths.
At final commit, verify expected class labels and exact saved observations while
allowing revisions changed by Apply itself; do not reauthorize old previews or
capture a duplicate scalar table. One shared input-hash stream implementation is
used by capture and verification. No post-Apply duplicate verification before
staging; final owner guard is the decisive publication check.

Decisive tests: strict JSON and negative-zero roundtrip, file guard/cancel/no
replace, native all/explicit cohorts, Unknown output, actual apply/new-project
save/reopen/replay, original bytes, protected missing sidecars, budgets and native
post-Apply verification/change refusal. Settle interfaces, then one cached build
of affected core/backend/CLI tests and batch; no GUI/binding/package build. F02
remains open for desktop workflow integration and selected end-to-end acceptance.

The first build compiles shared IO/publication and the backend target, then
catches three CLI test assertions missing their required diagnostic argument.
Add those messages and emit captured native stderr if an assertion unwinds.
No product/schema/threshold change. Rebuild only CLI test and pending batch; run
these and the already-built affected backend checks after the repair.

Affected native backend/CLI tests pass2/2 (0.45s total). Actual batch executable
help/preview/apply/list/saved-rule replay passes on the admitted118-cell project;
the fixed illustrative OR rule yields1 Positive/117 Unknown/0 Negative, all118
classes save and the result digest replays exactly. Original project/TIFF/rule
hashes are unchanged; this is rule workflow evidence, not biological phenotyping.
Logs/commands remain f02-cli-ctest.log and f02-cli-real/results.json.

Waste review: shared native JSON interpretation/reporting and cohort selection;
no frontend rule evaluator, duplicate scalar snapshot or per-cell stored recipe.
Use content/class verification after Apply, not its intentionally stale pre-Apply
row revisions. Stream the same input hash implementation and verify once at final
project commit. Three missing test diagnostics needed one targeted repair, with
native stderr now retained on failure. No new dependency/data, binding/GUI/package
build or broad science rerun. Next wire the desktop controls to these services;
F02 stays unchecked and plan remains8/22. Preserve concurrent UI edits.

### Planning refresh: all eight areas and separate restricted material

Updated the existing22-item expansion plan rather than creating a second ledger.
The next packet is F02 desktop integration over the delivered native backend/CLI:
rule editing/loading, explicit cohorts, preview, Apply/Undo and saved replay.
Added ordered implementation and acceptance steps, with source-bound invalidation
and change-triggered integrity checks instead of per-frame observation hashing.
Proprietary, NC and unreviewed materials remain distinct metadata/external pools;
added per-packet handling and final source/binary staging responsibilities.

Validation is document diff, local-link and checklist-count checks only. No code,
dependency download, build, bindings, fixture or package changed. Progress remains
8/22 complete,14 pending. Waste: two patch attempts matched wrapped prose
incorrectly and made no changes; narrower anchors applied the intended edits.
Next implement the F02 desktop packet, preserving concurrent UI changes.

### F02 desktop implementation: native panel

Changed inputs: private compiled marker panel and fluorescence-tool hook, plus
one native ImGui test target. Reuse typed rules/JSON, scalar snapshots, cohorts,
classification commands and persistence. Pending future delivery remains busy;
post-Apply observation verification runs on document changes, not every frame.
Loaded graphs remain intact unless explicitly rebuilt. Keep and Apply are
separate native Undo actions. No public Document/GuiOptions or binding changes.

Decisive check: actual native panel preview/Unknown, Keep, blocked Apply,
Apply/Undo, selected cohort, cancel, image replacement and exact saved replay;
then the same desktop route on the admitted118-cell project. Build affected GUI
and marker test once after interfaces settle, preserving the one cache and
concurrent main-GUI edits. No package/bindings or segmentation accuracy rerun.

Initial desktop tests pass2/2 (2.46s), including the existing fluorescence panel.
Review adds direct rule-file/malformed-load and saved-rule Load button checks,
plus desktop-applied class save/reopen verification and a readable-panel capture.
Only the private panel/test change: expose its existing file section initially,
and avoid scanning every captured row when the native source guard and document
revision are unchanged. Rebuild those affected inputs plus the standalone app;
retain initial logs. The first build included pending main-GUI changes, so its
larger translation-unit compile is recorded rather than starting a second cache.

The added malformed-load recovery check exposes stale test click coordinates:
the newly visible error moves the file button on the next layout frame. Settle
one frame before reading its current bounds; this changes only input automation,
not parser/error/UI behavior or assertions. Preserve f02-gui-ctest.log as the
failed run; retry the changed marker test only after rebuilding that test.

Final marker GUI suite passes1/1 (1.64s); the unchanged fluorescence suite passes
in the preceding run. Actual118-cell desktop file/saved loading, Unknown counts,
Apply/Undo, selected cohort and exact class/rule save/reopen pass. Source project
and TIFF hashes are unchanged. Standalone app opens that saved project from
System32 with Windows-only PATH, exit0. f02-gui-real/results.json and readable
marker-panel.png record the handoff. F02 is closed:9/22 complete,13 pending.

Waste review: reused native scalar evaluation and class/rule persistence; no
per-object/per-frame scientific recomputation. One targeted test repair settles
layout before reading button coordinates after an error. Preserve its failure.
The initial build consumed pending main-GUI changes; subsequent builds reuse its
object and compile only private helper/test and glob-triggered provenance inputs.
No bindings, package, new material or full science rerun. Next F03: settle the
minimal typed original/derived block source contract, preserve legacy TIFF
identity, then explicit immutable dark/flat recipes with analytical field/seam,
invalid denominator, cancellation and source/persistence checks. Keep proprietary,
NC and unreviewed inputs excluded; preserve unrelated desktop edits.

### F03 foundation: typed quantitative sources and dark/flat blocks

Changed inputs: additive ChannelSource capability over existing raw metadata/IO,
original-source version capture and an immutable dark/flat derived source/kernel.
No TIFF display-view prerequisite or legacy TIFF/F01 identity rewrite. Original
sources and explicit per-channel scales determine a versioned corrected domain.
Identical axes/channel order/calibration are required; no implicit broadcasting,
tile-local normalization or signed-value clamp. Padding uses validity intersection;
nonfinite/invalid denominators become per-channel NaN. Reject integer precision
loss beyond2^53 and bound simultaneous buffers before any decoder read.

Decisive checks: hand-calculated multi-channel values, signs/zero/invalid samples,
threshold edges/nonfinites/precision, exact whole-versus-tiled seams and selected
channel/Z/T, native source metadata/file/cancellation guards, malformed block
refusal and exact-limit work admission before reads. Use synthetic native readers
without TIFF views plus existing admitted quantitative TIFF fixtures. Settle this
API/test contract before one affected cached core/test build; no GUI/bindings or
new dependency/data. F03 stays open for persisted recipes and CLI/desktop/derived
analysis integration; do not label corrected measurements as original samples.

First cached foundation build succeeds; channel_source native CTest passes1/1
(0.12s), including its actual TIFF byte/version branch. No retries, bindings,
GUI build, dependency/data download or segmentation rerun. Existing core/UI
products and unrelated desktop edits are preserved. The new target is included
in native-only CI; local execution is the current evidence, not hosted execution.

Waste review: reused existing native raw IO, masks and file-version guards;
normalization is explicit so tiles do not rescan a global field or drift at seams.
Selected blocks retain bounded original buffers and one Float64 output; type
dispatch is resolved once per input block, with no Python callbacks.
The new capability is additive, preserving legacy TIFF hashes and avoiding a
premature broad workflow migration. Next retain exact correction recipes/source
associations for save/reopen, then thin CLI/desktop and derived-domain analysis.
F03 remains unchecked; overall9/22 complete,13 pending.

### F03 continuation: retained recipes and native CLI

Changed inputs: immutable native correction request/recipe and owned corrected
block report, typed image-property persistence and batch Keep/list/verify/preview.
Capture exact source locators/reader/series/version SHA and options, with a distinct
corrected domain/source identity. Initial automatic reopen explicitly supports
native TIFF, without changing the reader-independent block capability. Reuse
RawAnalysisRequest for owner/document/calibration authority, existing image
properties/Undo and guarded new-project save; no Document layout/friend change.
Workers hold only native immutable sources, not Document or Python callbacks.

Decisive tests: independent constant dark/flat TIFF oracle, signed values/counts,
worker reading versus owner-only controls, work/cancel bounds, one Keep/Remove
Undo, typed signed-zero/schema/identity integrity, exact project/source/block
save/reopen replay, same-address replacement and changed/missing field refusal.
Actual CLI Keep/list/verify/preview must match native results, refuse original/
field/SQLite sidecar paths, preserve input bytes and leave no output on bounded
save/cancel failure. Settle APIs, then one cached core/test/batch build; no GUI,
bindings, new dependency/data or original F01 science rerun. F03 stays open for
desktop controls and explicitly derived analysis consumption.

First build and dark_flat_workflow test pass1/1 (0.28s). The actual batch binary
also passes Keep/list/verify/preview from System32 with Windows-only PATH; repeated
block identity is exact and all original project/signal/field hashes remain
unchanged (f03-cli-actual/results.json). Native CLI and direct C++ reports agree.

Review finds that a changed-field Keep assertion can hit the existing duplicate
record check before its source guard. Remove the record before that attempt and
require WorkflowInputChanged, then Undo the removal and verify stale replay too.
This strengthens the named trust-boundary check without a product/recipe change.
One affected test rebuild is sufficient. Inspect build command lines: the remaining
compiler belongs to sibling napachan, not this cache, so it is not a cache lock.

Final strengthened workflow suite passes1/1 (0.21s), including actual changed-field
Keep refusal before metadata mutation and exact project/source/block replay.
Actual Windows-only-PATH batch Keep/list/verify/two previews agree exactly and
preserve input hashes (f03-cli-actual/results.json). Typed recipe storage and CLI
are delivered; F03 remains open for desktop and explicitly derived analysis use.

Waste review: one shared source/kernel/request and typed property codec; no
frontend numeric pipeline or per-object copies. Reuse native owner identity,
metadata Undo and atomic new-project save. Perform source validation at actual
capture/read/publication boundaries; avoid a second signal recapture during replay.
One test-only rebuild strengthens a masked guard case. Extra CMake regeneration
comes from concurrent UI header changes; command-line-scoped cache checks avoid
waiting on an unrelated sibling compiler. No GUI/bindings/package/science campaign
or new material. Next add the correction desktop panel and an explicit corrected-
domain analysis route over these native services, preserving original F01/F02
identities and concurrent desktop work. Overall9/22 remains unchanged.

### F03 continuation: explicitly corrected cell analysis

Changed inputs: additive corrected-source FluorescenceCellRequest overload and
run domain/reference accessors, reusing the current kernel/contours/publication.
Corrected input/recipe/publication interpretation is explicitly versioned; default
raw hashes/recipe/metadata stay unchanged. Correction recipes must already be
retained, and one reference per run/cell avoids copying field associations.
Deletion refuses while referenced by cells or run records. Pending captures
invalidate on lost associations; cache unchanged immutable image-properties
pointers rather than reparsing a correction recipe every frame.

Marker rules/cohorts explicitly select original or corrected observations. Default
cohorts stay original, known opposite-domain cells are excluded, and corrupt or
unknown publications still refuse capture rather than disappearing. CLI detection
opts into a retained correction; output guards protect its field paths. The marker
CLI/private panel use the native domain cohort helper and display the actual
domain. No corrected scalar is relabeled as an original observation.

Decisive check: independent corrected nucleus/body/cytoplasm values, equal known
geometry with distinct identity, empty results, work/cancel limits, retained
reference protection and stale refusal, one publication Undo, exact corrected
run/cell/project and marker replay, wrong-domain refusal and native CLI equality.
Reuse completed kernel/field evidence. Because shared F01 APIs/recipe parsing and
marker capture change, build affected native workflow/object/CLI/marker tests and
the existing marker desktop test in one cache batch, without bindings. Check the
retained real F01 run once against its stored digest for the concrete backward-
identity risk; no training/pilot sweep or broad science campaign. The marker GUI
check establishes default-route compatibility, not F03 desktop completion. New
correction controls remain the next packet; preserve concurrent UI source edits.

Affected cached build succeeds; all8 tests pass in2.96s, including existing
source/object/CLI/marker routes and default marker desktop compatibility. The new
corrected test independently proves14 nucleus/27 body pixels, nuclear C0 sum1401
versus original1541, body mean92, cytoplasm C1 mean2513/13-20, exact native/CLI
publication and source/run/cell/project/marker replay. Reference removal/stale
checks, work/cancel and one Undo pass. No build/test failure or retry occurred.

Actual batch preserves the real F01 stored digest4deb4bb86e9bb65fe6aae3ead3a83ab89878a7edee488ab53e1f92dbf95bb67a
and118 cells. It saves/replays one corrected synthetic cell with an explicit
marker-domain cohort; all input hashes remain unchanged. Windows-only PATH/
System32 receipt: f03-cells-actual/results.json. No new biological or speed claim.

Waste review: reuse kernel/contours/publication and metadata Undo; store correction
associations once and reference them. Default capture avoids a new metadata copy;
corrected work admits read and segmentation peaks before reading. Cache unchanged
immutable properties for owner polls; do not reopen fields or parse recipes each
frame. Changed public APIs/recipe parsing justify affected consumer checks and a
single retained real F01 digest check, not broader qualification. Preserve the
concurrent UI changes. No bindings, package, new dependency/data, new scheduler or
repeated pilot. Next correction desktop authoring/preview/selection, using these
settled APIs; F03 remains open and overall9/22 complete,13 pending.

### F03 desktop completion: correction authoring and explicit selection

Changed inputs: private compiled correction panel, fluorescence-tool source
selection and marker-domain controls over the delivered native services. Open
fields on a native worker, capture Document on its owner using a shared native
restore helper, and read owned preview blocks asynchronously. Keep/Remove use
existing native metadata commands; explicit Use hands a retained request to the
existing fluorescence job. Default runs remain original. Saved recipe load and
cell/run replay must retain exact identity; signed values remain visible with
explicit channel/crop/Z/T, counts and presentation-only range/preview sampling.

Settle controller/test APIs before one cached build. Ordinary polls do not open
fields or hash recipes: cache correction identity and immutable saved-rule
properties, with lazy unused-tool defaults. Publication counts track created IDs
on structural revisions so Undo/Redo stays truthful without per-frame fact hashes.
No public Document/GuiOptions or binding layout changes; preserve concurrent UI.

Decisive checks: actual hidden native ImGui field Load/failure, blocked owner
capture, negative C1 preview, recipe Undo/Redo/saved load, explicit Use and corrected
Run/Keep, negative compartments, cell Undo/Redo counts, exact project reopen/replay,
original-domain switch, corrected marker Preview/Apply/Undo, referenced-remove
refusal, stale fields, cancellation and replacement. Inspect one readable preview
capture and run the standalone app on the saved result with Windows-only PATH.
Recheck only changed native restore/corrected routes and affected GUI panels; no
binding/package/new data/dependency or segmentation qualification campaign.

First native checks pass2/2 (0.81s). The GUI suite catches a real metadata view
cache issue: Keep followed by immediate Undo can return to the cached pointer
before the UI observes the intermediate state, leaving the retained label stale.
Mark own metadata commands dirty and reconcile the loaded recipe's ID against
the current immutable list. Apply the same small fix to run/marker recipe lists;
retain last run ID for truthful Undo/Redo labels. Add explicit run-label Undo/Redo
assertions and failure state/frame diagnostics. No scientific/API/threshold change
or assertion relaxation. Preserve f03-gui-early-ctest.log; rebuild affected private
controllers/test and relink dependent GUI tests/app in the same cache.

After the cache repair, original fluorescence and marker GUI suites pass. The
correction test reaches reopened replay, original switch and marker Undo, then
its direct test-only open reintroduces both previously hidden windows in reverse
focus order. The retained failure frame shows the parent covering the child,
so clicks never reach saved-load/remove. Exercise the actual parent Dark/flat
correction button to open it, as the user does; assertions/product behavior stay
unchanged. Preserve f03-gui-ctest-r2.log and failure frame. Rebuild only the test;
passing native/default-panel checks and the unchanged app do not need repeating.

Cache owner process195808 is now terminal/missing; inspect confirms no active
build in this checkout/cache. Add a handoff mode to the native GUI driver that
stops before intentionally mutating fixtures, so its saved project remains
reviewable/replayable for standalone smoke. Normal CTest still executes all stale,
cancel and replacement guards. The concurrent build refreshed shared GUI source;
relink the affected default-panel tests with that library before final GUI checks.

Final GUI suites pass3/3 (5.96s), with affected native restore/corrected suites
already passing2/2 (0.81s). The native desktop course verifies negative preview
(-184), corrected C0/C1 nuclear sums1401/-1259, cytoplasm C1 mean2513/13-300,
Keep/Undo/Redo, exact reopened run replay, explicit original switch, corrected
marker Apply/Undo, stale/missing/refused-reference/cancel/replacement behavior.
Inspected signed preview is readable and correctly labels channel/domain/range.

Native handoff driver and standalone app both pass from System32 with Windows-
only PATH. Handoff skips deliberate fixture corruption so project/fields remain
reviewable/replayable; full CTest retains all guard cases. Input project/signal/
dark/flat hashes are unchanged (f03-gui-handoff/results.json). F03 is complete
under its declared subset:10/22 complete,12 pending. No biological/speed claim.

Waste review: immutable-pointer caches also need reconciliation after own writes
when immediate Undo returns to an earlier pointer. Retain loaded/last-kept IDs;
never reparse/hash all recipes per frame. Use actual parent commands in UI tests
when reopening tool windows; diagnostics caught the focus miss without weakening
assertions. Waited on verified process195808 instead of competing for the cache.
Preserve both failures; targeted repairs reuse native evidence. No bindings,
package, new material or broad campaign. For F04, settle native and private
consumer/test interfaces before a combined build; intermediate backend checks
must stay cheap, deferring GUI qualification until consumer interfaces settle.
Next F04: inspect the exact admitted native algebra configuration, then explicit
labeled finite linear mixing matrices, reusable factorization, signed derived
components/residuals, rank/condition refusal and source/recipe replay over the
shared channel source. Keep Python optional and restricted material excluded.

### F04 foundation: prepared native linear unmixing and channel source

Changed inputs: typed labeled mixing matrix, once-per-model thin SVD and shared
component/residual block evaluator; native source binding/grouping over existing
ChannelSource. Convention is measured = matrix*components, dimensionless matrix,
1..64 measured channels/components with full column rank, explicit condition
limit and signed output. Components precede labeled signed residual channels.
Nonfinite required inputs remain unknown; no fitted spectra or clamping. Resolve
sample dispatch once and reuse per-block scalar workspaces, not per-pixel solves
or allocations. Direct contiguous reads avoid a gather copy; sparse input groups
read no channel gaps. Only requested outputs allocate raster buffers.

Reuse installed/pinned OpenCV4.11.0 commit31b0eeea0b44b370fd0712312df4214d4ae1b158,
source SHA bb15d5c85bab38c1377c0c7dd7ff201b078bcdb41cc4ff9f85ac61aa56b5f631.
Selected static core library and four notices match the retained SDK manifest;
IPP/nonfree/Python and other optional runtimes are disabled (f04-native/backend.json).
No dependency/data download. Separate core-only algebra option from optional ML
training; reuse core when training already enables it. Keep its capability macro
source-local to avoid invalidating every native object. Shared exact integer
sample admission is extracted for F03/F04 without changing correction math.

Decisive checks: independent square/overdetermined mixtures and residual signs,
negative/zero/padding/nonfinite/overflow, rank and condition refusal, scaling,
row/column permutations, exact output subsets and tiled equality, sparse versus
direct reads, original/corrected source domains, work admission before pixels,
integer precision, cancel and source guards. Settle this native API before one
cached affected kernel/source build. No GUI/bindings/package or broad science
campaign. F04 remains open for source/recipe persistence, CLI/desktop and selected
consumer acceptance; GUI builds wait until those interfaces settle.


### Owner planning update: complete the remaining expansion contracts

Expanded the existing feature-expansion plan into ordered native/persistence/
consumer steps for all12 remaining items, preserving10 completed items and the
22-item canonical checklist. F04 foundation is in progress, not a completed
workflow. Sequence is F04, sparse storage/scale, open adapters, molecular methods,
SDK and final admission/release. A04 cannot use spots as substitute replicates;
unresolved data admission may defer its completion while independent SDK work
continues. Proprietary, NC and unreviewed materials stay distinct excluded
metadata/external pools, with derivative lineage checked at packet admission and
final staging. No restricted download, adapter or artifact move is authorized.

Validation: documentation diff and local link-target checks only; no product
build is warranted by this plan update. Waste review: reused the existing plan,
checklist and register rather than duplicating progress ledgers or reopening
delivered work. Existing native/UI changes remain untouched. Next implementation
action remains finish F04 native foundation and typed persistence/consumer path;
only its affected checks apply.


### F04 typed matrix record: exact native persistence boundary

Changed inputs: strict versioned ImageDataValue matrix encode/decode, shared by
future retained source recipes and consumers. Preserve binary64 coefficient bits,
channel indices/names/order, component names, input domain, explicit orientation
and condition policy without requiring numerical capability. Admit shape and
label limits before copying; refuse unknown fields/schema, wrong scalar types,
nonfinite coefficients and invalid indices. Decisive check: exact signed-zero
round trip through immutable native properties, maximum64x64 matrix admission,
and malformed-record cases, plus existing numerical/source and shared F03 tests.
One cached build of affected native tests only; no GUI, bindings or packaging.


F04 foundation evidence verified: initial native suites2/2 in0.35s and standalone
System32/Windows-only-PATH executable exit0; exact backend/library/notice receipt
exists. Typed-record extension builds successfully and affected suites pass2/2
in0.23s (f04-native/records-build.log and records-ctest.log). No product retry
or assertion relaxation. Current standalone receipt precedes this record extension.

Waste review: factor once, read actual selected channel groups, reuse workspaces
and allocate requested outputs only. Strict typed records reuse native properties
rather than introducing a second serializer/state store. Source-local optional
capability preserves core objects. One documentation script failed before writes
because Windows defaulted to GBK; explicit UTF-8 corrected it, without rebuilding
products. No GUI/bindings/package or new external material. Preserve unrelated UI
changes. F04 stays unchecked and10/22 complete. Next add retained source recipes,
owner capture/replay and parent-correction reference protection over these types,
then thin CLI and desktop integration after settling consumer interfaces.

### F04 retained native unmixing recipes and exact operator replay

Changed inputs: owner-captured native request, typed recipe retention/load/remove,
exact source/operator replay and correction-parent reference protection. Store
signal locator/series/identity, explicit matrix/domain, algorithm version, prepared
operator identity, input/derived source identities and optional retained correction
ID. A corrected request must reference a retained parent; removing that parent
refuses while an unmixing recipe uses it. Matches caches immutable properties so
ordinary polling does not reparse/hash parent recipes. Workers read owned sources
without Document access; owner publication checks live document/source/calibration
and parent again. Keep/Remove reuse metadata Undo. No new persistence store.

Add exact prepared operator identity (matrix, pinned backend/algorithm, inverse
coefficient bits and condition) once per model. Source identity moves explicitly
to qpx.linear-unmixed-source/2 and includes that operator; prior F04 foundation
receipts describe the prior source schema, with no accepted retained project to
migrate. Original TIFF/F03 identities and math remain unchanged. Replay refuses
changed operator identity rather than accepting numeric closeness.

Decisive checks: analytical original/corrected TIFF mixtures on a native worker;
exact typed/project signed-zero/source/operator/sample replay; single Undo/Redo;
parent removal/orphan/corruption/domain refusal; calibration/document replacement,
source/field changes, owner and cancellation guards. Run strict recipe schema
checks even without optional numerical support. Build affected unmixing,
correction and corrected-analysis native tests once in the existing cache after
interfaces settle; no GUI/bindings/package or repeated biological campaign.

First build compiles the native service, then fails in the new test: raw_metadata
returns a shared pointer, but the fixture used value member access. Correct only
the test to dereference metadata; preserve workflow-build.log. Rebuild affected
targets in the same cache. No contract/assertion change or broad rerun.

Native retained workflow and affected regression suites pass4/4 in0.78s after the
test-only pointer correction. Reuse that service evidence. Extend the same packet
with strict bounded native JSON/file IO over the typed matrix schema and thin
CLI Keep/list/verify commands, including explicit retained correction selection.
Keep copies coefficients into the recipe; replay does not depend on the authoring
JSON remaining present. Guard matrix/project/field inputs and SQLite sidecar
output paths, cancellation and native save budgets. No Python parser or second
recipe authority. JSON schema/duplicate/depth/size/type and exact signed-zero
round-trip checks run before optional numerical support; full CLI tests require
the enabled native backend and project support. Build changed workflow/CLI tests
and actual batch executable only, then check those routes; prior correction and
kernel suites need no repeat after IO/consumer-only additions.

The test driver also supports a handoff mode that stops before deliberate fixture
mutation, preserving a reviewable source/project for the actual batch executable.
Normal CTest retains every stale/source guard. Check actual CLI list and original/
corrected verification from System32 with Windows-only PATH against handoff
identities; input hashes must stay unchanged. This is consumer mapping/runtime
evidence, not a new science campaign or GUI/package qualification.

The CLI build gate found live shared-cache GUI build process145116 (MSBuild190900,
CMake regeneration174240). No competing build started. Finish documentation and
inspect that same live handle before starting the planned native CLI targets.

CLI build starts only after process145116 is confirmed missing and the cache gate
is clear. Matrix IO/CLI suite passes1/1 in0.47s; prior native service/regressions
remain4/4 in0.78s. Actual System32/Windows-only-PATH handoff driver and batch
list/original/corrected verify/Keep/new-project verify all exit0. Native source/
operator identities match and input hashes stay unchanged
(f04-cli-handoff/results.json). Valid synthetic projects/sources remain reviewable;
normal CTest retains intentional mutation guards. No bindings/GUI/package/new
material or new scientific/performance claim.

Waste review: factor/hash prepared operators once; use typed recipe authority
under JSON/CLI, retain coefficients by value and reference correction metadata
once. Reuse source/Undo/project-save services and prior kernel/regression evidence.
One redundant read-only corrected CLI verification was used to fetch a parent ID;
reuse its earlier report next time. The test pointer compile error cost one cached
repair; preserve evidence and inspect API return types before writing fixture
access. No competing cache or broad validation was introduced.

Next F04 packet: native fluorescence/marker integration for unmix outputs, then
desktop matrix authoring/preview/selection in one settled consumer build. Capture
source authority against an original measured channel while selecting component
outputs against derived metadata; reject ambiguous channel-specific ROI planes.
Add a distinct retained unmix reference, domain and versioned run/cell recipe;
protect its removal while cells/runs use it and preserve original/corrected
recipe identities. Marker cohorts/rules must explicitly distinguish all three
domains. Reuse existing segmentation, compartments, jobs and publication, with
independent signed component moments and exact project replay. Do not relabel
components as original channels or start a second algorithm. F04 stays unchecked,
10/22 complete,12 pending.

### F04 native unmixed cells and recipe-scoped marker rules

Changed inputs: source-bound cell capture now accepts retained native unmix
requests, selecting component/residual axes independently of measured-channel
source authority. Require channel-independent ROI planes; reuse segmentation,
compartments, contours/jobs and publication. Add distinct unmix references and
version3 run/cell provenance while preserving original/corrected hash/recipe
paths. Refuse unmix deletion while cells, run records or retained marker rules
reference it; parent correction protection remains transitive.

Unmixed marker rules require a recipe identity because C1 can mean different
components for different matrices/orders. Version2 typed/JSON rules add that
scope; legacy rules remain version1 with unchanged encoding. Cohorts exclude
intact other recipes, but preserve corrupt/unknown matching-domain publications
for explicit refusal. Native capture/verification checks scoped metadata without
rereading pixels; cache unchanged immutable properties and skip the new optional
metadata path on default requests. CLI detection adds explicit --unmix, mutually
exclusive with --correction, and protects parent field outputs. Marker CLI uses
the shared rule-scoped cohort helper. No second scientific algorithm or Python
implementation. An edit guard caught pointer-style selector spelling before a
CLI edit; inspect the actual source and apply the remaining edit, no build retry.

Decisive check: independent14 nucleus/27 body geometry, DNA sum1541(original)
and1401(corrected), signed Marker nucleus sum-141/body mean-2/cytoplasm sum87,
residuals and reordered/output-only axes; exact record/cell/project/marker replay,
Unknown samples, wrong recipe/order, orphan/corrupt/reference/Undo, work/cancel
and stale-field guards. Native/CLI equal-result and output protection checks plus
one retained real F01 digest protect legacy identity. Settle these interfaces
before one cached affected native build; defer GUI qualification to the combined
F04 desktop controls. No bindings/package/new data or repeated biological pilot.

First affected build and all8 native/CLI suites pass in1.92s, including legacy
original/corrected routes. Review found a concrete work-accounting edge: excluding
other unmix recipes hashes their geometry to ensure corrupt publications are not
hidden, so admit that temporary workspace before integrity checks and pass the
CLI work budget to the cohort helper. Add native job stale-association delivery
and establish both routes current before the deliberate field-mutation check.
Rebuild only affected marker/unmixed consumers and actual CLI; rerun their tests,
reusing the already passing unchanged segmentation/recipe regression suites.
No failed build/test, relaxed assertion or repeat biological campaign.

Final native/CLI checks pass: first8 suites1.92s; after cohort work accounting and
job-stale coverage, changed2 suites0.58s. Native handoff driver and actual batch
verify/scoped marker preview/unmixed detect-save-replay/marker Apply all exit0
from System32 with Windows-only PATH. Run/marker identities match native
expectations and input hashes stay unchanged. One retained real F01 replay keeps
exact run4deb4bb86e9bb65fe6aae3ead3a83ab89878a7edee488ab53e1f92dbf95bb67a
and118 cells. Receipt: f04-cells-handoff/results.json; valid synthetic projects
and fields remain reviewable. No new scientific/performance claim.

Waste review: reuse existing kernels/jobs/publication, store one source reference,
leave legacy identity paths unchanged and bind marker meaning to a recipe rather
than inferring component meaning from C indices. Cache optional scope metadata;
only hash other recipes when needed to exclude intact publications and admit
that temporary work first. Reuse passing native/regression evidence after the
small scope-helper change. No build/test failure, bindings, package, new external
material or repeated biological campaign. Update help alongside consumer edits
before the build; the late help-line fix was included by the final batch target.

Next F04 desktop: settle private matrix tool and fluorescence/marker controller
test interfaces before a combined GUI build. Use bounded native JSON/file IO and
prepared model/source services, preserve explicit matrix orientation/channel
labels/domain/condition, signed component/residual preview, retained-source Load/
Keep/Remove/Undo and exact saved replay. Select original/corrected/unmixed sources
explicitly; preserve the existing correction/default action semantics. Marker
controls need all three domains and retained unmix recipe scope, using the native
rule cohort helper. Open fields/read previews asynchronously and keep owner
Document capture separate; do not factor/hash/parse recipes per frame. Preserve
public GUI Pimpl layouts and independent UI edits. Existing wheels/SDK remain
frozen; changed native interfaces get matching optional bindings/SDK at their
settled P01 batch. F04 remains unchecked:10/22 complete,12 pending.

### F04 desktop completion: prepared matrix tool and explicit derived sources

Changed inputs: private compiled matrix tool, fluorescence source selection/
replay and marker scope controls. Worker loads bounded matrix JSON, prepares one
SVD and opens selected correction fields; owner captures Document when unblocked
using a prepared-model native request/restore overload. Native preloaded cell-
record replay reuses that source without reopening fields or solving again.
Keep public GUI Pimpl layouts and independent UI source edits intact.

Matrix drafts expose explicit measured index/name, component label, coefficient,
input domain and condition policy. New drafts are zero matrices requiring explicit
full-rank coefficients, not estimated spectra. Applying invalidates previous
preview; no parsing/hashing/factorization each frame. Preview reads owned sources
asynchronously, shows signed components/residuals, crop/Z/T, validity/finite counts
and presentation-only display range. Keep/Remove/Undo/load use existing native
recipes. Cached immutable record lists reconcile own writes and immediate Undo.

Fluorescence chooses original/corrected/unmixed explicitly, preserves old correction
actions, labels derived indices honestly and uses the same cell pipeline. Saved
unmixed Replay opens/prepares its source asynchronously before native owner
replay and the existing job. Marker UI adds three domains and explicit retained
unmix scope, using the native rule cohort helper; switching domains clears scope.
No Python/binding/package/new material or alternate scientific authority.

Decisive checks: actual native ImGui malformed/singular Load, coefficient edit and
Apply, blocked owner delivery, signed preview90 to-15, matrix/source recipe Undo/
Redo/load/remove, explicit Use/Run and signed compartments, cell/run Undo/Redo,
exact reopened asynchronous Replay, original switch, marker scope combo and
Preview/Apply/Undo, referenced removal refusal, field change, cancel and same-
address replacement. Preserve failure frame/state diagnostics. Inspect one signed
preview capture and run standalone app/native CLI against the valid saved project
with Windows-only PATH. Native prepared-operator/recipe mismatch and existing
F04 cell/source tests protect the changed trust boundaries; existing fluorescence/
correction/marker GUI suites protect default routes. Settle these interfaces,
then one affected cached build, no bindings or repeated F01 science campaign.

Build gate first found live GUI cache owner177624, then MSBuild196288. Both
handles are now missing; inspect the full cache gate before starting our batch.
No competing cache/build was started. Prepared interfaces and failure diagnostics
are settled; reuse any compatible GUI objects produced by the other build.

The first batch compiled core, then native workflow test linking failed with
LNK1181 opening Release/qupathxx_core.lib. A competing GUI cache build195472 was
live during failure; the core library now exists (196447966 bytes). Preserve
gui-build.log and inspect live processes/file state before retrying. No source
contract or assertion change is indicated by this link/cache failure.

Cached retry succeeds after core library is present and competing handles are
missing. All6 affected native/GUI suites pass in10.20s (gui-ctest.log), including
prepared model identity/mismatch, unmixed cells, actual new desktop course and
legacy fluorescence/marker/correction GUI routes. No source/test repair needed.
Inspected signed preview is readable and correctly names Marker output1 near-15,
input/output domains, crop10,7 /3x1 /Z0T0,3 finite/valid and display range-32..128.

Native handoff driver, standalone app and CLI list/verify all exit0 from System32
with Windows-only PATH. Saved project replays one cell with exact run SHA
d86425f01c748565438c171ff1095b1aec03c9800230e0d933e3c43082db76a0; input project,
fields and authoring JSON hashes are unchanged (f04-gui-handoff/results.json).
Handoff skips deliberate field corruption; full CTest retains those guards.
F04 is complete under the declared subset:11/22 complete,11 pending.

Waste review: reuse a worker-prepared model during owner capture and saved replay
instead of reopening fields or factoring twice. Reconcile cached retained IDs
after own metadata writes/Undo; keep source choice/marker scope explicit. No
per-frame parsing/hashing, bindings/package/new material or repeat biology check.
The transient cache link failure cost one retry; a process check is an observation,
not atomic ownership across independent conversations. Preserve failure/state
evidence and serialize the full compile-through-link window through one build
owner when coordinating authorized tasks. No second cache or lock framework.

Next D01: inspect current native expression/count/import persistence interfaces;
add a bounded ExpressionMatrixSource and transactional SQLite sparse sidecar with
gene-oriented queries, stable IDs, exact uint64 counts and source/chunk identity.
Reuse admitted full197MB Matrix Market data, native parsers/SQLite and existing
import/gene-view controllers. Keep old selected-gene projects readable without
forced migration. Finish native indexed read/import/interruption/source/relink
contracts before thin consumers; prove unprojected gene reads/reopen without
rescanning or densifying the source. Settle native/consumer/test interfaces before
an affected cached build. Preserve independent UI edits and restricted-material
separation; final L01/P01/P02 remain open.

### D01 foundation: native streaming indexed expression storage

Changed inputs: bounded ExpressionMatrixSource API, native Matrix Market/axis
stream importer, SQLite gene-oriented store and paged catalogue/sparse cohort
reads. Reuse existing pinned SQLite, native file guards/hash/atomic export and
shared integer/identifier lexical checks; preserve selected-gene parser limits
and identities. Qpx headered one-column axes and explicit Tenx ID/name/type plus
barcode TSV are separate declared profiles. Stable IDs need not have unique
display symbols. No guessed coordinates or new reader/dependency/material.

Stream each original input once with64KiB buffers and bounded4096-byte lines.
Keep uint64 counts as eight-byte blobs; reject duplicate coordinates rather than
summing or converting through double. Private staging may discard on any error;
only a completed, flushed new-only sidecar is published using existing atomic
export. Its source identity binds input/profile manifest and exact sidecar bytes;
per-gene hashes cover sorted original entries/axis identity. Full source metadata
is paged, not loaded eagerly. Cohort reads verify each selected sparse gene before
filtering, never scanning other genes or allocating a full dense matrix. Query
file versions and refuse SQLite sidecars; hash a store once on open/verified
relink, then retain an immutable source for indexed reads without original text.

Decisive checks: independent unsorted sparse/cohort/zero values,2^53+1/uint64-max
precision, stable repeated-symbol IDs, bounds/type/chunk/digest corruption,
input-offline reopen and exact relocated identity; malformed/truncated/duplicate
inputs, cancellation/stale input during staging, byte/work limits and no partial
publication/leaked private staging. Affected existing spatial/expression tests
protect lexical/default compatibility. Then one native real197229475-byte,
32285-feature/2702-observation/16031101-entry import: every retained six-gene count
must match the independent prior projection, plus an unprojected gene read/reopen.
Actual real source and admitted SQLite already exist; C/D have ample verified
free space, so do not redownload. Record bounded-work/store/import observations
without a universal speed/RSS claim. No GUI/bindings/package or science sweep.

D01 stays open for project-relative/owned associations, verified relink and
backed import/gene-view consumers after this native contract settles. Current
canonical count remains11/22 complete. Next build affected native targets once
in the shared cache after checking live owners; preserve independent UI edits.

First native build succeeds. Existing spatial/expression suites pass; the new
suite reaches its source-change interruption case but the observer never mutates
the input (failed fixture has unchanged final bytes). Waiting for staged SQLite
file growth misses the short final commit/copy window because pages stay cached.
Preserve ctest.log and expression-matrix-2243487946275800. Start the watcher
before import, observe private staging creation instead, verify its write and
assert it fired. Assertions remain strict: actual cancellation/source change must
refuse and leave no published store. Rebuild only the test and rerun its suite;
reuse the unchanged passing spatial/expression results. No product repair indicated.

The corrected observer fires, but its write is denied by the actual Windows
ReaderFileGuard (FILE_SHARE_READ only). This is a protected unchanged input, not
a stale import; preserve ctest-r2.log. Use a native append attempt and distinguish
ERROR_SHARING_VIOLATION from a successful mutation. A blocked write must preserve
input bytes and publish an exact complete snapshot; actual changes/cancellation
must refuse with no output. Verify the action fired in both cases. POSIX still
exercises actual mutation. Translate ReaderFileGuard version failures into the
workflow's typed stale error. No weakened invariant, sleep-based workaround or
platform-wide rerun; rebuild affected new source/test only.


### Planning refresh: remaining features and material separation

Owner requests a plan covering the outstanding features while keeping proprietary
and NC material separate. The canonical feature-expansion plan retains22 items,
11 complete/11 pending, and its ordered remaining-item/API/acceptance breakdown.
Replace obsolete delivered F01/F02 execution instructions with D01's actual next
slices: inspect current build evidence, affected native acceptance, one retained
full-source proof, project association/relink, then backed import/gene consumers.
The uncommitted native foundation alone does not close D01. Preserve concurrent
UI edits and completed science evidence.

Proprietary, NC/research-only and unreviewed references retain distinct registers
and external pools. No restricted downloads, adapters, build paths, training,
fixtures or open-release acceptance are authorized; derivatives retain source
lineage. Incremental admission belongs to each consuming packet, final staged
membership to L01/P02. No new pool, dependency or runtime licensing gate.

Waste review: removed stale delivered execution instructions instead of adding a
second checklist. Documentation-only change: diff/link/count checks, no build,
package or repeated scientific campaign. Next implementation remains D01 under
its existing native current card; this planning refresh claims no new test pass.


### D01 native foundation acceptance and next association slice

Third native build completes; expression_matrix passes0.42s (ctest-r3.log).
Reuse unchanged passing expression/spatial suites. One full native run exits0
from System32 with Windows-only PATH:32285 genes,2702 observations,16031101 entries;
all2702x6 prior counts equal,14550 nonzeros, and an unprojected gene reopens exactly.
Store354279424 bytes; import40426.1ms; peak accounted139078440 bytes under256MiB.
Input hashes before/after agree. d01-real/real.json and results.json plus the
shared feature-expansion receipt hold evidence. No speed/RSS or GUI claim.

Waste review: observer proves action and distinguishes Windows-denied writes
from mutations. Full-run orchestration first failed on copied environment-key
casing before native launch; corrected run imports once. A documentation script
needed explicit UTF-8 for Windows; retry preserved the already changed header.
Preserve failures and reuse fixtures/passing tests; no GUI/bindings/package or
restricted inputs. Header now distinguishes fingerprinting from expected-hash
comparison. No further product rebuild for this documentation change.

D01 stays unchecked;11/22 complete. Next settle typed project-relative/owned
expression associations and verified relink through existing project commands,
stable axis IDs and explicit optional coordinate joins. Then connect backed import
and gene selection with paged reads and existing jobs. Preserve old selected-gene
projects and independent UI edits; settle contracts before one affected build.

### D01 project associations: relative/owned paths and verified relink

Changed inputs: typed native association storage in existing ImageDataProperties,
project serialization/open hooks and shared native Keep/load/reopen/relink/remove.
Stable source identity excludes locator/ownership. External paths are absolute
live and relative on disk where possible; owned records use the canonical child
.qpx-expression/<file digest>.qmx. Owned retention requires a saved project.
Copy owned data once before destination project commit, including Save As and
snapshot save, reusing exact existing copies and refusing conflicts. Ordinary
same-location saves preserve bytes without rehashing entire stores. Remove/Undo
never deletes shared sidecars; a failed project commit may leave a complete
unreferenced copy, never an advertised incomplete store. No coordinates guessed.

Decisive checks: exact >2^53 counts through external/owned project persistence,
relocated directory trees, exact relink/Undo, missing-source preservation/refusal,
owned copy/Save As/async snapshot independence from originals, corrupt destination
and cancellation refusal, malformed path and source/alias output protection.
Existing project and matrix suites protect serializer/default compatibility.
Settle these contracts, then one affected cached native build after live-owner
gate; no GUI/binding/package/full real-matrix import repeat. D01 remains open for
spatial joins and backed import/gene consumers; preserve independent UI edits.


D01 project slice passes: cached native build succeeds; expression_matrix_project,
expression_matrix and existing project suites all pass in1.22s (project-ctest.log).
Exact uint64 values survive external/owned relocation and native async Save As;
relink/Undo, missing external sources, conflicting copies, changed identity,
malformed owned locators and source/hardlink-output protection pass. Current native
APIs are in expression_matrix_project.hpp. No coordinate association, GUI/import
integration, installed SDK or optional binding completion is implied.

Waste review: live shared GUI build195752/MSBuild174784 initially blocked our
cache gate; no competing build started. Their handles disappeared, then the full
gate cleared and one affected cached batch passed without repair/retry. Ordinary
owned saves skip whole-file rehash/copy; new destination copies hash while streaming
and reuse independently verified existing bytes. Keep/remove/Undo never deletes
shared content-addressed files. Final D01 count stays11/22. Next implement explicit
observation-ID/coordinate association and selected-gene snapshots over the matrix,
then use existing import/gene controllers; no full matrix import repeat.

### D01 spatial ID join and backed native gene-view session

Changed inputs: batched native observation-ID lookup, immutable explicit spatial
row/frame snapshot, bounded selected-gene projection and the existing GeneViewSession
worker. One connection/prepared statement joins ID batches, avoiding per-point
open/hash/lookup overhead. Count data remain solely in the matrix; materialized
point properties store source/barcode/frame identity. Capture checks image frame,
calibration and plane. Project matrix removal refuses referenced observations.
The native gene session uses its existing generation/coalescing/cancel mechanism
for indexed reads and existing legends; no new scheduler or Python logic.

Decisive checks: deliberately scrambled observation/object/feature order, exact
2^53+1 counts and measured sparse zeros, missing/duplicate IDs and invalid frame,
empty cohort/work bounds/cancel, changed store, point/source/frame save/reopen,
reference removal/Undo and backed gene switching/comparison/replacement. Reuse the
full real matrix import receipt; do not repeat import or new material downloads.
Build affected matrix-spatial/project/matrix, gene-view and expression suites once
in the shared cache after live-owner gate; no GUI/binding/package. Native consumers
are a foundation; backed import and actual desktop catalogue remain D01 work.


First spatial batch and all5 affected suites pass in1.16s. Boundary review then
found that a projected ExpressionDataset must bind the full typed coordinate
frame/units, not only a frame-name string plus matrix digest. Bind its source
lineage to the spatial snapshot digest and expose dataset_frame() for existing
RegistrationRequest's source/frame consistency contract. Add an independent same-
numbers/different-units identity check and a registration-field reuse check.
Rebuild only changed core/new test, rerun the new suite; unchanged passing matrix,
project, legacy expression/gene-view evidence remains reusable. No gate relaxed.


D01 spatial slice passes: one initial affected cached batch/all5 suites1.16s,
then changed spatial identity/registration checks pass0.13s after a targeted rebuild.
Spatial matrix, matrix project/storage, legacy expression and gene_view results
are retained in d01-native/spatial-ctest*.log. Exact scrambled joins, >2^53 raw
counts, sparse zeros, native comparisons/coalescing/replacement, offline text,
point/source/frame project replay and reference protection pass. Typed units and
coordinate provenance now bind the projected lineage; dataset_frame() matches
existing registration's source identity. No scientific/count contract was relaxed.

Waste review: batch ID joins use one connection/prepared lookup, and selected
projections share immutable coordinates/source and existing display jobs. Count
values are not duplicated into point facts; selected-gene switches do not fingerprint
or parse the original matrix again. A post-build boundary review justified one
small identity repair/rebuild; unchanged passing suites were reused. No full real
import, GUI/bindings/package, new data/model/dependency or restricted input.
Next finish backed import preview/publication and actual desktop paged catalogue/
gene controls. Reuse this native authority and the retained full real store;
D01 remains open and the canonical total stays11/22 complete.

### D01 native indexed import: shared base/parser/job and first-save ownership

Changed inputs: shared native image/project base selection and position TSV parser,
indexed preview/publication and overloads on the existing SpatialImportJob. Retain
old selected-count request/preview layouts and behavior. Preview validates the full
indexed source, joins explicit barcodes/frame/units/plane and creates no files;
publication creates count-free point facts and a single atomically new project.
Separate coordinate scopes keep overlapping barcodes distinct. Reuse one count
association, upgrading ownership only by exact content when explicitly selected.

Owned imports from a new image now keep a deferred absolute origin in unpublished
native state. The first save copies/verifies a canonical project child before
project commit and serializes only the relative child. This avoids a preliminary
project save or second publication. Unsaved recovery resolves its captured owned
origin and first primary save moves ownership to the selected destination. Saved
projects retain the existing relative/owned semantics. Original data are preserved.

Decisive checks: scrambled >2^53 values, preview without files, first-save owned
copy/reopen with original store offline, shared parser/default compatibility,
explicit calibrated transform, existing project/async import, distinct overlapping
coordinate scopes, ownership upgrade, stale/malformed/unknown coordinates, bad
expected hashes, plane/bounds/budget/cancel/output/copy conflict refusal and unsaved
owned recovery to first save. Then one native real backed import over the retained
full store/base/positions: every prior six-gene count and coordinate exact plus
unprojected native gene comparison. No repeat raw-matrix import or download.
Settle these interfaces and test driver before one affected cached native build.
No GUI/binding/package; actual desktop catalogue/import controls remain D01 next.


The first import batch compiles core successfully but the new test cannot find
nlohmann/json.hpp because that dependency is intentionally private to core.
Preserve import-build.log. Link the test explicitly to the existing pinned JSON
target, and keep backed import source/test under the same native project feature
as the existing spatial importer. This prevents a new JSON/base-service requirement
in project-disabled builds. Reuse compiled core; retry the pending affected targets
only. No source validation/assertion or license/dependency version change.


Corrected import build succeeds. All4 affected existing/parser/project/spatial
suites pass1.52s (import-regression-ctest.log). Pre-test review found that an
already-published output must refuse before checking an original input later taken
offline, matching the old import route and avoiding needless input work. Reorder
that cheap collision check; rebuild only the backed target and run its focused
suite. This is an output-error ordering repair, not relaxed source verification.


Backed import passes0.30s (import-ctest.log); the4 affected legacy/parser/project
suites pass1.52s. The actual real handoff exits0 from System32 with Windows-only
PATH using the retained full store/base/positions: all32285 genes preserved,
2702 coordinates/barcodes and every prior six-gene value exact, unprojected native
gene comparison/reopen works. All5 input hashes remain unchanged. d01-import-real/
real.json and results.json bind source/spatial/result/executable and output project
identities. One owned data copy is retained with this reviewable real project.
No repeat197MB raw import, new download/dependency, GUI/bindings/package or biology
campaign. No process-RSS/speed claim follows from this native handoff.

Waste review: reuse the same parser/base/job/publication authorities; defer first
owned copy to atomic save instead of a preliminary project save. Prepare shared
point frame facts once, keep values in the matrix, and separate coordinate scopes
rather than duplicating matrices. The missing test-private JSON edge cost one
cached retry; a pre-test collision ordering repair cost one targeted rebuild.
Unchanged passing suites were reused; failure logs remain. Actual desktop import
and paged gene catalogue are now the remaining D01 work. Preserve independent UI
edits, settle native catalogue/control boundaries, then one affected GUI/app/test
batch. Reuse this owned real project for desktop proof;11/22 stays canonical.

### D01 desktop indexed import and gene catalogue

Changed inputs: bounded native ID/symbol search pages, immutable project source-
open capture and scope enumeration, existing import-worker open/index/catalogue
operations, prepared-source preview reuse, and private desktop controllers.
The wizard chooses selected versus full indexed storage, opens an existing store
or indexes Matrix Market/explicit axis files on its native worker, previews exact
ID joins, then publishes/opens through existing controls. Keep legacy request/
preview/GUI public layouts; preserve independent styling edits in shared files.

Gene display captures only locator/identity on owner, opens on worker, keeps the
immutable matrix across gene changes, and uses the existing native gene session.
Catalogue pages/search coalesce to latest input; two equal queries share a page.
Scope selection stays explicit; saved typed frame/plane/calibration checks remain.
Exact Save As/relink must refresh runtime source handles even when scientific
snapshot identity is unchanged; otherwise later reads may target the old locator.
No full catalogue materialization, per-frame source fingerprint or Python logic.

Decisive checks: native literal/case/page bounds, immutable worker open and explicit
scope listing, same-identity/new-locator gene reads; actual wizard raw indexing,
prepared preview reuse, Import/Open buttons and full-source persistence; actual
gene catalogue paging/selection, exact values, live owned Save As and stale-store
refusal. Existing selected-gene GUI/registration/default routes stay covered.
Use retained real owned store/project for a native desktop handoff and inspected
scientific legend/coordinate rendering. No raw197MB matrix re-import, new material,
binding/package or broad biology campaign. Settle all consumer/test interfaces,
then one affected cached core/GUI/app/test batch after live-owner gate. Retain
failures and update this card with actual waste/results before D01 closure.


Initial combined cached build succeeds. All4 affected native/GUI suites pass3.78s:
backed import, spatial matrix, legacy gene GUI and actual indexed wizard/gene panel.
Checks click Import/Open, page through40 genes and choose an unprojected feature,
verify exact native values, follow owned Save As with old locator offline, and
show changed-store error. Source preview reuses the opened pointer. No assertions
or scientific tolerances changed. Native source requests/caches remain immutable.

For the real desktop handoff, extend the existing gene GUI driver to select the
native indexed scope and known unprojected/marker IDs, capture actual comparison
and workspace persistence without repeating count oracles. A final small controller
cleanup compares query text rather than unused input-buffer tails when sharing a
page, and avoids copying the project path every frame. Build affected GUI drivers/
app together, retain initial passing evidence and run the real paths once from
System32 with Windows-only PATH. Reuse the owned real project/store and inspect
one scientific capture; no raw-matrix indexing repeat, bindings or packaging.


D01 desktop handoff passes: final cached GUI/app build succeeds; real import driver,
full-app comparison driver and standalone app all exit0 from System32 with Windows-
only PATH. Retained real store/base/positions hashes are unchanged; all38 binding
objects match baseline. d01-gui-real/results.json binds commands/executables/project;
real-indexed-result.json binds the2702-observation scope, comparison and unchanged
project. Inspected preview shows2702 mapped spots/all32285 features. Comparison
shows aligned unprojected/marker maps on shared0..4.45435 log1p(counts),2451/577
zeros and0 missing. PNG conversion preserves exact PPM RGB. No raw reindex,
download, binding/package or broad science campaign. No biology/speed/RSS claim.

D01 is complete under its declared local indexed-count/explicit-coordinate scope:
12/22 complete,10 pending. Native relink/Undo is delivered; desktop index/import,
paged gene selection, Save As and error handling are delivered. Optional bindings/
SDK/release remain P01/P02 and final licensing membership L01. Independent UI edits
stay preserved; this is local source-scoped evidence, not a frozen release.

Waste review: one settled core/GUI/app batch passed; one handoff-driver/controller
cleanup batch added real capture and removed redundant query/path work. Reuse native
science/real store instead of repeating197MB indexing. Scope-preserving staging
keeps concurrent styling outside this feature commit. Next D02: inspect bounded
value/color/tile caches and display-only reuse; qualify100k observations/30k genes/
5M entries under256MiB accounted work, then extend existing tiled cell jobs with
fixed global parameters, seam ownership/global IDs and checkpoint/resume. Use
existing authorities and an admitted ROI, no new scheduler or F01 biology replay.

### D02 — selected-value reuse

The native gene session rereads counts for palette/opacity/transform edits even
when its immutable selected projection is unchanged. Reuse only the exact selected
feature order and spatial identity; source rebinding still discards the projection.
Check source currentness before reuse and before publication. One selected snapshot
is retained; no additional value-cache history or scheduler is introduced.
Changed-input check: build the existing gene-view and spatial-matrix native targets
in the shared cache, then run only those two suites. The spatial test verifies
shared count-buffer ownership, changed presentation, exact uint64 values, gene
switch invalidation and refusal after a populated backing store changes. No GUI,
raw real-matrix import, bindings, package or broad science campaign is required.

D02 selected-value slice passes: cached build succeeds; spatial-matrix and gene-view
suites both pass (0.28s total). Tests prove shared immutable count-buffer reuse,
changed rendered colors/transform, exact >2^53 counts and changed-source refusal
after cache population. Waste review: one affected build/test batch, no failures,
new dependencies, raw indexing, GUI/bindings or packages. Keep12/22 complete.
Next: native100k/30k/5M scale fixture under256MiB accounted work, remaining cache
accounting/coalescing, then bounded tiled cell seam ownership and resume.

### D02 — native sparse scale qualification

Previous slice is committed as7d9a1279. Add one explicitly invoked native scale
executable (not a default ctest fixture campaign): generate100000 observations,
30000 features and5000000 distinct sparse pairs spanning every observation/feature.
Declare256MiB for import/open/read/projection/view calls before execution. Compare
all selected counts/implicit zeros against an independent50-feature row formula;
exercise high-axis metadata, selected-value reuse, latest-generation cancellation
and active indexed-read cancellation. Report accounted import/retention separately
from timing; no allocator/RSS or universal speed claim. Reopen mode reuses the
fingerprinted store and cannot overwrite accepted receipts. Build only this target
in the existing cache, then generate/run once; preserve failures if any. No new
reader/data/dependency, Python product logic, binding or package work.

Scale slice succeeds on HEQPC from System32/Windows-only PATH. New native driver
build and one generated run pass:100000 rows,30000 features,5000000 entries,
135276000-byte accounted import peak;194971982-byte conservative retained view
accounting, both below268435456. Exact formula oracle covers all300000 selected
sparse values/zeros and200000 projected counts. Recolor reuses counts; latest
request wins after cancel, active read observes cancellation0.475ms this run.
Import6168ms, comparison106ms, recolor52ms are local observations, not guarantees.
Waste review: one build/one generated run; no failures/downloads/repeated real
import. Fixture uses~188MB total including indexed store; preserve/reopen it.
Default ctest never regenerates it. Guard the optional scale target with project
storage so disabled-storage builds acquire no JSON dependency through this test.

Next blocker found by scale evidence: native coordinate join1855ms is still done
on the desktop owner and GUI gene session uses64MiB. Factor owner capture into an
immutable request and submit its join using existing SpatialImportJob; expose/use
the declared bounded desktop budget, then test stale scope/cancel/large interaction.
D02 stays open; tiled cells/halo/seam/global IDs/checkpoint-resume remain afterward.

### D02 — asynchronous spatial joins and desktop budget

Scale evidence justifies moving the coordinate join off owner. Simpler route than
adding a SpatialImportJob operation: capture an immutable native spatial request
and let the existing GeneViewSession worker resolve it before selected values.
No new scheduler/thread; existing latest-generation cancellation/publication also
covers joins. Bound request retention and use the smaller request/session budget.
Desktop capture copies only document geometry/provenance; desktop gene session
uses the already-declared256MiB. Native snapshot capture remains synchronous for
existing explicit callers. Build native spatial/gene/import tests, scale driver,
affected GUI drivers and app together; run affected suites once and --reopen the
retained scale store to measure owner dispatch and worker join without reindexing.

The asynchronous-join batch passes: one cached combined build of three affected
native suites, scale driver, two GUI drivers and app. Five affected suites all
pass3.75s, including actual indexed selection/Save As, legacy viewing, immutable
capture surviving later document edits, identical request reuse, cleared join
rejection and changed-source refusal. No tolerances/assertions weakened.

Reopen the original114733056-byte scale store once (no regeneration/index/copy):
request construction6.926ms, owner dispatch0.083ms, worker join+comparison2025ms,
recolor55.868ms, active read cancellation0.594ms. Conservative retained accounting
202073662 bytes including request, below256MiB. Host/commands/executable/baseline
receipt hashes in d02-join/host.json; reopened values/block/source identities agree
with original scale evidence. This proves native dispatch, not full100k GUI FPS.
Waste review: reuse GeneViewSession rather than extending another job; one build,
five passing targeted suites and one retained-store reopen, no failure/retry,
new dependency, download, binding or package. Preserve concurrent UI changes.
Next D02: settle viewport/color-cache accounting/coalescing, then existing raw-cell
kernels/jobs → fixed threshold/normalization, halo/seam ownership/global IDs and
source-bound checkpoint/resume with independent admitted ROI checks.12/22 remain
complete; D02 is still open. Do not replay F01 biology or full-matrix indexing.

### D02 — finish bounded display work without duplicating tile services

Source inspection: GuiTiles already has64MiB GPU LRU, TileScheduler64MiB decoded
LRU/64MiB active reservation,256 pending-request ceiling/1024 cache-entry ceiling,
192 planned viewport tile ceiling, duplicate coalescing and generation rejection.
Keep existing cache/scheduler behavior/evidence; no replacement implementation.
Gene session retains one selected projection/two fields; independent serial
GeneDisplayCache has byte/entry eviction. Actual duplication: shared automatic
range computes each transform/color twice and sorts unused per-gene percentiles.
Prepare transformed fields once, pool range once and paint once; skip duplicate
validation of known iota rows (still validate every explicit cohort). Radius-only
changes use existing colors without a generation/job. Preserve final manual-field
identity, raw values, pooled percentile policy, clipping/colors and cancellation.
Decisive checks: one cached native/GUI/app build; expression/gene/spatial and GUI
suites, using existing bounded-cache/cohort assertions plus explicit shared-field
identity equivalence/radius no-job checks. No scale fixture reimport, binding or
package work. Existing scale/accounting evidence remains source-scoped; do not
rerun its whole pipeline merely for a timing refresh.

Display/cache slice passes: one cached build, all5 affected suites pass3.50s.
Pooled percentile/minmax, missing/constant cases, final manual request identity,
colors/transform/clipping, explicit cohort validation, byte/entry eviction and
legacy/indexed GUI routes remain correct. Radius-only changes keep result identity
and generation without queuing work. The new shared path prepares/transforms and
paints once per field; only pooled percentile sorting remains. No scale indexing,
binding, download, package, source snapshot or UI style work.
Waste review: source inspection reused bounded tile services instead of replacing
or replaying them; one build/test batch, no failure/retry. All remaining D02 work
now belongs to tiled cells and its scientific/persistence/consumer evidence.

Tiling constraint found: F01 context is smoothing-only. Use an expanded segmented
halo and deterministic core ownership, not core-only segmentation with smoothing
context. Detect boundary dependence on thresholded components BEFORE watershed/
area filtering; an interior watershed child can otherwise depend on a cut parent
without touching the halo itself. Capture global Otsu once over the whole ROI,
keep untiled recipe/run identities stable, then source-bound resumable tile records
and independent admitted-ROI seam/interruption equality. No whole-slide raster,
new scheduler or repeated F01 biology campaign. D02 remains open;12/22 complete.

### D02 — bounded native tiled cell geometry and dependency detection

Implement the tile kernel over existing fluorescence segmentation/measurement.
A tile carries global raw sample geometry, full analysis bounds and half-open
ownership core. Segmented halo covers Gaussian support and2*expansion; complete
cells are owned by first global row-major nucleus pixel. IDs bind source/recipe/
channel block/Z/T/global anchor, not local watershed label or tile layout/order.
Global nucleus/body RLE and exact raw compartment moments are returned; no whole
slide raster. Reserve cut-component traversal and worst-case output capacities
in the existing work estimator before allocation. Parent dependency is detected
on thresholded foreground before watershed/area filtering; unresolved foreground
or owned labels block complete status. Existing untiled calls pass no diagnostic
hook and keep output/run identity semantics.

This is the bounded kernel foundation, not D02 closure. Per-tile Otsu explicitly
refuses until the orchestrator captures/resolves the global ROI threshold. Next
is existing source/job orchestration plus source-bound checkpoints/resume and
admitted-ROI consumer evidence. Decisive current check: one cached affected native
build/tests: new tile seams against whole-ROI masks/moments across3 layouts/order,
cut-parent and filtered-foreground cases, halo/work/cancel guards; existing pure
cells, source-bound workflow and published-cell replay suites. No GUI/bindings,
raw matrix/indexing, data/dependency download or biological campaign.

Bounded tile foundation passes: cached affected native build succeeds; all4 suites
pass0.79s (pure cells, new tiles, source workflow, published-cell/recipe replay).
Three layouts plus reversed tile order preserve exact whole-ROI nucleus/body
pixels, complete raw compartment moments, ownership and global IDs. Cut parent
marks its interior watershed child unresolved; area filtering cannot hide cut
foreground. Insufficient halo, small work budget, cancelled work and per-tile
Otsu refuse explicitly. Existing untiled source/publication/replay suites pass.
Waste review: one affected native build/test batch, no failed retry, new dependency,
data download, GUI/binding/package rebuild or repeated biology/scale campaign.
The optional internal diagnostic hook leaves ordinary untiled output unchanged.

Next: native source-bound orchestration must resolve any global Otsu/normalization
once, read finite halo tiles on the existing job and persist complete tile records
with exact input/options/source identity. Add interrupt/resume equality and an
admitted real ROI; thin desktop controls afterward. Primitive coverage is not a
whole-slide workflow or D02 completion.12/22 remain complete.

### D02 — source-backed tiled job and global threshold resolution

Add immutable source/ROI/calibration capture and lazy finite halo reads, then
stream tile outputs through the bounded primitive. Global Otsu reuses untiled
Gaussian score preparation and256-bin/lowest-tie resolution: whole-ROI min/max
pass, histogram pass, then explicit-threshold segmentation. Count only each core's
ROI pixels; raw tile hashes must agree across passes. Constant Otsu skips the
histogram reread. Explicit thresholds require one segmentation pass. Keep source
identity separate from layout/options-bound request identity; no whole-slide
raster. Use the existing FluorescenceCellJob slot/cancel/stale delivery for typed
tiled reports, not another scheduler. Native streaming sinks receive immutable
tiles and must write bounded records rather than hold the entire slide in memory.

Decisive check: one affected native build/test batch, retaining pure/untiled/tile/
source/publication suites. New source checks compare global Otsu, masks and marker
moments with the untiled ROI; verify two layouts, bounded read dimensions/three
passes, raw mutation between passes, cancel/read barrier, calibration-stale and
constant-zero-cell delivery. This does not close resume persistence, correction/
unmixing adapters, admitted real-ROI or desktop controls. No download, matrix
index, bindings, GUI or package build.

Initial source build succeeds. Pure cells, bounded tiles and published replay
suites pass. Source workflow reaches its final constant-case assertion after
whole-ROI/tiled smoothed Otsu/mask/moment equality, bounded reads, raw-mutation,
job cancellation/stale/ready checks pass. Failed oracle assumed raw100 remains
exactly100 after binary64 Gaussian normalization; a constant input can have tiny
filter-rounding variation. Test the exact lo==hi Otsu branch with sigma0 instead;
keep the existing sigma.7 whole/tiled equality unchanged. No product behavior or
scientific tolerance changes. Retain first ctest.log and rerun only the changed
source suite after rebuilding its one target; do not repeat the3 passed suites.
Observed tool-output delay was not a live hung test: terminal ctest reports1.19s.

Source-backed slice passes after the constant-fixture repair: initial native build
succeeds; pure cells/tiles/published replay pass in initial ctest.log. Rebuilt only
the changed source suite; ctest-r2.log passes0.22s (0.29s total). No product/tolerance
change for the failed oracle. New tests prove global smoothed Otsu/masks/marker
moments equal untiled ROI across2 layouts, count4160 pixels once, perform bounded
3-pass reads, keep source IDs layout-independent/request IDs layout-bound, reject
raw mutation between passes, and use existing job cancellation/stale/ready state.
Exact sigma0 constant resolves100/zero cells. Existing source/replay checks pass.

Waste review: reuse the existing score preparation/Otsu rule and cell-job slot;
no new scheduler. One initial build/test batch plus one diagnosed test-only rebuild
and source-suite rerun; preserve failed evidence and all other passing evidence.
No matrix indexing, GUI/binding/package or data/dependency download. Streaming
callbacks must consume/write one tile rather than accumulate whole-slide rasters.
Next: source/request-bound checkpoint/resume around this native stream, explicit
correction/unmixing adapters, admitted real-ROI seam/interruption checks and thin
desktop controls.12/22 remain complete; D02 is not closed by native source tests.

### D02 — bounded source/options-bound tile checkpoints and resume

Checkpoint the existing tiled stream using the existing bounded binary checksum
codec and atomic new-only export helper. Persist one global threshold/raw-input
manifest, complete tile masks/IDs/raw moments and a final completion record only
when all tiles are complete. Validate checksum/shape/ownership/identity/geometry/
moment missingness at load. Resume pins the source/request/global threshold,
verifies current raw bytes once per tile and skips completed segmentation; stream
both reused/new scientific results. Reuse the existing cell job overload, not a
second scheduler. Distinct replay/segmentation counters and canonical output hash
exclude timings/path/runtime allocation metadata. Reserve codec/manifest buffers
inside existing work admission, enforce per-record/total storage ceilings and
preserve all existing/new files on failure. No overwrite of records or inputs.

Decisive check: one affected native build/test batch. Compare uninterrupted and
3-tile-interrupted/resumed records byte-for-byte including geometry/moments and
output identity, prove1 verification read/tile and completed job reuse; reject
changed options/raw bytes/corrupt record, enforce storage quota and resume with
raised quota, refuse to persist unresolved candidates as complete, and reopen an
actual authored OME-TIFF with a fresh reader generation. No real-data download,
GUI/bindings/package or repeated expression/science campaigns. Derived sources,
admitted real-ROI and thin desktop controls remain after this checkpoint slice.

Initial checkpoint build succeeds. Pure cells/tiles/published replay pass. Source
suite passes interrupted/resumed byte equality, reuse/read counts, old-job reuse,
options/raw/corrupt rejection, quota completion/resume and unresolved-record
refusal, then exposes a real file-reopen identity bug: TIFF ReaderSource.reader
contains its encoded display view. Including that opaque descriptor in the raw
source SHA makes channel/range presentation invalidate a fresh raw reader.
Fix source profile toqpx.tiled-fluorescence-source/2: retain actual reader_id,
raw metadata/series/calibration, scope and file-version set, exclude display-only
observed.reader for file-backed sources. Memory sources still bind reader/generation.
No assertion/tolerance weakened; no existing untiled recipe identity change.
Retain failed ctest.log, rebuild/rerun only source workflow for this source-key
change; reuse other3 passed suites. Old local tiled source-profile checkpoints
refuse via changed SHA rather than aliasing the revised identity contract.

Source-profile repair passes ctest-r2.log0.43s: actual OME-TIFF fresh-reader resume
now succeeds after display-channel changes; byte-identical resumed geometry and
source mutation guards remain passing. Before handoff, source inspection found
resume did one exists/stat per POSSIBLE tile (up to100k even for a short prefix)
and omitted orphan atomic staging from total storage. Replace with one streaming
scan of existing dedicated checkpoint members; count recognized staging bytes,
refuse unrelated/symlink members, preserve all files. Add quota-before-raw-work
and unowned-content preservation checks. This concrete cost/quota correction
justifies one checkpoint-source/test rebuild and source-suite run; no broad rerun,
cleanup deletion or new scheduler. Retain prior passing/failure logs.

Checkpoint slice passes final ctest-r3.log0.45s (0.51s total). Initial build/pure-
cell/tile/published-replay passes are retained; source-key fix and scan/quota fix
have their own targeted passing source runs. Interrupted3/8-tile resume reuses3,
segments5 and reads raw8 times rather than repeating threshold/segmentation passes;
all records including masks/IDs/moments/completion are byte-identical to full run.
Fully completed job reuses8/segments0. Options mismatch rejects before raw reads;
raw mismatch/corruption reject. Quota failure advertises no complete record and
raised quota reuses all8. Unresolved tiles create no reusable complete records.
Actual OME-TIFF fresh-reader reopen succeeds after presentation changes, then
actual source mutation refuses. Orphan staging counts toward quota before raw
work and remains preserved; unowned directory content is refused/preserved.

Waste review: one combined affected-native build/test batch, one diagnosed real
source-profile repair and one concrete checkpoint metadata/quota refinement; only
source suite rerun for those changes, no broad science or GUI/bindings/package
campaign. Reuse atomic export/binary codec/job and saved threshold/segmentation;
no new scheduler, source download, matrix reindex or deletion. Canonical output
hash excludes path/timing/runtime accounting, retaining exact persisted science.
Next D02: correction/unmixing adapters over this same source/job/checkpoint path,
then admitted real ROI independent seam/interruption checks and thin desktop
controls.12/22 complete; this native persistence slice does not close D02.

### D02 — corrected/unmixed sources through the same tiled authority

Add typed retained DarkFlatRequest/LinearUnmixRequest capture to the existing
tiled request. Reuse their immutable ChannelSource/read_work_bytes/read/currentness
and native fixed normalization/model; no per-tile normalization or factorization.
Unmixed selections address component/residual outputs with independent ROI C=-1;
raw owner authority selects the measured input channel. Bind domain/parent records/
source identity, invalidate missing recipes, and account derived-read chain peaks
alongside checkpoint/segmentation buffers. Preserve raw source-profile/2 and all
existing untiled recipe/identity contracts. Reports name the measurement domain
and correction/unmixing lineage. Same job and checkpoint mechanism, no scheduler.

Decisive batch: new authored native TIFF/field test uses fixed subtraction and
power-of-two mixing with independently calculable component/residual moments;
compare all whole-ROI masks/moments/Otsu with tiled corrected, unmixed and corrected-
unmixed routes, byte-identical interrupted/resumed persistence, fresh native
field/model replay, missing/changed recipes and actual field mutation. Build/run
new derived suite and affected pure/tile/source/published suites once in shared
cache. No real-data download, expression reindex, GUI/bindings/package or new
library. Existing F03/F04 source identity rules stay unchanged; admitted real ROI
and desktop delivery remain after this native adapter slice.

Derived slice passes first batch: cached native build succeeds and all5 suites
pass1.86s (pure cells, tile kernel, new derived tiles, original source/checkpoint,
published replay). New native TIFF/field fixture covers corrected, original-
unmixed and corrected-unmixed output/component/residual domains. Every returned
mask and raw moment plus global Otsu matches its untiled reference; subtraction/
diagonal-mixing sums match independent equations, residual sums remain0. All
interrupted/resumed records are byte-identical; fresh native field/operator replay
reuses8/segments0. Removed/renormalized recipes and actual changed flat-field
bytes refuse; recipe Undo restores native job delivery. Inputs stay unchanged
until the deliberate authored-field corruption case. Unmix backend is compiled
for Release and actual combined/unmixed completion records exist, not a skip.

Waste review: one affected native build/test batch, no failure/retry/download,
source fixture duplication, new backend, GUI/binding/package or matrix campaign.
Reuse F03/F04 sources/operators and the same job/checkpoint sink; reserve derived
chain read peak together with checkpoint buffers and report its upper bound.
Raw source-profile/2 and existing untiled/F03/F04 identities remain intact. Next
D02: admitted real-ROI seam/interruption evidence and thin desktop delivery; do
not infer whole-slide performance from authored small fields.12/22 complete.

### D02 — actual desktop tiled controls and admitted real-field evidence

Wire the existing fluorescence panel to typed tiled requests/job checkpoints,
using selected ROI/crop, retained correction/unmixing and the same native limits.
Expose tiled results folder, tile/halo/disk controls, ready/unresolved status,
reused/segmented counts, threshold/domain and artifact location. Keep single-region
preview/publication authority separate via typed results; toggle/replay/new owner
must clear stale tiled delivery. Actual button checks cover blocked Run, Run/
resume, stale calibration, changed-option refusal and return to untiled mode.

Add an explicitly invoked native real-field driver (not automatic ctest) pinned
to the already admitted CC0 BBBC039 TIFF SHA. Use256MiB work,128x128 cores/256 halo,
compare every certified complete cell's masks/moments bitwise against the untiled
whole-field reference, report genuine boundary candidates as unresolved, compare
an alternate layout and interrupted/resumed cell identities/geometry/status.
Never turn unresolved boundaries into complete records or relax comparisons.
This is real fluorescence-field/ROI evidence, not large whole-slide throughput;
retain that limitation against the D02 whole-slide exit requirement. No download,
new material, label accuracy campaign, bindings or package work.

One shared cached GUI/app/native-real target build, then affected fluorescence GUI
and native source suites once, plus one real-driver run from System32/Windows-only
PATH with unchanged input hash. Preserve independent UI edits. Inspect one actual
scientifically meaningful controller capture; no cosmetic QuPath equivalence.

Initial combined GUI/app/real-driver build succeeds;4 affected suites pass2.73s:
source/checkpoint, actual fluorescence panel, dark/flat GUI and unmix GUI. Tiled
Run/resume reuses6, blocks while saving/editing, invalidates calibration, shows
changed-options error and toggles back to legacy Run. Before real execution,
refine the independent whole-field completeness oracle: old per-label edge flags
do not encode cut-parent dependence. Independently trace raw thresholded8-connected
components before filtered watershed labels and apply the declared4px expansion
support guard; keep exact masks/moments and all-complete-candidate assertions.
Build only changed explicit real driver; do not repeat passed desktop suites.

Real-field first run passes (real.log/real-host.json, preserved):104 certified
candidates match whole-field geometry/moments bitwise,26 reference candidates
remain boundary/dependency-uncertified, two layouts and interrupted/resumed values
agree under256MiB. Review of this genuine unresolved case exposes a persistence
hole: certified cells from an unresolved tile reached the sink but were omitted
from saved artifacts, making desktop saved-results wording too broad.

Persist explicit partial QFU records with certified masks/IDs/moments plus
unresolved foreground/label status. Never reuse them as complete; reprocess and
byte-verify on resume, retain no completion marker while unresolved. Include all
complete/partial record digests in output identity. The real oracle will verify
persisted counts/status for EVERY tile and byte equality across baseline/resume.
This closes lost-data semantics without weakening boundary classification or
counts. Build only affected checkpoint/source, GUI fluorescence, explicit real
driver and app; reuse already passed dark/flat/unmixing GUI evidence. Rerun source/
fluorescence GUI and the real case into a fresh v2 folder; preserve first receipt.

Desktop/real-field slice passes. Initial combined build succeeds;4 affected native/
GUI suites pass2.73s. Actual tiled actions cover blocked Run, new6-tile processing,
resume reuses6/segments0, calibration-stale, changed-option error and switching to
legacy run. Inspected1280x1000 actual capture shows readable status/counts,
threshold/domain/path and disabled inapplicable publication buttons; lossless
PPM→PNG RGB equality/hashes recorded in capture.json. No pixel-parity requirement.

One real execution established104 complete-reference candidates (of130),26
boundary/dependency-uncertified, unchanged admitted CC0 input, but exposed saved
candidate loss in mixed unresolved tiles. Partial QFU records now retain certified
masks/IDs/moments plus unresolved status, are reprocessed/byte-verified on resume,
and contribute to output hash without creating a completion marker. Partial
native/GUI/app build succeeds; only affected source/fluorescence GUI rerun, both
pass1.12s. Final real-v2 run exits0 from System32/Windows-only PATH on HEQPC:
104 exact candidates across30/25-tile layouts, threshold399.4921875,8 reused/22
segmented on resume; all complete/partial records/status/counts byte-identical.
Output SHA70e5d36d1045a794c353d3d018497ecdf76d64dc00259bcff6ecc21a320d4ab0.
Maximum reported accounted work155830828 bytes <268435456. No biology/speed/RSS/
WSI-throughput claim; this input is696x520. Input SHA unchanged9eacee...f780ec.

Waste review: one combined build/test, one real-oracle contract refinement before
execution, then one concrete partial-output fix/build with only affected reruns;
retain both real receipts. Reuse admitted bytes, atomic/binary/job services and
legacy GUI evidence. No data download, bindings/package, broad biology or cleanup
deletion in this slice. D02 remains open because field evidence cannot stand in
for its whole-slide ROI exit. Official OME docs identify CC-BY4 LuCa5-channel
whole-slide OME-TIFF24960x34560, HEAD2085508976 bytes; metadata only checked so far.
Next acquire/admit that exact open sample and validate one bounded nonzero-origin
ROI with existing native services.12/22 complete; do not mark D02 closed yet.

### D02 — admitted whole-slide nonzero-origin ROI

Acquire the exact official OME LuCa fluorescence WSI (2085508976 bytes,
CC-BY-4.0, PerkinElmer via OME; license evidence on OME sample-data page).
Keep one external build fixture and acquisition receipt, not bundled source/CI.
Verify length/SHA and native open-codec metadata before admission. Adapt the
existing explicit real driver for one bounded nonzero-origin ROI, retaining
independent whole-ROI masks/moments/complete-candidate oracle, alternative seams,
interruption/resume and256MiB limit. Inspect channel axes before selecting the
ROI. No full-slide raster, new reader, broad campaign or packaging. Existing
BBBC/GUI evidence stays valid. Download once; poll the original session.

Native ROI driver builds successfully with explicit bounded case metadata and
global-to-reference coordinate mapping. Existing BBBC defaults remain supported;
new WSI route will measure all5 channels, segment DAPI0 and require a nonempty
certified reference set. Candidate ROI is level0(12000,16000,768,512), not a
full-slide raster. Single-stream transfer measured ~0.6MB/s; deliberately stop
that identified curl process, preserve385069056-byte prefix, fetch eight disjoint
remaining ranges into the same part file with ETag/Content-Range checks. Keep the
initial terminated session61132 and acquisition receipt; no blind restart or
second image copy. Active ranged acquisition session43599; poll it. Shared-cache
build session62172 completed0; no broader tests or binding rebuild required for
this explicit-driver-only change. Next admit exact hash and run this WSI case once.

Acquisition completes2085508976 bytes, SHA50cc938e259673c4e62385ceb49c8707cead252566932bf47ed20f2afc5614f0;
range checks pass, no duplicate image. Header/OME XML confirms BigTIFF/LZW,
24960x34560x5, DAPI/FITC/CY3/Texas Red/CY5,0.49799447890790055um pixels.
Initial native WSI run from clean PATH fails at the explicit1000 peak/candidate
limit before output (real.log/host.json preserved). Dense unsmoothed real ROI
requires a larger declared bound; make case max_cells explicit10000, preserving
all scientific parameters,256MiB admission and exact complete-set oracle. Build
only changed manual driver, run into wsi-roi-v2 and preserve initial evidence.

Second WSI attempt reaches tiled scientific comparison but rejects a returned
candidate (real-v2/host-v2 preserved). Diagnose the reference policy: its4px
sample-support guard used local ROI edges from the old full-source BBBC case.
For an interior WSI ROI, tiles have real samples beyond those edges; the guard
must use global physical source edges. Keep explicit domain-cut component and
ROI-touching nucleus/body boundary refusal unchanged. Correct only independent
oracle coordinate policy (BBBC at source origin unchanged) and add anchor/label
failure diagnostics; preserve exact masks/moments/entire certified-set checks.
No product-kernel change or tolerance relaxation. Build manual driver only.

D02 WSI slice passes (real-v3/host-v3 exit0, HEQPC/System32-only PATH). Admitted
native BigTIFF/LZW24960x34560x5; explicit ROI(12000,16000,768,512), DAPI0 and
all5 measured channels. All1374 certified cells of1527 whole-reference labels
match exact masks/all moments/IDs across24/20-tile layouts.153 boundary-dependent
candidates stay uncertified; native states remain unresolved, with all certified
output retained and no completion marker. Resume4 reused/20 segmented, every
complete/partial record byte-identical and same output SHA119cc343...c4c3b9e3.
Peak accounted work240760252 <268435456; cancellation at committed tile boundary
unwinds0.4172ms. Input SHA unchanged; no full-slide/biology/speed/RSS claim.

Waste review: preserve one2.085GB source; a measured transfer change reuses prefix,
not a redownload. Two actual refusals diagnosed (dense-data peak admission and
reference crop-vs-physical-edge policy); preserve both failed receipts. Three
manual-driver-only builds and one successful WSI execution, no product algorithm,
new reader/dependency, tolerance relaxation, broad suite, GUI, binding or package
campaign. JSON/diff/checklist checks suffice for the remaining documentation/
inventory edits. Durable lessons appended to compilexx. D02 closes13/22;9 pending.
Next selected outcome O01 native H5AD subset using the existing matrix source,
exact HDF5 admission and one native adapter/import route. Read its plan packet
before implementation; do not repeat D02 or package before settled P01/P02.

### O01 — native H5AD decoding and indexed import

O01 is now delivered locally, including native decoding/indexed/project/job route,
actual desktop import and admitted real count/ID/axis evidence:14/22 complete,
8 pending. Preserve independent UI work. Next selected implementation is O02 local
NGFF0.4/Zarr2 over the existing native source capability; read its plan packet.
Do not repeat O01/D02 or package before P01/P02.
Dependency/decoder source scope is dc2df398; reuse its29 independent fixtures and
HDF52.2 source/filter admission, native allocation/context and cancellation checks.
No Python application logic/runtime, temporary Matrix Market/TSV or extra store.

One shared transactional Writer owns SQLite staging, exact scalar blobs, gene
hashes, quota checks, input guards and atomic publication for both parsers. H5AD
reserves decoder work before SQLite receives the remaining allowance. Counts stay
uint64; continuous values stay finite doubles. Schema3 retains embedded raw source
coordinates/frame and exact matrix/layer/spatial keys, verifying their native
recipe identity on reopen. Original H5AD can go offline. Legacy schema1 bytes/
source/block contracts remain frozen; schema2 values/coordinates stay readable
without guessing its missing keys. No automatic migration. Source coordinate
frame hashes are prepared once per page/writer, not once per row.

SpatialImportJob.index_h5ad uses the existing indexing future. Existing backed
preview/publication selects embedded coordinates when no TSV is supplied, retaining
declared source plane/unit and explicit calibrated conversion/alignment. Guarded
TSV behavior remains intact. Count/continuous spatial projections and owned
project reopen use the same native source; no copied double counts on points.

Decisive evidence: build/feature-expansion/o01-index/recipe-build.log exit0,
recipe-ctest.log all6 suites pass2.28s (new H5AD matrix, decoder, legacy backed
import, spatial matrix, project matrix, legacy matrix). Covers dense/CSR/CSC in
both domains; exact values/IDs/axes/cohorts; source-offline coordinates; Unicode/
plane/calibration/extent codec; nondefault layer replay; malformed/quota/composed-
budget/pre-cancel refusal; corruption including outside selected cohort; native
index/preview/publication and owned continuous project reopen after calibrated
physical mapping, without original H5AD or external store. Original fixtures
unchanged. Frozen schema1 tiny file SHA174b4ea6...b0b6018 still matches. Final
HEQPC Windows-only PATH execution exits0 (native-only-v2.log/host.json), executable
SHAd37bf96326e8643801a1457a9ab58caba969659d708843ed6183c7ed624183d4.

Waste review: one namespace qualification compile repair; preserve all unaffected
objects and the failed build.log. First batch passes1.99s, then concrete provenance
review finds hash-without-selection-keys gap. Preserve that passing source delta
(passing-schema2.patch/base/SHA receipt) and admit a32KiB authored schema2 fixture.
Version new writes as3, share pure native identity code, and rerun only the six
affected suites; legacy fields/data assertions remain unchanged. HDF5 not rebuilt,
no full D01 real reindex, broad corpus, GUI, binding, SDK, wheel/package campaign.
Retain earlier receipts under their own source; do not transfer them to later code.
Detailed receipt and durable compilexx lessons updated; no duplicate cost ledger.

Next: wire the existing desktop import controller to native H5AD indexing and
embedded-coordinate preview. User must select counts versus continuous and an
explicit source coordinate frame/unit; never infer X/raw/layer semantics. Display
scope exclusions and saved domain/keys. Then admit one real H5AD comparison with
exact source counts/IDs/coordinates. Figshare13604177 metadata is retained only;
no payload or encoding acceptance yet. Reuse admitted open inputs/lineage where
possible; a normalized X cannot stand in for raw-count validation. Keep O01 open
until those exits pass. Bindings/SDK settle in P01; package once in P02.

O01 desktop/real-data slice: add an H5AD choice to the existing indexed input
controller and keep preview/publication native. Explicit value domain and source
coordinate units/frame, saved dataset keys, embedded-coordinate choice and scope
exclusions must be visible; no per-row frontend logic. Preserve the independent
ui::Checkbox change in gui_spatial_import.hpp. Extend actual GUI action checks
for indexing, preview/publish/open and stale input edits; build app/import GUI
and only affected native checks once after interfaces settle.

For real raw-count equality, reuse the already admitted CC-BY4 10x full32285-gene/
2702-observation/16031101-entry source and declared hires pixel coordinates.
Author a disclosed current-profile H5AD fixture losslessly from the frozen native
store using development-only h5py; app/runtime never uses that preparer. Preserve
ALL original IDs/counts/coordinates, not a conveniently smaller gene/spot subset.
Verify original store SHA79f407bf...aca7401 before/after, explicit coordinate TSV
provenance and derivative lineage. One native H5AD import plus every sparse entry/
ID/coordinate comparison against the retained original store/TSV establishes the
new format, not a repeated Matrix Market qualification. No new biological data
download, unreviewed/NC input or hidden normalized-X-as-counts substitution. The
Figshare candidate stays metadata only. Keep one H5AD fixture, source/evidence in
build/feature-expansion/o01-real, not bundled source/package. Inspect actual chunk
shape before choosing a cache optimization; preserve result identity/composed work.

Desktop/real first batch: cached native/app/GUI build exits0. Full real H5AD
comparison passes: all16031101 sparse entries,32285 feature IDs/order,2702
observation IDs/order and2702 exact ID-joined coordinates; input hashes unchanged.
Import16252.6ms,153567784 accounted bytes under256MiB (not RSS/speed comparison).
Actual data chunks256/128KiB exceed old64KiB cache:1MiB per dataset now fits three
numeric datasets +4MiB metadata within the existing8MiB reservation. No before/
after speed claim. Fixture lineage/admission is explicit CC-BY4, not upstream
legacy H5AD. Scoped staging guard passes; the entire historical fixture inventory
is not a byte staging inventory (authored fixture-group records lack per-file
hashes), so use the exact three real records, without weakening that guard.
Before GUI execution, move scientific interpretation fields to Coordinates step
so Image/files remains readable; then cached frontend-only rebuild/check. No
repeated real index or biological campaign; preserve its completed sidecar/receipt.

First GUI run reaches native H5AD publication, then fails Open-path assertion:
the test was invoked with a relative output root while its indexed Open assertions
expect an absolute saved path. Preserve gui/ and gui.log; rerun the same binary
with an explicit absolute new evidence root (no product/assertion change or rebuild).

Closure: layout-build.log exits0; native-ctest.log passes h5ad/h5ad_matrix0.63s.
Unchanged GUI binary passes gui-absolute.log after correcting only invocation root.
Actual Next controls index independent continuous H5AD; exact embedded-coordinate
preview, publish/open and changed-selection invalidation pass. Legacy selected/
indexed routes and gene paging remain checked. Real32285-gene/2702-spot desktop
preview/publication passes on the new H5AD sidecar with retained explicit TSV;
independent real native check already verifies all embedded coordinates. Inspected
h5ad-preview.png and real-indexed-preview.png: values/domain and actions readable,
registered overlay correct; no cosmetic parity campaign. Source/binary identities
and full dirty UI patch are retained in o01-desktop/source-receipt.json. GUI work
is not claimed to be a pristine commit while independent UI edits are in use.

Waste: one bounded relink for moving interpretation fields off crowded Files;
one invocation-only GUI retry, no assertion weakening/rebuild; scoped fixture
staging after existing group-level inventory fails byte-staging schema. Native
real import performed once; no h5py app fallback, raw MTX reindex, binding/SDK/
wheel/package or broad campaign. Plan/guide/receipt and durable compilexx notes
updated. O01 closes; O02 next, remaining L01/O02/A01-A04/P01/P02 stay open.

### O02 — local OME-Zarr native source

Previous goal turn is progress: O01 closes in8a2d0901 (14/22;8 pending).
Selected next outcome remains the full O02 contract, not a smaller format claim.
First implementation slice: local NGFF0.4/Zarr2 bounded metadata/chunk decoding,
explicit axes/transforms/units, scalar LE/BE C/F data and null/zlib/gzip codecs,
missing-chunk fill distinct from IO/corruption, cancellation and observed directory
inventory. Use the existing ChannelSource capability; native ImageServer/viewer/
persistence and complete admitted real store remain required before O02 closes.
No Python runtime, TIFF contract change, new codec dependency or package build.
Primary references inspected: ngff.openmicroscopy.org/0.4 and Zarr v2 storage spec.
63-companion ReaderSource cannot represent a chunked directory; do not truncate
its file set or label a metadata fingerprint as whole-store identity. Implement
private bounded store observation and selected-chunk guards first; settle the
proper descriptor/persistence extension before frontend wiring. Keep the existing
UI dirty state and passing O01 products/evidence. One native-only affected cached
build after interfaces settle; no GUI relink for this first decoder slice.

First O02 build stopped deliberately after observed broad core recompilation.
Direct zlib target linkage propagated a new include directory to every core file.
Scope its header include and feature define to ome_zarr.cpp, keep link-only codec
dependency, then rebuild affected inputs in the same cache. Preserve build.log/
partial objects; no second cache, dependency rebuild or GUI/binding/package run.

Scoped retry retains no new global codec include/feature flags, but MSBuild tracking
was already invalidated by the interrupted broad command, so remaining core
objects are recompiling. Preserve that actual cost; do not claim this retry was
only three compile units. TIFF/HDF5/zlib dependencies remain reused. After this
single cache completes, run only new OME-Zarr and the existing ChannelSource check;
repair only a diagnosed compile/test failure. O02 stays open for ImageServer/
directory provenance/persistence and full open real-store acceptance. Candidate
real data is the complete retained CC-BY4 OME tubhiswt-2D pair,512x512x2; preserve
both originals/all samples, not a convenient ROI or uncertain external download.

Scoped build compiles native store/metadata/decoder and links core; test compile
finds ADL ambiguity between local copy helper and std::filesystem::copy for literal
arguments. Rename fixture helper only. Review also finds large-origin cancellation
risk in physical-to-pixel mapping: cancel common origins first and retain integer
quotients/fractional offsets; add a1e20-origin regression. Require exact admitted
integral XY pyramid scale instead of silently tolerating anisotropy, and account
actual metadata vector capacities. One affected decoder/metadata/test rebuild;
reuse the now-complete core objects. No test assertion relaxation.

First focused native/ChannelSource run passes1.91s; Windows-only PATH decoder
run also exits0. Final file-guard review tightens absent-chunk checks: inspect
existing parent links and symlink_status so a newly dangling link is not mistaken
for declared fill. Rebuild only store/native tests and rerun those two focused
suites/native runtime; keep earlier passing receipts scoped to their source.

Final first-source slice passes: hole-guard-build.log exit0; OME-Zarr and existing
ChannelSource suites pass1.69s. Native Windows-only PATH check exits0, executable
SHA35ceae16344d906f2750c11991ee3d644336d9f33327cacda502b006e22f2849. Frozen169
fixture files/21669 bytes still match their manifest. Local source receipt retains
exact owned source/binary identities; earlier native logs remain diagnostic only.
No O02 checkbox closure:14/22,8 pending. Existing O01 GUI runtime remains frozen.

Next implementation: versioned directory provenance/descriptor (do not pretend
63-companion list or one metadata fingerprint covers the store), native ImageServer
view adapter and saved/reopened selected view/store identity. Keep the reader as
the sole decoder; preserve legacy TIFF identity/archive formats. Then complete
retained CC-BY4 tubhiswt-2D full512x512x2 lossless Zarr derivative, independent native
comparison of ALL samples and viewport/raw measurements. No new download/ROI
substitute, repeated H5AD campaign or packaging. Notes/guide/plan/reader admission
updated; broad first recompile cost and each concrete correction retained above.

Final source-byte audit finds Git text normalization changed committed fixture
JSON/inventory bytes even though Windows working-byte/native checks passed.
Add an exact-byte .gitattributes rule and renormalize only this fixture directory.
Verify every staged blob against frozen working bytes/manifest in one batch;
no pixel/source/runtime change, so no product rebuild or numerical rerun. Preserve
0ef1896b as the earlier local source state and commit this admission correction.

O02 continuation: previous goal turn is progress (0ef1896b/c08eacaa),14/22;
first finish directory provenance needed by native ImageServer/save/reopen. Add
an explicit bounded directory-inventory token, not extra companion entries or
whole-store byte-fingerprint claims. Version new provenance/source-verification
encodings while preserving legacy bytes. Metadata-file fingerprints must remain
primary-only; verification must check the native full directory inventory.
Settle the descriptor/codec interfaces and affected tests before one cached build;
common source-identity header changes legitimately rebuild its consumers. Native
image adapter, generic raw view, project/GUI and real full-store exits stay open.

Settled native slice now includes ImageServer RGB/raw adapter and generic raw_view/
with_raw_view/require_raw_current capability, with legacy TIFF default delegation.
Document raw-plane/view changes use it; TIFF view identity/serialization stays
unchanged. New QPV5/object and6/source-only directory records retain inventory;
old1..4 remain unchanged. QSV2 and JSON3 retain the same distinction. Metadata
fingerprints stay primary-only; native verification reconstructs/checks the full
observed directory. Report output cannot modify the input tree, including Windows
case aliases. Expose immutable decoder-owned descriptors and share the decoder
across independently closed display projections. RGB and decoder share work cap.
Decisive batch: new native Zarr server (RGB windows/padding, raw means, generic
planes/view mutation, QPV5/6/QSV2, primary-only fingerprints, stale whole-store
checks), decoder, existing source identity/channel/TIFF/project regressions.
Common SourceIdentity/ImageServer/Document headers changed, so one legitimate
native-consumer rebuild is expected; dependencies and old GUI products retained.
Project link/save/reopen and actual GUI/real-store acceptance remain next; do not
claim O02 closure from this adapter batch or package before P01/P02.

Wait verified against live desktop CMake PID115076/MSBuild192952 (references
disabled); no competing build started. That desktop build predates the native
ABI rebuild and must be relinked afterward before reuse; old GUI-only acceptance
cannot transfer to the new common interfaces. The processes are now terminal;
start the settled native batch. Reader/view metadata and whole-inventory check
reservations now compose with RGB/decoder work, including the final current check.

Native batch builds successfully. Five legacy/decoder suites pass; new server test
refuses tile_size2 because existing raw-statistics minimum is16. Correct the test
request to16, preserve the validator/assertions and failure log. Add the settled
opt-in full real-store comparison to that same test driver (no automatic data
fetch): full524288 samples/validity/axes/calibration/raw statistics and explicit
RGB equality with retained native TIFF, preserving original hashes. The independent
full37-file Zarr derivative is prepared and passes the unchanged staging guard.
Rebuild only this changed native consumer and rerun affected checks; no core
algorithm change, binding or package campaign.

Final native suites pass2.78s; full real-store native Windows-only PATH check
exits0: all524288 samples/validity/axes/calibration/raw summaries and explicit
RGB pixels match native TIFF. Source SHA hashes unchanged;37 input files pass
staging. Native rendered RGB inspected. UI library is dated07:29:38; all changed
common headers predate its07:23 build start (07:15/07:17), so relink only the app
against settled native core with references disabled. This is ABI integration,
not a transferred GUI/scientific acceptance claim. Preserve all live windows.

Adapter/provenance slice delivered locally: app-relink.log and two GUI consumer
relinks exit0; gui-abi-ctest.log passes shell/organization0.34s. No GUI source
implementation edits or binding object compile/relink. Existing Python binaries
stay with their original ABI; retained old binding objects cannot relink this
new core until P01. Native real-test executable SHA5469c4aa51cabee2d0d7d34e6e85a8f50a5aa722d6b22049adecd24e96065744.
Source/dirty-overlay receipts preserve the independent UI state; no acceptance
transfer from an older mixed native product. Full real derivative remains external
build evidence with all37 files and parents admitted in the canonical inventory.

Next: project reader name/factory and link/save/reopen must retain Zarr view,
options and expected directory identity. Reuse current entry/link machinery and
preserve TIFF payload bytes. Source review must compare directory tokens even
when .zattrs bytes match; primary-only hashes cannot imply matching whole-store
bytes. Then actual desktop open/raw channel/plane controls and saved real-store
acceptance. Public generic view/descriptor interfaces are now settled; keep next
edits private to avoid another common-header rebuild. O02 remains open14/22,
8 pending. Notes/guide/plan/receipt updated. Waste: verified cache wait, one test
request repair to the existing tile16 minimum, changed-consumer rebuild and
required ABI relinks; no weakened assertions or repeated science campaign.

O02 project/UI slice: previous goal turn is progress ec8e4618. Add a private typed
Zarr link payload to existing quantitative-view storage, retaining exact view,
resource options and expected directory identity. Preserve old TIFF payloads and
legacy record branches. Offline open/save must retain recorded facts without
filesystem scans on the owner; lazy reads enforce expected inventory. Native
source review compares directory tokens when metadata bytes match. Raw analysis
uses generic view and QRA5/6, preserving directory identity in natural/grid records.
Actual desktop image chooser and channel/plane/measure/keep controls delegate to
the existing native worker, adopting its prepared candidate without repeated IO
on the owner thread. Whole-directory checks belong at worker boundaries, not
frame polling. Shared output guard protects directory subtrees; registration
image identity now binds directory inventory. No new public headers or bindings.
Decisive combined cached batch: new project/offline/replay/change-review/corruption
and actual GUI channel/measure/keep/save controls, plus affected legacy raw/project/
save/registration/output-guard checks. Use the retained complete37-file open real
store for saved-project acceptance; no repeated decoder/whole TIFF comparison.
Preserve independent UI layout edits, especially FirstUseEver in gui_raw.hpp.

The combined build compiled the changed core successfully, then the new test failed on an unused private JSON helper include (the core deliberately keeps JSON private). Remove that unused include; do not widen dependency visibility. Resume the affected targets using the completed core objects and retain the original build failure log.
The next test compile exposed its missing public project_source.hpp definition (project.hpp forward-declares the review). Include the actual contract header; no core/API change. Both corrections are test-include ownership mistakes, preserved in build/rebuild logs. Resume the same target batch without rebuilding settled core.
Native ome_zarr_project passes0.87s: view/options/RGB, QRA5/6 replay, offline inspect/save, stale directory refusal, metadata-only changed-dependency review, registration identity and protected outputs. Optional real GUI driver will display the saved full tubhiswt project, channel metadata and retained record and capture the native framebuffer; existing independent full TIFF comparison is reused.
Affected native/project/save/application-entry checks pass. New GUI test fails before issuing a click because a required action is not visible/enabled; preserve frontend-ctest.log. Add action/state diagnostics to identify the real tab/layout boundary, then repair the driver or actual UI according to evidence. Do not drop the click assertion or replace it with a direct API call.
Diagnostic identifies Apply succeeded and Measure remained absent after the Data-tab click. The driver omitted ConfigInputTrickleEventQueue=false used by the existing native fluorescence/marker/unmixing click drivers; queued backend pointer events delayed the tab action. Match that established deterministic event setup and hide only the isolated control-test window. Keep actual clicks and all source/result assertions; the real full-app framebuffer route remains separate.
Event-setting-only correction did not fix the Data-tab transition (gui-event-test.log); that hypothesis was insufficient. Match the existing proven hover-frame -> press -> release helper as well, and capture the framebuffer on any missing action so a remaining visibility failure is directly inspectable. Product logic has not changed during these driver repairs; existing native results remain valid.
Hover-frame fix reaches the actual Measure action. The measurement result fails the expected all-channel means assertion; retain gui-hover-test.log. Add the actual captured rectangle/C/Z/T and means to the test diagnostic before changing product logic or expectations; do not guess the mismatched contract.
Measured request is exactly0,0 7x5 C0+3 Z0/T0; displayed means23/1023/2023. Independent replay of the existing Welford update gives errors0,-1.1368683772161603e-13,+4.547473508864641e-13. The new test incorrectly required floating mean bit equality to mathematical integers. Check mean error below1e-12 with exact region/axes/valid/finite/nonfinite counts and min/max. No raw values, identity/archive comparisons or implementation tolerances changed. Keep diagnostics and prior failure evidence.

Final O02 project/desktop slice delivered locally: consumer-build.log exits0; five
affected native suites pass1.60s, application_entry/project_save pass, and
gui-final-test.log passes actual C/Z/T/window/measure/keep/save/reopen0.69s.
real-project.log preserves full512x512x2 saved history/RGB with Windows-only PATH;
both native app source/project startups exit0. real-gui.log passes and inspected
real-desktop.png shows native real tiles, channel metadata, unknown calibration
and saved record. Reuse the earlier exact ALL524288-sample TIFF comparison.
O02 checkbox closes:15/22 complete,7 pending (L01,A01–A04,P01/P02).

Waste: two include ownership mistakes, then new GUI driver setup/hover and mean
bit-equality mistakes caused avoidable consumer retries. Match existing native
click helpers and numerical contracts before compiling a new driver. Test-only
repairs reused settled libraries/references-disabled builds. Keep all failures;
no science/identity weakening, packaging or repeated broad campaign. Independent
UI layout changes remain unstaged and are disclosed in source/runtime evidence.
Next bounded task is A01: full-universe native QC and versioned library-size/log1p
derived values with raw integer preservation, explicit zero/missing/continuous
domain handling, identity and persistence/replay. Start with D01's existing native
block/source services, settle the request/result/store contract before consumers.


### A01 — native full-universe QC and normalization

Previous goal turn is progress c6c5745d: O02 delivered,15/22 complete,7 pending.
Selected outcome: native full-source observation totals/detected features/explicit
groups, lazy library-size/log1p source and versioned persisted recipe/replay.
Use every feature in the declared matrix, never a display projection. Implicit
sparse absence is measured zero; zero libraries produce valid zero normalized
values and undefined group fractions. Continuous/already-normalized input refuses.
Retain exact uint64 totals with checked overflow and unchanged parent counts.

Reuse bounded ExpressionMatrixSource reads with automatic conservative feature
batches, avoiding one connection per gene and avoiding a dense whole matrix.
One retained immutable QC pass feeds multiple derived views. Recipe-only project
metadata (chunked within existing scalar bounds) refers to the existing parent
association; reopening/replay recomputes QC once on a worker and verifies identity.
No QC vectors in document metadata, no second normalized matrix copy, no per-frame
IO or new generic job scheduler. Derived virtual sources must refuse accidental
raw-sidecar association. Settle new standalone API/types before one cached core/
test build; preserve independent UI work and existing bindings/packages.

Decisive checks: hand-calculated sparse/explicit zero/zero-library/group examples,
uint64 beyond2^53 unchanged and overflow refusal, normalization/log values with
declared tolerances, batch/work/cancel/stale bounds, source/parameter/recipe
identity, project save/reopen/native replay and downstream native gene projection.
Real full-universe QC may reuse the retained32285-feature/2702-observation matrix;
no reimport/download or six-gene substitute. Keep A01 open until consumer exits
are proved. No broad platform/GUI/package campaign for this core slice.

First settled native batch builds without errors; expression_normalization and
existing indexed matrix/project/spatial suites all pass1.50s. Covers exact QC/
groups/zero semantics, large uint64/overflow, lazy values/identity, existing gene
projection, Unicode/chunked project recipes/replay and real batch cancellation.
Review additionally charges the caller's group definitions while their result
copy is live. Add an opt-in complete real-source check to this same driver:
independent dev-only integer sums/detection over all16,031,101 retained MTX
entries, then native QC of the existing .qmx and saved replay at a different
work/batch size. No source import, download, GUI/binding/package campaign.

Native A01 slice passes on final source: real-build.log exit0 and Windows-only
PATH real-native.log exit0, including all analytical/persistence/cancel/stale
assertions plus complete real32285-feature/2702-observation QC. EVERY total,
detected-feature count and explicit group equals the independent original-MTX
oracle across all16,031,101 entries;85825294 counts. Saved project replay at
256/64MiB uses253/385 batches with exact QC/recipe/derived-block identity.
QC4289.23ms is a cached named-host observation, not a comparison/RSS claim.

Keep A01 unchecked:15/22 complete,7 pending. Next actual desktop integration:
retain the raw matrix/spatial capture; compute QC/derived source on the existing
worker, explicitly rebind those coordinates to the derived source, then reuse
GeneViewSession. Present raw/normalized/log domains and QC summary without
per-frame scans or double display log. Keep/reopen recipes through the native
metadata/open-request services. SDK/thin optional Python remain P01.
Waste: first build/tests passed; one affected follow-up for group-input capacity
reservation plus real driver. No new public common-header ABI, source reimport,
normalized-store copy or GUI/binding/package rebuild. Reuse this native math/
identity evidence for the consumer mapping checks instead of another oracle run.

# UI workflow safety card at merge (2026-11-15)

Historical snapshot of the feature/ui-workflow-safety card when it was merged.

## UI workflow safety — Milestones 1–6 delivered, uncommitted

L3: **DONE_WITH_CONCERNS**. The bounded status summary joins nonempty parts;
512 native combinations pass. In-scope translation misses are zero: 187 new
15-language rows, invariant data kept literal and calibration double lookups
fixed. French/Russian 200% captures, zero layout lint, 86 contracts and 172
current-hash matrix cases pass. Frozen 14/38, public headers and command ids stay
exact. Cost: 41 compile attempts, 2 archives, 32 links, 1 diagnosed compile
failure (mixed auto types); resumed at the failed unit after source checks.
An extra affected rebuild shortened a clipping new French/Russian combo caption;
this exceeded the one-build target, with no scientific or geometry change.
[L3 report](../build/ui-workflow-safety/layout/l3-report.md). Changes uncommitted.
Next: stop; no specialist-window translation or further feature phase selected.

L2 visual design: **DONE_WITH_CONCERNS**. Refresh/Reset/Undo are distinct;
numbers use one compact token, destructive groups use the canvas divider/gap,
and the status summary/controls use content widths. All 48 glyphs, geometry,
notifications, analysis traces, English/French 200% lint/captures and 170 native
matrix cases pass. Frozen 14/38, public headers and command/caption identities
remain exact. Cost: 23 compiles, one archive, 17 links, no build failures;
one extra test-only coverage compile exceeded the strict one-build target.
[L2 report](../build/ui-workflow-safety/layout/l2-report.md). Changes uncommitted.
Next: stop; controller's independent real-path matrix, no further feature phase.

Layout/action placement: **DONE_WITH_CONCERNS**. Recovery and the app-wide icon
audit use captioned strips, shared fields, explicit selection captions/reasons and
a native rendered lint. Final matrix: 168 commands; 86 source contracts; English/
French 200% lint and focused lossless captures pass. Frozen 14/38 remain exact.
Build cost: 195 compile attempts, 12 archives, 194 links, 4 failures;
avoidable rebuild/observer/harness corrections exceeded the budget. Broad before
English-200% tracing was not collected; the report states that evidence gap.
[L1 report](../build/ui-workflow-safety/layout/l1-report.md). All changes remain
uncommitted. Next: stop; no further feature or packaging phase is selected.

Milestone 6 review fix delivered: branch-local non-inline request builders and
notification observation reduce the cell-builder frame 174,544 → 49,784 bytes;
run_gui is 361,072 and task draw remains 165,232 (three largest locals reported).
One compile/archive plus 16 relinks; 85 source contracts and all requested mapped
workflow checks pass with unchanged assertions/protected sources/frozen 14/38.
[Report](../build/ui-workflow-safety/m6/review-fix1-report.md); no build/check retry.
Next: controller's whole real-path matrix; no further implementation selected.

Task 6.4 fix round 1: standalone translation coverage passes on the final binary;
the test frame fell from 325,168 to 60,352 bytes with unchanged assertions/stack
size. One test compile/link; 85 source contracts and units vocabulary pass.
Notifications all passes with the retained harness's isolated LOCALAPPDATA.
Earlier environment/relocated-binary failures and corrections are preserved in
[the fix report](../build/ui-workflow-safety/m6/t64-fix1-report.md).
Product GUI and frozen 14/38 stay exact. Status DONE_WITH_CONCERNS for avoidable harness
cost; all requested checks pass, no controller command or further build remains.

Task 6.4 and the Milestone 6 handoff are complete: 85 source contracts and 166
recorded mapped native invocations pass, including the full matrix/complex walk
and both translation ratchet probes. The plan's stated default scope records 158
untranslated captions plus two translated-label secondary lookups (160 keys,
203 surface occurrences); missing captions were not translated. Frozen 14 native
libraries / 38 binding objects remain exact; no runtime Q:\cpp embedding.
Build/fixture/aggregation corrections exceeded the one-build budget; the actual
cost and unchanged assertions are in [the report](../build/ui-workflow-safety/m6/t64-report.md).
The receipt records individually unconfirmed defaults and coverage limits.
Next action: stop; only Milestone N awaits an owner decision. No controller check remains.

### Previous checkpoint

All seven corrections are complete: deferred closed-window work, invalid gene
state clearing, editor feedback expiry, TMA comparison/unused field and the all
runner. The [report](../build/ui-workflow-safety/m5/review-fix1-report.md) records
24 passing mapped checks and 75 source contracts, with existing assertions intact.
One affected cached build: 12 compiles, one GUI archive, 16 links, zero failures
or retry waste. All 25 cached units are current; frozen 14/38 and 120 public
headers are exact, protected UI files unchanged, and 21 runtime files have no Q:\cpp.
The receipt has the two requested clarifications. Item 3 is implemented; nothing
requested remains undone. The controller owns the subsequent real-path matrix.
Defaults remain individually unconfirmed. Milestone 6 follows; only Milestone N awaits an owner decision.


## 2026-11-16 - Native label and OMERO checkpoint snapshot

# Current implementation task

## Complete locally - OMERO raw blocks and ROI/result exchange

The selected native/CLI profile is delivered: bounded original-channel blocks
from explicit anonymous NGFF0.4/Zarr2 associations, original-coordinate vector
ROI exchange, and style-verified binary mask coverage through the existing PNG
and contour authorities. Blosc is an optional provider through the existing
reader ABI, with pinned open C-Blosc/LZ4 sources and their own licenses/notices.
See the [guide](omero-integration.md), [provider](../plugins/blosc-bytes/README.md)
and [source-scoped evidence](benchmarks/omero/README.md).

Decisive results: exact native raw HWC/endian/float bits, transforms/calibration,
Int64 ROI/shape IDs, planes, styles/packed-color types, nonfinite facts, protected exports, observed
source drift and mask holes/area/packing pass. Normal component build, CTest,
both configured ABI providers and offline preview reopening pass. Native checks
run with a Windows-only PATH and Python environment removed. Public IDR raw
image15190881 yields128x128x2 UInt16 at Z12 with6,088,996 NGFF response bytes
including stability reads. Public image9528943's4280 point shapes round-trip
with exact original facts. Independent exported-file checks pass. All102
protected baseline/previous/raw-phase products remain byte-identical. Current
source/products and observed public binaries have separate identities under
build/gui-aggregation/gap-closure/omero-raw-results.json.

Cost/correction: one component/codec cache plus affected repairs; reused the
retained core, PNG module and two passing label objects. Codec notice audit also retained fastcopy Zlib and locked-source attributions
without changing compiled bytes. No engine, GUI, binding,
SDK/wheel or release-package rebuild. An authored JSON initializer needed a
syntax repair. The public study exposed masks; native support reuses existing
contours and validates against the same GeoJSON winding projection, preserving
exact source facts/coverage. IDR's mask endpoint returns403, including the first
documented ROI reference; native diagnostics located that endpoint. The other
CC BY4.0 reference supplies the passing point route. No denied route was bypassed.
Receipt helpers needed path-separator/UTF-8 normalization and idempotent edits; documentation patches needed
context correction. One generated root fixture was moved into retained evidence
after checking absolute bounds, tracked state and active processes; no deletion.
Final source-fidelity review pinned every staged raw member to its owned byte
hash and held the existing native file guard through sampling. Affected raw/ROI
service units were rebuilt; native/ABI and offline checks pass without repeating
the public campaign.

Stop at this declared native/read-only outcome. IDR mask pixels remain live-
unqualified despite passing native fixtures. Private authentication, server
write-back, raster geometry editing, nonidentity transforms, full-image streaming,
desktop/bindings and packaging remain separate R15 selections. No server/account,
remote write, additional model, full QuPath parity or biological/platform/resource
acceptance is claimed. Source remains uncommitted; do not automatically start
another catalogue extension or replay the public/qualification campaigns.

## Complete locally - OMERO CMake build handoff

Owner continuation finished the remaining build-path evidence for the existing
public client. Root CMake configuration with OMERO/tests on and Python off passes
using cached admitted sources. The actual OMERO module builds normally against
the retained native core/dependency graph. CTest protocol checks, CLI startup
and offline preview-project opening pass; startup/open use a Windows-only PATH
and no Python environment. All88 protected baseline/candidate/label/earlier OMERO
products remain byte-identical. Exact source, commands and products are in
build/gui-aggregation/gap-closure/omero-cmake-results.json.

Only module source-path resolution changed, making the same definitions reusable
outside the root build directory. The five functional C++ units are unchanged.
One CMake component build, no engine/GUI/binding/codec/SDK/wheel/package rebuild
and no repeated public-server campaign. The shell required the recorded absolute
CMake binary; one documentation patch needed a context correction before writing.
The previous source-scoped OMERO and label receipts remain separate and exact.

Stop here. Source remains uncommitted. Original raw pixels, annotation/result
exchange, private authentication, write-back and packaging are separate owner
selections; the optional priority question has not yet been answered. Do not
infer raw acceptance, platform/biology/resource qualification or further parity
work from this handoff.

## Complete locally - public OMERO connection

Owner authorized open-source OMERO integration and asked to try a public server,
superseding the earlier exclusion. The selected read-only native outcome is
delivered: project/dataset/image browsing, typed original metadata, owned rendered
previews and offline preview-project creation. The optional C++ service reuses
the existing JPEG registry and project authorities; it does not extend the
path-only file ABI. See the [guide](omero-integration.md) and
[evidence](benchmarks/omero/README.md).

Decisive results: authored native protocol tests pass; all six CLI modes work
against public IDR project51/dataset51/image1884807. Lawo et al's study map states
CC BY4.0. The preview was inspected. Project reopening preserves exact IDs/JPEG
identity and embedded pixels, with raw capability and physical calibration
absent. Metadata and offline verification also pass with Python removed from
the environment. Existing output refuses before network work. All86 protected
baseline/candidate/label products remain byte-identical. The separate source/
object/product manifest is build/gui-aggregation/gap-closure/omero-results.json;
current source remains uncommitted and no published product is relabeled.

Cost/correction: only five new optional native client/transport/project/CLI/test
units and one scoped consumer repair were compiled/relinked. No core archive,
kernel, GUI, binding, codec, SDK/wheel or package rebuild. The first project
route used an invalid jpeg alias; corrected it to org.qupathxx.jpeg, pinned
temporary decoder bytes to the immutable preview digest and preserved the
refusal. Native GET-only transport uses platform HTTPS with validation,
explicit same-server redirects, bounds/timeouts and cancellation. No Java/
Python client runtime, credentials, account creation, upload or server deployment.

Stop at this public metadata/preview checkpoint. IDR's legacy OME-TIFF export
returned HTTP500 and full rendering redirected to the viewer; those observations
are retained, not converted into raw acceptance. Previews remain explicitly
uncalibrated and unregistered. Original raw access, private login/SSO, write-back,
remote geometry, streaming, desktop controls and packaging remain separate R15
selections. No broader QuPath, platform, resource or biological claim follows.

## Complete locally - native labels and offline masks

The selected native/CLI outcome is delivered: bounded SLIC regions, per-cell
subcellular spots and offline unsigned TIFF labels share one captured native
label/result boundary. Original raw statistics, rational geometry, exact
input/model/result identities, jobs/cancellation, undoable publication/removal,
kept project recipes and protected new-only exports belong to C++.
See the [guide](label-analysis.md) and [evidence](benchmarks/label-analysis/README.md).
That label checkpoint excluded live integrations; the later public OMERO outcome
above supersedes that exclusion. No model runtime, weights, restricted code/assets
or additional codec was introduced.

Decisive results: four native suites pass, including the existing fluorescence
regression;15 standalone batch routes pass with Python removed from the runtime
environment. Native project save/open/exact replay, byte-identical CSV/QPL,
zero parent counts, sparse/UINT32_MAX IDs, wrong-file/shape/type/input refusals,
corrupt kept associations, edited publication preservation and source bytes pass.
Polygon boundary centers and half-open rectangles retain the existing native
predicate. All78 protected baseline/candidate products remain byte-identical.
Current source/products and commands are identified separately under
build/gui-aggregation/gap-closure/results.json; this source work is uncommitted
and does not inherit candidate9 qualification.

Cost/correction: incremental affected objects/tests and batch dispatch linked
against retained dependencies and15 unchanged CLI objects. No engine archive,
GUI, binding, wheel, package or broad corpus build. Preserved diagnostics cover
the manual include, nucleus-required reducer mismatch (fixed by sharing its raw
accumulator), lazy source capture and independent TIFF fixture offset. Review
also fixed boundary bounds and complete kept/publication fact checks. Patching
needed hunk consolidation/context fixes; the harness needed Windows environment
key normalization. Claude stopped at its account quota before review; no retry.
Five generated root fixture folders were moved to build evidence after checking
absolute boundaries, tracked state and active processes; no data was deleted.

Stop at this native/CLI checkpoint. Desktop controls/bindings and new packaging
remain separate selections. ICC stays R3; eligible reader/project expansion
R2/R5; broader scientific cases R6/R8/R10; whole-slide/process and Linux/macOS
desktop evidence R12/R14. Specific model runtimes and further offline exchange
remain R15 opportunities. Do not automatically reopen these catalogue rows or
claim full QuPath, biological, platform or resource acceptance.

## Complete - publish the molecular Windows prerelease

Owner-selected [Windows candidate9](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.9)
is public at packaging commit a9422a1b. All seven asset names/sizes/SHA-256 digests
match admitted local bytes: three unchanged product/source ZIPs, a ten-member
verification ZIP, RUN, manifest and checksums. Anonymous metadata downloads and
all four ZIP routes pass; tag target is exact and all eight prior releases are
unchanged. See [publication evidence](benchmarks/molecular-desktop/README.md#windows-candidate9-publication).

Reused the passing native/UI/install evidence and already completed Linux
material-admission CI (36 passed); no new product build or qualification campaign.
The current guide and publication receipt preserve the pre-publication source
ZIP's identity and recorded Windows workstation/prefix scope.

Cost/correction: API text needed explicit UTF-8 instead of Windows GBK; draft
verification needed the existing release ID because the tag route omits drafts.
One upload, no release recreation/asset retry. Raw local evidence remains under
build/gui-aggregation/molecular-delivery/publication/. Record these documentation
changes in the follow-up commit/push, then stop. No SDK/wheel, research pilot or
further feature phase is selected by this continuation.

## Complete - Windows molecular desktop delivery

Owner-selected continuation delivered: bda67ad8 is pushed to origin/main, and
the validated desktop has matching source, dependency-source and notice assets.
See [the candidate guide](windows-molecular-desktop-candidate.md) and
[the workflow receipt](benchmarks/molecular-desktop/README.md#windows-desktop-package-handoff).
All three ZIPs are under build/gui-aggregation/molecular-delivery/. The finite
desktop-only profile binds the retained native base, distinct five-unit
correction and frontend without relabeling or packaging the frozen SDK.

Decisive results: all67 desktop,1190 source and17 dependency ZIP/stage members
pass admission;35 packaging checks pass (one host symlink-capability skip).
A fresh external per-user Windows prefix passes real kidney project save/open,
exact result replay and byte-identical CSVs, then app startup with Windows-only
PATH/no Python environment and package-local resources. Removal deletes68 owned
files (67 package + one development probe), preserving all7 research/user files.
All71 frozen and six validated candidate product hashes remain exact. This is
existing-workstation/prefix evidence, not fresh-VM or biological acceptance.

Cost/correction: one public-native probe object/link; no engine, GUI, binding or
SDK rebuild and no repeated numerical campaign. Corrected the new stager's
missing-file diagnostic; confirmed source/UI/ImGui EOL-only correspondences.
Removed only two generated bytecode caches after using the frozen source tools;
all stage memberships and ZIP hashes remain exact. Use Python -B for that check.
That checkpoint stopped before release upload; the subsequently selected
publication is recorded above. SDK/wheel and further features remain unselected.

## Complete locally - molecular desktop plan end to end

Delivered the owner-selected remaining Genes workflow through native A03 PCA/
clustering and A04 replicate differential expression, connected to full-source
QC/normalization and A02 ROI pseudobulk. Both spatial and nonspatial retained
matrix routes work. Native scientific/persistence/export authorities remain
shared; no Python analysis, implicit preprocessing or pseudo-replicates were added.

The complete guide and requirement-by-requirement evidence are in
[the desktop guide](molecular-desktop.md) and
[the receipt](benchmarks/molecular-desktop/README.md). The active local candidate
is build/gui-aggregation/runtime/Release/qupathxx_app.exe; final sources/products
are identified in build/gui-aggregation/results.json. Prior A02 products and all
failure evidence are preserved. The owner's continuation selects the source
commit as this handoff; Git history records its identity. No publication.

Decisive results: four GUI suites, native A03 regressions and58 source contracts
pass. Actual controls cover QC/log-normalization Keep to normalized PCA, explicit
SVD, fitted archive/Keep/open/verify/exports, selected-ROI raw pseudobulk to DE,
analytical effect/p and absent statistics, both saved exact identities, native
exports/refusals, one-undo metadata, blocked/stale/cancel/image scopes and closing
Genes after Run. Normal/French200 values and stored-vs-draft settings are readable.
App startup passes with Python removed from the environment; input bytes and all71
protected product hashes remain exact.

A demonstrated native correctness blocker required a scoped repair: mixed JSON
signed/unsigned comparisons rejected an admitted UINT64_MAX seed on archive open.
Typed native comparisons preserve strict admission and exact old archive/refit
identity. Five units were rebuilt into a distinct candidate core; frozen SDK,
cache, binding and published products remain exact. No ABI/schema or numerical
assertion was weakened. Native boundary and high-seed persistence regressions pass.

Actual correction cost: two combined GUI compilations plus scoped native/test
repairs/relinks. Cheap compilation caught a Windows macro collision; source lint
caught vocabulary/caption wiring; the failing saved-open test gained explicit
result/error checks; oracle mapping stopped naming a retired checkout. A parallel
relink hit the running test executable's Windows lock and was rerun sequentially.
Render inspection prompted explicit stored parameters beside next-run drafts.
All failures/logs remain available; no broad qualification or binding campaign.

Source handoff checks: all22 files match the passing source checkpoint before
these documentation updates; all six executable hashes, the corrected native
candidate and71 protected product hashes remain exact. Reuse that evidence and
check the staged diff/links; no product build is needed for the commit.
Stop at the completed molecular source handoff. Further features, packages and
publication remain separate owner selections.

## Complete locally - desktop regional summaries and pseudobulk

Owner selected desktop access to existing molecular services. Deliver one complete
Genes-tool workflow first: explicitly selected area annotations and cohort labels,
native regional summaries or raw-count pseudobulk, readable exact results, Keep/
reopen/remove through existing project services, and protected new-only summary/
membership CSV exports. Native ROI membership/registration, UInt64/domain rules,
overlap checks, recipe identity and persistence remain authoritative. PCA/clustering
and differential-expression desktop consumers remain subsequent selected slices.

Reuse the indexed Genes source worker and native owner-captured spatial requests;
expensive membership/reduction/replay/export runs on owned worker inputs. Capture
image/plane/source/document context and refuse stale Keep. Closing only hides the
tool. Use shared controls/path fields/output gate and all15 caption translations.
Preserve native core/SDK/binding/runtime products; no scientific kernel, public
binding or package change. Settle controls/test interfaces before one combined
frontend-only build. Evidence and dirty-source backup: build/gui-aggregation/.

Decisive checks: real controls over an authored indexed map, literal UInt64 counts,
ROI memberships/labels/overlap/empty means, cancellation and blocked/stale scopes,
oneundo Keep/remove, saved exact replay, both native CSVs/new-only/protected-path
refusals, normal/French200 readability and existing Genes/QC source contracts.
Reuse A02 numerical evidence; do not run a new broad molecular/platform campaign.
Delivered the controller/panel,38 complete caption rows and focused actual-widget
tests. CSV fields live in the persistent Genes owner and central path registry;
plane eviction cannot leave dangling picker/drop pointers. The new workflow test,
existing Genes and normalization tests,58 source contracts and app project-startup
smoke pass. Exact UInt64 results, saved replay, one-undo metadata, new-only and
protected exports, cancellation/stale/blocked/image scopes and readable normal/
French200 values are covered. All71 protected product hashes remain exact.

Cost/correction: the cheap test compile caught a missing mutable closure before
the large GUI unit compiled. One draw-site lint assumed fields always draw in
their owning file; it now checks the explicit persistent-owner/plane-view wiring
and still requires exactly one draw per field. One frontend build; no core,
binding or package rebuild and no broad qualification replay.

The local runnable candidate, source/evidence identity, usage and remaining scope
are linked in the [desktop receipt](benchmarks/desktop-expression-aggregation/README.md)
and [guide](expression-aggregation.md). Stop at this handoff. PCA/clustering and DE
desktop consumers remain separate selected slices; do not automatically begin
them. Source changes remain uncommitted; no publication was requested.

## Storage cleanup -2026-11-15

Owner requests reclaiming project temporary/build-artifact storage. Inventory and
cleanup records live in build/storage-cleanup-20261115/. Remove only verified,
unused compiler intermediates/bytecode outside protected caches and frozen
products. Preserve scientific inputs/results, source/dirty worktrees, archives,
failure evidence, installed host/SDK and the single current native cache.
For retained inactive binary/font build copies, lossless NTFS compression may
reclaim space while preserving exact bytes and paths. Decisive checks: resolved
path/reparse/worktree/process boundaries before mutation, manifest-recorded
deletions, exact hashes for compressed/frozen products and source status.
Complete:3,219 unused compiler/bytecode files removed and934 retained inactive
binary/font build files losslessly NTFS-compressed, each with exact SHA-256
before/after checks. Per-file allocated storage reclaimed3.73GiB; other drive
activity is excluded from that figure. All71 frozen/current product hashes and
main HEAD remain exact, source status changes only this card, and no file action
failed. Scientific data/results, archives, installed products/current native cache
and the active UI worktree remain intact. No native build or commit.

Correction: the first pass was paused to use actual DirectoryInfo parent traversal
and avoid repeated process scans. Its receipt is retained; all selected cache
deletions were reconciled before resume. Final results.json verifies the complete
manifest and protected bytes. Stop at this storage handoff; no broader deletion
or new qualification campaign is selected.

The selected native-format workflow is delivered. The owner now requests cleanup,
simplification and a source commit.100% QuPath parity is not required. Preserve
scientific inputs, calibration, measurements, exact stored identity and export/
replay. [AGENTS.md](../AGENTS.md) and the
[Windows checkpoint](api-delivery-plan.md#next-delivery-checkpoint-windows-research-workflow)
own scope and validation policy.

## Cleanup and commit

The cleanup consolidates duplicate artifact checks in the typed-plugin stager and
moves accumulated status cards into the existing
[task history](current-task-history.md). Each delivery receipt remains its own
qualification authority. Dirty-source backup and cleanup evidence are retained in
build/format-cleanup-2026-11-14/.

Decisive checks: existing typed-family validation through the simplified admission
guard, corruption/missing-artifact refusals, local links, material/deployment
regressions and clean staged diffs. Scientific services, ABI headers, plugins,
build recipes and qualified products retain their bytes; no native rebuild is
needed. Commit is authorized. No push or publication is selected.

Cleanup checks pass: the existing76-command report and four admission refusal
controls,18 material/deployment tests (one Windows symlink skip),192 local links
and diff checks. Native source/host/SDK and all68 frozen artifacts remain exact.
The existing history archive was reused with its prefix and old card bytes
preserved. The requested local source commit is the handoff; Git history owns its
commit ID. No additional implementation or validation campaign is selected.

## Delivered native formats

C++ owns scientific/application logic and publication; Python remains optional.
Additional formats are native plugins with actual licenses, notices, source
obligations and exact identities under the [license policy](reader-license-policy.md).
NC/research-only/proprietary required materials are excluded; unknown terms remain
unreviewed. Java/Groovy compatibility is outside owner scope.

| Workflow | Qualified declared profile |
| --- | --- |
| PNG | [Static typed color/depth/Adam7, raw/display/calibration](benchmarks/png-reader-plugin/README.md) |
| JPEG | [8-bit Gray/RGB, orientation/calibration](benchmarks/jpeg-reader-plugin/README.md) |
| PNM/PAM | [P1-P7 UInt8/UInt16, explicit transfer interpretation](benchmarks/pnm-reader-plugin/README.md) |
| BMP | [Selected BI_RGB header/palette/RGB/ppm](benchmarks/bmp-reader-plugin/README.md) |
| WebP | [Simple static lossless VP8L, straight RGBA](benchmarks/webp-reader-plugin/README.md) |
| NRRD | [Attached raw scalar XY/XYZ, dtype/endian/Z/calibration](benchmarks/nrrd-reader-plugin/README.md) |
| ICS | [ICS1/IDS or inline ICS2, typed C/Z, guarded file sets](benchmarks/ics-reader-plugin/README.md) |
| Counts and point TSV | [Typed matrix/annotation plugins and matching host/SDK](benchmarks/format-family-plugins/README.md) |

Owner-selected NRRD, ICS and matrix/annotation workflows are complete locally.
The [family guide](format-family-plugins.md) specifies exact UInt64 counts/IDs,
complete position joins, frames/classes/planes, native image bounds, new-only QMX
and one undoable annotation append. Schema4/per-object receipts retain mappings;
legacy QMX schemas1-3 and exact old hashes remain intact.

## Qualified products and evidence

FP04 source context is fb5289df36ac708fa1ee45b581ad1c2d66daa404c3dc6eed5460480160c78f1c.
build/format-family-plugins/results.json binds76 native/SDK/CLI commands,18 valid
profiles,50 exact parser refusals and23 selected native suites.32-member package/
ZIP admission,16 deployment commands, seven-image-plugin exact display and removal
preserving user outputs pass.18 material/deployment tests pass with one Windows
symlink skip. Disabled branches compile; that is not a second linked-profile
qualification. No GUI/binding/wheel delivery is claimed.

Keep the single active native cache/host/SDK under build/format-family-plugins/.
All68 earlier frozen artifacts and prior inventory groups remain exact. Preserve
source snapshots, failures, intermediate products, fixtures and cleanup archives.
Outside comparison builds are retired after checked boundaries/process use.
The measured47-byte inactive directory-provenance correction preserves the original
11489+path-adjusted exact/one-below witness while fully charging active metadata.
The [receipt](benchmarks/format-family-plugins/README.md) records corrections and
cached build costs; scientific, identity and quota assertions retain their contracts.

## UI workflow safety - delivered and merged

The [UI safety plan](ui-workflow-safety-plan.md) Milestones 1-6 and the follow-up
layout/translation pass are committed on feature/ui-workflow-safety and merged
here. The [receipt](benchmarks/ui-workflow-safety/README.md) lists results,
coverage limits and the defaults taken without individual owner confirmation;
evidence stays in the sibling checkout's build/ui-workflow-safety/. Before the
merge the branch passed 140 of 140 native matrix cases on real paths and 86
source contracts. Optional native Milestone N remains unselected.

## Remaining scope and stop condition

The [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
and [178-row census](format-plugin-census.json) catalogue conditional opportunities.
Other encodings, directory-store annotation targets, geometries, stores, writers,
remote IO, UI and bindings need a concrete selected workflow and their own
contracts/admission/evidence. The full format roadmap remains open.

Stop after cleanup and the requested source commit. No additional parity,
platform/corpus, biological, speed or public-release campaign is selected.


<!-- Archived current card before PNG label-mask continuation, 2026-11-16. -->

# Current implementation task

## Complete - simplify, organize and commit native integrations

Owner-selected cleanup is complete. The local source commit contains the pending
native label workflows, OMERO services and optional Blosc provider. OMERO CLI
parsing, preflight and execution now share one private frontend with native tests;
repeated atomic/new-only exports share one helper. New C++ is readable, label
CMake tests are grouped, and completed status detail is preserved exactly in the
existing [task history](current-task-history.md).

Decisive checks pass: four label/fluorescence/workflow/CLI suites, three OMERO
CTests, configured codec/mask ABI checks, all nine OMERO CLI routes, offline
preview reopening and both standalone command entry points. Tests use a Windows-
only PATH without Python environment variables. Scientific formatting-only
changes retain canonical source equivalence; no numeric or identity assertion
was weakened. The root Python-off configuration, local links and staged diff
checks pass. Verbatim upstream notice bytes/whitespace are preserved through a
file-specific Git attribute. All 112 preceding products remain byte-identical.

The existing integration cache builds this distinct source under
build/gui-aggregation/gap-closure/cleanup/. That directory retains the55-file
initial source snapshot, exact checks/products and cleanup receipt. Windows
MODULE outputs needed a library-output correction: the prior DLL was restored
from its verified copy, and output/PDB relocation cost one extra codec compile
pass. Targeted re-staging preserved original notice bytes after Git line-ending
normalization was detected. No engine archive, GUI, binding, SDK/wheel, release package or public/corpus
campaign was rebuilt/repeated. Git records the authorized local commit; no push
or publication is selected.

## Delivered workflow and support

- [Native labels](label-analysis.md): bounded SLIC, subcellular spots and unsigned
  TIFF masks; shared native capture, measurements, publication/undo, persistence,
  exact replay and protected exports. [Evidence](benchmarks/label-analysis/README.md).
- [Native OMERO](omero-integration.md): public metadata/previews, explicit linked
  NGFF raw blocks and original-coordinate ROI/result exchange. Public raw and4280
  point shapes have earlier source-scoped evidence; masks have native fixtures.
  IDR denies live mask pixels with HTTP403. [Evidence](benchmarks/omero/README.md).
- [Blosc provider](../plugins/blosc-bytes/README.md): existing native image ABI,
  pinned open sources, exact module identity and actual component notices.
- [Windows candidate9](windows-molecular-desktop-candidate.md): preserve its
  native/runtime/source identities and published assets unchanged.

The [format inventory](format-plugin-coverage-plan.md) and
[license policy](reader-license-policy.md) retain detailed support/admission
boundaries. Earlier receipts remain their own qualification authorities and do
not qualify changed source or instruct another campaign.

## Stop condition

Stop after the local commit with a clean working tree. Private OMERO auth/write-
back, live IDR masks, raster editing, nonidentity transforms, whole-image streaming,
desktop/bindings and packaging remain unselected. R0-R16 and the format census are
reference catalogues, not promises of full QuPath parity. Keep C++ authoritative,
Python optional and each plugin's actual license/source/distribution obligations.


<!-- Archived PNG label card before owner-selected full qualified catalogue goal, 2026-11-16. -->

# Current implementation task

## Complete - native PNG label-mask interoperability

Owner request: close remaining gaps. A08/R15 now imports externally produced
integer grayscale PNG labels through the existing optional PNG plugin and shared
native label services. Original label IDs, original-image raw measurements,
geometry/publication/undo, save/reopen, exact replay and protected CSV/QPL/project
exports use the existing authorities. C++ owns interpretation; Python is optional.
The preceding cleanup card is preserved in [task history](current-task-history.md).

### Decisive result

Seven affected native checks pass, including the existing label/fluorescence/
workflow/CLI suites, reader-plugin host, missing-provider refusal and configured
PNG integration. Independent1/2/4/8/16-bit fixtures retain stored codes including
65535. Decoder identity drift, malformed/color/alpha/palette/wrong-size masks,
changed sources, cancellation and insufficient work refuse. Twelve standalone
batch routes pass with Windows-only PATH and Python environment variables removed.
Native and CLI PNG records, CSV and QPL agree exactly.

The pre-change TIFF project retains record48058aca and its original CSV SHA256
12dba940a9c17894fd7f86394b5538ff9459972f6d264c0948c2cf8679d9de41.
TIFF's existing parameters and exact identities are preserved. PNG recipes bind
format and exact provider/version/contract/options/module/dependency identity;
the composed decoder receives a bounded allowance within remaining work.

The root Python-OFF configuration and actual generated CMake consumer compilation
pass. The integration test explicitly owns its header-only JSON dependency.
All398 local documentation links, diff checks and new-test formatting checks pass.
All130 preceding products remain byte-identical, including the frozen candidate9
and admitted PNG module. There was no engine archive, GUI, binding, SDK/wheel,
release packaging or public/corpus campaign. Changes remain local and uncommitted;
no push or publication is selected.

### Evidence and actual correction

The existing integration cache retains distinct objects/runtime, source/product
hashes, fixtures, commands and checks under
build/gui-aggregation/gap-closure/png-labels/. Reuse compatible passing objects.
A preflight script first used an obsolete receipt key and stopped before native
execution. One new-test compile needed the explicit workflow-exception header;
only unfinished targets were resumed. CMake review caught the private JSON include
before its consumer compile. A documentation patch needed the exact existing
matrix-row context. No assertion, quota, numeric or exact-identity rule was weakened.

[Guide](label-analysis.md#offline-label-masks) and
[receipt](benchmarks/label-analysis/README.md#png-label-mask-continuation) record
support and identity; the [format inventory](format-plugin-coverage-plan.md) and
[license policy](reader-license-policy.md) retain per-profile admission boundaries.

### Stop condition and remaining scope

The complete native/CLI PNG-mask workflow is delivered. Desktop/bindings/release
packaging, NGFF labels, color/palette/alpha-mask interpretation, model execution,
private/write-back OMERO and broader biology/platform/resources/speed acceptance
remain separate selections. R0-R16 and the format census remain reference
catalogues under the owner scope policy. Preserve the frozen Windows products;
select the next concrete workflow before extending their support claims.


## Archived QOI interoperability card2026-11-17

# Current implementation task

## Active goal - close all other qualified catalogue

Owner explicitly selected all qualified formats and application/integration gaps,
confirmed2026-11-16. The full objective persists across packets and turns. Use the
existing [format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
and [R2-R16 queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
as the authorities; this card is the single active implementation outcome, not a
replacement or smaller definition of the goal. Do not mark the goal complete when
one plugin/workflow passes. Closed/accepted earlier scopes remain delivered.

Qualification requires actual open/commercial-use-eligible schema, implementation,
codecs and required materials, a representable native contract and evidence for
the promised workflow. Investigate remaining candidates and contract-first items;
unknown terms stay unresolved, not silently admitted or counted as implemented.
Excluded Java/Groovy, restricted materials and cosmetic pixel parity stay outside
owner policy. Scientific, resource, host/package and biological acceptance require
their own actual evidence; no synthetic substitute or inherited qualification.

### In progress - QOI native image interoperability (I08/R2)

The official QOI specification1.0 is CC0; the pinned MIT reference codec is a
separate development oracle only. Implement a standalone optional C++ plugin
through the existing image ABI, with no core/Python/codec runtime dependency.
Expose canonical lossless RGBA8 codes, including hidden RGB and alpha regardless
of the informative channel hint. Treat color-space0 as encoded sRGB; convert
linear RGB to sRGB for display only. Raw codes stay exact. Calibration is unknown.
Use strict bounded decode, all six opcodes, cancellation and exact native source/
implementation identity. No image/benchmark corpus download is selected.

Before building, finish the codec/ABI/resource contract and consumer tests.
Decisive checks: independently authored opcode/edge cases and pinned reference
agreement; native C ABI buffers/requests/status/work/cancel; region CLI crops/raw;
SDK project attach/save/reopen/replay, missing/changed implementation and source;
license/staging guard, relocated offline module build and owned-file removal.
Preserve source/failure/results and exact module identity in the existing evidence
and material inventories. Reuse the retained ReaderPluginKit/SDK and native
libraries; no core/GUI/binding rebuild is needed for this reader.

### Current result and build decision

The retained automatic region route refuses an independently authored valid QOI
with OpenSlide cannot open or recognize (recorded before implementation). The
CC0 specification and exact MIT oracle revision/hashes are recorded separately.
Codec, bounded shared ABI helper and native/SDK tests now have settled interfaces.
One combined plugin/test/reference/consumer CMake build reuses the retained kit
and SDK; its decisive risk is exact opcode/alpha/linear-display interpretation
and persisted source/module identity. No native engine/binding build is needed.
All40 authored/reference profiles pass metadata/raw/display/crops/buffers, but
the final budget-test precondition still counted the borrowed scaffold's single
channel instead of RGBA's four. Correct the decoded-byte proof and rebuild only
that test target. The production module is unchanged; preserve the first failure
and continue the remaining refusal/native-consumer checks without weakening them.

The corrected direct ABI check passes all40 positives and26 refusals; all five
native project/source/implementation modes pass. The retained region CLI exposed
an actual consumer gap: --raw accepts TIFF only. Close it through a shared typed
C++ raw-file export service, with CLI parsing delegating to that service. Preserve
legacy TIFF schema1 and NumPy bytes; plugin schema2 retains the existing native
reader snapshot/metadata. Guard every primary/companion through read/export;
refuse unsupported directory-store snapshots and missing-validity array exports.
Before the affected cached frontend/service/test build, pin the old TIFF output
and exercise raw QOI/linear/channel subsets, source aliases, TIFF-only options,
work/axis/missing-provider failures and other admitted plugin file profiles.
The module/SDK/reference objects remain passing; no engine/archive rebuild.

The first raw service/CLI checks pass exact QOI linear codes and the original
TIFF NumPy bytes/report. A development harness imported unavailable NumPy before
native work; use independent stdlib header/byte validation instead of installing
or weakening array checks. An owned hard-link fixture reproduces source corruption
in the retained preview CLI. Pull this correctness blocker into the same native
region exporter: shared PAM serialization, atomic/new-only publication and
existing native source/companion/directory alias protection. Only that service,
CLI and its affected tests need rebuilding; the QOI module remains unchanged.

Final handoff check now freezes actual module/executable/manifest hashes around
the small native ABI/SDK/CLI group. Earlier development logs did not bind every
artifact digest, so the final group supplies that missing source/binary evidence;
reuse authored/reference fixtures and all unchanged core/codec qualification.
Include the new shared exporter, legacy TIFF byte/report equality, raw plugin
samples and preview hard-link regression. This is not a scientific/platform corpus
campaign. After this group, stage and independently rebuild only the module kit.

The24-member initial package passes admission. Its relocated source kit rebuilds
without core/Python discovery, and the independent module passes all40/26 ABI
profiles with its own recorded binary identity. Automatic command review rejected
the combined deployment/removal proof before execution, without a specific reason;
no files changed. Split native consumer checks from narrowly scoped, verified
package-file removal. Preserve the passing module/build instead of rebuilding.

The independent binary passes fresh native project create/reopen. Original-module
deployment and narrowly checked24-file removal pass, preserving six user files
and offline metadata/history. The only remaining build-system risk is the root
CMake wiring for the new service/test: configure native-only with retained JSON
source and no optional dependencies, verify the generated entries, and do not
compile the unchanged core. Final docs/package checks reuse the same module.

Native-only root configuration passes and wires the new service/test with Python
OFF and no discovery. A post-configure harness misspelled `utf-8-sig`; corrected
only the project-file read and reused the successful configure. No rebuild/retry.

### Evidence and preservation

The preceding PNG-mask work remains passing and uncommitted, with its exact card
in [history](current-task-history.md) and source-scoped receipt under
build/gui-aggregation/gap-closure/png-labels/. The initial full-goal snapshot and
140 protected products are retained under build/gui-aggregation/catalogue-closure/.
Use the one integration checkout/cache/runtime; do not overwrite prior modules,
fixtures, packages, qualification or dirty-source work. No commit/push/publication
is selected by this packet. The published Windows candidate9 stays frozen.

### Completion and next action

After QOI passes, update its existing I08/R2 row with evidence and continue the
next qualified gap under the full active goal. Admission investigation, additional
readers/writers/stores, native interoperability, consumer surfaces, automation and
applicable display/resources/host/delivery work remain open in their existing rows.
A packet closeout is progress; the goal ends only after a requirement/evidence
audit proves the whole qualified catalogue scope complete. Run broad validation
only for changed inputs or a named release/platform/scientific risk.

Final outcome2026-11-17: QOI, the shared native raw/PAM exporter and the thin CLI
pass the declared local handoff. Final24-member directory and ZIP admission pass;
all140 protected products and the original18-file dirty snapshot remain exact.
Results: build/gui-aggregation/catalogue-closure/qoi/results.json. No publication,
commit or full-goal completion is claimed. Next: typed native annotation geometry
with a concrete open box/polygon import provider; reuse existing native Roi,
preview/apply, identity, project and undo authorities.


## Archived annotation geometry card2026-11-17

# Current implementation task

## Active goal - close all other qualified catalogue

Owner explicitly selected eligible formats and application/integration gaps on
2026-11-16. The full goal persists across packets. The existing
[format roadmap](format-plugin-coverage-plan.md#full-format-roadmap-2026-11-14)
and [R2-R16 queue](api-delivery-plan.md#execution-order-for-finishing-the-remaining-work)
own remaining scope. Java/Groovy compatibility, restricted materials and cosmetic
pixel parity remain excluded. Unknown terms remain unresolved. No packet alone
completes the goal or establishes broad platform, biological or speed acceptance.

## In progress - native annotation geometry plugins (A06/R4/R15)

The delivered annotation ABI accepts only one point per object, blocking native
box/polygon exchange. Add a separately versioned annotation ABI2 entry; preserve
matrix ABI1, point ABI1 and their exact receipt/row identities. The declared
geometry transport is existing native QRO version1, not arbitrary serialized
application objects. Bounded host-owned row/text/geometry buffers carry exact
source IDs, class labels and explicit C/Z/T planes. The existing native geometry
decoder/Roi authority validates primitives, paths, holes and topology. No plugin
receives a mutable Document or publishes project/results.

Add one independent, openly licensed pixel-geometry TSV provider with an exact
named schema for rectangle/ellipse/polyline/points/polygon/multipolygon. Reuse the
existing preview/apply transaction, explicit class mapping, source/target guards,
class/source/module identity, native bounds, undo and persisted object receipts.
Directory-store targets and inferred affine/unit transforms remain unsupported.
No Java, Python fallback, third-party parser, model or external asset is needed.

Settle the ABI, native quota/identity contract and provider interfaces before one
combined cached build of the changed host sources, module, CLI and tests. Reuse
the frozen native SDK/libraries and geometric kernels; do not rebuild bindings,
GUI, wheels or an entire engine. Decisive checks: C layout/v1 compatibility,
independent exact primitive/path/plane/ID cases, invalid/malformed buffers and QRO,
work/cancel/Unicode, stale/invalid atomic apply, undo/save/reopen/offline receipts,
changed source/module identity, old point preview/receipt identity and mixed
matrix/annotation catalogs. Then admit the matching module/source and changed
native consumer overlay with independent offline kit and removal evidence.

## Preceding result and preservation

[QOI and shared raw/PAM export](benchmarks/qoi-reader-plugin/README.md) pass the
local handoff:40 positive/26 refused profiles,32 native checks, eight-reader raw
exports, distinct offline module build,24-member directory/ZIP admission and
owned-file removal preserving six user files/offline history. Results and exact
source/product identities are in build/gui-aggregation/catalogue-closure/qoi/.
All140 original protected products and the18-file prior dirty snapshot are exact.
The [archived card](current-task-history.md) records its diagnosed harness failures
and automatic review rejection, which was resolved by smaller verified operations.

One integration checkout is active. Preserve earlier PNG work and frozen SDK/core,
published candidate9, GUI/bindings/wheels and all evidence. The new host/source
needs its own identity. No commit, push or publication is selected. After this
packet, continue the next qualified catalogue gap; do not mark the goal complete.

Current implementation: separate ABI2 admission/session dispatch and geometry
transport reuse the native decoder and preview/apply authority; the standalone
geometry TSV encoder is in place. No changed host build has run yet. The retained
point CLI control first replaced its option argument instead of its output path,
so parsing refused before publication. Correct the selected output argument and
keep that failure; no product/interface change is involved.

The corrected retained CLI control now pins the original point preview/receipt
identity. Interfaces and consumers are settled: annotation ABI2 uses bounded QRO1
pages; original point/matrix ABI1 identities remain separate. Before the combined
cached build, native and independent controls cover18 encoded profiles,41 parser
refusals, five native geometry/bounds refusals and malformed-provider seams.
Compile only changed host sources, the new module/CLI/tests and affected old
point/matrix tests against retained SDK libraries. Preserve the frozen core and
use a separate matching source/kit identity, with no Python discovery.

First cached host build compiled the loader/runtime, then stopped because the
standalone overlay lacked the private SQLite header used by shared matrix import.
Add the already-retained SQLite include to that development harness and resume
the affected target. Preserve passing objects; no product/source contract change.

The combined build then passes. Pre-run interface review found that an immutable
RGB test target cannot represent C0 planes; switch the test to the existing
authored quantitative TIFF oracle and verify all768 raw codes before capture.
Rebuild only that test. All44 unchanged transitive host headers match the retained
SDK source. The extra core.hpp difference is one friend declaration, with no
layout/behavior change; the four ABI/import/runtime headers are deliberate new
overlay inputs. This compatibility evidence does not replace the new host identity.

The first hash-bound group passes C layout and reaches cancellation in the direct
ABI test. That new test expected numeric5, but the unchanged image/format status
contract defines CANCELLED=1 (5 is INTERNAL_ERROR). Use the named cancellation
constant and rebuild only the test executable. Preserve the module and first
failure; no implementation/status contract or assertion strength is changed.

The corrected group passes all18/41 direct profiles, create/reopen/offline,
native invalid/stale/quota/cancel cases and valid source-change identity. The
first controlled malformed-metadata case correctly refuses and destroys its
handle, but its test event writer used Windows text CRLF while the byte oracle
requires LF. Switch only that test writer to binary mode, rebuild only the bad
module, and reuse the hash-compatible passing host/module/test evidence. Keep
the first event bytes and failure; continue the still-missing fault/CLI/v1 checks.

All57 bound native checks now pass; original point preview/receipt/project rows
remain exact, and mixed ABI1 matrix/point plus ABI2 geometry catalogs load cleanly.
The new module/host archive/CLI have distinct recorded identities. Next decisive
handoff: configure root native-only CMake using retained JSON/SQLite sources to
verify ABI2 kit/module/test wiring, without a core build; admit the standalone
module/source kit, rebuild it offline against only that kit and verify relocation
and owned-file removal. The changed native consumer stays a separate overlay over
the exact earlier SDK; package its exact source context alongside that base's
existing source archive/notice receipt. Preserve all passing products and failures.

Root native-only configuration passes ABI1/ABI2 kit, module and test wiring with
Python default OFF; no core compile. The first stager lookup assumed lib/ instead
of the SDK's imported lib/qupathxx/ location and stopped before staging. Correct
the lookup from the actual target manifest; reuse the unchanged qualified binaries.

The133-member module/kit/native-overlay package passes material admission. JSON
headers match the pinned MIT archive with individual notices retained; the
SQLite interface header/notice matches its pinned public-domain source. Neither
is a module dependency. Next build risks are offline source completeness and
public-only C++ header/archive linkage over the exact frozen SDK. Build the
relocated module, one public SDK consumer and only the three overlay services plus
CLI from staged sources; verify exact native project/receipt values. No core,
kernel, GUI, binding, wheel or base SDK rebuild is justified.

The relocated module, public-only SDK consumer and source-rebuilt native overlay/
CLI all build and pass ten additional native checks. Exact geometry/receipts and
the original point control agree; both rebuilt binaries keep separate hashes.
The full native header closure adds one unchanged C ABI header, giving45 exact
matches plus the documented friend-only and four deliberate overlay differences.
Private133-file deployment/removal passes, preserving six user files and native
offline geometry/history. No recursive delete or frozen-product rebuild occurred.
Finish the final docs/source package/ZIP/link/diff and140-product preservation
checks; a documentation patch context was corrected without applying partial
edits. Then record the completed A06 scope and move to the next qualified named
annotation format. The full goal remains active.

Final outcome2026-11-17: A06 pixel-geometry TSV, ABI2 and native CLI/SDK
overlay pass57 native and ten offline checks,18 exact/41 refused profiles,
133-member directory/ZIP admission and owned-only removal preserving six user
files. All140 protected products, previous snapshots and QOI source remain exact.
Results: build/gui-aggregation/catalogue-closure/annotation-geometry/results.json.
The full qualified-catalogue goal remains active; next is A02 ImageJ ROI/ZIP
admission and native interoperability through this delivered geometry authority.


##2026-11-17 - selected native ImageJ ROI/ZIP interoperability delivered

Optional org.qupathxx.imagej-roi1.0.0 uses separate annotation ABI3 for source
names/properties/colors and explicit native plane interpretation. Shared C++
Roi/preview/apply/identity/class/project/undo authorities remain authoritative.
The selected literal ROI217..229/stored-Deflate ZIP/ZIP64 contract passes26
authored positive/37 refused profiles,28 ABI3 controls,57 affected legacy checks
and exact native/CLI/SDK/offline paths.185 admitted directory/ZIP members carry
matching new sources/notices/kit/native overlay/CLI;185 owned files are removed
while six user files and saved offline results remain exact. All161 prior
products,12 source-before snapshots,18 original dirty snapshots and QOI product
sources are preserved. Evidence: build/gui-aggregation/catalogue-closure/
imagej-roi/results.json and docs/benchmarks/imagej-roi/README.md. This is a
source-scoped local engineering delivery; the full qualified-catalogue goal
remains active. No base SDK/core/runtime/GUI/binding/wheel rebuild or publication.


##2026-11-17 - selected native WKT/WKB and shared source producer delivered

A03 adds optional org.qupathxx.wkt-wkb1.0.0, image-pixel-wkt-wkb/1, over
unchanged annotation ABI3/native authorities. Seven basic2D types, raw/both/mixed
byte-order WKB, hex transports and WKT preserve literal binary64 coordinates,
strict closure, source membership and exact IDs.48 authored positive/79 refused
profiles and14 process checks pass. Shared fixed-arena rows/facts/QRO encoding/
producer callbacks also pass ImageJ26/37 profiles and12 process checks under a
new source/binary identity. Source/SDK/native host/CLI reuse is input-bound.
Both independent staged source builds, selected offline10/6 and8/6 profiles,
coexistence and unchanged public SDK consumer match exact native outputs.194
individually admitted directory/ZIP members and owned-only removal pass; nine
user files survive and both results reopen without providers.354 prior products/
source-package members, seven source-before files,18 original dirty snapshots,
QOI sources and40 earlier inventory groups remain exact. Evidence: build/
gui-aggregation/catalogue-closure/wkt-wkb/results.json and the A03 receipt. No
kernel/host/GUI/binding/wheel rebuild, publication or full-goal closure.

##2026-11-17 - A04 OME ROI XML and native ABI4 metadata integration

Selected OME2016-06 vector annotations are delivered locally through the optional
org.qupathxx.ome-roi provider. Native image/unbound selection, seven primitives,
binary64/affine interpretation, planes, immutable source metadata and separately
negotiated Locked reuse native Roi/frame/capture/apply/undo/project/CLI/SDK.
QOX2 retains inherited/rebound/undeclared namespace contexts for QName metadata;
that final correctness repair has its own module/source/package identity.
Candidate01 source/binaries/packages/ZIP/receipts remain frozen. Expat2.10.0's57
selected inputs retain MIT/CC0 terms; the exact OME schema remains CC-BY-4.0.
No upstream image/test/model/NC/unknown asset or Java/Python runtime is admitted.

39 positive/64 refused profiles and17 native/CLI checks pass, with exact independent
geometry/DOM metadata oracles, source/plane/lock/state/undo/persistence and bounded
XML/allocator/cancel controls.50 affected native ABI compatibility checks retain
original ABI1 and ABI2/3 identities; frozen WKT/ImageJ modules match exact previews,
receipts and objects. The unchanged host/CLI/SDK reused that evidence after the
module-only namespace repair. Changed producer candidates pass WKT48/79 and
ImageJ26/37. Offline OME11/8 profiles and15 SDK/coexistence checks pass.207-member
directory/ZIP admission and removal pass: nine user files remain, two saved
results reopen provider-free.554 earlier products,24 source-before snapshots,18
original dirty snapshots, QOI sources and41 prior inventory groups remain exact.

Corrections honored the existing interactive-lock/scripted-edit contract, made
an intended overflow fixture actually overflow, used UTF-8 for private Expat C
compilation and corrected source-selection/harness paths. Failed attempts and
source/fixture variants remain. No numeric/identity assertion was weakened; only
affected module/test inputs were recompiled over the frozen base SDK. Durable
lessons were appended to compilexx's canonical note. Evidence: build/gui-
aggregation/catalogue-closure/ome-roi/results.json and the A04 receipt. No full
core/kernel/GUI/binding/wheel, corpus/platform, biological/speed, commit, push or
publication campaign. The broad formats AND application/integration goal remains
active; A05 eligibility intake is next, not an assertion of universal parity.

##2026-11-17 - A05 ASAP XML and shared bounded XML source support

Selected native ASAP Dot/PointSet/Polygon/Rectangle/two-endpoint Measurement
are delivered through an optional ABI3 provider. Original decimal text and
upstream double-to-binary32 values, Coordinate Order, exact document-index IDs,
independent display names, group ancestry/attributes/class mapping and root-group
color are retained through the shared native geometry/import/undo/project/CLI/SDK
authorities. Existing fixed-arena XML/QOX2/Expat support is shared with OME, whose
separate schema/image/binary64/plane/lock interpretation remains unchanged. New
XML module identities remain distinct from old frozen products. No ASAP/Qt/
pugixml runtime/code/dataset/test/image/model dependency; exact reference/author
notice GPL-2.0-or-later, implementation AGPL-3.0-only, Expat MIT/CC0. Aperio
schema terms remain unreviewed; open reader code is not format admission.

ASAP33/69 and17 native/CLI checks pass; shared OME39/64 and17 checks pass.
Direct controls include finite exact float32/ties/subnormals, SSE mode/exception
mask refusal, bounded XML allocation/cancellation, probe/fill ownership, source
staleness, explicit planes/classes, undo/redo and persistence.15 offline/SDK/
coexistence checks pass: ASAP12/8, OME8/6 and four loaded providers.218 directory/
ZIP members pass exact source/license admission/removal; nine user files remain
and two saved results reopen without providers.1404 prior products,17 source-
before snapshots,18 original dirty snapshots, QOI sources and42 prior inventory
groups remain exact. A04 host/CLI/SDK/base runtime stay byte-exact; only optional
modules/new tests were compiled in the reused cache.

The controller's extra unused class maps correctly changed recipe identity; the
repair matched per-case native options and reused the bound passing native suite.
Staging reused an identical borrowed test after duplicate ownership refusal. A
final hardware-exception guard has a distinct ASAP candidate and affected checks.
One intermediate offline test DLL was overwritten before separate archival; its
source/primary/test/packages/receipts remain, but that offline binary is not
claimed retained/reused. The corrected offline DLL/manifest are explicitly frozen
and have fresh evidence. This preservation correction and other durable lessons
are in compilexx's canonical note; failed attempts remain and no numeric/identity
assertion was relaxed. Evidence: build/gui-aggregation/catalogue-closure/asap-xml/
results.json and the A05 receipt. No full core/kernel/GUI/binding/wheel, broad
platform/corpus, biological/speed, commit/push/publication or full-goal closure.

## 2026-11-17 - A08 NGFF integral-label candidate2 local handoff


Continue the named external-label import/publication/export/replay workflow. The
full goal covers eligible formats AND application/integration gaps; A08 is one
selected workflow, not full NGFF/QuPath/catalogue completion. No Java, restricted
material, GUI, binding, wheel, base-core or new scientific kernel build is selected.

The native prerequisites pass19 controller routes and the retained TIFF/PNG exact
identity consumer. New host-owned bounded directory snapshots share the old
observer/digest authority without changing frozen ZarrStore layout or old file/
compound link/1 identities. SOURCE_STORE negotiates a separate16-byte extension
over the unchanged72-byte image ABI1 table; directories alone use link/2/source/3.
All eight integral label types normalize through one native authority while exact
signed/unsigned source codes retain64-bit identity. Deep empty trees, reparse/
ancestor replacement, whole membership mutation, bounded simultaneous observation,
cancel, decoder OFF, saved explicit-root review and unavailable providers pass.
Old saved TIFF/PNG record/CSV bytes and PNG QPL remain exact at original paths.
The scoped receipt is build/gui-aggregation/catalogue-closure/ngff-labels/
foundation-results.json;177 retained header/context files and30 products are frozen
in foundation-source.zip and foundation-products.zip before further cache reuse.
These checks do not deliver the actual NGFF import workflow.
The initial collector omitted caret-prefixed translation units, so that source
ZIP is explicitly a partial header/context snapshot. Its original receipt is
preserved and corrected; fully enumerated later application/provider sources
include translation units, headers and shortened MSBuild log names. Their own
passing A08 receipts provide current qualification without reconstructing or
claiming an unobserved old source snapshot.

Actual foundation corrections: missing test export macro, retained measurement
object dependency, test-only JSON include and two mixed-separator Windows path
expectations. Logs, sources and available products are retained under ngff-labels.
The second path correction was avoidable duplicate-test repair. No numeric,
source or persisted identity check was weakened; no algorithm object was rebuilt.

Current settled implementation contract: optional org.qupathxx.ngff-labels owns
NGFF0.4/Zarr2 interpretation using existing native OmeZarrSource decoding. A new
separately negotiated LABEL_DESCRIPTION16 callback returns at most64KiB neutral
owned metadata; core parses that neutral schema, never NGFF files. Plain STORE
budgets stay exact; label stores reserve an additional1MiB descriptor workspace.
The generic native frame service checks explicit source/reference/target planes,
units/calibration, one-to-one level-zero XY lattice and integral crop offsets.
Relative scale/calibration tolerance is1e-12; index offset tolerance is1e-9.
Original transforms/metadata still participate in exact persisted identity.
No resize, external-reference fetch, inferred registration or invented classes.

Native NgffLabelMaskOptions keeps authored and producer provenance distinct;
both declare the target raw-input SHA, and only producer origin requires a model.
The new pointer option alternative preserves old captured variant storage. Native
workflow integration retains the directory snapshot, exact code map, original
metadata and store/content/decoder identity through measurements, atomic keep/
publication, save/reopen/replay and protected CSV/lossless QPLM1 bundle export.
Legacy QPL1/TIFF/PNG methods and encoding remain separate. The native CLI is thin.
Provider, ABI, frame/archive and actual workflow tests are being implemented in
parallel with disjoint ownership. Their active source is not yet build-qualified.

Before the next combined cached build: finish these interfaces/consumers, retain
passing products (done), verify exact reused algorithm inputs, then compile only
changed application/observer/adapter/consumer objects in the existing A04 cache.
Decisive checks are actual optional-provider decoding and exact64-bit maps;
source/reference crop/plane/calibration/metadata; complete publication/replay/CSV/
bundle routes; mutation after capture and protected directory outputs; ABI/trust/
budget/cancel/OFF controls; and retained old file/compound/TIFF/PNG identities.
No broad corpus/platform qualification or packaging of intermediate substeps.

Interfaces and owned sources are now settled; configure02 passes. A demonstrated
native persistence blocker is also fixed: direct Project.save/save_new previously
protected target-image sources but not a kept external directory mask. They now
call the same native label-record output guard before database/staging creation;
provider-free metadata save outside the source remains available. The new thin
qupathxx_labels CLI calls the same shared operations as batch/native consumers.
All five reused scientific object inputs still match exactly. Combined build08
will compile changed ABI/store-description, neutral import/frame/archive/workflow/
project guard and thin consumers, the actual optional provider and focused tests;
then run the decisive routes above. Foundation source/product ZIPs are retained.

Build08 and integration01 pass: actual provider/native workflow, all integral
types, null/zlib/gzip/chunk boundaries, explicit source/target planes/crops, native
measurements/contours, authored/producer persistence/publication/replay, CSV and
QPLM1, metadata/identity tampering, whole-store currentness and native project/
CSV/bundle protection. Separate OFF-description and ABI trust controls pass,
alongside19 affected old foundation routes and exact retained TIFF/PNG identities.
Next missing checks: fresh-process CLI creation over those passing fixtures and
the same optional-provider workflow with host builtin NGFF decoding disabled.
Only the latter needs one small OFF-stub/test target; reuse all passing products.
These source-scoped primary checks precede matching source/native/plugin notices
and install/run/removal evidence; A08/full goal completion is not claimed yet.

Standalone01's12 fresh-process routes and decoder-off01's unchanged native
workflow assertions pass. SDK consumer build10/run also pins the exact addon/
frozen-core libraries and reproduces native CSV/bundle bytes through public APIs.
No kernel, base core, wheel or binding rebuild occurred.

Before final packaging, review found a real metadata blocker: reserializing a
precise opaque property through binary64 can change its numeric text. Candidate1
is preserved separately with232 exact source inputs,49 products and its passing
receipts. Candidate2 extracts the existing bounded JSON tree into generic reader/
JSON headers with compatibility adapters; the provider copies original property
object spans. Only label attributes admit opaque wide integer tokens; source IDs,
array/options JSON and scientific scalar bounds stay strict. Integral mask arrays
admit only integer/null fills before successful open, preventing implicit numeric
fill conversion. No native pixel/measurement kernel or frozen SDK source changes.

Next decisive combined build11: new generic-parser tests, changed provider and
actual workflow tests (primary and host-decoder-OFF). Keep passing b0c86a addon/
CLI/SDK products intact. Prove exact opaque integers/decimals through descriptions,
objects, CSV, bundle and replay, plus strict malformed-fill refusals and unchanged
default parser/annotation interfaces. Then finish source/material/install/removal
checks. The material guard adds only the verified W3C-20150513 report license;
unknown/restricted/NC source admission remains excluded.

Build11 passes. Candidate2 generic parser, primary workflow and host-decoder-OFF
workflow pass; exact opaque property spans reach all declared outputs and malformed
integer fills refuse. Standalone02's12 routes and public SDK consumer02 also pass
with the new manifest/module, exact native CSV/bundle bytes and source tokens.
Unchanged ABI/frame/archive/normalizer/old-format controls retain their passing
receipts after exact binary checks; b0c86a addon and scientific objects stay frozen.
Next: verify root Python/decoder-OFF configuration wiring, then one matching
source/native/plugin material stage, installed run/removal and final ownership/
source/retention checks. No further native rebuild is selected absent a failure.

Root native configuration passes with Python/NGFF decoder OFF and reader plugins
both OFF/ON, without Python discovery or a product build. Stage01 then stops at
an older exact-SDK-header material record's missing font ancestor. No output or
inventory write occurred. Preserve failing stager/inventory identity; diagnose
the exact selected record and use a valid observed matching source lineage.
Do not relax admission, delete old parents or recreate unrelated font assets.
Diagnosis isolates the500-byte generated sdk_abi.hpp. Its actual authored
template and NativeSdk.cmake are eligible and verified in the frozen source ZIP;
the1944 substitution reproduces its exact bytes. Stage02 keeps that header in
the already required separate frozen SDK, stages the actual generator sources
and a derivation receipt, and marks the external input explicitly. No historical
graph/guard/font/product is changed and no source/header identity is substituted.
Stage02 passes that boundary, then correctly finds an omitted fourth kit member
in the controller's input receipt: extensions.cmake was generated beside three
copied files but not listed. Preserve the old receipt/log; record its exact
observed bytes and matching native ABI-version declarations. No kit, source,
binary or admission assertion changes; retry the completed inventory check.

Stage03 passes all364 package members and material admission. The next unresolved
risk is relocation/runtime loading and ownership-safe removal of that exact
package. Run the installed CLI and public SDK consumer with co-located provider/
dependencies in a native-only environment; require exact record/CSV/QPLM1 matches,
then remove only hash-verified owned files after path/process checks. Preserve
user projects/results/notes and verify provider-free metadata after removal.
No source, kernel or binary rebuild is needed for this deployment check.

Deployment01 passes16 native routes using the exact relocated package; all364
owned members are removed after path/process/hash checks and33 user files remain.
Saved result metadata reopens provider-free and unavailable replay refuses. The
remaining checks are archive/member admission, source/product retention and the
relocated SDK CMake configuration (configure only, reuse matching consumer build).
Then close this local A08 slice and leave the broader catalogue goal active.

SDK package configure passes. Closeout01 then stops on the development controller's
invalid encoding alias utf8-sig while reading its generated project. Preserve that
controller/diagnosis, use utf-8-sig, and reuse the exact successful configure rather
than rerunning it. Compiled source/product checks already pass; no native or
package inputs change for this correction.

The pinned NGFF0.4.1 specification commit106c3010dcb4079eb0a69868a11cc5943f942fdf
and five exact notice/schema/specification references are retained. Reports use
W3C-20150513; software examples use Apache-2.0. No upstream dataset/model/example
asset is admitted. The normative nested image-label.source reference profile was
cross-checked with primary OME implementation; retain the spec-example discrepancy
and explicit/default reference origin rather than guessing. Additional plugin
code/dependency identities, licenses, notices and source obligations must accompany
its actual eventual source/native package. Other eligible work stays in its R row.

Follow the [Windows checkpoint](api-delivery-plan.md#next-delivery-checkpoint-windows-research-workflow)
and [validation policy](api-delivery-plan.md#validation-and-build-policy). Keep one
active integration checkout/cache/runtime; earlier frozen products, source/dirty
snapshots, fixtures, failures and cleanup archives remain preserved. No commit,
push, publication or further feature phase is selected by this prerequisite work.

Closeout: candidate2 native/CLI/SDK/OFF routes,364 package/ZIP members,16 installed routes,33 preserved user files, source/retention and relocated SDK configuration pass. The controller encoding correction reused the successful configure. Broader formats AND application/integration goal remains active; no commit/push/publication or frozen runtime rebuild. Final evidence: build/gui-aggregation/catalogue-closure/ngff-labels/results.json.

## 2026-11-17 - M05 Loom typed matrix local handoff


The [selected native workflow](benchmarks/loom/README.md) imports an explicit
Loom3 matrix/layer, row/column ID datasets, optional feature names and complete
native position TSV into QMX. All eight integral and IEEE16/32/64 types follow
explicit UInt64-counts or finite Float64-continuous semantics; integer-to-double
conversion requires exact representability. Original IDs, signed zero, frame/
coordinates, native QMX/project ownership and source/implementation identity stay
native. Graphs/unselected attributes/layers are declared omissions.

Separate matrix ABI2 transports typed metadata/pages and independent file/work
allowances. Schema5/external-dense-typed/1 has distinct kind/label-bound recipe/
source hashes; public DTOs/vtables/options and old inline COMDAT helpers stay
unchanged. ABI1/schema4 count bytes remain exact. One shared bounded HDF5 helper
uses the frozen static instance, held read lease and raw filtered-chunk validation
before HDF5 conversion, including string-reference chunks. No process sandbox or
total-process bound is claimed. Native source/currentness/cancel/publication
services remain authoritative; Python is development/control only.

Native delivery checks pass34 effective commands:19 selected successes in
integration03 plus15 remaining integration04 commands. Actual provider/types/
codecs/domains/frame/numerical/identity/refusal checks, typed callback and real
source changes, provider-free/OFF metadata, duplicate options, exact legacy
schema4 bytes and affected old point-import/undo/quota behavior pass. CLI/public
SDK QMX bytes/source/frame/receipt identities match native default-budget
references; owned project save/reopen passes. Root Python/NGFF-OFF configuration
passes with format plugins OFF/ON and generated ABI2 kit, no Python discovery.

Six application units and thin consumers were compiled over the exact A08
b0c86a addon and frozen FP04 core4bf957. No core/kernel/UI/binding/wheel rebuild.
The passing186 observed inputs/10 targets/16 products are recorded;186 source
inputs and13 non-runtime products are separately frozen in candidate1. Prior23
source snapshots,15 archives,364 A08 members and45 inventory groups remain exact.

Actual corrections are retained in the packet: transitive HDF5 umbrella-header
include directory; controlled manifest package-relative paths; a mutation-test
precondition corrected to distinguish denied writes from actual between-call
edits; selection of complete retained legacy TIFF fixtures; and format-CLI exit2
instead of the label CLI's exit1. No numeric, identity, decoded-allocation or
material assertion was weakened. Six pinned BSD2 upstream Python files are
nonexecuted references, never product logic/runtime/fallback. Schema prose stays
metadata-only. Three durable lessons were appended to compilexx's notebook.

## Remaining M05 handoff

Matching licensed source/native/plugin staging, relocatable SDK configuration,
installed native run/owned-member removal, ZIP admission and final source/
retention/docs closeout remain. Stager, deployment controller and docs have
separate owners; only root executes stage/removal and merges inventory/evidence.
HDF5/zlib source/generated interfaces retain actual component terms. The exact
A08 companion and FP04 SDK/core/full source remain separate required products;
external sdk_abi.hpp uses its eligible template/recipe derivation, never a copied
header admission around old invalid ancestry. No further native build is selected
without a diagnosed failure. No commit/push/publication is selected.

Stager and deployment controller are ready with static checks. Before executing:
stage the exact qualified16 products/186 inputs once, preserving45 old inventory
groups and original HDF5/zlib/host/reference terms. Then verify the relocated SDK
configuration and installed CLI/SDK outputs, and remove only hash-verified owned
members after path/process checks while preserving all user/managed results.
No new native compilation or numerical campaign is needed for these conditions.

The broader formats AND application/integration catalogue remains open. Other
NGFF/N5/AnnData-Zarr, image/ROI/writer/application profiles and R rows keep their
own terms/contracts/evidence. The frozen built-in image reader's malformed
integral floating fills remain I22/R15; A08 protects its own selected profile.
Follow the [Windows checkpoint](api-delivery-plan.md#next-delivery-checkpoint-windows-research-workflow)
and [validation policy](api-delivery-plan.md#validation-and-build-policy). Keep
one active integration cache/runtime and preserve fixtures, passing products,
source/dirty snapshots, failures and cleanup archives.

Closeout:284 package/ZIP members admitted; relocated SDK configure,9 installed commands and3 post-removal checks pass;11 user files survive owned-member removal. Native34-check scope and frozen-source/product boundaries stay distinct. Exact receipt: build/gui-aggregation/catalogue-closure/loom/results.json. Full formats AND application/integration goal remains active.

## I22/R15 exact native integral-fill source correction (2026-11-17)

The [native source candidate](benchmarks/ngff-fill/README.md) closes the demonstrated
round-before-validation correctness case in OME-Zarr image fills.214 authored
cases pass:133 exact old/new valid data/identities,71 open refusals and10 undefined
null read refusals. The old baseline accepted42 malformed cases; the candidate
accepts none. Both read the same663 unchanged fixture files/native versions.
Existing source/server/project regressions pass, including statistics, saved
records and replay; all170 original fixture hashes are unchanged. Two native
units,104 observed compiler inputs/six targets/11 products and seven retained
product identities have their own [receipt](../build/gui-aggregation/catalogue-closure/ngff-fill/results.json).
Frozen core/runtime/GUI/bindings/wheels and the stricter A08 label profile remain
separate; no format/codec or release qualification is transferred.

Configure01 failed because the addon import was child-scoped; an explicit parent
import fixed configure02 before one combined build. The first controller
preflight used a mixed-case Windows environment dict key; uppercase SYSTEMROOT
fixed it before any native command. Failed receipts/recipes and the
[pre-closeout card](../build/gui-aggregation/catalogue-closure/ngff-fill/card-before-closeout.md)
remain. No scientific assertion or stored identity was weakened. The broader
formats AND application/integration goal remains active; no new phase or
publication is selected by this closeout.

## M07 AnnData Zarr local delivery (2026-11-17)

The [workflow](benchmarks/anndata-zarr/README.md) imports selected local Zarr 2
counts and continuous matrices into native schema 6 QMX and an owned project.
The 27 native, CLI, SDK and pinned producer commands and nine installed and
post-removal commands pass. Schema 4 and both schema 5 domains remain byte
exact. The local package and archive admit 285 members and removal preserves
nine user files. The sampled 1,024 × 1,024 sparse input required 22.8 MB of QMX
and 45.6 MB peak output space. One test corrected a comparison between recipe
and file-bound hashes. A final SDK-only link captured a source edit that landed
after the combined build started. Both earlier receipts remain preserved.


## R11 completed QMX project attachment retry

The [matrix-attach workflow](benchmarks/matrix-attachment/README.md) passed
sixteen Windows commands over the retained M07 native archive. Saved project
failures left both counts and continuous QMX outputs available for a later
attach, with exact owned bytes and receipt checks after offline reopen. Input
aliases, wrong frame/hash and preexisting outputs refuse. A configure target
collision was corrected by importing the exact retained M05 archive privately.
No additional format, GUI, binding or release is selected.
