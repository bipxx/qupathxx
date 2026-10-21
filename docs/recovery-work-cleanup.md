# Abandoned recovery work

Autosave creates a private `<destination>.work-<32 hex digits>` directory under
the destination's publication lock. The new `owner` file contains a versioned
signature and that directory's random token. It is closed before SQLite starts
writing `snapshot.qpxproj`. Successful publication still atomically moves only
the completed snapshot into place. Normal teardown removes known files; if a
sharing violation prevents teardown, it retains the marker for a later retry.

After publishing a snapshot to that exact destination, the autosave worker
cleans up to 32 marked abandoned directories for it. Publication takes priority
over collection: cleanup for that write starts only after publication. The
same native lock spans ownership validation, writing, publication and cleanup.
A second cooperating process cannot clean a writer's active directory.
There is no age or PID heuristic: a directory
under this lock cannot belong to an active cooperating writer for this destination.

Cleanup checks the exact destination prefix, lowercase hex suffix, versioned
marker and token. It preflights every child before removing anything. Only the
marker, SQLite snapshot and its three known journal/WAL/shared-memory sidecars are
allowed. Extra files, subdirectories, symlinks, Windows junctions/reparse points,
invalid markers and old unmarked directories are preserved. It removes individual
known files and then the empty directory, with no recursive traversal/deletion.
It never opens, changes or deletes the published checkpoint or primary project.

The implementation assumes cooperating writers and ordinary local filesystem
ownership; the marker and advisory lock are not a security boundary against a
process deliberately replacing paths while cleanup runs. A crash before the
marker is fully written can leave unrecognized debris, which is preserved.

## API and reporting

```python
import qupathxx as q

# Exact destination of an interrupted writer; the checkpoint need not exist.
removed = q.Project.cleanup_recovery_work('study.qpxrecover')
```

The call is synchronous, acquires the same publication lock and returns the
number of removed directories. This explicit API sweeps all recognized directories
for the supplied destination. Invalid destination extensions reject; lock or
filesystem failures raise. It needs an existing parent directory. Calling it
again after successful cleanup returns zero. It can reclaim marked work from
another, now exited session when the caller supplies that session's destination.

Automatic cleanup runs on the existing single autosave worker, with no extra
queue or render-thread traversal. A cleanup failure does not suppress a new
recovery write: `AutosaveStats.work_cleanup_error` records the failure separately
from `error`, and ImGui shows it in Recovery. A later writing request retries.
The 32-directory cap bounds deletion per autosave; a later successful write can
collect another batch. This cap does not bound parent-directory traversal or OS
I/O time. `work_directories_cleaned` accumulates completed successful cleanup counts for
that controller; a scan that throws can have partially removed earlier directories
without reporting their count. `write_ms` includes the cleanup scan and deletion.
A cancelled or failed publication does not start automatic cleanup. A checkpoint
whose unchanged state already satisfies the request does not start
a new worker or cleanup pass. Explicit cleanup remains available in that case.

Memory use is independent of checkpoint size and directory count. Cleanup streams
the immediate parent directory and at most five children per candidate, and reads
only a fixed-size marker. Runtime scales with parent entries and removed files;
cold/network directories can still be slow. Automatic cleanup checks cancellation
between parent entries; it cannot interrupt an in-flight filesystem operation or
the preflight/deletion of one directory. The existing worker is joined at shutdown.

## Evidence and remaining scope

`tests/test_recovery_work_cleanup.py` kills a real native autosave after its
private SQLite file appears, proves cleanup rejects while its writer is still
alive, and checks explicit cleanup and next-session automatic reuse after death.
Other tests check primary/checkpoint byte preservation, idempotence, all known
sidecars, bounded automatic batches/full explicit sweeps, unexpected entries,
token/marker validation, file links (when available), a real Windows
junction, publication locks and deletion failure/retry without lost publication.
The native recovery suite also verifies automatic cleanup and disabled-storage
rejection without side effects.

`tools/benchmark_recovery_work_cleanup.py` measures a standalone cleanup pass and
a second pass with bounded cleanup after a 100000-object autosave. `--gui` keeps all 100000
objects selected in a narrow viewer and records frame work/GPU payload. The
temporary directories contain known interrupted-file fixtures; process-kill
correctness is covered separately by the tests. Creation time is excluded.

The [headless Windows run](benchmarks/recovery-work-cleanup-1000-windows.json)
records 2.87 s for the standalone full sweep of 1000 directories/5000 files.
In a separate pass with another 1000 directories present, capture took 14.5 ms,
the checkpoint became visible after 256 ms, and the worker finished after 330 ms,
including its bounded 32-directory cleanup. Maximum poll time was 0.012 ms.
Explicit cleanup then removed the remaining 968 directories outside the worker
measurement. This is incremental collection, not a claim that all debris is gone
after one automatic write.

The [desktop run](benchmarks/recovery-work-cleanup-view-1000-windows.json) records
16.9 ms frame p95, 37.0 ms maximum frame work and 34.5 ms owner-thread capture.
All 100000 objects remained selected, with 792 visible overlays and 3 MiB GPU tile
payload. The capture pause remains an optimization target. The full standalone
sweep in this run took 2.68 s; it was measured before opening the viewer, and the
explicit 968-directory remainder was removed after it closed.

The [observed intermediate baseline](benchmarks/recovery-work-cleanup-before-bounding-windows.json)
deleted all 1000 directories before publication and spent 3.35 s in its worker.
That result motivated publication priority and the 32-directory cap. Its first
standalone sweep took 8.38 s. These are separate single-host local runs with
variable filesystem caching, not an algorithmic speedup of full deletion or a
general latency bound. Cold/network storage and sustained pressure remain unverified.

This packet implements cleanup for an exact recovery destination. Automatic
discovery/cleanup across earlier sessions' different destination names, unmarked
legacy debris and initial CLI loading progress remain open. Opt-in
[checkpoint retention](recovery-retention.md) now protects live sessions while
collecting old managed files; reviewed deletion remains available independently.
The full M0-M7 implementation goal remains active.
