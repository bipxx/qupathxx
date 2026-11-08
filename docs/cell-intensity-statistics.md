# Retained cell intensity statistics: R6-FEATURE1

Status: [locally verified workflow](benchmarks/cell-statistics-r6/README.md) at
60a0274e. Production results, replay, QCL10/11, QCA10, owning bindings, CSV/CLI and
selected ImGui export are delivered. Broader R6/scientific/resource work remains open.
The [current task](current-task.md) owns the next action. This is one delivered
vertical workflow, not completion of the R6 feature family.

## Reference and values

The pinned [QuPath0.7 detector](https://github.com/qupath/qupath/blob/v0.7.0/qupath-core-processing/src/main/java/qupath/imagej/detect/cells/WatershedCellDetection.java)
emits nucleus sum/min/max/range/standard deviation and cell/cytoplasm min/max/
standard deviation, in addition to means. The cached actual0.7 runtime generated
[six scalar references](../tests/fixtures/cell-intensity-statistics-v1/reference.json)
with [script/JAR hashes](../tests/fixtures/cell-intensity-statistics-v1/manifest.json).
An older local cell94 Java source is not the current reference.

The new measurement format is `qpx.cell-intensity-statistics/1`. It summarizes
exactly the decoded RGB and transformed channel samples already supplied to the
selected detector recipe, on its existing nucleus/body masks. RGB units are
8-bit device values; transformed units/meaning follow the recorded OD versus
stain feature space and existing recipe precision. This does not create raw
fluorescence measurements or make different input/sampling recipes equivalent.

- Nucleus: pixels bearing this nucleus label. Cell: pixels bearing this body
  label, including the nucleus. Cytoplasm: body pixels with no nucleus label.
- Retain compensated sum, minimum, maximum and sample standard deviation for
  each RGB and transformed channel. Range is maximum minus minimum; existing
  canonical means retain their existing arithmetic and values.
- Standard deviation is sqrt(M2/(n-1)); use stable online central moments, not
  sum-of-squares subtraction. Singleton SD is unavailable, not zero. Constant
  multi-sample SD is zero. Empty cytoplasm is absent. Unknown historical
  statistics are also absent and must not be reconstructed from means.
- RGB foreground must be finite and in[0,255]; transformed foreground finite,
  with signed values allowed. Ignore background samples rather than treating
  padding as data. Reject invalid shape/label relationships, overflow, exhausted
  budget and cancellation. Retain a complete result or publish nothing.
- Scalar reference/independent numerical tests use1e-12 relative/absolute scale
  tolerance; use existing float32 versus float64 source precision intentionally.
  This is engineering/statistical correctness, not biological acceptance.

The shared reducer in cpp/cell_intensity_statistics.hpp handles uint8/
float natural inputs and double grid inputs, all compartments, finite checks and
explicit extra work-memory admission. Its result and accumulator arrays are
charged before allocation. Both production kernels feed these moments inside their existing pixel
accumulation loops, with no second read/decode or separate feature pass.

## Delivered API and persistence route

1. Keep CellFeature's existing counts/means and layout intact. Expose the typed
   statistics structures at the native API, and store one contiguous statistics
   vector per CellResult. Add an O(1) owning/value-returning per-label accessor
   and an optional measurement-format identifier. Valid historical labels return
   unavailable; invalid labels still refuse. A declared new format with zero
   cells remains distinguishable from an old record.
2. New requests capture the new measurements. Saved replay preserves its recorded
   measurement policy, including old records without statistics. Keep input RGB
   formats/hashes and detector algorithms unchanged: measurement format is
   separate output identity. Include that identity and retained statistics in
   request/result verification. Preserve existing means and historical bytes.
3. Add QCL10 for natural-grid measured results and QCL11 for explicit-grid measured
   results. Use direct versioned fields in the existing codecs, not nested full
   archive envelopes/copies. Natural10 writes the actual algorithm explicitly;
   grid11 writes an explicit area/legacy-nominal/exact-nominal descriptor kind.
   Do not infer nominal geometry merely from version>=6 or exact support merely
   from version>=9 for these new versions. Reuse existing fields/validation and
   append known statistics per label. Old QCL1-9 keep exact decode/reencode and
   replay meanings. Validate counts, finite ranges, extrema and SD availability.
4. ObjectFeatureTable.cell_feature continues its existing mean/count schema.
   Capture selected-row statistics separately and expose an owning per-row
   accessor, preserving the existing forest feature schema/options. Use compact
   storage for known rows so old mean-only snapshots do not allocate a large
   empty statistics object per row. QCA10 retains optional statistics per row,
   including mixed historical/new sources; source kinds0-5 retain their meanings.
   New model feature selections belong to R8 and must not silently change old
   model feature order or units.
5. Add opt-in cell-table statistics columns with an explicit format column;
   existing default CSV schemas remain unchanged. Existing versions reach12, so
   reserve13/14 for statistics without/with capture calibration. Empty/unknown
   values emit empty cells, distinguishing unavailable singleton SD from zero.
   Reuse CellTableRequest/export jobs and thin native CLI/ImGui consumers.
6. Bind the new immutable owning/value structures and accessors through nanobind.
   Verify ownership after document/result retirement and typing. Settle this
   surface and persistence before one combined cached candidate and one stub
   generation. Reuse existing environments/readers/masks. Exercise affected
   installed SDK, export/CLI/batch, desktop and save/reopen flows.

## Verification scope

Native tiny masks preserve old means while producing independent exact
compartment expectations and correct availability at n=0/1/2. The pinned
source/masks agree with independent NumPy statistics at the correct source precision.
Checks cover natural, area and both nominal coordinate policies, capture-time channels,
empty cytoplasm, selected-row snapshots, mixed QCL/QCA versions, malformed
statistics refusal, atomic budget/cancellation and zero-cell format roundtrip.
Old reference archives retain byte/replay meanings. Installed export, owning
nanobind and typing checks close the declared route. Exact old grid workspace and
classifier snapshot quotas are preserved through frozen legacy bookkeeping; new
measured storage remains fully charged. CONTEXT1/SAMPLING1 sampling evidence is
reused, with no broad qualification or new biological/host acceptance.


## Python inspection and export

```python
result = q.detect_cells(request)
if result.cell_count:
    statistics = result.intensity_statistics(1)
    if statistics is not None:
        print(statistics.nucleus.channels.minimum)
        print(statistics.nucleus.channels.sample_standard_deviation)
record = document.keep_cells(request, result)
options = q.CellTableOptions()
options.include_intensity_statistics = True
csv = q.cell_measurements_csv(q.CellTableRequest(document, [record], options))
```

Historical records return None rather than invented statistics. Singleton SD and
empty cytoplasm are unavailable; a multi-sample constant channel has SD zero.
ObjectFeatureTable.cell_intensity_statistics(row) inspects captured selected-row
values when the existing cell_rgb preset captured cell measurements; unknown or
uncaptured values return None. This adds no new forest feature preset.
