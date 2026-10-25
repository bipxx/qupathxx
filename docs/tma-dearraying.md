# Automatic TMA dearraying

Native detection, Python, ImGui preview/apply, managed-viewer detection and CLI
proposal export, including explicit physical sizes, now pass local engineering
checks. Real-slide/biological validation and full supported-host/package qualification
remain open. These local checks do not qualify a distributable wheel.
Seven independent synthetic masks and a pinned upstream capture define the
workflow's geometry, missing-core behavior and refusal cases. The
[fixture manifest](../tests/fixtures/tma-dearray-v1/manifest.json) binds the raw
masks, preregistered contract, capture command/log and observations. These are
synthetic engineering references; biological and large-slide acceptance remain open.

The [pinned QuPath detector](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core-processing/src/main/java/qupath/imagej/detect/dearray/TMADearrayer.java)
and [plugin](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core-processing/src/main/java/qupath/imagej/detect/dearray/TMADearrayerPluginIJ.java)
use supplied grid dimensions and approximate core diameter, with density used
to mark missing cores. The detector documents its expectation of some observed
cores in every row and column. The capture invokes its binary-mask stage;
full RGB preprocessing and its ImageJ implementation have not been copied.

## Preregistered observations

Integer ellipse centers and exact rational pixel-center membership generate the
masks independently. Requested dimensions are 3 rows by 4 columns. The two-pixel
center tolerance was committed at `7613660` before running QuPath. These units
are pixels in the small synthetic masks; they do not define whole-slide or
physical-unit tolerances.

| Case | Upstream dimensions | Maximum center error | Missing flags |
| --- | --- | --- | --- |
| Aligned | 3x4 | 0 px | Match |
| Interior missing core | 3x4 | 1.4142 px | Match |
| Rotated, two missing cores | 3x4 | 2 px | Match |
| Skewed, missing core | 3x4 | 2.2361 px: outside tolerance | Match |
| Anisotropic cores | 3x4 | 1.4142 px | Match |
| Entire middle row unobserved | 2x4 | Requested dimensions not established | Requires review/refusal |
| Blank input | 1x1 | Requested dimensions not established | Requires review/refusal |

All raw outcomes remain in the capture. Four of five full-grid cases satisfy
the fixed geometry and missing-flag contract. The skewed discrepancy remains
open; no tolerance was adjusted. Neither a successful capture process nor the
upstream returning a grid establishes acceptance for a requested layout.

Run the offline checks from the repository root:

```powershell
python tools/reference_tma_dearray.py --check
python tools/assess_tma_dearray_reference.py
python -m pytest tests/test_tma_dearray_reference.py
```

The assessor pins the runtime/capture identity and mask hashes, rejects missing,
duplicate and malformed cases, and records geometric discrepancies explicitly.
Its process can complete successfully while reporting a scientific discrepancy.
The 18 controls cover altered inputs, identity, coordinates, densities and
refusal cases. Native engineering checks are separate from this upstream assessment.

## Native contract and current evidence

`tma_dearray.hpp` exposes `TmaDearrayOptions`, immutable `TmaDearrayResult`
proposals, `tma_dearray_work_bytes` and two `detect_tma_grid` overloads. One
accepts an owning RGBA preview with explicit level-zero dimensions; the other
plans one natural pyramid read and applies the existing exact area filter.
Core width/height default to level-zero pixels. `TmaCoreUnits.Micrometers`
requests use separate calibration axes, as described below.

Opaque pixels contribute integer luminance to an Otsu threshold with explicit
dark-on-light or light-on-dark polarity. Eight-connected components are admitted
by supplied size, extent and shape limits; components touching invalid support
or the image boundary are excluded. Projection groups and a least-squares
affine lattice establish every requested row and column. A completely absent
axis rejects rather than inventing geometry. Observed centers remain measured;
only absent slots use fitted centers. Elliptical tissue density sets missing
flags, with separate valid-support checks. This is an independent algorithm,
not a port of QuPath's ImageJ preprocessing or detector.

Preview dimensions, candidate count, pair work and conservative payload bytes
are bounded. Server admission precedes decoding; malformed buffers, changed
metadata and closed sources reject. Cancellation is checked through the CPU
stages and after synchronous IO. Reader caches and process RSS are outside the
payload ledger. SHA-256 input and versioned recipe identities accompany each
proposal; detection itself never mutates a document.

