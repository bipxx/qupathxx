# Cell speed follow-up - precise costs and exact integer geometry

Final settled cell-pipeline medians are **320 ms native versus 292 ms QuPath**:
about **9.5% native overhead remains** on this single Windows ROI. Strict speed
parity is not achieved. Scientific outputs and stored geometry identity stay exact.
[Results](summary.json), [shared checklist](../../speed-gap-coverage.md) and the
[current task](../../current-task.md) own the remaining target. This follows the
[previous source batch](../speed-gap/README.md); published binaries stay frozen.

## What the profile actually showed

A temporary private-CPP profile on parent4a7000f9 measured approximately:

| Baseline stage | Observed milliseconds |
| --- | ---: |
| RGB/domain/OD preparation | 23 |
| Gaussian/Laplacian | 5 |
| Nucleus segmentation | 106 |
| Feature/body/mask assembly | 62 |
| GEOS context creation, per mask | about1 |
| GEOS segment allocation/free, per mask | 22-29 |
| Per-face intersection, per mask | 58-67 |
| Cold containment across 2,237 pairs | 104-141 |

[Raw profile](profile-run.log), [temporary patch](temporary-profile.patch) and
[containment probe](covers-probe.json) retain scope. Context pooling would target
about 1 ms and introduce locking complexity; it was not implemented. Warm
containment still cost 98-133 ms, so cache warmup did not eliminate that work.
These are diagnostic baseline timings, not a breakdown of the final binary.

The first improvement keeps tagged nominal endpoints native and allocates GEOS
segment objects only for ambiguous labels. Original left-to-right/top-to-bottom
orientation is reconstructed exactly from packed endpoint order. General masks
retain their previous network construction. This removes unused objects without
changing clipping, contours, vertex/edge admission or topology validation.
Independent predicates were also split into two joined ranges; repairs and shared
retained-size updates stay sequential. An eight-task experiment showed no clear
benefit (383 ms versus379 ms detector probes), so its policy was discarded.

## Exact unit-grid containment proof

For dense nominal unit-pixel grids with an integer rectangular ROI, the final
implementation validates actual vector geometry against the label rasters:

1. Confirm the same representable lattice/ROI and ordered component labels.
2. Require actual geometry area and pixel counts to match the raster.
3. Check every unit contour edge separates its label from another label/background.
4. Require its complete boundary length to equal the raster boundary length.
5. Check that every nonzero nucleus label is the same body label at that pixel.

The polygons have already passed GEOS validity. Valid polygon boundaries cannot
repeat unit edges, so edge membership plus equal length establishes complete
boundary equality. The valid bounded polygons therefore represent the same
closed pixel unions as the masks; label-wise pixel inclusion proves continuous
containment, including boundaries. This replaces the general relate query with
an exact, stronger mask/geometry agreement check. It does not approximate
membership, skip shape validity or alter any geometry construction/ordering.

Unsupported/fractional/curved cases retain GEOS Covers and sequential repair.
A conservative density guard (at most1,024 raster samples per cell) avoids full
mask scans on sparse large grids, using the prior predicate there. No public API,
schema, processing policy or precision changed. The new helper is private.

Tests cover same-label/same-area geometry shifted to wrong positions, bodies that
lose nucleus pixels, cancellation,65 nonempty2x2 inclusion patterns, an empty
case, a hole with shared-boundary contact, fractional fallback and the 512-cell
serial/archive/failure fixture. These supplement the existing84 topology cases.
Six affected suites pass in 2.37 seconds: cells, cell_records, tissue_contours,
geometry, geometry_edits and project_save. The original failed test log is kept:
adding value-heavy fixtures overflowed the monolithic test's Windows stack
(exit0xC00000FD). Stain transforms hold roughly 24 KiB of tables. Moving fixtures
to a separate test translation unit fixed this without raising stack limits or
removing checks. Test output is unbuffered; temporary product diagnostics are gone.

