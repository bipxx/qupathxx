# Recovery maintenance progress and cancellation

Autosave now reports checkpoint publication separately from completion of its
background maintenance. The existing `state`/`completed` fields keep their
acknowledged-job semantics. `checkpoint_published` becomes observable on `poll()`
as soon as native atomic publication succeeds, even while the job is still busy.
It describes the current job, not whether a previous checkpoint exists.

`AutosaveStats.maintenance_phase` is `idle`, `scanning`, `collecting`, `complete`
or `cancelled`. `retention_inspected` counts attempted regular checkpoint headers;
`retention_removed` counts removals in that pass. These live counters are atomic
native progress, copied into owner-thread stats by `poll()`. They are advisory
counts, not a percentage: discovering a total would require another directory scan.
No live document, filesystem traversal or worker-owned string crosses the polling
boundary. `stats` returns the most recently polled owner-thread values.

After publication, Recovery shows scan/collection counts and **Stop maintenance
(Ctrl+F10)**. The shortcut works with the section collapsed, subject to the normal
text-entry/modal guards. It only acts on a busy job whose checkpoint is already
published. Plain F10 remains a new recovery request; Ctrl+F10 never starts one.
Stopping shows a pending status until the worker acknowledges the request, then
reports that the published recovery is retained. Retention's integer/day inputs
and status text fit the normal narrow sidebar. `GuiReport.retention_cancelled`
and `retention_inspected` expose replay results. Finite embedded GUI runs join and
acknowledge autosave before constructing the returned report, just as they already
do for explicit saves; their reports no longer depend on render-loop timing.

## Cancellation boundaries

`ProjectAutosave.cancel()` remains an owner-thread request. Before publication,
it keeps its existing semantics: publication can win the race, otherwise the old
checkpoint is retained. After publication, cancellation cannot undo that durable
checkpoint. The job still completes as `ready`, increments `completed` and retains
its session lease. `cancel_requested` reports the request; `retention_cancelled`
reports whether retention observed it.

The native pass checks cancellation before scanning, between directory entries,
before collection and before each removal. It passes the flag to SQLite's progress
handler during header inspection. Cancelling a partial scan skips all collection,
because that partial catalog cannot establish a trustworthy newest set. During
collection, already completed removals remain completed; an in-flight removal can
win the cancellation race. Future candidates are left intact. A subsequent write
resets progress and can finish retention; an unchanged project still needs the
existing explicit retention API or a later changed autosave to run another pass.

Cancellation is cooperative. One current OS operation, SQLite busy wait, native
snapshot release or filesystem iteration can finish first. The existing SQLite
busy timeout is one second; no hard latency bound is promised for cold/network
I/O. Shutdown joins the worker. The synchronous `Project.prune_recoveries` API
remains synchronous and has no cancellation token; the autosave controller owns
this new cancellable path. Cancellation failures are not surfaced as save errors.

## Scan path and profiling

The pass now avoids filesystem identity probes when recorded primary strings
already match. It protects the anchor by ordinal case-insensitive name on Windows
and exact name on POSIX. It reads the anchor's hard-link count once and preserves
the existing filesystem-equivalence checks when aliases actually exist. Aliased
primary paths still use their equivalence fallback. Windows regular-file/directory
preflight uses one attribute query and rejects reparse/device entries rather than
repeating the same query through `symlink_status`. Active-session leases, bounded
heaps and final under-lock validation are unchanged.

`RecoveryRetentionStats.metadata_ms`, `ownership_ms` and `removal_ms` profile
header/scope/ranking work, marker/lease/candidate work and final collection.
Autosave/GUI reports expose the same totals prefixed with `retention_`. They do
not include all directory enumeration, anchor setup or final local destruction;
`elapsed_ms`/`retention_ms` and total worker time retain their separate meanings.
Cancellation and errors can yield partial stage timings.

`tests/test_recovery_retention.py` holds real SQLite exclusive locks in a separate
interpreter while the native controller reports progress, cancels, preserves its
new checkpoint and every candidate, then retries successfully. Desktop replay
stops maintenance through Ctrl+F10 while Undo changes the live authority; captured
checkpoint values remain independent. Other regressions check anchor hard links,
primary hard-link aliases, Windows case folding and active oldest sessions that
must not starve inactive candidates. The existing liveness, move, legacy, marker,
failure/retry and full GUI suites remain required.

The installed wheel passes 337 tests with two unavailable Windows file-symlink
cases skipped. All fourteen native suites pass in full, core and minimal builds.
Collection cancellation also verifies that completed removals stay counted while
later candidates and the current recovery remain available.

The retention benchmark now records stages and live phases. `--cancel-after 100`
stops headless scanning after progress reaches 100 attempted headers and verifies
zero removals plus an intact new checkpoint. Comparisons preserve the original
distinct-session workload and deletion/ownership rules; source fixtures are native
publications with only age timestamps changed. Full M0-M7 scope, cross-session
orphan work, initial CLI loading progress and broader storage/pressure gates stay
open.

## Recorded Windows runs

The [full headless pass](benchmarks/recovery-retention-1000-windows.json) inspects
1001 headers and removes 32 historical checkpoints. Publication becomes visible
after 250 ms; retention takes 8.90 s, including 1.07 s metadata/ranking, 7.78 s
ownership work and 16 ms collection. Maximum owner poll time is 0.16 ms. This
does not establish a full-scan speedup over the preserved
[8.48 s baseline](benchmarks/recovery-retention-before-progress-windows.json).
Stage profiling isolated marker access as the dominant remaining ownership cost;
an experimental native read path did not improve it and was discarded.

The [viewer pass](benchmarks/recovery-retention-view-1000-windows.json) records
8.38 s retention during 900 frames, with 16.9 ms frame p95 and 17.7 ms maximum
frame work. All 100000 objects stay selected, with 792 visible overlays and 3 MiB
GPU tile payload. Historical graphs/pixels are not decoded. These are separate
single-host local runs, not general timing bounds or cold/network evidence.

The [cancelled pass](benchmarks/recovery-retention-cancel-1000-windows.json)
observes 100 headers, requests cancellation while scanning and completes about
9.4 ms later. It removes zero candidates, preserves every historical checkpoint
and verifies the new 100000-object recovery. Its request publishes after 254 ms;
the partial scan takes 878 ms before cancellation. The timing includes normal
poll cadence and is not an interruption guarantee for a blocked OS/SQLite call.
