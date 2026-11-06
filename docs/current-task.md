# Current implementation task

Owner scope update:100% QuPath parity is not required. Small cosmetic differences
are acceptable when results, interpretation and workflow usability are unaffected.
The [scope policy](api-delivery-plan.md#result-fidelity-and-scope-policy) and AGENTS.md
supersede earlier full-target instructions. Unselected inventory gaps are optional;
do not start work simply because a row remains Partial. Scientific correctness,
calibration, persistence/export/replay and meaningful overlay/display checks stay.

## Selected next outcome — representative large-slide tissue workflow

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

Remaining delivery:freeze this source, build one wheel with build tag2 using the
existing cache, install it once and verify the changed installed route and
identity; ship corresponding sources/notices and publish the scoped candidate.
Do not repeat the accepted full qualification, old fresh-host campaign or add
optional parity work. Changes to packaging/provenance require member/runtime
checks, not another binding/API rebuild.

Waste review:the first native test used a Python alias; it was corrected and only
that target retried. The snapshot test confused fixture relocation with native
layout change; a same-path old/new comparison resolved it before touching code.
One combined bindings/UI/CLI build was reused for all consumers. Required build
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
