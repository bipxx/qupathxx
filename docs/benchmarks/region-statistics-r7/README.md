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

## Native candidate and plan review — 2026-11-05

The trace-review state above is historical. The current native implementation
build [completed successfully](native-first-build.log), followed by
[seven passing affected native suites](native-first-tests.log) in 4.94 seconds:
region_statistics, analysis, measurement_tables, project_batch, cell_records,
cells and viewer_session. Tested source is 17b6cf7c plus the uncommitted REGION1
production/tests patch; Windows MSVC Release, existing gridfrontends167-package
cache, project support enabled. This candidate is distinct from installed GRID1.
No REGION1 bindings, stubs, wheel or install was produced.

Checks cover same-pass moments/old means, alpha/ROI/policy eligibility, singleton,
empty/cancel/stale behavior, immutable retention, QRI1 codec/parent binding,
replay, save/reopen, CSV and frozen legacy payload/table/batch boundaries.
Missing/orphan/tampered SQL payload tests remain an explicit N1 check; native
success does not prove the pending Python/UI/CLI installed workflow.

Planning closeout: resumed the existing build handle instead of restarting it;
reused its pass and ran only the seven affected suites. Oversized document reads
and a wrong nested-worktree sibling path recurred; use bounded sections and
absolute sibling paths for the next task. The queue now specifies N1 storage
boundaries, N2 combined consumers, N3 installed closeout and N4 R7 handoff, with
the existing task/retry waste review required before each transition.

## N1 storage boundaries — 2026-11-05

[Cached build](n1-build.log) and [region/project suites](n1-tests.log) pass.
[Semantic output](n1-test-details.log) confirms all six SQL mutations reached
and were rejected by the production loader: missing, orphan, tampered bytes,
legacy parent, changed parent mean and downgrade. A pristine project reopens
with statistics intact. Old fixture and quotas remain unchanged. Same MSVC
Release/cache and 17b6cf7c plus REGION1 patch scope as the native candidate;
only test/CMake inputs changed for N1. No production repair was required.

Waste review: an explicit-encoding omission interrupted a Markdown update after
the C++ test edit; the live build was retained and passed. Subsequent scripts
use UTF-8 explicitly. A path inventory also guessed an absent package directory;
next consumer discovery starts from actual top-level directories. Reuse the
seven earlier suites; no binding/stub/wheel/install/full campaign ran. N1 is
complete; next settle all N2 bindings/SDK/managed/UI/CLI consumers together.

## N2 staged consumer closeout — 2026-11-05

The [combined build](n2-build.log) passed and the distinct REGION1 stage loads.
[Identity](n2-candidate-identity.json) matches current compiler-input bytes to the
loaded source-tree hash; it preserves the dirty source identity and product
hashes. This is not installed-wheel acceptance. GRID1's installed product/stage
remain unchanged; the reused probe interpreter points at the new stage, with
its former .pth bytes preserved locally.

[First Python run](n2-python-first.log):57 passed,3 GUI skips,1 failure. The
synthetic v2 fixture retained schema27 analysis_statistics, so migration failed
before its intended rollback injection. A shared versioned post-schema26 table
helper now services19 old-fixture modules while preserving their format-specific
edits and assertions. [All21 affected migration cases](n2-migrations-corrected.log)
pass; their exact [selected nodes](n2-migration-nodes.txt) are retained. Do not
repeat the57 unchanged passes to repair fixture construction.

[Three new GUI/managed cases](n2-gui-first.log) pass, covering interactive
statistics/keep/reopen, managed command/save and GUI batch save/resume. The SDK/
CLI cases in the first run cover owning nested values, default absence, singleton
SD absence, replay/CSV, batch resume and incompatible-kind refusal. Batch SDK/CLI
explicitly use the same stain recipe (SDK's default is OD).

One [stub generation](n2-stubs.log) was performed. [Valid typing](n2-typing-valid.log)
passes; [negative typing](n2-typing-invalid.log) reports exactly three intended
errors. [All14 public-stub/signature checks](n2-stub-checks.log) pass.

The actual [mouse-clicked checkbox](n2-controls-clicked.png) produced and retained
four-sample statistics; [proof](n2-control-proof.json) records zero reported
control overflow. The first shallow scroll did not reach the control and is
retained locally, not treated as proof. The expanded [failed saved summary](n2-saved-summary.png)
visibly clipped combined channel min/max lines despite a zero overflow counter.
Split those lines in the shared presentation lambda. The [incremental GUI repair
build](n2-ui-repair-build.log) passed without recompiling binding sources; stubs/
typing are reused because signatures did not change. [Eight affected GUI cases](n2-gui-repaired.log)
pass; the [repaired expanded summary](n2-saved-summary-repaired.png) was visually
inspected and all values are readable. Instrumentation cannot replace inspection
of raw ImGui text.

An attempted Markdown update hit a full disk and truncated only the current card;
test/helper writes had not begun. The card was reconstructed. Future writes use
a temporary sibling and replacement after successful completion. [Cleanup receipt](disk-cleanup.txt)
retains hashes/paths for about551MB of verified duplicates removed while keeping
canonical inputs; active cache/fixtures/logs/failure evidence were preserved.
Check free capacity before the next package operation. A compiler from imagejxx
was distinguished from the actual QuPathxx handle; no unrelated process was
stopped. The long original bindings build was resumed, never restarted.

N2 is complete within this staged scope. Next N3: one cached wheel/install after
checking capacity; reuse native, migration, typing and GUI evidence by exact
source/configuration. Verify installed save/reopen/replay/export and package
members/runtime smoke. No full campaign, extra stub pass or new sampling/model
authority. Broader R7/R12/research/hosts/release and M0-M7 remain open.
