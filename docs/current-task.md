# Current implementation task

Goal: implement the project plan end to end and verify optimized performance.
The full M0 through M7 scope remains active. M2 and M3 remain open.

Current sources include tiled rendering, overview, RGB display controls, atomic
commands, bounded undo/redo, general immutable ROIs, cell/nucleus objects, a shared class registry, multi-selection and spatial queries. GEOS
provides polygon topology; ellipse metrics/masks use analytic geometry. Python
and ImGui share the native model. See geometry-spatial.md and document-commands.md.

Evidence: 341 passing Python tests including shapes, interchange, browsing, workspace migration, recovery and GUI replay
(two file-symlink cases skipped because this Windows account cannot create them; the junction case passes);
fourteen native suites with optional geometry/readers/interchange enabled and disabled;
installed GUI wheel checks with packaged runtime libraries and licenses.
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
on the existing worker. Cross-session orphan work and obsolete lock-sidecar
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
Next improve viewer metadata/capture latency and implement cross-session work discovery and initial loading progress;
retain open M3 numerical comparisons and feature-cache contracts. Preserve all unfinished
M2 work: OS/DPI delivery, resize/minimize, docking, fluorescence/ICC, prolonged
cache pressure and vendor benchmarks. Persistence, interchange, analysis,
classifiers, scripting and packaging work under M4-M7 remains active.

Outstanding M0 decisions: project license choice
(asked asynchronously; no license selected yet), broader malformed-image
fixtures and numerical QuPath comparison tolerances. GEOS dependency selection
and packaging are now recorded with source identity and independent tests.
