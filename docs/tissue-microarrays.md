# Tissue microarrays

The native document owns one editable TMA grid. Python uses the same C++ model,
and the ImGui **TMA...** panel (Ctrl+Alt+T) exposes creation, missing flags,
case IDs, core geometry, labels, translation, per-core batches and metadata CSV.
This is an initial research workflow, with broader QuPath comparison still open.

The [desktop capture](images/tma-workflow.png) shows a synthetic nine-core grid,
case ID editing and a gray missing core. The separate
[10,000-core stress capture](images/tma-grid.png) exercises catalog clipping.

## Model and edits

A grid has a name, unique row and column labels, and at most 10,000 slots
(at most 512 labels per axis). Creation uses level-zero pixel centers, two
independent step vectors, and analytic ellipse width and height. Rotated grid
steps are supported; each core ellipse remains aligned to the image axes.

Each occupied slot is an ordinary ellipse annotation with a stable object ID
and immutable TMA metadata. A **missing** core remains an object with geometry
and metadata. Deleting a core creates a **vacant** slot. Adding into a vacancy
allocates a new object ID. Deleting the final core removes the grid. Multiple
cores may share a case ID.

QuPath's [TMA core model](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core/src/main/java/qupath/lib/objects/TMACoreObject.java)
provides the workflow reference for elliptical core ROIs, missing flags and
case identifiers. This implementation does not promise Java object compatibility
or automatic QuPath grid interchange.

Creation, case/missing edits, relabeling, translation and dissolution are atomic
undoable commands. Translation includes descendants and cell nuclei by default.
Dissolving a grid keeps its annotations; `clear_tma_grid(delete_cores=True)`
also removes the cores and their descendants. Generic ROI replacement preserves
TMA tags and rejects converting a core to a non-ellipse or non-annotation object.

```python
import qupathxx as q

document = q.Document(q.Image.demo())
options = q.TmaGridOptions()
options.rows, options.columns = 2, 3
options.origin = q.Point(50, 50)
options.row_step = q.Point(0, 100)
options.column_step = q.Point(100, 0)
ids = document.create_tma_grid(options)
document.set_tma_missing([ids[1]], True)
document.set_tma_case_ids({ids[0]: "case-001", ids[1]: "case-001"})
snapshot = document.tma_grid()
metadata_csv = q.tma_metadata_csv(snapshot)
```

Snapshots retain native values independently of later document edits or Python
object lifetimes. The default snapshot budget is 64 MiB; the maximum is 256 MiB.
Axis configuration is shared across cores and charged once per distinct retained
allocation. Equal configurations supplied by independent native owners are
charged separately. A
separate TMA revision avoids rebuilding the panel catalog after unrelated
detection edits. The panel clips its catalog to at most 512 inspected slots per
frame; this bound is not a hardware frame-time guarantee.

Managed viewers accept `tma_grid_edits(options)` through `ViewerSession.execute`.
Subsequent metadata commands use `ObjectEdit.tma_metadata(id, metadata)` with
`core.tma_core.with_missing(...)` or `.with_case_id(...)`. The native owner applies
the existing expected-revision, command-budget and cancellation rules.

## Persistence and analysis

Project schema 11 stores one checksummed grid configuration per image and one
metadata row per occupied slot. Versions 1–10 remain readable and upgrade on
save. Existing project snapshots preserve the grid and each core's metadata.
Malformed positions, duplicate slots, orphan metadata and invalid configurations
are rejected. GeoJSON export reports `tma_metadata_omitted` and requires explicit
loss permission; it exports ordinary annotations without a TMA grid.

Set `ProjectBatchOptions.tma_only = True` to analyze present cores using existing
ROI, tissue, cell or pixel workflows. `include_missing_tma = True` includes
flagged missing cores. Automatic image scope skips images without grids;
explicitly requesting such an image fails. Explicit object selections intersect
the eligible core set. Empty eligible grids produce empty analysis tables.
The captured request freezes both core geometry and metadata.

The CLI provides `--tma-only` and `--include-missing-tma` with existing batch
commands. The panel's **Per-core batch...** action selects present cores and
opens the common batch launcher. Resumable checkpoints include TMA metadata in
their canonical digest. Case IDs, flags and geometry therefore participate in
checkpoint integrity and request provenance.

