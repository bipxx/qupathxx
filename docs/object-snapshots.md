# Immutable object snapshots for saves and recovery

Project-enabled builds maintain immutable object blocks alongside the canonical
document map. A block covers at most 256 consecutive object IDs and contains only
occupied IDs in a compact sorted vector. A sparse ID near the unsigned 64-bit limit allocates one block;
it does not allocate the intervening ranges. Blocks share immutable ROI storage
and own native names, classes, parent IDs, kinds, nuclei and object revisions.

Every object transaction prepares replacement blocks before changing canonical
state. Geometry validation, history budget checks and all fallible allocations
precede publication. Commit transfers prepared map nodes without allocating.
Undo/Redo, class renames, subtree deletion, interchange and ordinary edits use
the same path. Blocks that become empty are removed. Each block caches its
logical payload; replacing a block updates the document total incrementally.

Save capture copies handles to occupied blocks and checks their cached aggregate
payload, rather than copying every object and traversing ROI coordinates again.
Worker-only documents retain these blocks and the captured object count. The
SQLite writer iterates their immutable values in native ID order. Recovery
metadata includes the captured count even though worker snapshots have no live
object map. Normal synchronous saves continue using the canonical document map.

Loading builds the blocks on the loader thread before publishing a document.
The first later save therefore uses the same capture path as subsequent saves.
Copied live documents share immutable blocks until an edit prepares replacements.
Native-only payloads can retire on a worker without Python reference cleanup.
Project-disabled builds omit block maintenance.

Capture work scales with occupied blocks, classes, images and saved views.
Class-registry copying and image/view traversal remain linear in their own
counts. The memory budget still conservatively charges the object payload even
when it is physically shared, plus block and handle overhead. This is a logical
snapshot estimate, not a hard process-memory cap.

The live document keeps both its canonical map and immutable save values. This
uses additional block handles, object values and string storage, while ROI coordinate
storage remains shared. A local edit clones at most 256 values per affected
block. Large batch edits and loading also build those save values, so capture
improvement must be assessed alongside edit/load cost and memory use. No schema
or file-format change is required.

The native boundary test covers three blocks, cell/nucleus geometry, class
renaming, copy isolation, subtree deletion, Undo/Redo, empty-block reclamation and
recovery metadata. Python checks cover exact captured graphs at block boundaries,
mixed edits, failure atomicity, payload changes, reload-before-first-save and
sparse unsigned IDs. Existing save/recovery, archive, interchange, history and
desktop checks remain required.

## Paired Windows recordings

The preserved wheel from commit `176412c` and the new wheel run the same
100000-object workloads in separate interpreters. Each JSON recording includes
its native binary SHA-256. The warm local SQLite
[before](benchmarks/object-snapshot-before-100000-windows.json) and
[after](benchmarks/object-snapshot-after-100000-windows.json) runs record:

| Measurement | Before | After |
| --- | ---: | ---: |
| Owner-thread capture | 16.50 ms | 0.0246 ms |
| Complete request | 16.60 ms | 0.0570 ms |
| Worker write/release | 366.83 ms | 356.36 ms |
| Logical snapshot bytes | 38749408 | 38793256 |

Both verify captured data, newer live edits remaining dirty, generation
reconciliation and the same 12554240-byte file size. These are single warm runs;
the capture reduction is not a general storage-latency guarantee.

The [document before](benchmarks/object-snapshot-edits-before-100000-windows.json)
and [after](benchmarks/object-snapshot-edits-after-100000-windows.json) recordings
retain the edit-cost tradeoff. Rename p95 changes from 0.0021 to 0.0052 ms;
creation records 286.13/220.76 ms, Undo 273.96/117.34 ms and Redo 130.76/140.44 ms.
Batch timings vary substantially and do not establish a batch-speed improvement.
The rename history payload remains 815 bytes. Additional live object/string
storage is separate from history and the logical snapshot estimate.

The [2560-by-1440 viewer](benchmarks/object-snapshot-view-100000-windows.json)
keeps all 100000 objects selected, with 805 visible overlays and 3 MiB GPU tile
payload. Save capture records 0.0686 ms, sampled CPU frame p95 17.26 ms and maximum
18.38 ms. The post-capture camera edit remains unsaved, and captured annotation
content survives reload. Broader classes, geometry, hardware, memory pressure,
cold/network storage and vendor readers remain outside these recordings.

Reproduce with `tools/benchmark_project_save.py` (optionally `--gui`) and
`tools/benchmark_document.py`, using the installed wheel's interpreter.


The final installed wheel passes 736 Python tests, with two unavailable Windows
file-symlink cases skipped; all twenty-two native suites pass in full/core/minimal
builds. Nine new Python cases verify the block/payload contracts. Recovery-dialog
replay now waits for the existing visible/enabled button readiness condition;
identity, captured-state, injected-error and framebuffer assertions remain intact.
