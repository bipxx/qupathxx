# Background recovery loading and checkpoint deletion

In-window **Open recovery (F11)** now shares the ordinary project loader's
single worker slot. SQLite validation and native graph construction run while
the current workspace remains editable. Only a complete successful load stages
the existing Save/Discard/Cancel replacement decision. Failures preserve the
current authority. The opened recovery keeps its base generation and dirty state,
so a changed primary still requires a new save destination rather than a silent
rebase.

**Cancel opening** prevents adoption while an already running load finishes.
Closing also cancels adoption and joins outstanding work at shutdown. Cancellation
does not interrupt OS I/O or the native synchronous validation operation. Recovery
and ordinary loads cannot queue behind one another. `GuiReport.recovery_load_ms`
records recovery worker time separately from `project_load_ms`. Native/Python
`Project.recover(path)` and initial CLI opening remain synchronous; this worker
is the in-window desktop loading path.

## Reviewed checkpoint deletion

Select a discovered recovery file or enter a path, then use **Delete selected
checkpoint...** in Recovery. Ctrl+Shift+Backspace starts the same review. Review
runs on a worker, validates the recovery header and computes a streaming content
fingerprint while holding the native checkpoint writer lock. The current model
remains editable until the review dialog appears. That dialog names the absolute
file path, UTC date and image/object counts and explains that the action only
deletes the checkpoint. It does not discard live work, alter the primary or turn
off automatic recovery saving. Another session can publish another checkpoint.

Cancel leaves the file intact. **Permanently delete checkpoint** launches a
second worker that reacquires the writer lock, validates the file and compares
its identity, session and content fingerprint with the reviewed file. A changed,
missing, locked or invalid file rejects instead of deleting a newer checkpoint.
An error is shown in the Recovery section and another review can be requested.
A successful deletion removes only that file, refreshes the local candidate
list and invalidates an older in-flight discovery result. Existing primary bytes,
source images and external document references are preserved. The current live
model, dirty state, history and selection are not replaced by maintenance.

Periodic autosave pauses during review/confirmation/deletion. After deletion its
next deadline starts a fresh configured interval; an unsaved live workspace can
therefore create a new checkpoint later. Own-session cleanup on explicit save
continues to use its existing ownership rules. Other sessions' checkpoints are
preserved by default. Opt-in [retention](recovery-retention.md) can collect old
managed checkpoints after their session owners exit. Live owners can publish
again after a reviewed deletion.

```python
import qupathxx as q

reviewed = q.Project.review_recovery('study.qpxrecover')
print(reviewed.path, reviewed.images, reviewed.objects, reviewed.created_unix_ms)
# After the caller chooses this exact reviewed checkpoint:
q.Project.discard_recovery_checked(reviewed)
```

`RecoveryInfo.fingerprint` is populated only by `review_recovery`, not ordinary
metadata inspection/discovery. Passing an unreviewed `RecoveryInfo` to checked
deletion rejects. The original identity-only `discard_recovery(path, identity)`
API remains available with its existing semantics; the desktop uses the checked
API. No recovery schema or on-disk format has changed, so earlier checkpoints
can be reviewed and deleted as well.

The fingerprint is streaming FNV-1a plus byte count, using a 64 KiB buffer. It is
an accidental-change consistency check for cooperating writers, not cryptographic
tamper detection or an authorization boundary. Both review and checked deletion
read the complete file and validate SQLite metadata under the same advisory lock
used by checkpoint publication. The application does not promise protection
against processes bypassing those locks or malicious fingerprint collisions.
The existing zero-byte lock sidecar is retained so its inode cannot be replaced
under a competing writer; its existence does not indicate a live owner.
Memory is bounded independently of checkpoint size; large/cold/network file
review can still take time on the worker. Native calls are synchronous; the UI
orchestrates them asynchronously and joins confirmed deletions before returning
from a finite `show` run. `GuiReport.checkpoints_deleted` counts completed removals
and `recovery_maintenance_error` reports failures.

## Remaining maintenance scope

This implements explicit deletion of valid selected checkpoints. Corrupt files
are rejected and remain for manual filesystem management. [Moved-primary
recovery association](recovery-association.md) and
[exact-destination abandoned work cleanup](recovery-work-cleanup.md) are implemented.
Opt-in automatic [retention](recovery-retention.md) is implemented. Cross-session
work discovery/cleanup and initial CLI loading progress remain unfinished.
Discovery matches validated project identity and recorded paths within the immediate
configured directory. No recursive directory deletion or broad deletion matching
was introduced.

## Evidence

`tests/test_recovery_maintenance.py` checks stable review, replacement detection
even when header fields/file size/mtime match, primary-byte preservation, republish
after removal, rejection of unreviewed/invalid files, writer locks and Windows
sharing-violation failure/retry. The native recovery suite checks reviewed
replacement and deletion with storage enabled and preserves disabled-build checks.

`tests/test_gui_recovery_maintenance.py` covers confirmation, Cancel and blocked
underlying undo, invalid paths, finite-run completion and live editing during a
locked file review. A separate process holds a real SQLite exclusive lock while
recovery loading is cancelled or stages replacement; Undo on the old document
proves loading did not block the render thread. Existing recovery/workspace
replay continues to verify pixel rebinding, dirty decisions and failure handling.

`tools/benchmark_recovery_open.py` measures a 100000-object checkpoint beside a
selected 100000-object live scene. Its default mode loads/replaces/reselects;
`--maintenance` reviews and deletes the separate checkpoint while preserving the
live scene. Both modes verify primary bytes and canonical identity. CPU frame
measurements include concurrent worker I/O and exclude buffer-swap wait. Warm
local dense images are the measured scope; cold/network/vendor and prolonged
memory-pressure workloads remain unverified.


The [recorded loading run](benchmarks/recovery-open-100000-windows.json) measures
178.6 ms worker validation/load, 16.9 ms p95 and 38.4 ms maximum CPU frame work.
It retains no external old-document owner, so replacement includes owner-thread
retirement of the old 100000-object graph. That transition remains a measurable
pause rather than a guarantee of uniformly short frames. This recorded live graph
was supplied through `add_document` and retains owner-thread release; native-factory
graphs now use the [retirement worker](project-retirement.md). The
[maintenance run](benchmarks/recovery-maintenance-100000-windows.json) records
19.6 ms standalone review of the 12.5 MB checkpoint, 16.8 ms p95 and 19.2 ms
maximum CPU frame work during reviewed deletion. Both retain 100000 selected
objects, 792 visible overlays and 3 MiB peak GPU tile payload. These single local
warm-cache runs do not establish general load/deletion/retirement bounds.