Metadata CSV lists every slot, including vacant ones, with zero-based indices,
labels, case IDs, missing flags, object revisions and pixel geometry. Analysis
CSV adds `tma_` context columns to existing rows. Cell tables retain their
existing behavior: a core with zero detected cells contributes no cell rows.
Use metadata CSV to retain the full slot inventory.

`tma_context_csv(snapshot, table)` joins tables with `source_object_id` (preferred)
or `object_id` and checks `source_object_revision` by default. It rejects stale
core revisions, non-core selectors, malformed CSV and duplicate context columns.
Tables without revision selectors require explicit `require_revision=False`.
Input and output are bounded to 64 MiB by default, at most 256 MiB.
The GUI metadata exporter uses a native background task, a captured snapshot and
atomic new-file publication. Existing files are preserved.

## Case summaries

`tma_case_summary_csv(snapshot)` exports one inventory row per case ID. Cores
without a case ID each receive their own row keyed by `unassigned_core_id`.
Missing cores count toward inventory but contribute no measurements. Vacant
slots are reported in the repeated grid context; these grid counts are not
additive across summary rows.

Supply `table`, `value_column`, and optionally `weight_column` to aggregate
replicate-core measurements. Every measurement row contributes to the mean;
this is a pooled row mean, so cores with more rows have more influence.
The weighted mean is `sum(value * weight) / sum(weight)`. Omitted weights are
one. Zero total weight leaves the weighted mean blank. Cases without measured
rows retain blank statistics rather than zeros. Sums, extrema, row counts,
measured core counts and excluded missing-core row counts are exported together.

```python
case_csv = q.tma_case_summary_csv(
    snapshot, measurements_csv, value_column="mean_red", weight_column="pixels"
)
```

Input rows select a captured core using `source_object_id` and
`source_object_revision`, or `object_id` and `object_revision`. Supplied revisions
must match the snapshot, including rows for missing cores. Tables without
revisions require `allow_unversioned=True` and are marked unversioned in the
output. Non-core selectors, malformed records, duplicate headers, nonfinite
values, negative weights and unrepresentable arithmetic are rejected. The exact
input bytes are identified by SHA-256. Input and output each have a 64 MiB
default limit, configurable up to 256 MiB through the native/Python API.

`save_tma_case_summary_csv(snapshot, output_path, input_path="", ...)` performs
bounded binary file reads, revision-checked aggregation and atomic new-file
publication in C++. It returns the published byte count and releases the GIL.
The input file is checked for changes before publication. Existing destinations
are preserved. ImGui uses this same native file exporter on its worker thread.

The native batch executable provides the equivalent single-image workflow:

```console
qupathxx_batch --export-tma-cases project.qpxproj cases.csv --image 1
qupathxx_batch --export-tma-cases project.qpxproj cases.csv --image 1 --measurements core-values.csv --value-column mean_red --weight-column pixels
```

`--image` can be omitted only for a project containing exactly one image.
`--allow-unversioned` is explicit and never bypasses supplied stale revisions.
`--csv-bytes` sets the independent input/output limit (default 64 MiB, maximum
256 MiB). Success emits a `qpx.tma-case-export/1` JSON record with image,
snapshot revision, output path and published bytes. Exit codes are 0 for saved,
1 for data/I/O errors, 2 for malformed arguments, and 130 for cancellation.
The CLI retains and verifies the project file observation through publication.
The [initial installed file/CLI validation](benchmarks/tma-case-file-initial-windows.json)
passes 27 new API/CLI cases, 75 combined TMA cases, 47 native suites and
packaged typing checks. [Eight minimal native CLI probes](benchmarks/tma-case-file-native-cli.json)
match the previous API bytes and preserve protected files; that executable
imports no Python or OpenGL runtime. The [full installed Windows validation](benchmarks/tma-case-file-validation-windows.json)
passes 2,417 tests (13 optional skips, three unchanged strict legacy scientific
xfails), and a captured ImGui export saves the expected weighted result without
UI overflow. The shared exporter/CLI source `083da0c` is promoted after
[Windows/Linux](benchmarks/tma-case-file-windows-linux.json),
[both Mac native](benchmarks/tma-case-file-macos.json) and
[portable consumer](benchmarks/tma-case-file-portable.json) audits.
The input-loading candidate below has separate pending gates.

