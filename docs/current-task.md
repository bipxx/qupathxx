# Current implementation task

Owner scope update:100% QuPath parity is not required. Small cosmetic differences
are acceptable when results, interpretation and workflow usability are unaffected.
The [scope policy](api-delivery-plan.md#result-fidelity-and-scope-policy) and AGENTS.md
supersede earlier full-target instructions. Unselected inventory gaps are optional;
do not start work simply because a row remains Partial. Scientific correctness,
calibration, persistence/export/replay and meaningful overlay/display checks stay.

## Active task - candidate6 handoff and remaining gap status

Owner selected G01-G05 closure; [checklist](remaining-gaps-checklist.md) records
actual progress without converting broad unqualified claims into passes.
G01 comparison is complete: native305.6ms /QuPath313.1ms, exact2,237-cell RGB/
partitions; overlapping ranges/high load prevent general superiority.
G02 selected coarse whole-slide case/repeats/viewer pass; wider level-zero cells,
dense scenes, long-duration/GPU residency remain open. G03's new558-cell NDPI
crop matches masks/RGB/calibration/independent arithmetic; broader biological/
context domains remain open. G05 supported saved-data controls pass, but actual
upstream crop data has an invalid self-intersecting ring and is refused before
publication. Preserve strict refusal until explicit repair/admission semantics
and decisive result-preservation checks are designed.

G04 candidate6 freezes the exact tested tree2875ca45 without a native rebuild.
Wheel69 immutable members/DLL origins, static wheel70/7 and native59/6 closures,
installed native/Python density/import/bidirectional reuse, standalone app/batch,
and uninstall with29 output hashes preserved pass. Source includes all568 raw
compiler inputs,12 authenticated external dependency archives plus vendored
LodePNG source/notices. No new binary qualification is borrowed from candidate5.
Next: publish the matching source/wheel/native candidate and verify asset hashes.
Then keep broader G02/G03/G05 and concrete invalid-ring compatibility gap open.
[Evidence and limits](benchmarks/gap-closure/README.md).

Waste/corrections: no compile, binding rebuild or corpus campaign. Removed unused
SciPy assignment import and reused successful captures. Corrected one selected
test name and used development pytest against the explicitly installed package.
Two controls pass; the sole failing assertion expected obsolete class flattening.
Replaced it with retained-parent/reopen checks; focused repair passes. Final source
archive must include that test correction; refresh only source/receipt, not the
already verified wheel/native assets. No further timing search or intermediate
binary packaging. Optional O01-O05 stay deferred, not parity obligations.

## Delivered task - remaining gaps checklist

[Remaining gaps checklist](remaining-gaps-checklist.md) separates the completed
28/28 consolidation and contour optimization from five open evidence/delivery
categories and five optional expansion categories. These are scope categories,
not a count of defects or mandatory implementation tasks. No new feature,
benchmark campaign or binary release is selected by this planning task.

Suggested next for the speed question: G01, one bounded current-source/QuPath
comparison using existing fixtures/harnesses. Latest native-versus-native timing
does not establish a QuPath win. G02-G05 apply to selected workload/support claims;
optional O01-O05 require a concrete user need.

Check: local checklist links and git diff whitespace; documentation only.
Waste review: reused existing acceptance records instead of creating another
per-API ledger or rerunning builds. Corrected stale consolidation introduction
and labeled the older direct QuPath comparison as historical. Next: select one
outcome from the checklist; keep completed workflows closed.

## Delivered task - exact nominal contour-cycle optimization

[Contour evidence](benchmarks/contour-optimization/README.md): observed detector
median341.3 ->302.9ms (11.2%) in six alternating baseline/candidate pairs on the
selected512x512/2,237-cell ROI. Twelve serial contour pairs show nuclei113.7 ->100.0ms
(12.0%) and bodies135.9 ->111.3ms (18.1%). This is a local matched result, not general
QuPath superiority or an overall workflow/archive ratio.

A single unambiguous closed cycle in the noded raster boundary graph proves a
simple shell. Carry that certificate and known label to avoid duplicate pre-clip
validity/interior queries; move standalone polygons into ownership. Ambiguous,
multi-ring/hole/disconnected and non-nominal/general faces retain GEOS validity.
All final clipped ROIs still pass full validity/nonempty/positive-area checks.
Robust clipping, mapping, order, labels/area, limits and cancellation stay intact.
No public trusted-validity parameter, schema, extra thread or new decoder.

Three affected native suites pass2.62s. All596 nominal/general topology patterns
compare exact WKT and admit exact work while refusing one byte less. Every measured
pair has identical ordered geometry, edge/vertex/work/retained counts. Final
installed replay preserves all2,237 cells' masks, ordered geometry and features.
Final tree2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e
(568 inputs) matches the source manifest/generated provenance/installed runtime.
All38 binding objects reused; only changed private core/test/provenance inputs
compiled and existing frontends relinked. Same cache/runtime and retained fixture;
no new release, broad numerical campaign or repeated archive publication.

Waste review: the duplicate-query/clone-only trial showed~0.6% median change within
noise and was not accepted as a gain. Initial serial A/B/A drift likewise gave
no usable speed claim. One bounded interleaved comparison resolved host-time drift;
then the measured repeated validity cost justified the exact cycle proof. Four
warmups/12 contour pairs and four warmups/six detector pairs retain all samples,
never timing workers concurrently. Profiling source was restored with fresh
timestamps; accepted workers emit no diagnostics. Trials/failures remain preserved,
and durable notes are in compilexx.

Stop this bounded batch after matching source handoff. Existing consolidation
remains28/28 complete; remaining intersection/nucleus work requires a new measured
opportunity. Do not add percentages across batches or claim general speed parity.

## Delivered task - paired native raster identity optimization

Owner-selected [native detector optimization](benchmarks/native-detector-optimization/README.md)
removes duplicate source/domain hashing for nominal nucleus/body masks. In the
isolated same-process alternating probe, raster construction21.28 ->15.49ms:
27.2% stage reduction, about5.79ms saved. Both masks retain separate label/domain/
counter/component/budget/expected-digest admission; only an internally produced
identity is reused for the same immutable source/input. General rational-grid
construction remains unchanged. No public trusted-hash API/cache/new worker.

Full detector A/B/A medians329.8/334.0/330.0ms under31-36% other-process CPU load
do not demonstrate an overall gain. Preserve those samples; do not rerun to chase
a favorable median or claim QuPath parity. Diagnostic profile identifies contour
trace~122ms and nuclei~86ms as larger costs; contour clipping/order is unchanged.
The selected duplicate-work reduction is evidenced at its own stage only.

Three affected native suites pass2.23s, plus the final pair test after opt-in probe
edits. Pair-versus-single nominal/fractional/context/general values and exact
per-mask budget/identity/label/cancel refusals pass. Full outputs preserve exact
RGB, both masks, ordered geometry, stored features and density CSV for all2,237
Cells. Fresh archive replay verifies in the final installed runtime. Final tree
a3cbffa4ae17136523f40464a09acfc5504a8132231694cfcd4d82efa8b3f6e7 has568 inputs;
source manifest/generated provenance/installed module agree. All38 binding objects
remain byte-identical. Existing cache/runtime refreshed; frozen products/fixtures/
archives and diagnosed failures remain intact. No new release or schema.

Waste review: copy2 restored old timestamps and MSBuild reused a diagnostic object;
retain that invalid candidate run, touch restored source and rebuild only the
changed object/relink. Fix the probe's stain array/vector conversion and respect
request tickets when verifying replay. Compare stored geometry directly; recompute
one replay through the native request. The isolated alternating probe resolves
duplicate-hash benefit without another full pipeline/QuPath campaign. Temporary
profile instrumentation is removed. Durable lesson added to compilexx.

Stop this bounded batch after matching source handoff. Further contour/nucleus
changes need a specific measured optimization and exact identity evidence; broader
speed superiority remains unproven. Consolidation remains28/28 complete.

## Delivered task - focused post-consolidation speed comparison

Owner-selected fresh comparison is [complete](benchmarks/post-consolidation-speed/README.md):
current native cell pipeline median324.8ms versus314.1ms QuPath, an observed10.7ms/
3.4% native overhead on the same512x512 ROI. Native range320.4-330.3ms overlaps
reference294.9-346.3ms; other-process CPU averages23.5%/26.8% of logical capacity
over complete engine lifetimes. This one block establishes neither a reliable
remaining wall-time gap nor speed parity/superiority. No product change is justified
by this small noisy difference alone.

Exact RGB and canonical nucleus/body partitions agree across engines for all2,237
Cells. Native ordered mask hashes match the retained accepted result; calibration,
analysis area and RGB sums agree. Six native/24 JVM warmups and six measured
samples each use the existing recipe. Native cell detection dominates at320.5ms;
publication4.3ms, capture0.06ms, read7.8ms and density9.4ms are separate. CPU is
reported for all engine threads and includes possible JVM JIT/GC effects.

CSV/archive/reopen ran once each, retaining about13.6MiB total evidence instead
of per-iteration large archives. The formats/retained work differ, so their single
observations are descriptive, not a save/reopen or whole-workflow speed ratio.
Native versus scripted reference statistics also have no equivalent-work ratio.
Source/runtime remains15f6a9baf4ce114189cdbdcb00ccc565062e84a670fdb1348eeb393eb7856011;
no C++ rebuild, new runtime, wheel, release or broad test campaign occurred.

Waste review: reuse accepted scientific qualification, capture one final oracle
pair and keep every timing sample. Do not repeat blocks merely for a favorable
median or change scientific safeguards to chase an overlapping10ms observation.
Stop this measurement. If further latency/CPU reduction is selected, the useful
next task is a bounded internal profile of the current native detector before
choosing a specific optimization. Consolidation remains28/28 complete; broader
QuPath parity and reliable general speed superiority are unclaimed.

## Delivered phase - native consolidation NC01-NC28

The [single consolidation checklist](consolidation-coverage.md) records **28 complete,
0 in progress,0 planned**. Shared C++ density/manifest/reuse/export and NuCLS import
services, native training-cohort selection, focused bindings, thin controls,
shared frontends, boundary cleanup, API/stubs/guides and source handoff are complete.
Stop this selected phase; do not automatically start another release, optional
QuPath feature or speed campaign.

[Final boundary receipt](benchmarks/consolidation/boundary-receipt.json) records
raw native source tree15f6a9baf4ce114189cdbdcb00ccc565062e84a670fdb1348eeb393eb7856011
(567 inputs), independently matched to the generated manifest and installed module.
Compiler and control/stub/tool manifests identify tested files. All11 shipped
examples and three package modules are reviewed; native preview owns pixels,
calibration and reader dispatch. Pixel controls take supplied projects/ROI IDs/
models; independent synthetic/NumPy/CSV checks remain runnable in tests/tools.

[Boundary evidence](benchmarks/consolidation/README.md#shipped-python-boundary-closeout):
new native preview suite passes;10 installed changed-example cases pass with one
optional GUI interaction skipped;15 public stub/signature checks and10 focused
binding ownership/range cases pass. Negative/overflowing numeric conversions now
raise ValueError before altering options. The final installed stub check matches.
Only two focused binding objects changed;36 others, including the expensive main
binding, were reused. No scientific algorithm, tolerance or codec dependency changed.

[Consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair)
retains source-scoped native/installed authority checks. Native and thin Python
routes agree on8 density rows/CSV, bidirectional2-image resume with zero computation/
project copies and checkpoint replay. Both NuCLS routes preserve the32/28-object
subset's exact legacy pixels, geometry/nuclei, calibration, IDs/revisions, classes
and metadata. Source inputs stay unchanged. The archive golden roundtrips exactly
and refuses resealed off-by-one charges. Three training examples and standalone
app startup smoke pass. Unchanged authority inputs are explicitly matched against
the final tree; these accepted scientific checks are reused, not repeated.

Waste review: preserve exact historical eager/current WKT admission layouts,
correct native output-error precedence and test preview versus manifest guards
according to their actual contracts. Move independent checks to tools without
erasing them. Resume completed fixture outputs after a checker API mistake.
Catch binding range-conversion errors at the language boundary. Details/failures
and new durable lessons are retained in evidence and compilexx's canonical notebook.

Matching source is handed off on origin feature/scientific-grid-159. The one
cache and active runtime (main build/consolidation/development/qupathxx), frozen
products, fixtures, archives and failure evidence remain preserved. There is no
new wheel/binary release,100% parity or reliable general speed-superiority claim;
those broader scopes remain separate from this completed consolidation.

## Delivered phase - C++ cell kernel optimization L1/L2/L4

[Evidence](benchmarks/cell-kernel-optimization/README.md): direct interior
watershed neighbors and same-byte Windows SHA buffering preserve exact outputs.
Native median328 ms versus348 ms retained old module (~5.9% faster). Extended
QuPath median347 ms spans305-403 ms; native has a lower median in this run, but
no reliable general speed win/strict parity is confirmed. The seven affected
native suites pass;28 pre-change thin cases and26 ImageJ cases plus mixed/hash
boundary/finalization tests pass. Four full RGB/mask/CSV comparisons and four
reopened detector checks retain all2,237 cells and ordered geometry exactly.

Raw tree ef7338dd16763fc5c62b5aeb501a4b1eeb6540acb0f6bb5a17c343e175727584 has530
inputs. Only private native inputs/provenance rebuilt; all37 binding objects are
unchanged. Same cache/runtime and shared DLLs, one small retained control module;
no new wheel, install, binary release or language-boundary change.

Waste review: six JVM warmups did not settle the first reference (412 ->318 ms).
Expose explicit counts and repeat only that reference with24 warmups/six measured;
retain both runs and completed native results. Extended variability remains, so
do not chase favorable medians or compare historical absolute times across host
windows. No extra predicate workers, geometry shortcut or identity-token framework.

Next planned phase is L5's shared headless C++ density/selection/resume/export
service, then L6 importer and L7 thin controls. Conditional L3 sharing is deferred
until profiling justifies its ownership complexity. The scientific/performance
claim remains scoped to this ROI; stop this batch after source/evidence push.

## Storage maintenance -2026-11-07 complete

Owner interrupted optimization to recover disk space. No L1 implementation had
started; source was clean. [Receipt](benchmarks/storage-cleanup-20261107/receipt.json):
standard NTFS compression of144 inactive artifact folders saves1.51 GiB;
all10,369 files retain their original SHA256. C: had18.46 GiB free at verification;
other activity also freed space, so only the measured1.51 GiB is attributed here.

Active source/cache,37 binding objects, selected module/runtime bytes, published
products, fixtures, original ZIPs and storage-cleanup-20261105 are preserved.
Recursive removal of verified ZIP duplicates was rejected by automatic approval
review (blocked by policy); no folders were deleted or deletion retry attempted.
Normal NTFS compression preserves regular-file behavior; no WOF/reparse encoding.

Waste correction: Linux SDK link entries cannot be read as regular Windows files;
exclude those SDKs and preserve partial hash progress rather than repeat an
all-tree scan. No product build/test campaign or cache regeneration. Next planned
work remains L1's C++ watershed optimization; preserve the expensive binding cache.

## Delivered prerequisite - untangle codebase before optimization

Owner-invoked untangle-codebase with Comments+Tests completes U1-U6 in the
[decision log](untangle-decision-log.md): neutral JSON/dependencies, self-contained
watershed modules, shared contour/archive authority, density reuse, replay
vocabulary and explicit panel target membership. Independent repeat reviews
found no further worthwhile issue in these addressed areas. Meaningful science,
archive, compatibility and quota tests remain; redundant/stale comments and a
superseded replay acceptance subset are removed.

[Evidence](benchmarks/untangle-codebase/README.md): ten standalone header checks,
16 distinct affected native suites across two source-scoped steps,66 preserved
enum members/order, four real-slide runs with all2,237 masks and ordered geometry
exact. Final raw tree d1bee927656c1ada8b05774f9cb6eb1216fe9238d4e0144f32fdad9b5ded9910
has530 inputs; development runtime is the existing cache Release folder. No new
binary release, speed claim or completed Python workflow migration follows.

Waste review: an incorrect save-test target name stopped the first command;
verified qupathxx_save_tests and reused the completed core/targets. The repeat
scan found a writer-embedded encoder missed by helper-name searches; final small
core relink reused all37 binding objects byte-for-byte. The monolithic binding
compile dominated elapsed time; do not repeat it for documentation/stub changes.
Preserve logs/failures/products. No cleanup or full qualification was attempted.

Next planned task is L1's measured interior-neighbor optimization. L2-L4 retain
exact hashes/identity and one final paired result; L5-L7 remain the explicit
native workflow migration queue. Stop this structural prerequisite after source
push; no algorithm change has started and candidate5 stays frozen.

## Prior phase - optimize C++ logic and thin Python control (plan ready)

Delivered S11's next optimization batch: implicit nominal segments, bounded
predicate fallback and an exact dense integer-grid geometry/containment proof.
[Follow-up report](benchmarks/speed-gap-followup/README.md) and
[shared coverage](speed-gap-coverage.md) own source scope and checks.
Final settled medians are320 ms native versus292 ms QuPath (~9.5% overhead,
about28 ms). Strict speed parity remains open; no general speed win is claimed.
All2,237 cell partitions, RGB/calibration/area, CSV and ordered geometry stay
exact. Six affected suites pass; new adversarial/inclusion/hole/cancel/archive
checks validate the proof. Sparse/fractional/curved inputs retain GEOS checks.
Raw source tree d9f6b3c2348f473367358ec726515b07756f8af20eb8050f4d64d96646acfdbe
has523 inputs and its own runtime-delivery receipt. Candidate5 stays frozen.

Waste review: precise profiling ruled out context pooling (~1 ms/mask). Eight
predicate workers gave no clear benefit and were discarded. One-warmup reference
samples were unsettled; longer warmup without intersample oracles corrected a
premature parity interpretation. Extra fixtures overflowed a large Windows test
stack; isolate value-heavy tests up front, preserving checks and stack limits.
Use narrow probes during development; no broad qualification/binding recompile.
Closeout diff checking flagged a required blank context marker in the retained
profile patch; a file-specific whitespace attribute preserves that evidence.

Logic diagnosis: [source comparison and isolated probes](benchmarks/speed-gap-followup/logic-analysis.md)
show repeated general neighbor access in shape growth costs~23 ms; an equivalent
interior-offset path takes~8 ms, preserving all2,237 nucleus labels in36 total
original/variant/control runs. Nucleus subtotal falls105 ->87 ms in the probe.
Small scalar SHA updates cost3.48 versus0.92 ms for the same buffered bytes;
input hashing repeats for both masks. Neither is yet an integrated speed result.
Production source/binaries stay unchanged; strict parity remains open.

Waste review: reused raw RGB, masks, headers and MSVC Release settings in tiny
standalone probes instead of rebuilding bindings/full workflows. Box counters
rule out explosive basin filling on this ROI. Corrected one summary script's
shadowed builtin using retained output; no benchmark rerun. Early guessed/glob
source reads were unhelpful on PowerShell; use rg file inventory and exact paths.

Owner direction: all shipped scientific and workflow logic belongs in C++;
Python only parses/options/submits/polls/displays. The [ordered L1-L7 plan](api-delivery-plan.md#c-logic-optimization-and-thin-python-control-plan)
and [live checklist](speed-gap-coverage.md) cover native optimization, shared
density/manifest/resume services, native NuCLS import and thin consumers.
AGENTS.md now enforces that boundary. Existing kernels are already native;
Python density reuse/selection, NuCLS centroid/import and raw training cohort
selection are migration gaps. Independent test oracles remain in test/tool code.

Plan review/waste: reuse the native density panel's existing headless workflow,
JSON, guards and atomic exports rather than creating another runner. Preserve
both existing receipt formats through native adapters. Separate the measured
speed batch from workflow migration; do not claim that moving controls speeds
up the cell kernel. No native build/test is needed for this plan-only change.

Next: L1 exact interior-neighbor access and affected edge/numeric checks; L2
buffer identical hash bytes, L3 share immutable work if justified, then L4 one
final integrated/source-scoped comparison. L5-L7 consolidate workflow authority
and bindings after settling interfaces, with one combined consumer build. Keep
scientific/persisted identity exact. No new reader expansion, generic GPU/thread
framework or binary release is selected.

## Delivered phase - label-local contour performance

Owner-selected C1-C4 are complete: phase profile, bounded label-local GEOS
polygonization, affected tests/exact-output checks and source handoff.
[Report](benchmarks/contour-performance/README.md) and
[8-row checklist](contour-performance-coverage.md) own scope and evidence.
All four workflows retain exact RGB/masks/CSV and 2,237 QuPath-equivalent cell
partitions; ordered WKT/labels/pixels remain exact. New 84-pattern topology
regression and five affected suites pass. Raw source tree
08f5967e9e7de1109a00f9985c9c98344d1c7ab9e6523413d9f7d76557d6c756
has its own module/runtime receipt. Cached bindings were reused.

Native full-workflow medians are 3,215/2,988/3,564 ms (before/candidate/later
original control), an observed 7-16% reduction. Host drift is substantial;
no new QuPath timing ratio, RAM, whole-slide or universal speed claim follows.
Published candidate5 remains unchanged; these are development sources only.

Waste review: phase-level timings selected the largest measured cost; reused
GEOS topology handling, existing boundary labels and benchmark/reference outputs.
The added regression initially gave only the reference grid a context pixel;
corrected the setup and preserved the failure. Avoid guessed-path/oversized
source reads and always specify UTF-8 on this host. Temporary profiling is absent
from the candidate. Prior cleanup rejection was not retried; comparisons remain.
Next action: stop after source push. Clipping, geometry creation and eager reopen
reconstruction remain measured opportunities, without automatic follow-up work.

## Delivered phase - focused candidate5 workflow performance

Owner-selected B1-B4 are complete: fixed workload, paired reference, bounded
fix and source/evidence handoff. [Report](benchmarks/candidate5-performance/README.md)
and [12-row coverage](candidate5-performance-coverage.md) own details.
All four paired ROI workflows agree on 2,237 cell partitions and RGB/calibration.
Native cell pipeline is 1,079 ms versus QuPath 486 ms (about 2.2 times slower).
Resource-only process peaks are 217/1,262 MiB including runtime startup. These
are local bounded-ROI observations, not whole-slide or overall superiority.

Fixed a demonstrated compatibility grid-cell archive reopen bug and removed
polygon WKT reparsing while retaining exact persisted identity. Five affected
native suites pass; full workflow improved about 11% against the later original
control, with inconsistent isolated contour probes disclosed. Development raw
source tree dddf5bb3412a58e2ade9317f46e21d06d2857ca387ba40db37d0fd5fbedeaa41
has its own runtime/source receipt. Published candidate5 remains immutable and
still has the newly discovered archive limitation; no candidate6 is selected.

Waste review: capacity refusals exposed linked raw-basin/contour/statistics
allowances; establish a coherent admitted recipe before timing. Trace actual
reader requests rather than guessing context. Separate oracle allocations from
resource measurements. Keep failures and contrary controls visible. Private CPP
changes reused cached bindings and affected suites; no broad qualification.
Temporary comparison runtime deletion was rejected by automatic approval review
("blocked by policy"); copies remain local. UTF-8 is explicit after a host GBK
document-read failure. Next action: stop this selected phase after source push. Further work requires a
selected user outcome; contour/reopen cost is the measured performance opportunity,
and any future binary handoff must package the archive repair under a new identity.

## Delivered phase — Windows candidate5 packaging and publication

Owner accepted packaging the latest signed-distance workflow as candidate5.
Ship one CPython3.12 Windows x64 wheel (build tag5), native ZIP, matching source,
unchanged pinned dependency sources, example project/CSV and concise install guide.
Include the delivered ImGui project-density/resume code; older-engine receipts
recompute rather than implying cross-upgrade cache reuse. Keep candidate4 frozen.
This is local engineering support, not fresh-host/platform, signing, speed or
biological acceptance. No new feature/kernel/reader or broad test campaign.

| Task | Existing path / decisive check | State |
| --- | --- | --- |
| P1 freeze and package | One cached wheel build, only provenance compiled; explicit native targets relinked; source3155df80 retains raw522-input tree2ddb06c5da4d | Complete |
| P2 isolated installed route | All67 immutable members and mapped DLL origins match; signed jobs/CSV/save/reopen/replay/managed ImGui pass; native ZIP opens installed project | Complete |
| P3 distribution and removal | Wheel/native dependency closures complete, AGPL/notices and12-source inventory verify; uninstall preserves all10 output files | Complete |
| P4 publish | Candidate5 public with19 assets; GitHub sizes/SHA256 digests match before/after publication and tag targets3155df80 | Complete |

Before build: native behavior and Python interface are unchanged from the signed
delivery. Packaging changes need source identity, installed-file/dependency closure
and missing install/removal checks, not repeated numeric/native qualification.
Waste review: reused release scripts/cache/source archives; no binding recompile,
new decoder, new download or broad numeric/fresh-host campaign. An unpublished
draft's tag endpoint returned404 after successful uploads; retrieved releaseID
from the authenticated list, verified existing assets by ID, then published.
No rebuild/re-upload. Static manifests intentionally make no signing/fresh-host
or exhaustive dynamic-loading claim. No novel engineering/biology claim added.
[12/12 coverage](windows-candidate5-coverage.md), [release/install guide](benchmarks/windows-candidate5/RUN.md)
and [delivery receipt](benchmarks/windows-candidate5/README.md) close the outcome.
Local artifacts/native app:main build/windows-candidate5; test wheel was removed
from its isolated environment. Candidate4 and prior products remain frozen.
Publish closeout docs and stop; no next feature or validation phase selected.

## Delivered phase — signed Cell-to-annotation boundary distances

Owner accepted the original R9 signed-distance gap. Extend the existing distance
request/job/record/export path with an explicit metric; preserve centroid behavior
and old QSD1 records. No new reader, orchestration framework or full-parity target.

Contract: selected Cell centroids (nucleus preferred, body fallback) to the nearest
individual selected Annotation area boundary, including holes and multipart rings.
Negative inside that annotation, positive outside, canonical zero on its boundary;
minimum absolute distance wins, exact ties use lower target ID. Regions are not
unioned. Pixels or positive finite independent X/Y micrometre calibration. Curves
require explicit 512-segment polygonization. One Z/time plane, no pixel reads.
Bound capture/work, vertices and segment tests; cooperative cancellation and stale
guards remain native. Preserve exact saved float64 identities and historical CSV.

| Task | Easiest existing path / decisive check | State |
| --- | --- | --- |
| T1 contract and native implementation | Existing metric/request/job with immutable bounded boundary capture; independent geometry/calibration oracles and failures | Complete |
| T2 retain/replay/export | QSD2 boundary geometry; QSD1 unchanged; exact saved hash/CSV and native replay | Complete |
| T3 consumers | Thin nanobind fields, CLI operation and existing ImGui panel; same frontend lifecycle | Complete |
| T4 delivery | Native suites9/9 plus final archive suites2/2 in6.89s; Python/CLI/managed ImGui, legacy identity, types and exact cross-interface CSV pass | Complete |

Build risk/check: native headers and payload change; settle all consumer contracts
before compiling cheap affected targets, then one binding/staging pass. Published
candidates and prior app bundles remain frozen. Do not replay broad qualification.
Waste review: reuse the distance pipeline instead of another record/job/frontend
family; preserve centroid capture layout so old archive admission stays valid.
First native run passed. Review added close-to-edge and vertex-limit checks, then
only affected checks. Initial exploration produced truncated reads; narrow reads
avoid repeating that. Reuse the native app/CLI while the binding TU compiles.
Review found QSD2 loading repeated the full nearest search. Like QSD1, load now
checks the chosen distance/geometry and identities; exact segment totals are
validated arithmetically. Explicit Verify owns full recomputation. Final signed/archive suites2/2 pass; cached consumer relink and staged checks
pass, preserving the compiled binding objects.
The large binding TU was the actual critical-path cost; it compiled once while
standalone checks proceeded. Staged source522-input tree 2ddb06c5da4d matches the
raw compiler manifest and runtime. [14/14 coverage](signed-distances-coverage.md),
[guide](signed-distances.md) and [source/runtime evidence](benchmarks/signed-distances/README.md)
close this slice. Local app/Python runtime:main build/signed-distances/runtime.
This is a separate development runtime; candidate4 wheel/SDK and prior apps remain
frozen. No new binary release, indexed-search speed, biology or full-parity claim.
Compilexx records durable lessons. Publish source and stop; no next phase selected.

## Delivered phase — ImGui density resume/reuse

Owner accepted a previous-run folder selector and reuse/recompute indicators.
Delivered private controller/GUI receipts over existing native replay/capture,
records/CSV and the CLI's exact engine/geometry/revision/calibration/policy/limit
rules. Each saved image publishes a bounded checksum receipt before combined
CSV; no final report is required to resume. Eligible unchanged images reference
original checkpoints without kernel work or QPX/pixel copies. Changed/missing
results compute; eligible corruption fails explicitly without replacing data.

T1/T2 complete: both affected native suites pass,2/2 in1.64s. Tests prove0 kernel
calls for reuse/chains, partial receipt recovery, changed geometry/calibration,
unrelated metadata, smaller quotas, engine changes, duplicate JSON/checksum/
replay errors and separated folders. Actual desktop first run computed2; second
run computed0/reused2 with0 new QPX copies. All old run files and source remain
byte-identical;31/27 counts and all20 native CSV columns agree. [10/10 coverage](gui-density-resume-coverage.md),
[guide](gui-project-density.md) and [desktop/source evidence](benchmarks/gui-density-resume/README.md)
close this scope. Local app:main build/gui-density-resume/application;56 members,
raw520-input tree e83ae913ef42. Older pre-receipt/different-engine runs compute.
Published candidate4 and Python consumers remain unchanged; no new binary release.

Waste review: first tests passed; one boundary review added required-field
admission and Windows case-alias nesting checks, then only affected suites and
one cached app build. Reused strict JSON, atomic publication, downloaded fixtures
and numeric oracles. No bindings, wheel, new dependency version, decoder, generic
cache framework, broad qualification or fresh biology/speed claim. Compilexx
records durable lessons; preserve prior apps/outputs. Publish sources and stop;
no further feature or validation phase is selected.

## Delivered phase — ImGui project-wide density panel

Owner continued image/class selection, native cohort/calibration preview,
background progress/cancel and combined CSV/error reporting. Delivered as a new
standalone app with a private controller over existing native density/records/
CSV/save_new; no new SDK fields, batch enums, bindings or wheel. Saved-source
SHA/review gates, new directories and independently owned worker Documents
preserve the displayed workspace and input files. Per-image failures continue;
completed whole-image outputs survive cancellation and CSV quota failures.

T1/T2 complete: affected controller suite passes in0.82s, including active cancel,
stale source, quoted CSV, quota and a collision followed by a successful third
image. Actual final desktop preview/run shows32/28 selected Cells,31/27 in-FOV
counts,2 saved images/2 rows and0 failures. Both saved and original source bytes
remain unchanged; unchanged candidate4 Python reopens/verifies both checkpoints.
Selector changes clear reviewed rows; short image IDs make both rows readable.
Folder opener works; CSV opener reached the Windows chooser without changing
associations. [Guide](gui-project-density.md), [14/14 coverage](gui-project-density-coverage.md)
and [desktop/source receipt](benchmarks/gui-project-density/desktop.json) close
this scope. Final local app:main build/gui-project-density/application,56 files,
about40.6MB; its519 raw compiler inputs match recorded tree23111b162413. Reader
DLLs/notices match candidate4; published candidates and Python consumers stay
frozen. This is local Windows desktop evidence, not a new binary release or
fresh-host, biology, segmentation, platform or speed acceptance.

Waste review: a fixture conflated historical/current revision-bearing SHA;
retain exact stored SHA/CSV and use native replay verification. Long names hid
preview rows; private GUI repairs reused controller/kernel evidence and cached
objects. WindowsPath sorting differed from CMake's string sort in the receipt
observer; corrected only that script, without rebuilding product code. Original
failures and intermediate products remain in main build/gui-project-density.
Compilexx records durable lessons. No full kernel/binding/package campaign.
The selected outcome is closed; publish sources and stop. No next phase selected.

## Delivered phase — reusable NuCLS PNG/CSV import

Owner accepted generalizing the pilot into a standalone Python importer. Match
PNG/CSV stems across two directories, optionally select named stems; require
explicit X/Y calibration and outlines versus centroid-marker representation.
Preview missing files, malformed rows, classes and coordinate problems. No save
unless every pair passes; write a new QPX only. Preserve selected class plus
source classification/row provenance in native metadata. Do not infer tissue,
windows, calibration, clip coordinates, repair topology or resample images.
Reuse native ROI validation, Project.save_new and delivered file guards; keep
candidate4 and delivered density consumers unchanged. No C++/wheel/UI work.
T1/T2 complete: first focused run28/28 passes. Native outlines/marker centroids,
classes/provenance, malformed/missing pairs, budgets, changed input and no-replace
output have evidence. Added Pillow header-size-refusal coverage after review;
final affected module29/29 passes in1.57s. Installed CLI demonstration passes:
2 public cases/60 nuclei, all classes and original RGB pixels exact on reopen;
outlines correctly reject the known out-of-image box; explicit markers reproduce
the prior pilot's counts31/27 and areas104006/101124px2. No new downloads/build.
[Guide](nucls-import.md), [14/14 coverage](nucls-import-coverage.md), source/test
receipt and installed workflow evidence close the selected outcome. Push and
stop; no further phase or broader native test campaign selected.
Waste review: settle this small format contract before coding; no query language,
general adapter registry, new download, segmentation or native qualification.
Execution waste review: no failed test/implementation run. Public outline
rejection is expected and recorded; reused downloaded assets and prior independent
FOV/count-area oracles. Native ROI validation and save_new own topology/publication.
One final module pass covered the added real Pillow size-refusal case; no native
build, wheel, install or unrelated tests. Compilexx records the reusable lesson.

## Delivered follow-up — public multi-image research pilot

Owner accepted running the delivered selector/runner on a real research project.
Owner has no local project. Select a tiny public NuCLS corrected single-rater
subset:2 FOVs from different TCGA cases, author-published CC0 and0.2um/px.
Use annotation FOV support for the denominator, not a claimed tissue ground truth;
represent annotated nuclei as native Cell objects, without claiming whole-cell
segmentation. Preserve source CSVs and rectangle/polygon centroid semantics. No new reader.
Decisive check: generated class manifest -> existing installed runner ->2 CSV
rows matching independent rectangle-area and annotation-centroid counts.
Local read-only metadata inventory found test fixtures and the duplicated-crop
demonstration, not an identified research cohort.
Inventory is retained in main build/research-density-pilot/local-project-inventory.json;
reuse it instead of rescanning. No biological acceptance claimed.
Completed:7 directly downloaded public assets total396720 bytes;2 distinct cases,
60 annotated nuclei represented by centroid markers. Actual preview/generation/
batch CLI produces2 rows:31 and27 in-FOV nuclei,0.00416024 and0.00404496mm2,
7451.493183 and6674.973300 nuclei/mm2 of annotated FOV. Counts and areas match
independent arithmetic; saved project bytes unchanged. [Pilot coverage, sources
and evidence](benchmarks/research-density-pilot/README.md) close all6 selected checks.
No native build, tests/package reinstall, model training or feature expansion.
Waste review: initial inventory stopped on an unavailable historical artifact;
the corrected read-only pass records unavailable/schema-ineligible entries and
preserves all files. No repeated test campaign or fixture substitution for the pilot.
First public import exposed a one-pixel out-of-image annotation box, correctly
rejected by native Document. Preserve the failed analysis.log and partial output;
use1px native Cell/nucleus markers at all original annotation centroids for this
centroid-only pilot, rather than clipping source annotations or resampling pixels.
Record source shape types, mask/RGB extent differences and representation limits.
The corrected centroid-only workflow passes. Exact source hashes, failed first
import and source annotations remain preserved. Durable compilexx note records
bounded public-data selection and explicit representation/denominator semantics.
Next: no further implementation or validation phase selected; report this pilot's
scope rather than treating public annotations as whole-cell/clinical acceptance.

## Delivered phase — class-based density manifests

Owner accepted a thin Python manifest generator. Select images by ID (all images
by default), resolve exact class names into explicit window/tissue/exclusion/Cell
IDs, and preview counts, calibration and native request errors before publication.
Require explicit Cell classes or all-Cells selection. Write only a new manifest
when every selected image passes. Reuse the delivered runner's file guards and
atomic JSON writer; keep candidate4 and the batch runner unchanged.

T1/T2 complete: standalone generator,22 focused tests passing against installed
candidate4, and actual isolated CLI generated-manifest to runner demonstration:
2 images/8 rows with unchanged counts289/121/168/0 and areas40576/17536/23040/0
per image. Reused the preserved duplicate-crop project; original bytes unchanged.
[Guide](density-manifest.md), [12/12 coverage](density-manifest-coverage.md) and
[receipt](benchmarks/density-manifest/receipt.json) record scope. Native candidate4
and batch runner unchanged; no native build, wheel, install, GUI or segmentation.
Waste review: exact-name selection avoids a new query language or hierarchy
policy; one object scan per image, one project open, no kernel work in preview.
Fixture repairs: Document.set_class takes RGB; ellipses use add_roi(Roi.ellipse).
The curve case passed alone, then final22 passed including repeated selectors
and native policy options. Nondefault planes require quantitative TIFF, so the
invalid synthetic plane fixture was removed; native plane capture stays unchanged
with no new plane-coverage claim. Raw failures are preserved in main
build/density-manifest. Lesson in compilexx: inspect installed stub semantics
before fixture authoring; no second validation framework. No further phase is
selected; publish this outcome and stop.

## Delivered phase — project-wide density runner

Owner accepted the proposed thin Python runner. Delivered over candidate4 unchanged:
explicit per-image windows/tissue/exclusions/Cells, sequential native density
jobs, per-image QPX checkpoints, combined CSV, failure reporting, cancellation
and reuse of unchanged saved results. No native/binding/UI changes or new wheel.
[Project-density coverage](project-density-coverage.md) tracks this outcome.

T1 complete:standalone examples/project_density.py with a bounded versioned JSON manifest.
Every run writes a new directory. --resume-from reads earlier immutable image
checkpoints; it does not overwrite or duplicate earlier projects. Optional
record_id selects an existing native saved result. Reuse requires native replay/
input identity and exact native source-tree identity; it is labelled reused,
not a numerical re-verification. Changed inputs compute anew; corrupt caches
fail explicitly. Native geometry, jobs, serialization and CSV stay authoritative.

T2 complete:focused multi-image tests cover native agreement, CSV identities/quoting,
per-image failures, cancellation, resume without kernel work, changed-input
recomputation, corrupt checkpoints, byte limits and input/output protection.
Cancellation polls the native job; synchronous project open/save must return
before cancellation can finish. Partial/cancelled runs publish density.partial.csv.

T3 complete:standalone installed-candidate4 CLI demonstration computed2 image
entries/8 ROI rows, then reused both checkpoints with0 new QPX copies. Each
image has the accepted506-cell crop counts289/121/168/0 and areas40576/17536/
23040/0px2. The same crop is deliberately used twice to test namespaces, not
independent specimens. Source bytes remain unchanged and checkpoints reopen/
replay. [Guide](project-density.md), [source/test receipt](benchmarks/project-density-runner/receipt.json)
and [CLI evidence](benchmarks/project-density-runner/workflow.json) cover all16
selected requirements. Native source remains cbf4102f; the standalone runner has
its own source hash. No reader/segmentation/platform/resource campaign, GUI work,
native build or wheel/install was needed. Native headers/bindings stay unchanged.

Waste control:open the source project once; hash at setup/final and stat between images;
one single-image checkpoint per computed image, never a whole-project save per
image. Resume references existing checkpoints instead of copying their pixels.

Waste review:29 focused tests pass against unchanged installed candidate4.
Initial23 passed and one test used Project.dirty as a method; corrected its
property accessor and ran that test alone. Final code review added reduced-budget
native readmission, SQLite-sidecar protection and bounded checkpoint identities;
the affected module then passes all29 cases. Native values/replay, source-record/
checkpoint reuse without jobs, unrelated/relevant edits, failures, active cancel/
SIGINT, rollback/byte caps and preserved inputs/outputs have evidence. No build,
wheel or install. The standalone orchestration reuses native authorities rather
than adding C++ batch enums/UI/stubs. Canonical compilexx notes record the reusable
lessons. Inputs, previous outputs/checkpoints and failed logs are preserved.
Closeout normalized doubled Windows line endings in the archived failure-log
view after diff-check flagged them; original raw log remains preserved. No product
code changed and no behavioral checks were repeated for that documentation fix.
The selected outcome is closed; no further feature or validation phase is selected.

## Delivered phase — tissue-aware ROI cell density

Owner continued the recommended density phase. Delivered selected cell centroid
counts per mm2 of valid tissue for each selected ROI, with explicit tissue and
exclusion cohorts, calibrated area, CSV, retention/replay and shared Python/CLI/
ImGui consumers. Candidate3 stays frozen. This is per-ROI tissue density; radius
maps, smoothing, segmentation, new readers and additional platforms are outside
this selected phase. The previous50/50 neighborhood checklist remains closed;
[density coverage](density-coverage.md) tracks this phase separately.

Contract:valid footprint=(ROI intersect image bounds intersect union(tissue))
minus union(exclusions). Count each explicitly selected Cell ID once per ROI if
its chosen centroid covers that footprint and does not cover an explicit
exclusion (including its boundary). Use existing closed geometry coverage;
hole interiors are out and hole boundaries follow that established coverage.
Overlapping ROIs produce independent rows, never an implied pooled total. Area
is pixel area*calibration_x*calibration_y/1e6. Positive finite calibration is
mandatory; empty area yields count0 and undefined density. A zero count with
positive area has density0. Rectangles/polygons/multipart are exact linear
inputs; ellipses require explicit polygonize_curves acceptance and use the
existing512-segment polygon for BOTH membership and area, captured in identity.

T1-T3 complete locally (2026-11-06):density capture/kernel/jobs, retained QDN1
records/schema29/CSV and thin shared frontend/Python/CLI/ImGui. All9 affected
native checks pass, including closed-edge regressions, nearest/neighborhood and
legacy migration/snapshot budgets. Python/API/stubs:25 passed;16 existing opt-in
GUI cases skipped. New actual density cancellation/retry passes separately;
positive typing passes and negative typing has3 expected errors.
[Native receipt](benchmarks/density-workflow/native.json),
[consumer receipt](benchmarks/density-workflow/consumers.json) and
[506-cell staged workflow](benchmarks/density-workflow/staged-workflow.json)
record source scope. The4 ROI rows match independent area/count oracles:
40576px2/289 cells,17536/121,23040/168 and empty0/undefined. SDK/CLI/GUI CSV bytes,
keep/verify/save/reopen and standalone reopening agree. Inspected desktop shows
readable values/units/status and registered overlays,0 UI overflow.

One bounded observation:20000 coincident Cells in500 overlapping ROIs,10M
predicates/1000 Boolean operations,27.46MB planned work,69.10MB process peak and
119 Python observer ticks during released-GIL wait. This is diagnostic evidence,
not speed superiority or an RSS guarantee. Geometry calls remain synchronous.

T4/T5 complete:one frozen wheel build (tag4) compiled only provenance/relinks,
then one isolated install passed the506-cell SDK/CLI/GUI route, including actual
Inspect, CSV/save/reopen and standalone reopening. All67 immutable installed wheel
members match; mapped GEOS/OpenSlide DLLs are inside the package. Static runtime
closures pass;515 raw compiler inputs match the source ZIP and12 unchanged
dependency-source archives verify. [Candidate4](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.4)
is public with19 assets, every remote size/SHA256 checked before and after
publication. Native source cbf4102f2cffe6ed8f71549087ae5bf74cdcd8e2, tree
5a98347c5447c5b8f414d299a3411e9554c72188f92e1ca3ecd7c8c69aeee96e.
All40 selected requirements are closed; see the
[delivery receipt](benchmarks/density-workflow/delivery.md) and
[completion audit](benchmarks/density-workflow/completion-audit.json).
Support remains local Windows x64/CPython3.12 engineering delivery. Signing,
fresh-host/additional-platform, exhaustive parity, biological and speed acceptance
are not claimed. No further phase or validation campaign is selected.

Final products/runtime are main build/density-workflow/delivery and installed.
The tools interpreter resolves the final installed runtime. Preserve active
source/cache, products, reference scenes, failures and archived
build/storage-cleanup-20261105. The stage is superseded; no recursive cleanup
was attempted. Candidates2/3 remain frozen and PRESET1 stays parked.

Waste review:first fixture was outside allowed image bounds; translated it
without changing its oracle and retained the failure. Removed quadratic payload
rescans. Grouped Document fields into one shared-header rebuild. The large main
binding unit used>16 CPU minutes; order cheap changed CLI/frontend targets first
in future combined builds. Its pass was preserved when a later MSVC CLI lambda
failed; one cached repair reused all binding objects. That repair also fixed
GUI target-array/mode guards and true closed-edge index bounds, and charged
candidate capacity/growth. All follow-up native/Python/desktop checks pass.
Durable lessons are appended to compilexx's canonical notebook. No intermediate
wheel/install, broad corpus/platform campaign or additional feature is selected.
The bundled closeout command was rejected by automatic review with only
"blocked by policy"; explicit file diffs/copies completed the same handoff safely.
No binary inputs changed and no rebuild/reinstall was needed for documentation.

## Delivered phase — radius-based cell neighborhoods

Implementation requested after candidate2. Selected addition: for each
selected source cell/object, count selected target centroids within an explicit
radius, inspect the table, export CSV and save/reopen/replay. This adds the missing
R9 neighborhood operation; existing cell detection/measurements and nearest
spatial distances are reused. All five planned tasks are delivered. Detailed contract,
API paths and five ordered tasks are in the [next-phase plan](api-delivery-plan.md#next-phase-radius-based-spatial-neighborhoods).

Coverage is maintained in the [detailed checklist](neighborhood-coverage.md):
requirements and source-scoped evidence, with the execution queue kept here.
T1-T5 complete; coverage50/50 selected-phase requirements verified.
[Windows candidate3](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.3)
is published with18 assets, each remote size/SHA256 matched before publication.
Frozen native source618b0dd0a6f39cc062941c6ea524ac22cb863bc5, tree
b30bafcb4c982c55d7069276e4c82b9119d731ef7a69d519e50a19599dcbcd95.
One cached wheel build (tag3) refreshed only provenance/linking and one isolated
installation passes the506-cell Python/CLI/ImGui/CSV/verify/save/reopen route and
standalone reopening. All67 immutable wheel members match the install; mapped
GEOS/OpenSlide DLLs come from its package. Wheel/native runtime inspections pass;
all506 raw compiler inputs match the source archive;12 unchanged dependency
archives verify. The source tree matches the tested stage, so native oracle,
quota/cancel/stale/legacy and dense/GIL evidence is reused within that scope.
The20000-cell coincident observation has399980000 comparisons and68.60MB process
peak, without a dense matrix or speed/RSS guarantee. Both inspected GUI captures
show readable counts/radius/units and registered cell overlays,0 overflow.

The selected outcome is closed. No further build/API/segmentation/parity/platform
campaign is selected. The [delivery receipt](benchmarks/neighborhood-workflow/delivery.md)
records public state, support and disposable-state disposition. Final packages/
runtime live in main build/neighborhood-workflow/delivery and
build/neighborhood-workflow/installed. The native cache and all previous
candidates remain preserved; the tools interpreter resolves the final installed
candidate rather than the superseded dirty stage.

Waste review:T2's schema generator omitted27/28 tables; only that test target
was corrected/rerun. T3's missing CLI-test header and hidden GUI target flags
needed only test/provenance/GUI compilation and relinks, reusing binding objects.
The genuine TypeAlias required a stricter identity/canonical-member check, not a
native rebuild. Package assembly's blanket embedded-build-identity assertion
incorrectly covered the region-only utility, which does not link that authority;
its exact cache/wheel/installed bytes are checked, while the app/batch/module
embed the clean revision/tree. The partial draft/log is preserved; no binary
changed and the final wheel was not rebuilt/reinstalled. Native/package/compiler
and corresponding-source checks pass; publication and final requirement-to-evidence
closeout are complete.
Keep candidate2 frozen and
PRESET1 parked. After each task/failure, record the result, decisive check, waste
and next task here; do not create another ledger or recurring review campaign.

Planning waste review:the earlier suggestion to add a generic cell-analysis
workflow duplicated existing capabilities. Reading the actual spatial headers
identified radius counts as a real missing operation. Reuse owner capture,
calibration, indexed centroids, jobs and native persistence/export patterns;
settle all known SDK/CLI/ImGui fields before one combined consumer build.

## Delivered outcome — representative large-slide tissue workflow

R12 continuation delivers whole-slide tissue at an explicit native pyramid
level while high-resolution ROI statistics remain bounded. The new opt-in
`canonical_pyramid` reads the selected level once at origin zero; this fixes
OpenSlide decoder-phase differences across processing tile sizes without
changing legacy recipes. SDK, CLI and ImGui share the native implementation.
QTM6 retains the new recipe; QTM1-5 and inactive legacy memory charges remain.

Local changed-source evidence passes:7 affected native suites;120 focused
Python cases initially passed,8 skipped, and the sole failing case passed after
a test correction. Snapshot accounting includes project-path bytes: both old
and new engines charge11512 at the same relocated fixture path. The test now
accounts for relocation and retains exact-budget admission and budget-minus-one
rejection. No native compatibility change was needed.

CMU-1.svs level2 (2875x2057) produces identical labels/input hashes at tile256
and512 and zero differences against the independent threshold oracle. There
are5913875 supported samples,1083798 foreground samples and688 components.
Cancellation leaves no result/retained record. Native estimate102136112 bytes
at tile256; measured observer peak271642624 bytes includes Python oracle and
reader caches. This is pyramid tissue, not level-zero cell analysis or a speed
superiority claim. Python ROI statistics/CSV/QTM6 saved replay and ImGui tissue
CSV/save/replay pass; standalone opens the combined Python project. The captured
overlay aligns with tissue, scale is visible and the UI reports zero overflow.
See [evidence and commands](benchmarks/large-slide-workflow/README.md).

Delivery complete: [Windows candidate2](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.2)
is published with16 assets, each remote size/SHA256 verified. Native source is
`bee4736f2ad0aeffc9907542c873f06efd335cc7`, tree
`b52fa9b85799b7ce0b68780a84f2c95ff993c69b848ce7b24a9b74d54215e8f7`.
The cached wheel build refreshed provenance/linking, reused settled bindings and
produced build tag2 with AGPL SPDX metadata/license. One isolated installation
passes Python and ImGui large-slide/CSV/QTM6 replay, all3 focused installed tests,
and standalone reopen. All63 installed package members match the wheel; wheel
and native runtime inspections pass; all497 raw compiler inputs match the source
archive, and12 unchanged dependency archives authenticate. No fresh-host result
is claimed for candidate2. Final packages:main build/large-slide-workflow/delivery;
installed runtime:main build/large-slide-workflow/installed. The selected outcome
is closed; no new parity extension is selected. Automatic approval review blocked
retirement of superseded stage/test directories; they remain intact outside the
selected delivery. See the [delivery receipt](benchmarks/large-slide-workflow/delivery.md).

Waste review:the first native test used a Python alias; it was corrected and only
that target retried. The snapshot test confused fixture relocation with native
layout change; a same-path old/new comparison resolved it before touching code.
One combined bindings/UI/CLI build was reused for all consumers. The restored
build environment lacked pip; invoking the installed backend directly avoided
an unnecessary installation. An identical duplicate license and the native
archive basename were corrected before publication, without changing binaries. Required build
tools were restored for the demonstrated decoder defect. Optional PRESET1 source
is preserved byte-exact in build/stain-presets-r7/parked-source and Git stash
88f1eba9fe33025dd696ef3fbf6e8c7e55c6363f; keep it parked.

## Delivered baseline — Windows candidate handoff

The selected [Windows workflow](windows-workflow.md) and its targeted fresh-host
handoff pass. [One evidence summary](benchmarks/windows-workflow-checkpoint/README.md)
links the local and hosted receipts. The declared engineering
[prerelease is published](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.1)
with16 assets, sources, guide and deployment evidence. Do not merge the parked native PRESET1 work or start another parity
extension. Unselected M0-M7/R0-R16 features are optional; the declared candidate scope remains unchanged.

Native code is frozen MODEL1 source6219a08f, tree
432eab13b81702d423091d8bdd2113d75be9ada3ab2b9a05804128b80cccc892.
Owner-selected project license is AGPL-3.0-only. The license overlay preserves63
original members and changes only metadata/RECORD/notices; all65 seed shipping
members matched before native extraction. No native rebuild or stub generation.
The application source archive verifies all495 compiler inputs. A12-archive
source bundle provides dependencies, including OpenSlide's16 SDK source projects
and build patches, and nanobind/robin-map. Fonts retain unmodified assets/notices;
NumPy and Windows/MSVC/graphics are separate prerequisites.

Fresh Windows run42099947943 at harness commitc0aeba86 passes installation,
real-slide Python/ImGui ROI/tissue/statistics/CSV/save/replay, standalone reopen,
actual wheel/native removal and preservation of7 user files. Image20261101.353.1
uses declared MSVC runtimes and pinned test-only Mesa; PATH excludes development
SDK directories and mapped GEOS/OpenSlide come from the installed package.
Local Windows11 Pro10.0.26200 also passed the original workflow. Do not infer
arbitrary GPU/Windows support or biological/performance parity from these results.

Published candidate tag: v0.1.0-windows-candidate.1 on bipxx/qupathxx;
release targetf92f0659, deployment harnessc0aeba86, native source6219a08f.
Final local files: build/windows-workflow-checkpoint/delivery-agpl. Hosted receipt
and source inventory are durable under docs/benchmarks/windows-workflow-checkpoint.
The seed/runtime/cache and candidate data remain at the previously recorded
main-workspace paths. Only main and this active source checkout remain; source
archives from cleanup are preserved. The old probe is a different dirty stage
and is ineligible for acceptance.

## Parked scope

PRESET1 P1 passed focused native checks locally. Its native sources and CMake
changes remain uncommitted and were excluded from the handoff commits/binaries.
P2/P3 and exact import identity remain optional, with the existing normalization
observation retained. R2-R16 and the23 Partial rows form a reference catalogue,
not a count of mandatory remaining jobs.

## Latest closeout and waste review

Fresh handoff initially stopped on draft-token visibility, Windows environment
case handling and external fixture DNS, before numerical acceptance. Scoped
fixes changed only the harness and authenticated fixture access. Failed runs and
logs remain linked in the evidence summary; all candidate hashes stayed fixed.
A duplicate permission retry did not change the cause: inspect the failed phase
and its actual authorization/environment before redispatch. Source collection
reused cached pinned archives; no compiler, full platform/corpus qualification,
registry or performance campaign was added. After publication, stop at this
handoff and report the supported scope rather than pulling in new features.

The superseded pre-license archive in build/windows-workflow-checkpoint/delivery
remains after automatic cleanup review rejected deletion (blocked by policy).
It is not the selected candidate; use only delivery-agpl or the prerelease assets.
