# QuPath TMA grid interchange

This work targets direct grid exchange using QuPath 0.7.0 `.qptma` text files,
through the native model, Python, standalone project commands and the ImGui TMA
panel. The bounded native codec, protected file IO and atomic document import
are implemented and the focused installed Python checks pass. Candidate 156
implements project CLI import/export and atomic new-project saving.
The final project-enabled wheel passed all 80 QPTMA cases, packaged-stub and
typing checks. The integrated desktop path now passes all nine new GUI cases
and 17 existing TMA GUI cases. Its combined local suite passes 148 installed
cases with no skips, and all 50 native suites pass. See the
[local integration receipt](benchmarks/qptma-integrated-local-windows.json).
Exact-source hosted qualification is next; no supported release or biological
acceptance is inferred. Automatic TMA finding remains separate work.

## Frozen upstream behavior

The reference is QuPath revision `04ccfa4fb7d43e9b566393e08e83690b72248d44`.
Its [TMADataIO public API](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-gui-fx/src/main/java/qupath/lib/gui/tma/TMADataIO.java)
exports grid dimensions and ordered core names, bounds, presence and case IDs in
a tab-delimited `.qptma` file. The file's image hint is informational for this
native importer; it must never cause implicit image opening or path resolution.

`tools/upstream/qupath_tma_reference.groovy` creates a synthetic 2×3 grid using
public QuPath APIs, calls its exporter, imports that export, and separately
imports fractional wire bounds. The actual capture is in
`tests/fixtures/qupath-tma-0.7.0`. The manifest binds the generator, raw files and
log; the optional distribution ZIP and all 496 runtime files were rechecked
against the previously pinned runtime identity after capture. No external slide
pixels are present and no upstream parser/detector implementation is copied.

The capture demonstrates that the upstream writer truncates origins to integers
and rounds sizes upward; its importer accepts fractional bounds. The fixed
examples retain duplicate case IDs, empty IDs, missing flags and tested Unicode,
comma and quote text. The log retains an upstream quote-syntax warning. This is
a fixture-scope observation, not a promise that arbitrary quoted text is safe.
The first exported line is an ephemeral wrapped-server identifier; its bytes
are retained and hashed, but recapture does not promise the same identifier.

## Implementation and acceptance sequence

1. Add an owning native `.qptma` value and bounded UTF-8 text parser/writer.
   Validate exact fields, dimensions, row-major slot count, finite positive
   geometry and presence values. Reject malformed, duplicate/truncated headers,
   unsupported empty geometry, NULs and quota/coordinate overflow. Preserve
   supported core names, case IDs and fractional values. Keep image hints inert.
2. Convert imported values into one compound native edit using existing TMA
   metadata ownership. Preserve per-core names/bounds/missing/case fields, expose
   the chosen axis labels explicitly, reject an existing grid and ensure
   all-or-nothing history, undo/redo and persistence round trips.
3. Expose the same values and conversions through thin nanobind bindings, owned
   snapshots and generated stubs. Check malformed/oversized inputs, GIL/lifetime
   behavior and captured values against the actual upstream fixtures.
4. Add bounded file loading and non-overwriting atomic export, native project
   CLI commands and ImGui import/export with a concrete grid preview and explicit
   reporting of omitted native-only metadata. A `.qptma` file does not carry the
   full project, native IDs, class/hierarchy, axis-label or analysis history.
5. Export a native-produced file and import it using the pinned QuPath public
   API; compare all supported fields. Keep upstream writer quantization visible
   when recapturing exported geometry. Run installed, project, desktop and
   cross-platform gates at the exact source before engineering acceptance.

## Current native engineering evidence

The [native round-trip receipt](benchmarks/qupath-tma-native-roundtrip-windows.json)
records all 48 minimal native suites passing, including captured input parsing,
atomic document history, invalid-geometry rollback, existing-grid rejection,
non-overwriting file export, cancellation, byte/slot limits and a 10,000-core
round trip. `QptmaData` owns complete row-major values. Conversion to native
edits preserves names, bounds, missing flags and case IDs; supplied axis labels
are explicit. Export reports native IDs, grid names and axis labels omitted by
the upstream grid format, plus class/parent fields when present. Vacant slots
reject export; a retained missing core has geometry and remains representable.

Actual native-produced files were imported by pinned QuPath. All six cores in
each file match their expected names, fractional bounds, missing flags and case
IDs, including literal quotes and surrounding spaces. The first failed capture
is retained: applying CSV quoting inserted additional literal quotes. The
corrected codec treats quote characters literally in tab-delimited fields;
tabs and line breaks in string fields reject rather than change values.

Parsing accepts the fixed 0.7.0 seven-column schema, UTF-8 BOM/CRLF and a missing
final newline. Unknown or repeated headers, extra/truncated rows, malformed
presence values, nonfinite/hexadecimal/underflowed coordinates and invalid
geometry reject. Source hints are never opened. Byte limits default to 8 MiB
and cannot exceed 64 MiB; axes have at most 512 entries and grids 10,000 slots.
Names and case IDs have the native model's 4,096/256-byte limits. File inputs
are size-checked before reading and verified against their captured file version.
Publication creates a new file atomically and preserves existing destinations.

Python copies of mutable data are validated before native filtering/IO releases
the GIL; document import retains the owner thread. The minimal installed wheel
passes 189 focused cases with 22 optional project/reader/feature skips. All 56
QPTMA functional, reference and auditor-control cases pass. Generated stubs match
the installed module; strict typing passes three workflows and rejects all three
dedicated invalid QPTMA calls. Hosted auditors require these named cohorts and
the native marker. Candidate 156 adds project persistence/CLI tests and commands. Final local installed-wheel and desktop validation is recorded above.
Integrated platform acceptance remains pending; see the [current delivery order](api-delivery-plan.md). No supported
release, biological or hardware acceptance follows from this native fixture scope.

## Desktop workflow

Open **TMA...** (Ctrl+Alt+T), expand **QuPath grid exchange**, enter a `.qptma`
path and click **Load grid preview**. File reads/parsing run on a worker.
The selected image remains unchanged. Review rows/columns, present/missing
counts and the clipped core list; blue/gray outlines show present/missing cores
on the image. A native spatial index culls outlines; at most 512 are drawn per
frame, with a zoom-in notice when the visible set exceeds that limit.

Click **Apply imported grid** to create one undoable native command. Existing
grids require explicit clearing first. An edited or switched target invalidates
the preview; changing the input path clears it. **Discard grid preview** or
closing the panel drops unapplied values. Cancellation does not undo a file
whose atomic commit already succeeded, and such an export remains reported saved.

For export, review the omitted metadata, acknowledge it, choose a new output
path and click **Export QuPath grid**. The worker writes an immutable grid
capture. Existing files and protected project/image inputs are preserved.
Saving and reopening the project retains imported core metadata and later edits.
The local captured GUI export was imported by pinned QuPath with all six cores'
names, fractional bounds, missing flags and case IDs exact. Both
[preview](benchmarks/qptma-integrated-local/qptma-preview.png) and
[export](benchmarks/qptma-integrated-local/qptma-workflow.png) captures were inspected.

The integration uses the same codec and commands as Python and
`qupathxx_batch --import-qptma PROJECT GRID.qptma OUTPUT.qpxproj --image ID` /
`--export-qptma PROJECT OUTPUT.qptma --image ID`. Project imports create a new
project atomically; `Project.save_new` enforces snapshot/file limits and guards
source continuity. The CLI opens sources without repairing hot rollback journals;
explicit normal project opening remains the recovery path. Dormant journal
headers remain readable. Source hints in `.qptma` never select an image.
