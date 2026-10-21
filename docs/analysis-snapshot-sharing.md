# Shared immutable analysis snapshots

Explicit saves and autosave/recovery now capture archive blocks instead of
copying every analysis record, string and lookup entry on the owner thread.
This changes the implementation of [analysis provenance](analysis-provenance.md);
schema 3, scientific values, record IDs and public Keep/Recheck behavior remain
the same.

Each retained record is allocated as a native `shared_ptr<const AnalysisRecord>`.
Its ROI, input digest, parameters and summary never change after validation.
Verification tokens, weak source references and the per-object lookup index
remain in the live owner's map; they do not enter a worker snapshot.

A document groups record references into blocks of 256 slots. Capturing a save
copies const block references and the captured record count. For 100000 records
there are at most 391 blocks, rather than 100000 record/string/index copies.
The worker iterates the captured blocks directly and skips empty slots.

Once a snapshot or native document copy shares a block, the owner seals it
permanently. A later append/removal prepares a writable clone of the affected
block before changing its slot. Only the new block is unsealed. This avoids
using a changing shared-pointer reference count as a synchronization decision.
Workers read record arrays that remain immutable for their entire lifetime.
A native document copy/assignment also seals shared blocks, preserving the
existing independent-authority and unverified-copy contracts.

Freed slots are reused, so repeated removal/append does not grow slot storage
without bound. The maximum slot count follows the existing 100000-record limit.
Removing the final live record clears its block/free-slot storage; a new archive
starts independently while an older snapshot can still retain its old blocks.
All fallible allocations precede slot publication/removal. An insertion failure
does not publish a partial slot/index or consume a document revision.

Only native immutable records/ROI values are shared with the worker. No live
proof map, source weak reference, document lookup index or Python ownership is
exported. Worker retirement can release old record payloads after the live
document has removed them. The existing source/server snapshot ownership rules
still apply separately.

The payload estimate now includes native record allocation/control overhead.
Snapshot budgets additionally charge complete retained blocks and the block
reference list, including mostly empty backing blocks. Shared data still counts
toward the conservative retained snapshot budget. This is not a physical RSS
measurement or a promise that shared data is free. Native record allocation and
slot references add some bookkeeping overhead to append/load; no uniform
whole-application speedup is claimed.

## Evidence

Five Python regressions in `test_analysis_snapshots.py` cover:

- A writer held under a real SQLite lock while the owner removes records,
  reuses slots across block boundaries and appends different parameters.
- Clearing the whole live archive and creating a new one before that writer
  commits; the saved baseline still contains every original record.
- The same capture isolation through recovery/autosave.
- Budget rejection for mostly empty retained blocks, with a compact-archive
  control that fits the same budget.
- Subprocess teardown after the last live document/analysis owner is removed,
  while a save still owns the captured blocks.

Native analysis tests copy and assign 520-record documents, mutate both sides
across block boundaries, clear/reuse slots and retain the other copy's records.
Existing save-baseline, archive validation, recovery, provenance and GUI tests
also remain required.

The fresh [copying baseline](benchmarks/analysis-records-copy-baseline-100000-windows.json)
and [shared-block recording](benchmarks/analysis-records-shared-100000-windows.json)
use 100000 retained records and 100001 objects on the same identified Windows
machine. Owner-thread capture falls from 52.56 ms to 17.17 ms (about 67% less).
Append changes from 77.3 ms to 84.4 ms; validated load remains about 282 ms in
these two recordings. The shared run records 497 ms worker write, 0.035 ms maximum
save poll and 16.86 ms sampled viewer frame p95, with 910 visible objects and
3 MiB GPU tile payload. These are individual dense-image runs, not a statistical
performance guarantee; viewer timing excludes its first ten frames and swap wait.

The separate [blocked-writer edit recording](benchmarks/analysis-records-shared-edit-100000-windows.json)
verifies the saved old record and exclusion of its post-capture replacement,
with the live project still dirty. Capture is 16.41 ms, the first removal is
0.0217 ms and the following append is 0.0167 ms. This checks that copying the
whole archive was not merely deferred to the first edit.

The workload deliberately retains many versions of one captured ROI/result;
it does not benchmark independent large-geometry analyses. Object/class map
capture and its budget traversal remain linear, and total capture is still near
one display frame. Further object snapshot sharing, broad archive geometry,
source/vendor stress, native external-owner retirement and initial rendering
latency remain optimization targets. Full M0-M7 acceptance remains open.