## Warmup correction and actual results

The ordinary one-warmup/three-measured protocol initially looked like parity
(native 385/376 ms versus QuPath 399/375 ms), but QuPath samples trended downward.
That interpretation was premature. A separate steady mode now performs six
warmups and six measured full workflows with no mask oracles between samples.
Correctness remains independently gated under the unchanged default protocol.

| Settled protocol | Native median | QuPath median | Native/reference |
| --- | ---: | ---: | ---: |
| Implicit segments/two predicate ranges, before exact proof | 363 ms | 288 ms | 1.26 |
| Exact proof, before sparse execution guard | 329 ms | 299 ms | 1.10 |
| Final source with sparse guard | 320 ms | 292 ms | 1.095 |

Final native samples range 316-327 ms; reference samples 277-330 ms. Other-process
CPU averaged about14%/16% of logical CPU time across final engine lifetimes.
Host/JVM variation persists; there is no statistical equivalence or general
native-speed-superiority claim. Nucleus segmentation is now the next concrete
profile target; constructor pooling and extra predicate workers are not supported
by this evidence. The remaining gap is about28 ms in the paired medians.

All four final correctness workflows preserve exact RGB, nucleus/body label bytes
and full density CSV against the prior frozen native output. Eight comparisons
against the unchanged pinned QuPath reference have identical 2,237 cells, source
RGB, calibration, area and nucleus/body partitions. Final saved-project geometry
has the exact prior ordered labels/pixels/WKT hashes. Save/reopen succeeds; all
steady runs retain the same cell count/RGB sums/calibration/area and native saved
density identity. Density support is the explicit analysis ROI, not inferred
biological tissue. This is not whole-slide segmentation or biological acceptance.

## Source identity and reproduction

[Receipt](receipt.json), [523-input source manifest](source-inputs.sha256) and
[runtime manifest](runtime.sha256) identify the development module. Raw tree:
`d9f6b3c2348f473367358ec726515b07756f8af20eb8050f4d64d96646acfdbe`.
Compiled revision is4a7000f9+dirty; the source commit adds documentation afterward.
The new test translation unit accounts for522 -> 523 compiler inputs. Cached
binding objects were reused. No release wheel/native app was rebuilt or published.

From the active checkout, using fresh output names and the retained runtime:

```powershell
$py = '../../../.venv/Scripts/python.exe'
$bench = 'docs/benchmarks/candidate5-performance'
& $py "$bench/native.py" ../../fixtures/CMU-1.svs ../../speed-gap-followup/native-new ../../speed-gap-followup/runtime-delivery --expected-tree d9f6b3c2348f473367358ec726515b07756f8af20eb8050f4d64d96646acfdbe --context 0 --steady --label integer-geometry-followup
```

For the reference:

```powershell
& '../../upstream/qupath/runtime/QuPath-0.7.0 (console).exe' script "$bench/qupath.groovy" --args ../../fixtures/CMU-1.svs --args ../../speed-gap-followup/qupath-new --args steady
& $py docs/benchmarks/speed-gap-followup/compare.py ../../speed-gap-followup
```

The comparison command checks recorded named blocks; it launches no benchmark.
Omit --steady/third steady argument for the default four-run correctness protocol.
Use measure.py as in the previous report to add actual engine PID/system CPU
counters. The steady mode has no mask-oracle allocations, but its lifetime RSS
still includes startup and repeated workflows; no new resource-only RAM claim is
made. Native/reference statistics and archives differ, so overall/save/reopen
numbers are not equivalent-work ratios.

Large raw projects/masks and frozen experiment modules remain local under
build/speed-gap-followup. Compact receipts, profiles and failures are tracked.
Published candidate5 and prior releases are unchanged. More full workflows are
not a prerequisite for the next step: reuse exact evidence, profile the remaining
nucleus sub-stages, and make one measured change before a final paired check.
