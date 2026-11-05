# R7-REGION1 trace evidence

## Observed gap — 2026-11-05

`analysis_types.hpp` exposes retained OD/stain means. `RawRegionStatistics` exposes
mean/min/max per finite raw channel, without dispersion; dense RGB's server has
no quantitative read_raw implementation. The raw route therefore does not supply
the selected dense RGB/OD/stain retained-dispersion workflow. Cell intensity
statistics already has compensated sums, extrema and sample-SD moments for the
actual sampled RGB/transformed values. Reuse that reducer/result vocabulary and
the existing AnalysisRequest/read/transform/job/record/table path.

The pinned upstream [source manifest](upstream-sources.json) identifies QuPath0.7.0
commit04ccfa4fb7d43e9b566393e08e83690b72248d44. Paths were resolved from the Git tree;
all three downloaded leaves match their Git blob SHA and recorded SHA256. The
intensity plugin's basic features use RunningStatistics; variance divides by n-1,
with undefined variance/SD for n<=1. This source inspection establishes the chosen
primitive definition, not whole-plugin/input-sampling or specimen agreement.

Five [legacy records and independent channel contracts](../../../tests/fixtures/region-intensity-v1/manifest.json)
were captured from installed f72c1992+dirty before any public layout change. They
cover OD/stain means under geometry/winding policies plus a singleton. Channel
inputs retain existing float32 transforms, including signed stain values. OD
samples additionally match independent float32-rounded log10 calculations. Python
fsum/min/max/stdev provide independent moment expectations on those exact sampled
sequences. All five old mean records reopen/replay/verify; their exact table/batch/
JSON boundaries are pinned. The CSV has no dispersion fields.

The first capture saved the archive, then attempted the unbound Python property
AnalysisRecord.payload_bytes and stopped. The resumed observer reused that archive
without overwriting it. Record-level size was then captured by a small native observer against frozen
headers. Its [values/header hashes](../../../tests/fixtures/region-intensity-v1/native-boundaries.json)
match the preserved GRID1 production headers and all five Python table bounds.
MSVC/x64 sizes are result256, request24824, record288, channel statistics104 and
compartment statistics216. Old record payloads are2141 (geometry/singleton) or2149
(winding); corresponding table bytes67948/67956. These values were measured
before any result layout edit; they are compatibility witnesses, not a corpus
recapture or an implementation test for the missing feature.

The [first observer build](pre-layout-build.log) failed because AnalysisRecord
collections return values while the observer used pointer access copied from
neighboring tissue/raw APIs. Read the actual return type, used explicit const
AnalysisRecord&, and [rebuilt only the observer](pre-layout-build-corrected.log).
The [run](pre-layout-run.log) succeeded. Production layouts/kernels/bindings were
unchanged; observer provenance is a36c38c1+dirty and is not relabeled as the
installed f72c1992 GRID1 product. No new wheel/install/stubs were needed.

## Trace review and next action

The selected [contract](../../region-intensity-statistics.md) is ready for the
production owner/layout/storage review and implementation. The pre-layout observer
is complete. No REGION1 production type/kernel/binding, installation or stub
generation has occurred. Existing cell/raw/native numerical
evidence is reused only in its scope; new selected-domain expectations still need
implementation verification. Empty-support errors and singleton absence are
explicit. Broad resolution/grid, hue, texture, median and specimen workflows stay
in R7/R12 and are not erased by this first region-statistics task.

Observed waste: a runtime fingerprint tree.json was mistaken for a Git-tree
response; a guessed snapshot directory was read; an unbound Python method was
assumed from its C++ declaration. The cached runtime inventory was preserved.
Used actual discovered paths, inspected JSON keys and binding declarations, and
fetched only the three pinned reference leaves after resolving their tree paths.
An initial broad main-branch web lookup is not the acceptance reference. Future
lookup starts with the cache's purpose/schema and pinned tree; future Python
observers check bindings as well as headers. No new reader, result wrapper,
scientific engine, environment or specimen corpus was introduced.
