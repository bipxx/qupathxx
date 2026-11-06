# Radius-neighborhood coverage checklist

Scope:the [selected next phase](api-delivery-plan.md#next-phase-radius-based-spatial-neighborhoods).
Execution order and waste review live in [current-task.md](current-task.md).
This is requirement/evidence coverage, not a second task queue or full QuPath
feature inventory. Update affected rows after each task and diagnosed failure.
Existing nearest-distance or candidate2 evidence does not verify new code.

Status: **Pending** = no implementation evidence; **Implemented** = code present,
decisive check outstanding; **Verified** = linked passing source-scoped evidence;
**Blocked** = named external constraint. A failure remains visible until its
cause/correction and decisive rerun are recorded. Check boxes mean Verified.

Current coverage:48/50 requirements verified;2 pending publication and final
closeout. This is requirement coverage, not a time estimate or overall project
completion percentage.

## T1 — contract, capture, radius traversal and jobs

| Done | ID | Required behavior | State / evidence |
| --- | --- | --- | --- |
| [x] | N01 | Additive options/request/row/result/job names and explicit algorithm/input/result identity; preserve nearest-distance APIs | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N02 | Positive finite radius; explicit pixel/micrometre units; invalid enum/radius/budgets rejected | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N03 | Source1..100000/target0..100000 cohorts, canonical ID order; duplicate/missing IDs rejected | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N04 | Capture existing nucleus preference/body fallback, object IDs/revisions/kinds/geometry hashes and one Z/time plane | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N05 | Micrometre units require valid positive X/Y calibration; anisotropic metric; no unit fallback | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N06 | Binary64 distance<=radius includes exact boundary; immediate inside/outside cases match independent expected counts | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N07 | Exclude the identical ID by default; distinct coincident objects count; include-self option works | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N08 | Empty targets yield zero and one ID-ordered row per source; arbitrary cohort order preserves outputs/identity | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N09 | Immutable owner capture; worker has no live Document/reader access and performs zero pixel IO | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N10 | Indexed point-radius traversal, calibrated conservative pruning, no dense pair matrix or neighbor-list allocation | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N11 | Exhaustive independent small-scene count oracle agrees across several radii/scales and index split boundaries | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N12 | Snapshot and worker/index/result payload admitted before work; actual retained allocation checked; limits described as native allowances | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N13 | Actual distance comparisons bounded; exhaustion fails without a partial result | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N14 | Direct-call cancellation before/during traversal; no publishable partial result | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N15 | Job run/poll/wait/cancel/retry, one outstanding request, owner-thread guards, GIL-ready wait_work and joining shutdown | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N16 | Object/calibration/owner mutation refuses stale job publication; direct and job results agree | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |
| [x] | N17 | Existing spatial index/nearest-distance behavior and identity pass affected regression checks | Verified — [T1 native suites](benchmarks/neighborhood-workflow/t1-tests.log), [source identity/scope](benchmarks/neighborhood-workflow/t1-native.json) |

## T2 — native retained results, storage and CSV

| Done | ID | Required behavior | State / evidence |
| --- | --- | --- | --- |
| [x] | N18 | Native keep/list/get/status/verify/remove; bounded retained-record count/bytes and stable IDs | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N19 | Distinct versioned neighborhood payload/table; preserve QSD1 distance rows/hashes and old project behavior | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N20 | Save/reopen retains exact counts, cohorts, radius/units, geometry/calibration and recipe/input/result identity | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N21 | Transactional schema upgrade when needed; failed write preserves prior project | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N22 | Replay verifies saved inputs; geometry/calibration changes report stale; unrelated new objects do not join explicit cohorts | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N23 | Historical results remain inspectable/exportable without claiming current verification | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N24 | Corrupt/truncated/checksum-invalid payload, invalid IDs/counts/scales and allocation limits rejected | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N25 | Native locale-independent CSV:one row/source with identity/centroid, count, radius, units and recipe hashes | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N26 | Bounded atomic CSV export; cancellation/failure preserves existing output and removes temporary publication state | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |
| [x] | N27 | Project snapshot/save accounting includes new records; exact old inactive snapshot limits preserved | Verified — [T2 source/scope](benchmarks/neighborhood-workflow/t2-native.json), [initial results](benchmarks/neighborhood-workflow/t2-tests.log), [legacy-generator correction](benchmarks/neighborhood-workflow/t2-legacy-test-repair.log) |

## T3 — shared Python, CLI and ImGui consumers

| Done | ID | Required behavior | State / evidence |
| --- | --- | --- | --- |
| [x] | N28 | Nanobind options/request/result/rows/job and retained-record APIs; no independent Python computation | Verified — [Python/CLI run](benchmarks/neighborhood-workflow/t3-python-tests.log), [alias correction](benchmarks/neighborhood-workflow/t3-stub-alias-repair.log) |
| [x] | N29 | Python direct/job parity, result lifetime/owner shutdown and wait/GIL behavior | Verified — [direct/job/lifetime checks](benchmarks/neighborhood-workflow/t3-python-tests.log), [observed GIL release](benchmarks/neighborhood-workflow/resources.json) |
| [x] | N30 | Generated public stubs expose settled names/options; focused positive/negative typing/signature checks | Verified — [alias/member correction](benchmarks/neighborhood-workflow/t3-stub-alias-repair.log), [valid typing](benchmarks/neighborhood-workflow/t3-typing-valid.log), [3 invalid forms rejected](benchmarks/neighborhood-workflow/t3-typing-invalid.log) |
| [x] | N31 | CLI supports explicit source/target cohorts, operation/radius/units, save/export/reverify and clear invalid-input errors | Verified — [Python/CLI run](benchmarks/neighborhood-workflow/t3-python-tests.log), [alias correction](benchmarks/neighborhood-workflow/t3-stub-alias-repair.log) |
| [x] | N32 | Packaged CLI results/counts/CSV agree with the native/Python contract | Verified — [Python/CLI run](benchmarks/neighborhood-workflow/t3-python-tests.log), [alias correction](benchmarks/neighborhood-workflow/t3-stub-alias-repair.log) |
| [x] | N33 | Shared SpatialFrontend supports neighborhood run/poll/cancel/retry/keep/history/export without duplicate kernels | Verified — [native frontend/CLI suites](benchmarks/neighborhood-workflow/t3-consumer-native-tests.log); one combined binding build plus targeted include/GUI-flag repair |
| [x] | N34 | ImGui operation/radius/units and cohort controls are usable; count output visibly distinct from nearest distances | Verified — [506-cell GUI/CSV/replay](benchmarks/neighborhood-workflow/staged-workflow.json), [actual cancellation/retry](benchmarks/neighborhood-workflow/t3-gui-cancel.log), [inspected capture](benchmarks/neighborhood-workflow/desktop.png) |
| [x] | N35 | ImGui run/cancel/retry, inspect/keep, export and save/reopen through actual interactions | Verified — [506-cell GUI/CSV/replay](benchmarks/neighborhood-workflow/staged-workflow.json), [actual cancellation/retry](benchmarks/neighborhood-workflow/t3-gui-cancel.log), [inspected capture](benchmarks/neighborhood-workflow/desktop.png) |
| [x] | N36 | Radius/units/counts/errors readable; no misleading scale/geometry or hidden failures; no cosmetic-equivalence requirement | Verified — [506-cell GUI/CSV/replay](benchmarks/neighborhood-workflow/staged-workflow.json), [actual cancellation/retry](benchmarks/neighborhood-workflow/t3-gui-cancel.log), [inspected capture](benchmarks/neighborhood-workflow/desktop.png) |
| [x] | N37 | Settle consumer fields before one combined cached build and one stub generation; record any necessary corrective rebuild | Verified — [native frontend/CLI suites](benchmarks/neighborhood-workflow/t3-consumer-native-tests.log); one combined binding build plus targeted include/GUI-flag repair |

## T4 — installed workflow and bounded resource observations

| Done | ID | Required behavior | State / evidence |
| --- | --- | --- | --- |
| [x] | N38 | Bounded calibrated scene uses existing retained/imported cell objects; no segmentation rewrite/requalification | Verified — [pinned 506-cell import and translated crop](benchmarks/neighborhood-workflow/scene.json), [workflow](benchmarks/neighborhood-workflow/staged-workflow.json); no segmentation rerun |
| [x] | N39 | Frozen candidate wheel/native source identity; one final isolated install and exact installed-member checks | Verified — [final installed identity/members](benchmarks/neighborhood-workflow/installation.json), [506-cell installed route](benchmarks/neighborhood-workflow/installed-workflow.json) |
| [x] | N40 | Installed Python compute/inspect/keep/CSV/save/reopen/replay matches known counts | Verified — [final installed identity/members](benchmarks/neighborhood-workflow/installation.json), [506-cell installed route](benchmarks/neighborhood-workflow/installed-workflow.json) |
| [x] | N41 | Installed ImGui compute/inspect/keep/CSV/save/reopen and standalone opening of Python project pass | Verified — [final installed identity/members](benchmarks/neighborhood-workflow/installation.json), [506-cell installed route](benchmarks/neighborhood-workflow/installed-workflow.json) and [inspected installed UI](benchmarks/neighborhood-workflow/installed-desktop.png) |
| [x] | N42 | One packaged CLI workflow passes; reuse native numerical evidence across wrappers | Verified — [final installed identity/members](benchmarks/neighborhood-workflow/installation.json), [506-cell installed route](benchmarks/neighborhood-workflow/installed-workflow.json) |
| [x] | N43 | Representative dense cohort records size, comparisons and resource observations; bounded/cancellable behavior; no unmeasured speed claim | Verified — [20000 coincident cells,399980000 comparisons,68.6 MB process peak](benchmarks/neighborhood-workflow/resources.json); stage source scoped; no general speed/RSS claim |
| [x] | N44 | Changed-source cancellation/stale and old-nearest compatibility concerns covered by focused evidence, not full qualification replay | Verified — [T2 cancellation/stale/legacy evidence](benchmarks/neighborhood-workflow/t2-native.json), [T3 shared-mode regressions](benchmarks/neighborhood-workflow/t3-consumer-native-tests.log), [actual GUI cancellation](benchmarks/neighborhood-workflow/t3-gui-cancel.log) |

## T5 — source-matched Windows candidate handoff

| Done | ID | Required behavior | State / evidence |
| --- | --- | --- | --- |
| [x] | N45 | Frozen Windows wheel/native package with runtime/archive/notices/license checks; AGPL project license, openly licensed readers/codecs | Verified — [frozen package/source inventory](benchmarks/neighborhood-workflow/candidate.json), [installed runtime origins](benchmarks/neighborhood-workflow/installation.json); wheel/native static runtime inspection and standalone ZIP reopen pass |
| [x] | N46 | Matching raw-byte application source inputs and authenticated unchanged dependency sources accompany delivery | Verified — [frozen package/source inventory](benchmarks/neighborhood-workflow/candidate.json), [installed runtime origins](benchmarks/neighborhood-workflow/installation.json); wheel/native static runtime inspection and standalone ZIP reopen pass |
| [x] | N47 | Example/install/run guide explains cohorts, centroid semantics, radius/units, budgets, cancellation and saved-result limits | Verified — [install/run/API/limits guide](benchmarks/neighborhood-workflow/README.md), [working example](../examples/neighborhood_workflow.py), [actual installed route](benchmarks/neighborhood-workflow/installed-workflow.json) |
| [ ] | N48 | Public candidate assets and remote SHA256/size match local frozen artifacts; prior candidate remains available | Pending |
| [ ] | N49 | Current card, coverage checklist and support statement close against actual evidence; no full-parity/platform/biology/speed claim | Pending |
| [x] | N50 | Disposable-state disposition documented; preserve fixtures/source/cache/failures, respect existing cleanup-policy rejection | Verified — [disposition in delivery receipt](benchmarks/neighborhood-workflow/delivery.md); final products/cache/fixtures/failures preserved, no repeat of blocked recursive cleanup |

## Deferred scope

Density/edge correction, signed distances, k-nearest graphs, per-class matrices,
tiled aggregation, new segmentation/readers, fluorescence, integrations, extra
platforms and distributed workers are outside this checklist. They remain
optional inventory items; empty boxes above are only this selected phase.