The focused Linux native probe passes all five preregistered full-grid cases
within the unchanged two-pixel tolerance (maximum observed error approximately
2.85e-14 pixels), with exact missing flags. It rejects blank and unobserved-row
inputs and checks singleton/one-axis grids, exact/minus-one byte admission,
polarity, invalid support, malformed readers, source changes, cancellation,
natural-level coordinates and retained proposals. Existing QPTMA import gives
one atomic undoable edit and wire round trips. An initial expanded test used
an invalid pyramid level-zero fixture; that fixture was corrected without
changing the detector. This focused probe links the new kernel against a
retained native core. Subsequent configured Windows and Linux builds each pass
five selected native suites. The isolated Windows module passes 214 installed
cases with ten declared opt-in desktop skips, all 53 inherited desktop cases,
seven strict positive typing files and three intended negative typing errors.
Generated stubs match the installed extension. See the
[local receipt](benchmarks/tma-dearray-native-local.json). The desktop cases
exercise existing TMA/QPTMA/viewer behavior, not an automatic dearraying UI.
These checks establish neither biological nor release acceptance.

Python mirrors the two native overloads and options. Result grid/center values
are returned as copies so Python callers cannot mutate a retained proposal.
`tests/test_tma_dearray.py` exercises the independent contract, retained results,
preview/server identities, byte admission and atomic publication.

## Completed frontend route

Open the **Tissue microarray** panel and expand **Grid detection and QuPath
exchange / Detect cores in this image**. Supply rows, columns and approximate
core dimensions in level-zero image pixels, choose polarity and missing-tissue
fraction, then select **Detect grid preview**. Preview limits bound dimensions
and working memory. The same native `TmaDearrayOptions` can be supplied through
`GuiOptions.tma_dearray_options`, including its additional admission/fit limits.

Detection runs in the existing exchange worker. Its owning result and grid
share immutable storage; the preview controller does not copy the core grid or
run another detector. The panel displays candidate count, lattice residual and
inferred slots, with existing blue/gray overlays and a bounded core list.
**Apply grid preview** performs one atomic QPTMA import, followed by normal
undo/redo and project persistence. Discard or panel close clears an unapplied
proposal. Image identity, reader identity and document revision guard delivery
and apply. `GuiReport.tma_dearray_result` retains a completed preview; controller
apply/discard/cancel/stale handling releases that preview reference.

The managed viewer exposes `session.detect_tma_grid(image_id, options,
expected_revision=None)`. It copies parameters into the existing typed queue,
resolves the live native reader on its owner and returns `event.tma_dearray`
without editing the document. Publication checks the captured document identity,
revision, reader and workspace epoch. Result payloads count toward
`ViewerLimits.result_bytes`. A producer explicitly applies the proposal through
the existing queue:

```python
event = session.wait(session.detect_tma_grid(image_id, options))
if event.state == "succeeded" and event.tma_dearray is not None:
    apply_ticket = session.execute(
        image_id, q.qptma_grid_edits(event.tma_dearray.grid),
        expected_revision=event.document_revision,
    )
```

The viewer must still run on the Python main thread; the producer uses the
thread-safe endpoint. Consume completed tickets as described in the
[live-viewer contract](live-viewer.md). Apply uses the existing edit-count,
command and result limits. An existing grid must be cleared explicitly before another import.

The standalone batch executable exports a proposal from one explicitly selected
project image. It opens the project without journal recovery, protects project
and linked-image inputs and atomically creates a new file. Existing output,
invalid/duplicate options, missing image selection, cancellation and exhausted
budgets leave the source and destination unchanged. Applying remains explicit:

```text
qupathxx_batch --detect-tma source.qpxproj proposal.qptma --image 2 --rows 3 --columns 4 --core-width 32 --core-height 32
qupathxx_batch --import-qptma source.qpxproj proposal.qptma applied.qpxproj --image 2
```

`--help` lists polarity, density, component/fit, preview/source pixel, memory,
candidate/pair-work and output-text bounds. `--core-units px|um` selects size units (default pixels).
Micrometer requests require the selected document's calibration; it is never inferred. JSON output records input and
recipe digests. The original project remains a separate file.

At compiled source `a492ea5`, five selected native suites pass on Windows and
Linux, including owner and standalone-command semantics. The isolated Windows
module passes 222 installed cases with 19 declared desktop skips, nine new
dearray desktop cases, all 53 inherited desktop cases, seven positive typing
files and three intended negative typing errors. Generated stubs match the
module. The first CLI fixture incorrectly used a C++ setter name instead of
Python's `active_image` property; its failure and corrected run are retained,
with all 317 compiler inputs unchanged. See the
[frontend receipt and inspected captures](benchmarks/tma-dearray-frontends-local.json).

The earlier axis candidate's original jobs all completed, but its strict engine
and portable audits rejected a stale QPTMA control-case manifest. The independent
collection guard and corrected manifest pass 330 auditor controls locally. See
the [unaccepted campaign record](benchmarks/tma-axis-196-audit-rejection.json).
Those artifacts do not qualify these newer dearraying APIs.

## Native delivery route

