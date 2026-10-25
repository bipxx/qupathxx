# Automatic TMA dearraying: reference preparation

Automatic dearraying remains unimplemented in qupathxx. Seven independent
synthetic masks and a pinned upstream capture now define the first native
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
refusal cases. The native detector still needs implementation and verification.

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