The TMA panel's Export tab offers **Summarize replicate cores by case**, an
optional measurement CSV, value and weight columns, and an explicit unversioned
input opt-in. A native worker captures the grid and settings, reads at most
64 MiB, verifies the input file has not changed, and publishes a new output file
atomically. Cancellation discards unpublished output. Existing files and input
files are preserved. The [initial local installed validation](benchmarks/tma-case-initial-validation-windows.json)
passes native, Python and desktop checks. The [final panel-label validation](benchmarks/tma-case-validation-windows.json)
also passes the full Windows suite. The initial Mac candidate exposed a missing
C++17 floating parser overload; the [portable signed-decimal replacement](benchmarks/tma-case-portable-parser-controls.json)
passes the [fresh full Windows validation](benchmarks/tma-case-validation-windows.json)
and 46 [macOS arm64 native suites](benchmarks/tma-case-parser-arm64.json).
The case-summary source `914b7e2` is now promoted after
[Windows/Linux](benchmarks/tma-case-windows-linux.json),
[both Mac native architectures](benchmarks/tma-case-macos.json) and
[portable builder/consumer](benchmarks/tma-case-portable.json) audits.
The shared file API/CLI additions have their own pending gates described above. This does not establish biological or
upstream equivalence. The [scale profile](benchmarks/tma-case-profile-windows.json)
retains every trial and verifies counts and statistics independently through
100,000 measurement rows; its timings are synthetic observations.

Automatic dearraying, rotated ellipse geometry, direct
QuPath TMA import/export, survival analysis, biological validation and supported
hardware acceptance remain future work.

The original grid packet's [local validation receipt](benchmarks/tma-local-windows-linux.json) records
the installed wheel/source identities, all 1,818 passing Python tests (six
optional skips), 23 TMA API cases, eight desktop cases, Windows GUI/headless and
Linux training/minimal/sanitizer results. The [GUI probe](benchmarks/tma-10000-gui-windows.json)
inspected at most eight catalog slots per frame with zero UI overflow in the
captured 10,000-core scene. Its timing is a workstation observation, not a
supported hardware guarantee.


## Input loading candidate

The input-loading candidate checks the observed regular-file size against the
CSV budget before opening the data stream, then reserves storage for an admitted
file. Per-chunk limits and before-publication file-version checks remain active,
including if the file changes after the initial observation. The table schema,
statistics and new-file publication contract are unchanged.

A [native regression](benchmarks/tma-case-input-native-windows.json) measured
1,114,112 read-transfer bytes when rejecting a 2 MiB file under a 1 MiB budget
before the change, and zero after it. The
[paired CLI pilot](benchmarks/tma-case-input-pilot-windows.json) retains 32 fresh
process observations, including eight explicit warmups, at three valid sizes
and one oversized case. All valid output hashes and independently checked
statistics agree. The largest 24 MB table's median peak working set fell from
41.8 MB to 31.1 MB. Timing differences are small and remain unaccepted as general
performance or hardware evidence.

Windows accounting comes from [GetProcessIoCounters](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getprocessiocounters)
and its [IO_COUNTERS](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-io_counters)
read-transfer field. These are process-wide bytes read, including cached I/O;
they do not measure physical storage traffic. The [initial installed checks](benchmarks/tma-case-input-initial-windows.json)
pass 47 native suites, 77 focused cases including 29 file/CLI cases, packaged
stubs and typing checks. Twelve altered-evidence controls are rejected. The
[full installed Windows validation](benchmarks/tma-case-input-validation-windows.json)
passes 2,419 tests, 13 optional skips and three unchanged strict scientific
xfails. Its visually reviewed ImGui capture reports saved state, weighted mean
3.5 and zero UI overflow. The [Linux-only audit](benchmarks/tma-case-input-linux-partial.json)
passes all three Linux configurations; complete hosted acceptance remains open.