1. Reuse `plan_thumbnail`/`read_thumbnail` for bounded low-resolution RGBA input.
   Freeze per-axis geometry, core-size units, polarity, alpha/invalid support,
   source/working limits and cancellation before adding the detector. Unknown
   physical calibration must reject a physical-unit request explicitly.
2. Detect bounded candidate components and fit the supplied lattice, including
   rotation/skew, per-core confidence and missing slots. Require requested rows
   and columns to be established; expose ambiguous/empty inputs for review or
   refusal. Use these independent centers and fixed tolerances as the first
   oracle. Keep the upstream discrepancy visible in comparisons.
3. Return an immutable proposal with an owning `QptmaData` export, parameters,
   input identity and confidence. Reuse the current `GuiTma` preview controller,
   native grid edits and a single atomic apply. Capture document/image identity
   and revisions before work; stale/cancelled proposals must not mutate state.
4. Expose the same native result through thin Python bindings, owner-queued
   managed-viewer operations and CLI preview/export composed with existing
   project/QPTMA commands. Verify preview/apply, undo/redo, save/restart/export,
   cancellation, resource rejection and copied-input/retained-result lifetimes.
5. Complete independent fit/detection fixtures and supported-host qualification,
   then biological/upstream, large-slide, anisotropic physical calibration and
   reproducible workflow/provenance acceptance. All original M0-M7 requirements
   remain active; this reference packet does not complete M6.


## Physical sizes and required qualification cohorts

`TmaDearrayOptions.core_units` defaults to `TmaCoreUnits.Pixels`; existing
pixel request recipe bytes are preserved against the retained `a492ea5`
module. For `Micrometers`, width and height resolve independently as requested
size divided by micrometers per level-zero pixel. Unknown, partial, nonpositive,
nonfinite or unrepresentable calibration rejects before the detector reads pixels.

For a standalone server request, omitted `pixel_width_um`/`pixel_height_um`
options use the reader's complete metadata pair. A preview-only request requires
an explicit complete pair. The owner-side `tma_dearray_options_for_document`
helper captures the document's actual calibration, including overrides, and
rejects an explicit pair that contradicts it. It returns owning options and
never changes the document. ImGui, managed requests and CLI use this same helper.
Managed capture occurs when the owner starts the command; the existing revision
and identity guards reject calibration changes before publication or apply.

`result.core_size` records requested units/dimensions, resolved level-zero pixel
sizes, actual calibration and whether it came directly from the reader.
Micrometer recipes use `qpx.tma-dearray-physical/1` over the effective pixel
recipe, requested/resolved sizes, calibration and its reader/captured origin.
Equivalent pixel geometry with different physical context therefore has a
separate identity. Fit residuals and rotation bounds retain the existing
core-normalized coordinate policy. Conversion does not alter the fitting kernel.

```python
options = q.TmaDearrayOptions()
options.core_units = q.TmaCoreUnits.Micrometers
options.core_width, options.core_height = 16, 12
captured = q.tma_dearray_options_for_document(document, options)
proposal = q.detect_tma_grid(document.server, captured)
```

```text
qupathxx_batch --detect-tma calibrated.qpxproj proposal.qptma --image 2 --rows 3 --columns 4 --core-width 16 --core-height 12 --core-units um
```

The ImGui selector labels both dimensions in the selected units and displays
the document calibration. Preview limits expose source/output pixel admission
as well as working memory; component/fit limits expose native bounds and work
limits. Changing a parameter clears the old preview. Cancel and state remain
above the preview list. The [inspected physical capture](benchmarks/tma-dearray-physical-local/installed/physical-preview.png)
shows 16 x 12 um with 0.5 x 0.25 um/pixel document calibration.

Compiled source `4b0e131` passes five selected suites on each Windows/Linux host.
The isolated Windows package passes 281 selected installed cases with 23 declared
desktop skips, 13 dearray desktop cases and 53 inherited desktop cases, positive
typing and six intended negative typing errors. A real generated OME-TIFF
pyramid selects level 1 while 16 x 12 um resolves to 64 x 96 **level-zero** pixels
at 0.25 x 0.125 um/pixel. This is synthetic reader/calibration integration,
not vendor, biological or whole-slide acceptance. See the
[physical-size receipt](benchmarks/tma-dearray-physical-local.json).

The producer and engine/Mac-native/portable auditors now require named dearray
coverage: independent geometry/missing flags, admission/cancellation/history,
physical sizes, explicit enabled/disabled project semantics, 51 installed cases,
13 GUI cases or their exact declared headless skips, one dedicated positive
typing file and two negative typing files (six errors). All 373 inherited/new
auditor controls pass, including independent source collection and wiring
checks. Actual local native/Python/GUI/typing evidence passes the new helper;
older pixel-only native and installed evidence rejects. These required gates
still need a complete fresh-candidate campaign before platform acceptance.
