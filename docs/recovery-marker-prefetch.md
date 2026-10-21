# Bounded recovery marker prefetch

Retention now overlaps small ownership-marker reads with header inspection. The
autosave controller still accepts one job with no backlog. Within one retention
pass, a lazy native pool has four readers and at most four pending metadata
records/results. Its queue also has a hard four-task cap for exceptional paths.
Readers only check regular marker files and compare their bounded content with
the expected project/session signature. They never receive models, images,
geometry, Python objects or GUI resources.

The coordinator still inspects all same-directory checkpoint headers, ranks the
newest set, acquires inactive-session leases and manages the bounded oldest heap.
Results are consumed in submission order. Header metadata can advance by a small
bounded window while earlier marker I/O is pending; no full-directory list or
unbounded collection of futures is retained. The same-session lease-sharing
behavior stays on the coordinator, so multiple inactive destinations from one
session remain collectible and active oldest sessions cannot starve other
candidates.

## Deletion and lifecycle invariants

- Publication and private-snapshot release still precede retention.
- Missing, non-regular, unreadable, oversized or mismatched markers stay protected.
- Global session-lease acquisition remains serialized and non-creating.
- All header inspection and pending ownership results finish before collection.
- The publication lock, header/generation/time checks and a fresh marker read are
  still required for each deletion. Prefetch never authorizes removal by itself.
- Cancelling a partial scan drains/discards pending results and removes nothing.
  Collection cancellation preserves already completed removals and stops at the
  next existing safe boundary.
- Every started reader is joined on completion, cancellation and exceptions.
  Partial thread-start failure joins the threads that did start. No reader handle
  survives acknowledgement of the controller job.

The pool is created only when an aged candidate needs ownership inspection. Fixed
reader count, queue cap, pending window and existing heap bounds keep memory and
open handles bounded independently of directory size. Reads can still block in
the OS; cancellation cannot interrupt an in-flight call. This adds at most four
read-only marker handles during a pass, while scientific session leases remain
in user state so an idle scientific project directory stays movable on Windows.

`RecoveryRetentionStats.marker_prefetch_peak`,
`AutosaveStats.retention_prefetch_peak` and `GuiReport.retention_prefetch_peak`
report the high-water mark of pending coordinator records, bounded by four. They
describe the latest completed pass, not a live thread count. The UI continues to
show header/removal progress and the existing stop action; it does not expose
reader implementation details.

`metadata_ms`, `ownership_ms` and `removal_ms` continue to measure disjoint
coordinator stages. Ownership now includes submission, result waits, lease checks
and heap updates. Background read latency overlaps metadata and other reads, so
these fields are not summed reader-thread work or aggregate CPU utilization.
Total pass and worker times retain their existing meanings, including the
distinction between publication visibility and maintenance completion.

## Verification

The existing retention, cancellation, alias, marker and GUI regressions remain
required. Additional coverage checks oversized markers, the pending-record bound,
cancelled-reader teardown before an immediate directory rename and a subsequent
full explicit collection. The controller remains alive across that rename,
separating transient reader handles from its user-state session lease.

An ownership fallback regression covers a controller that publishes project A,
switches to B (releasing A's lease), then returns to A while lease setup fails.
Before an unleased overwrite, it invalidates an old matching managed tag as a
known unmanaged record. If that is impossible, it preserves the previous
checkpoint and reports a failed write rather than publishing with a misleading
ownership tag. A later successful lease can restore management. Unknown marker
contents remain preserved; a Windows read-only-tag test checks rejection.

The benchmark keeps the same 1000 distinct native closed sessions, 100000-object
new snapshot, age/count policy and 32-file deletion cap. Marker checks are not
cached or bypassed, historical graphs/pixels are not decoded, and fixture creation
is excluded. Preserved single-reader results identify the source revision for
comparison. Cold/network/vendor workloads, sustained pressure, cross-session
orphan work, initial CLI progress and all other open M0-M7 requirements remain
unfinished.

## Recorded runs

The [headless run](benchmarks/recovery-retention-1000-windows.json) inspects all
1001 headers and removes the same 32 old checkpoints in 1.92 s, compared with
the recorded [8.90 s single-reader baseline](benchmarks/recovery-retention-before-prefetch-windows.json).
Publication becomes visible after 289 ms; the whole worker takes 2.20 s.
Metadata/ranking takes 1.32 s, coordinator ownership waits/selection 532 ms and
collection 26 ms. Maximum poll time is 0.075 ms and pending records peak at four.
These separate local single-host runs show a roughly 4.6-fold headless reduction,
not a general throughput guarantee.

The [cancelled run](benchmarks/recovery-retention-cancel-1000-windows.json)
observes 100 headers and finishes 9.2 ms after cancellation, with zero removals
and an intact published recovery. Pending records again peak at four. It does not
establish a latency bound for blocked OS operations.

Viewer timing varies more. The [final 900-frame run](benchmarks/recovery-retention-view-1000-windows.json)
records 7.67 s total retention, of which metadata inspection takes 7.13 s and
ownership waits take 341 ms. Frame p95 is 13.5 ms, maximum frame work 55.8 ms and
owner-thread snapshot capture 51.1 ms. All 100000 objects stay selected, with
792 visible overlays and 3 MiB GPU tile payload. An
[initial prefetch run](benchmarks/recovery-retention-prefetch-initial-view-windows.json)
records 2.00 s retention, 16.9 ms frame p95 and a 44.0 ms capture pause. Both are
retained to show variability; no uniform viewer speedup or short-frame bound is
claimed. Metadata latency under concurrent viewing and snapshot capture remain
performance targets, alongside cold/network and sustained-pressure workloads.

Verification: 341 installed-wheel tests pass with two unavailable Windows file-
symlink cases skipped. All fourteen native suites pass in full, core and minimal
builds. No scientific project/recovery schema version changed.
