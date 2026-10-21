# Background explicit project saves

`ProjectSaveJob` saves one immutable native snapshot on a worker. ImGui's
**Save project (Ctrl+S)**, **Save current and open/recover** and **Save and close**
use it. Normal image editing, undo, browsing and document changes remain usable
during an ordinary save. The save button is disabled while its one job is in
flight; repeated requests do not build a snapshot queue. Workspace replacement
actions wait for the save, and close/save-before-replacement choices wait for
its durable result.

```python
import qupathxx as q

project = q.Project.open('study.qpxproj')
job = q.ProjectSaveJob(snapshot_budget=256*1024*1024)
job.request(project, project.path)
# Continue editing the owner-thread project while the native worker serializes.
status = job.poll()  # Nonblocking. Call regularly to acknowledge completion.
status = job.wait()  # Join and reconcile when a blocking result is appropriate.
assert status.state == 'saved' and status.applied
```

The worker owns a private canonical clone. Immutable geometry and embedded pixel
storage are shared; linked readers use metadata-only descriptors. Undo journals,
selection, spatial indexes and OpenSlide caches are excluded. All source authority
is accessed on the owner thread. No Python-owned project/document references
are transferred to or released by the worker. The existing synchronous
`Project.save(path)` remains available for native/Python batch callers.

## Reconciliation and failures

A successful worker uses the same SQLite FULL transaction, schema migration,
identity checks and optimistic save generation as synchronous save. The owner
thread acknowledges the committed path/generation and the **captured** project
and per-image revision baseline. It does not replace the live canonical graph.
Post-capture object, image, calibration, class and camera edits therefore remain
unsaved. Undo history and live selection are preserved. A subsequent save captures
those newer edits. Save-as resolves relative links against its actual destination
and preserves the previous primary.

Completion applies only if the original live instance, base path and generation
still match. If a caller restored/replaced that project or saved it separately,
the file can be committed while its live acknowledgment is rejected. Status is
`detached`, `applied` is false, and the error explains that the destination must
be reopened to use its generation. No silent merge or rebase occurs.

`state` is `idle`, `saving`, `saved`, `error`, `cancelled` or `detached`.
`completed` counts durable commits, including detached results. `generation` is
the most recent committed generation. Errors preserve the live save baseline;
existing primary rows/schema roll back through SQLite. A failed/cancelled first
save can leave an empty SQLite file at its new destination; arbitrary files are
not deleted as cleanup. A writer contending with another writer can fail its
bounded SQLite busy wait or stale-generation check and can be retried.

`cancel()` requests cooperative interruption during serialization or before
commit. A commit that wins the race is reported as saved, with its real baseline;
cancellation does not undo a committed file. It cannot interrupt OS I/O. The job's
destructor joins and acknowledges an authorized save instead of cancelling it.
Keep the controller on the project's owner thread, and poll or wait before using
its updated generation. The native authority remains single-threaded.

## Viewer and recovery behavior

The viewer captures after publishing that frame's current camera/display state.
After a normal save it clears only its own recovery checkpoint, and only if no
newer canonical edits remain. Other sessions' checkpoints are retained. A dirty
post-capture workspace keeps its checkpoint and stays marked unsaved; an older
checkpoint can legitimately refer to the previous primary generation.

Save-and-close or save-before-replacement keeps editing blocked and waits for the
latest stable canonical state to be committed before proceeding. If a window
close arrives during an ordinary save, the viewer first waits for that operation;
if newer edits remain, it then offers the usual dirty-close choices instead of
silently saving or discarding those edits. Save errors leave the dialog and
workspace available. Saving over a selected open target reloads the successful
new generation on the loader worker. Finite embedded `show` runs join and reconcile
an outstanding save before returning. Initial CLI loads remain synchronous;
ordinary project and [in-window recovery loading](recovery-maintenance.md) share
one asynchronous loader slot.

Default capture budget is 256 MiB, configurable up to 4 GiB through
`ProjectSaveJob(snapshot_budget=...)` or `GuiOptions.save_snapshot_budget`.
The conservative estimate includes retained geometry/pixels and canonical
containers/text, and excludes allocator/runtime/GEOS caches and SQLite buffers.
Capture/estimation run synchronously and can reject before launching a worker;
large captures can still delay a frame. Explicit save and autosave have independent
one-job budgets, so their retained payloads can overlap. There is no RSS bound or
streaming project serializer claim.

## Interrupted primary transactions

A killed writer can leave SQLite's hot rollback journal. Read-only connections
cannot roll it back. The native database opener now probes its header read and
retries **only** `SQLITE_READONLY_ROLLBACK` using a temporary read/write connection
without file creation. SQLite recovers its transaction, then the loader returns
to a read-only connection. Normal opening, including schema 1, stays read-only
and performs no migration. Recovery requires write access to the database and
journal directory; permission failure leaves the journal for a later writable
open. No journal file is guessed at or manually deleted by the application.

## Evidence

`tests/test_project_save.py` covers capture isolation, exact baseline acknowledgment,
metadata/document removal during serialization, save-as links, injected rollback,
stale/foreign writers, competing jobs, explicit cancellation, detached results,
destructor completion, capture budgets, empty projects and legacy migration.
A subprocess is killed only after a valid hot-journal header is observed during
a real native write. Native reopening restores the previous complete project;
a Windows read-only-attribute variant verifies recoverability after permission
is restored. SQLite integrity checks pass after rollback. These process-death
tests do not simulate power loss or storage hardware failure.

`tests/test_gui_save.py` verifies live undo during a locked save, preserved recovery
for post-capture edits, ignored repeat/new-project shortcuts while busy, pending-save
close ordering, finite-run acknowledgment and visible budget errors. Existing
workspace, recovery, migration, view and lifecycle replay verifies shared save
choices. The fourteenth native CTest suite checks snapshot isolation, reconciliation,
stale generations and disabled-storage behavior.

The hot-journal crash tests launch the underlying interpreter and verify its PID
matches the subprocess handle before killing it. On Windows the virtualenv
launcher has a separate PID; terminating only that launcher would leave the
native writer alive and would not prove killed-writer recovery.

`tools/benchmark_project_save.py` measures a 100000-object explicit save while the
live graph answers spatial queries or remains selected in the viewer. It verifies
that the committed file contains captured values and that newer edits remain dirty.
Worker time includes native snapshot release. Desktop CPU frame work includes
capture and concurrent writes and excludes buffer-swap wait. Warm dense sources
and local storage are the measured scope; cold/network/vendor, power loss and
prolonged pressure remain unverified.


The installed Windows [headless run](benchmarks/project-save-100000-windows.json)
records 16.1 ms capture, 338 ms worker write/release and 0.020 ms maximum poll,
with 204 concurrent live spatial queries and an estimated 38.7 MB retained payload.
The [desktop run](benchmarks/project-save-view-100000-windows.json) records 17.1 ms
capture, 350 ms worker write/release, 16.9 ms p95 and 18.4 ms maximum CPU frame work.
All 100000 objects remained selected, 805 overlays were visible and peak GPU tile
payload was 3 MiB. The recorded post-capture camera change remained unsaved.
These single-run values describe the measured workload, not general latency bounds.
