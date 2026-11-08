# Contour construction optimization

Observed local detector median is **341.3 ->302.9ms**, an **11.2% reduction** in
six interleaved baseline/candidate pairs on the selected512x512/2,237-cell ROI.
Serial contour medians improve **12.0% for nuclei** (113.7 ->100.0ms) and
**18.1% for bodies** (135.9 ->111.3ms), from12 interleaved pairs.
[Detector samples](detector-pairs.json), [contour samples](contour-pairs.json),
[receipt](receipt.json). This is one local workflow observation, not a general
QuPath superiority or release claim. It does not add to prior percentages as a
promised cumulative speedup.

## Remove repeated work without changing geometry

The native contour constructor already derives directed edges from raster label
boundaries. This patch carries each selected face's known label forward, moves a
standalone polygon instead of cloning it into its owning collection, and retains
an exact simple-cycle certificate for the narrow case described below. The
certificate replaces a duplicate pre-clipping GEOS validity computation; it does
not change geometry construction, mapping, clipping or final validation.

The exact certificate applies only to a **single unambiguous nominal raster cycle**:

1. The boundary-run emitter produces the planar pixel-transition graph. Runs split
   at every change in the two adjacent labels; crossings/touching junctions are
   therefore noded. Edges are axis-aligned raster boundaries, not arbitrary input
   geometry or an external assertion of validity.
2. Sorted directed origins have no duplicates. Following the unique outgoing
   edge visits each edge once and closes at its initial vertex; missing endpoints
   or revisiting an edge before closure already throw. Any repeated vertex would
   revisit its unique outgoing edge.
3. Exactly one resulting closed ring is present. In this noded planar graph its
   vertices/edges cannot cross or repeat, so it is a simple shell with no nesting
   or hole-assignment question. Its positive raster area is still checked.

Ambiguous diagonal/touching vertices, multiple rings/holes/disconnected parts,
non-nominal scientific rasters and streamed/general contours keep the existing
GEOS validity path. Every final clipped polygon/multipolygon still passes full
GEOS validity, nonempty and positive-area checks in Roi::multipolygon. World
coordinate mapping, robust GEOS intersection, vertex/edge limits, cancellation,
label/raster-area agreement, canonical output order and resource charges remain.
No trusted-validity flag is exposed through the public SDK or Python.

The compact face certificate is local to one native trace and uses the historical
face-query allowance; budget admission precedes retaining a nominal face. The
estimated work and retained bytes remain equal to baseline, as checked in every
measured pair. There is no new cache, thread, reader, public header/binding or schema.

## Profile and discarded interpretation

[Temporary profile](profile.log), [diagnostic patch](profile.patch): per-mask robust
intersection costs about59-67ms, validity/area about17-21ms, final ROI construction
about18-23ms, polygonization12-14ms, face queries1-3ms, mapping~2ms and extraction
~1.5ms. Scope cleanup falls into adjacent stage intervals; these are diagnostic
observations with clock/log overhead, not candidate speed measurements.
Local GEOS3.15 source confirms the existing C intersection uses robust overlay;
we did not bypass it, switch precision or repeat a failed rectangle shortcut.

The initial duplicate-query/clone-only trial was inconclusive: interleaved medians
changed by~0.6%, with paired differences overlapping noise. Its
[complete samples](duplicate-work-trial.json) are retained. The earlier
[serial A/B/A block](initial-serial-drift.json) drifted enough that the later
baseline outperformed the candidate. Neither is used to claim an improvement.
The meaningful candidate adds the exact raster-cycle proof to avoid redundant
pre-clip validation while retaining final validity. Do not interpret the smaller
trial as separately established speed savings.

## Verification and measurement

[Three affected native suites](native-tests.log) pass in2.62s: tissue_contours,
cells and cell_records. Nominal versus independent general contours now compare
596 cases: all81 three-label2x2 patterns, three larger checkerboard/nested/
disconnected patterns and all512 binary3x3 masks. Every pattern checks exact WKT
and admits its exact work allowance while refusing one byte less. Existing
cell numeric, cancellation, worker-failure, archive and contour-limit assertions
remain in those suites.

Every interleaved trace/detector pair has equal ordered geometry, edge/vertex counts,
estimated work and retained bytes. The [final installed replay](installed-check.json)
retains all2,237 Cells, exact nucleus/body masks, ordered WKT and stored features;
a freshly computed replay result verifies through the native request contract.
Prior density/export numerical evidence is reused because its authorities and
inputs are unchanged, rather than rerunning a broad qualification campaign.

Two persistent workers load retained baseline/current modules and warm four times.
The controller alternates order for12 serial-contour pairs, then six detector pairs;
**timed operations never overlap**. Geometry/hash observations occur outside timers.
The detector measurement includes its actual native concurrent nucleus/body trace
path. All samples are retained; no new QuPath/JVM run or archive output was needed.
Process scheduling and background load can still affect these samples; no formal
confidence interval, general platform win or overall workflow/archive ratio is claimed.

[Worker](worker.py), [controller](interleave.py), [source hashes](source-inputs.sha256)
and [build manifest](build-manifest.hpp) identify final tree
`2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e` (568 inputs).
All38 compiled binding objects are unchanged. Existing app/batch/region/module
relinked, and the same development installation was refreshed. Temporary profiling
source was restored using a fresh timestamp; accepted workers emit no diagnostics.
Frozen binaries/fixtures/source archives and the earlier trial remain preserved.

Reproduce with fresh output stems in the existing local work directory:

```powershell
$py='../../../.venv/Scripts/python.exe'
$bench='docs/benchmarks/contour-optimization'
& $py -I "$bench/interleave.py" ../../contour-optimization fresh-contours
& $py -I "$bench/interleave.py" ../../contour-optimization fresh-detector detect
```

The directory's baseline.pyd and the retained project path are required; these are
named local comparison inputs, not distributable release artifacts. Stop this
bounded batch after source handoff. Remaining intersection/nucleus costs do not
justify changing precision, relaxing results or adding workers without a measured
new opportunity.
