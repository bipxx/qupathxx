# Checkpoint retention

Retention is opt-in. New controllers and desktop windows leave it disabled.
When enabled, a successful autosave publishes its new checkpoint first, completes
its existing bounded work-directory cleanup, then runs retention on the same
worker. It drops the private canonical snapshot before scanning and never holds
a live document/Python graph for maintenance or queues a
second job. The default policy keeps the newest 10 checkpoints, protects files
younger than 30 days, and removes at most 32 old inactive checkpoints per pass.
Both the age and newest-count conditions must permit deletion. This is not a
hard storage quota: active, recent, legacy and otherwise protected files can keep
the directory above the configured count.

```python
import qupathxx as q

policy = q.RecoveryRetention()
policy.enabled = True
policy.keep_latest = 10
policy.max_age_days = 30
policy.max_deletions = 32
writer = q.ProjectAutosave()
writer.retention = policy  # copied; captured by the next request
writer.request(project)
stats = writer.wait()
print(stats.checkpoints_pruned, stats.retention_ms, stats.retention_error)

# Synchronous explicit pass anchored to a checkpoint that must remain available:
result = q.Project.prune_recoveries('current.qpxrecover', policy)
print(result.inspected, result.removed, result.failed, result.error)
```

Policies validate 1..1024 newest checkpoints, 1..128 removals per pass and finite
1..36500 age days, including while disabled. The explicit API requires a regular
valid recovery header for the anchor when enabled; disabled policies return without inspecting
files. Storage-disabled builds reject maintenance without filesystem changes.

## Scope and ownership

A pass scans only the anchor's immediate parent directory. It considers validated
recovery headers with the same project UUID and recorded base path (or the same
unsaved-project UUID). Separate save-as branches, other project identities,
unreadable/corrupt checkpoints, links and Windows reparse points are preserved.
The anchor is always protected. Both the captured UTC creation timestamp and
filesystem modification time must exceed the minimum age; future timestamps and
recent copies/edits remain protected. The newest set includes all valid same-scope
checkpoints, including unmanaged or active ones, with deterministic path ordering
for equal timestamps.

Every new native autosave attempts to acquire a project/session OS lease in
`<user recovery state>/sessions/<project UUID>-<session UUID>.lock`. After successful
publication it writes a closed `<checkpoint>.session` marker identifying that
project/session. No open lease handle lives in the scientific project directory;
Windows folder moves/copies therefore retain their existing behavior. All
checkpoints/copies bearing one live session's marker stay protected together.
The controller holds its lease while idle as well as while writing; `wait()`
does not release it. It releases the lease on destruction or after successfully
publishing for another project identity. Failed switches retain the previous
lease. Destruction cancels/joins work before releasing ownership. Process death
releases the OS lease without timestamps, PID reuse or heartbeats.

Windows uses exclusive native file sharing for the user-state lease; POSIX uses
`flock`. Publication retains its separate checkpoint writer lock. Cooperating
collectors leave lease and publication-lock inodes in place. Markers/lock files
can remain after checkpoint removal; existence alone does not indicate liveness.

Retention verifies the closed marker and opens the corresponding existing local
lease without creating one. It holds that lease through deletion/release, sharing
one held handle across candidates from the same session. Missing, invalid, busy
or unreadable markers/leases protect the checkpoint. Older app versions' files,
other users/machines' session state and files whose local state cache was cleared
remain protected because no known existing local lease can prove inactivity.
If user-state lease setup is unavailable, autosave can publish as unmanaged,
reports a separate retention error and does not collect checkpoints. An old
matching managed marker is invalidated before such an overwrite; if that cannot
be done, publication rejects and the previous checkpoint is preserved. A later
successful lease can replace the known unmanaged marker with a managed one.
Unknown files occupying a marker path are preserved and reported, not overwritten.

Explicit reviewed deletion remains available for a live or unmanaged checkpoint;
retention does not change that deliberate selected-file workflow.

## Bounded selection and deletion

One metadata-only scan keeps a heap of the newest configured count and another
heap of the oldest eligible inactive candidates, capped by the deletion limit.
Native [marker prefetch](recovery-marker-prefetch.md) overlaps at most four small
ownership-marker reads; header ranking and session-lease acquisition stay serialized.
Only those candidates' native leases stay open. Memory and held leases are bounded
by the policy, independently of directory/file size; historical documents, pixels
and geometry are not constructed. Metadata checks do not establish full ROI/source
recoverability. The pass deletes candidates oldest first, excluding the retained
newest set. It reacquires each publication lock and
revalidates project scope, session, captured generation, creation timestamp and
unchanged filesystem modification time before removing that checkpoint alone.
It never changes the primary, current anchor, source images, live model or history.

