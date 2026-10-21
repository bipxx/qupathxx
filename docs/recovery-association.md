# Recovery association after moving a project

Recovery files retain the absolute primary path and save generation captured
when they were written. Moving or copying a workspace can leave that recorded
path missing, or still pointing at the original copy. Recovery can now explicitly
associate the captured work with another primary path **of the same project
identity**, while retaining its original save generation.

```python
import qupathxx as q

items = q.Project.find_recoveries('moved/renamed-study.qpxproj')
restored = q.Project.recover(
    items[0].path,
    base_path=items[0].associated_base_path,
)
# Review the recovered workspace, then save to restored.path or a new destination.
```

`Project.restore_recovery(path, base_path=...)` provides the same validated
association before its in-place authority swap. Leaving `base_path` empty retains
the previous behavior and uses the checkpoint's recorded primary path. Association
checks a read-only native primary header/identity/generation transaction, not
an additional full document/image load. A wrong identity, missing/unreadable
primary, recovery-as-primary or unsupported schema rejects before the current
workspace is replaced. No checkpoint or primary bytes are rewritten by choosing
a location. Schema-1 primaries stay schema 1 until explicit save migrates them.

The checkpoint's **captured** generation is never advanced to match the chosen
primary. If another writer has saved that primary since capture, recovery can
still be inspected but saving onto it rejects as stale. Save the recovered work
to a fresh path instead. A copied primary at the captured generation can be
updated without altering the original copy. An unsaved pre-first-save checkpoint
has no primary baseline and cannot acquire one through this association option;
recover it normally and choose a new save destination.

## Discovery and desktop behavior

`find_recoveries(base_path)` now examines recovery files in that primary's
immediate parent directory and matches validated project identity as well as the
recorded path. Renaming the primary, retaining an older sidecar filename or using
a custom checkpoint name no longer hides a matching saved-project checkpoint.
It does not scan recursively or search other disks/cache directories. The base
identity/generation is read once per scan. Each candidate uses metadata/count
validation only; image pixels, annotation geometry and full-file review hashes
are not decoded/scanned during discovery. The existing newest-first bounded
result heap and `limit=1..1024` contract remain.

`RecoveryInfo.base_path` always remains the path recorded in the checkpoint.
`associated_base_path` is the verified primary location suggested by this scan,
or empty when it cannot be associated. `base_status` is `relocated` when the path
changed but identity/generation match, `matching` at the recorded location,
`changed` when a known primary no longer matches the captured baseline, or the
existing `missing`/`unreadable`/`unsaved` states. An old checkpoint whose recorded
path has been replaced by a different project remains discoverable at that path,
with no association suggestion; it can still be recovered to a separate file.
Corrupt files have no trustworthy identity, so only the existing current-filename
prefix rule adds them as error rows. Standalone `inspect_recovery` continues to
report the recorded primary's status and provides no relocation suggestion.

In Recovery, **Find recovery files (F5)** runs this scan on the existing discovery
worker and expands results when found. Selection prefills the visible optional
**Primary location** field. Blank means use the recorded location; entering a
path explicitly requests verified association. `GuiOptions.recovery_base_path`
prefills that field for embedding. **Open recovery (F11)** passes the choice to
the existing loader worker, then uses the same dirty-workspace decision. The
replacement dialog names the primary path that recovered work will use. Wrong
choices fail without blocking live editing or changing authority. Existing save
generation checks protect explicit GUI/background saves as well as synchronous API
calls. The metadata suggestion is a hint; the loader validates the chosen file
again, and a later save checks its generation inside the write transaction.

Both launchers accept the same explicit option:

```text
python -m qupathxx --recover moved/checkpoint.qpxrecover --recovery-base moved/study.qpxproj
qupathxx_app --recover moved/checkpoint.qpxrecover --recovery-base moved/study.qpxproj
```

`--recovery-base` requires `--recover`. Existing `--image-id` and `--smoke`
behavior is preserved.

## Image links and remaining scope

Association changes the live primary path, not the checkpoint's image descriptors.
Relative image links continue to resolve from the checkpoint's current directory.
Moving primary, sidecar and assets together while preserving their relative layout
therefore restores their links. A checkpoint moved alone, or moved into a different
relative directory layout, may need the existing verified `relink` operation.
Absolute external links retain their literal location. No speculative prefix
replacement or pixel-identity claim was added; source fingerprints remain M4 work.
Checkpoint files in another directory can be selected manually but are not found
by the current primary-directory scan.

Automatic retention, abandoned recovery-work cleanup and initial CLI loading
progress remain unfinished. Native-graph retirement optimization is described in
[project retirement](project-retirement.md); graphs supplied through external
`add_document` control blocks retain the owner-thread safety path.

## Evidence

`tests/test_recovery_association.py` verifies moves/copies/renames with real linked
pixels, metadata/classes/calibration/views, checkpoint-byte preservation, original
copy preservation, stale-generation rejection/save-as, wrong identity, missing
primaries, unsaved checkpoints, custom names, corrupt-prefix visibility, explicit
relink after a checkpoint-only move and schema-1 read-only association/migration.
The native recovery suite checks metadata discovery, relocated association and
saving the copy without advancing the original.

`tests/test_gui_recovery_association.py` replays explicit association, F5 discovery
and automatic prefill, pixel rebinding and save, failed association with usable undo,
stale GUI saves and both CLI launchers. Full storage, recovery and migration
regressions remain part of installed-wheel verification.

`tools/benchmark_recovery_association.py` places a 100000-object checkpoint beside
1000 unrelated empty checkpoints. It verifies one identity match despite a primary
rename, explicit association and save, unchanged checkpoint bytes and no recreation
of the old primary location. `--gui` includes background discovery/load/save while
100000 objects are selected in a narrow view. Construction and buffer-swap wait are
excluded; warm local storage is the measured scope.


The [headless Windows record](benchmarks/recovery-association-1000-windows.json)
finds the one matching checkpoint among 1001 files in 687 ms and validates/loads
its 100000 objects in 167 ms. The
[viewer record](benchmarks/recovery-association-view-1000-windows.json) measures
642 ms standalone discovery, 169 ms recovery worker loading, 14.6 ms native worker
retirement, 17.0 ms CPU frame p95 and 26.7 ms maximum frame work, with 792 visible
overlays and 3 MiB peak GPU tile payload. The viewer scan/load/save occur on their
existing workers. These warm local, single-run measurements do not bound arbitrary
directory sizes or storage/reader latency.
