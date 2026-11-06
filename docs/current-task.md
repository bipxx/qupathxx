# Current implementation task

Owner scope update:100% QuPath parity is not required. Small cosmetic differences
are acceptable when results, interpretation and workflow usability are unaffected.
The [scope policy](api-delivery-plan.md#result-fidelity-and-scope-policy) and AGENTS.md
supersede earlier full-target instructions. Unselected inventory gaps are optional;
do not start work simply because a row remains Partial. Scientific correctness,
calibration, persistence/export/replay and meaningful overlay/display checks stay.

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