The newest set describes the scan, not a transactional directory snapshot.
Independent explicit removals/publications can change the catalog concurrently.
Active session leases prevent cooperating publication into selected candidates;
changed/missing candidates are preserved or reported rather than blindly removed.
Markers and advisory locks are consistency mechanisms for cooperating processes,
not a security boundary against arbitrary filesystem replacement or tampering.

`AutosaveStats.checkpoints_pruned` is cumulative. `retention_ms` and
`retention_error` describe the latest completed writer job's pass. Cleanup failure
does not turn successful publication into a failed save; errors are shown
separately in the desktop Recovery section. An unchanged project that needs no
new checkpoint does not automatically start maintenance. The explicit API can
run a pass in that situation. File-removal failures retain the file and can be
retried; the pass reports the first error and failure count.

The full directory scan, SQLite metadata reads, OS locks and deletion can take
time on cold/network storage. The deletion cap does not bound scan time. Automatic
cancellation now stops between scan entries/removals and skips deletion after a
partial scan. [Progress and cancellation](recovery-maintenance-progress.md) distinguish
publication from worker completion. One in-flight operation/busy wait can finish
first, and shutdown joins the worker. The explicit API stays synchronous.

## Desktop and evidence

Recovery exposes **Remove old inactive checkpoints**, **Keep newest** and
**Minimum age (days)**. Settings apply to subsequent writer requests in that
window. Python launch options expose `recovery_retention_enabled`,
`recovery_keep_latest` and `recovery_max_age_days`; the desktop deletion cap is 32.
These session settings are not persisted into scientific project files.
`GuiReport.checkpoints_pruned`, `retention_ms` and `retention_error` expose replay
results separately from explicit reviewed deletions.

`tests/test_recovery_retention.py` exercises age/newest gates, idle native owners,
unknown/legacy/corrupt files, unrelated identities and save-as branches, bounded
oldest-first batches, automatic publication/collection, invalid policies, writer
lock failure/retry and desktop options. A separate actual interpreter holds an
idle lease while its whole directory moves; the checkpoint stays protected until
that process is killed. Existing association/recovery tests remain regression
coverage for moves, generations, pixel rebinding and reviewed deletion.

`tools/benchmark_recovery_retention.py` generates 1000 small historical checkpoint
fixtures published by distinct closed native sessions (only timestamps are aged),
then publishes a native 100000-object
snapshot before collecting 32 old files. The optional viewer keeps all 100000
objects selected. Fixture creation and final explicit file teardown are excluded
from measurements. Real publication/liveness semantics are tested separately.

The [distinct-session headless baseline](benchmarks/recovery-retention-before-progress-windows.json)
records 16.2 ms capture and checkpoint visibility after 255 ms. The worker took
8.73 s in total, including an 8.48 s retention pass; maximum poll time was 0.10 ms.
That full metadata/lease scan remains a measurable maintenance cost even though
the new checkpoint publishes first. The benchmark uses real small native
publications, then changes only their capture timestamps and filesystem mtimes
to exercise expiration without waiting sixty days. It removes 32 of the 1000
historical files and preserves the current 100000-object checkpoint and primary.
These single-host warm-local results do not establish general timing bounds.

The [distinct-session desktop baseline](benchmarks/recovery-retention-view-before-progress-windows.json)
records an 8.67 s retention pass concurrent with a 900-frame viewer. Capture took
14.7 ms; frame p95 was 16.9 ms and maximum frame work was 17.3 ms. All 100000
objects remained selected, with 792 visible overlays and 3 MiB GPU tile payload.
The scan stays off the render thread, but its duration still delays controller
completion and can delay shutdown. This baseline motivated the subsequent
[maintenance progress/cancellation and profiling](recovery-maintenance-progress.md)
work. Broader cold/network/storage-pressure performance remains unverified.

Automatic discovery/cleanup of abandoned work directories belonging to earlier
session destinations, obsolete lock-sidecar collection and
initial CLI loading progress remain unfinished. All other open M0-M7 requirements
remain active.
