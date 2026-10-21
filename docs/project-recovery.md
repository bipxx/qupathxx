# Autosave and recovery

The native `ProjectAutosave` controller captures an immutable save snapshot on
the owner thread, then writes it on one background worker. The controller has
one job and no backlog. Autosave leaves the live project's path, save generation,
dirty flag, document revisions and edit history unchanged.

```python
import qupathxx as q

project = q.Project.open("study.qpxproj")
project.document(project.active_image).add_rectangle(q.Rect(10, 10, 100, 100))
autosave = q.ProjectAutosave()
autosave.request(project)  # Or supply an explicit .qpxrecover destination.
status = autosave.wait()   # Desktop callers poll instead of waiting.
if status.state != "ready":
    raise RuntimeError(status.error)
info = q.Project.inspect_recovery(status.path)
restored = q.Project.recover(status.path)
assert restored.dirty
restored.save("recovered-study.qpxproj")
```

`request` returns false for a clean project, an in-flight job, or unchanged state
whose published file still has its recorded size/time. A missing or changed file
is checked again by the worker. Errors from snapshot budgeting/invalid paths are
synchronous; metadata-hint checks can also fail synchronously. Worker filesystem,
database and publication failures appear in `AutosaveStats`.
States are `idle`, `writing`, `ready`, `cancelled`, `clearing` and `error`.
`poll` never waits for IO. `wait` drains the current operation and any queued
clear. `clear` cancels publication and asynchronously removes only the file owned
by that controller/session. `cancel` preserves an earlier checkpoint; publication
can win a cancellation race. Destruction cancels pending publication and joins
the worker, including an explicitly requested clear.
Cancellation is checked between canonical rows and through SQLite's progress
handler; it cannot interrupt an OS filesystem call already in progress.

## Snapshot and publication contract

Snapshots copy canonical maps/text, classes, calibration, image descriptors, IDs,
revisions and camera/display views. They share immutable ROI storage and embedded
dense pixels. File-backed sources use metadata-only snapshot servers, so autosave
does not read pixels or retain live OpenSlide reader/cache handles. Selection,
undo/redo, spatial indexes and derived measurements are not copied. Live edits,
class/color/calibration changes, image removal and relinking remain independent
after capture. Large snapshot maps are released on the worker before completion,
so GUI polling does not free the whole document.

The default estimated payload budget is 256 MiB, configurable up to 4 GiB. The
estimate includes map/text overhead, per-object retained geometry payload,
embedded pixels and copied link metadata. Shared geometry/pixels can be counted
more than once. It is not a process-RSS limit: allocator behavior, native geometry
caches, SQLite buffers and the active workspace have separate costs. A budget
failure leaves the primary and previous recovery unchanged.

Each controller has a random session identity. Default saved-project files are
`<project path>.recovery-<session>.qpxrecover`. Unsaved projects use the user state
directory (`LOCALAPPDATA/qupathxx/recovery` on Windows, or XDG state storage on
Unix). Explicit destinations require the `.qpxrecover` extension. Primary/image
paths and existing filesystem aliases are protected. A target belonging to
another identity, base path or session is never overwritten.

The worker acquires a nonblocking OS file lock, writes a unique sibling work
directory, commits the canonical SQLite snapshot and recovery metadata with
`synchronous=FULL`, performs `quick_check`, and atomically replaces the recovery
file. Windows sharing/lock conflicts receive a bounded worker-side retry. IO
failure/cancellation preserves the previous published checkpoint. Normal cleanup
removes only known files in the exclusively created work directory; it never
recursively deletes unknown contents. Zero-byte `.lock` files remain to avoid
lock-inode replacement races; their existence is not evidence of a live process.

Recovery files have application ID `0x51505258` (QPRX), distinct from ordinary
`0x51505858` (QPXX) projects, and use canonical schema 2 plus a recovery metadata
table. Older project readers reject their application ID. `Project.open` rejects
recovery files; use `recover`. Metadata records project/session identity, original
primary path/generation, capture time and image/object counts. The loader checks
these against the snapshot, then validates canonical document state normally.

Recovery restores the original primary association/generation and marks the
result unsaved. An unchanged primary can be saved explicitly; if another writer
advanced it or replaced its identity, normal save conflict checks reject the
overwrite. Save to a new destination to preserve the recovered branch. No
automatic rebase, merge or substitution of the current primary generation occurs.
`restore_recovery` replaces a project's authority atomically; previously held
document references remain alive but belong to the previous workspace.

## Desktop workflow

The **Recovery** panel defaults to a 60-second interval; zero disables periodic
recovery. **F10** writes a checkpoint immediately. It can use a custom destination
when the primary folder is unavailable. Startup discovery runs in the background
for interactive launches; **Find recovery files** refreshes it. Candidates show
UTC capture time, object count, corruption errors and changed-primary status.
`find_recoveries` returns the newest candidates up to its configured limit, using
bounded result storage. **F11** opens the selected/path-entered recovery.

Opening recovery stages a validated project before asking whether to save or
discard the current workspace. Cancel and failed saves retain the current graph.
The new graph, selection and GPU reader are published at a frame boundary after
resources are prepared. The selected checkpoint is kept until it has been loaded;
another session's checkpoint is not automatically deleted on save or close.

Closing an unsaved workspace offers Save, Keep recovery, Discard and Cancel.
Keep recovery waits for publication of the latest canonical state. Write/save
failure leaves the dialog and workspace available. Keyboard edits are blocked
under these dialogs. Explicit Save and Discard clear only this session's recovery;
other sessions' checkpoints remain available. In embedded callers, Discard closes
the viewer and removes its checkpoint but does not invalidate retained document
references. Finite `frame_limit` runs return to their caller without forcing a
close decision; their caller still owns the in-memory project.

Both launchers support explicit recovery:

```powershell
python -m qupathxx --recover study.qpxproj.recovery-SESSION.qpxrecover
qupathxx_app --recover study.qpxproj.recovery-SESSION.qpxrecover
```

`Project.discard_recovery(path, expected_identity)` explicitly removes a validated
checkpoint; mismatched/invalid files and active writer locks reject.
[Reviewed checkpoint deletion and background recovery loading](recovery-maintenance.md)
are now available in the desktop. The checked native API compares the reviewed
file content before removal. Abandoned-work-directory cleanup and automatic retention
remain delivery work. [Moved-primary association](recovery-association.md) now
validates project identity while preserving the captured generation.
[Explicit primary saves](project-save.md) run on a worker with owner-thread baseline
acknowledgment; initial CLI project/recovery loading remains synchronous. Interrupted work directories are ignored during discovery; incomplete
databases are never promoted. Retention across many sessions and power-loss/
network-filesystem guarantees remain unverified.

## Evidence and performance

Native/Python tests cover isolation during live edits, primary preservation,
closed/missing links, budget limits, alias protection, foreign sessions, locked
SQLite databases, replacement failure/retry, legacy-primary preservation, stale
writers, discovery, and process termination before/around publication. Windows
read-only replacement failure verifies that the previous checkpoint stays intact.
Termination tests accept either the complete previous or complete new checkpoint,
check database integrity and verify that the primary is unchanged. This is not
a power-loss fault campaign. Real ImGui replay covers periodic/manual recovery,
replacement, primary-save cleanup, session retention and each dirty-close choice.

`tools/benchmark_recovery.py` records owner request/capture, background write and
release, polling latency and concurrent live queries. Its `--gui` mode records
capture plus frame work with 100000 selected rectangles and 828 visible overlays.
See the [headless](benchmarks/recovery-100000-windows.json) and
[viewer](benchmarks/recovery-view-100000-windows.json) records. Scope is one dense
source and local storage; vendor readers, cold/network storage, power loss and
long-running pressure are excluded.

The installed Windows wheel run records 15.7 ms owner request (15.3 ms capture),
240 ms background write/release and 0.027 ms maximum polling time, while the live
document completed 144 region queries. Estimated retained payload was 39.2 MB.
The viewer run records 15.6 ms capture, 16.9 ms p95 and 17.3 ms maximum measured
CPU frame work after warm-up, with 3 MiB GPU tile payload. These numbers describe
that workload, not a bound for every document or storage device.
