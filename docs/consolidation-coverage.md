# Native logic consolidation checklist

Detailed acceptance checklist for L5-L7 of the [delivery plan](api-delivery-plan.md#c-logic-optimization-and-thin-python-control-plan).
[Current task](current-task.md) owns the execution queue; [phase summary](speed-gap-coverage.md)
links here rather than maintaining a second detailed list. This is the single acceptance checklist for consolidation. Status records only
evidenced work; the Python migrations are not yet implemented.

The outcome is one C++ authority for shipped selection, workflow state, reuse,
import interpretation, scientific values, persistence and export. Python parses
arguments, constructs native options, submits/polls/cancels and displays native
reports. ImGui and CLI use those same services. Independent test/reference code
may remain Python. Scientific/persistent identities stay exact; cosmetic parity,
new segmentation features and a binary release are outside this consolidation.

## Current coverage and next tasks

**17 complete, 1 in progress, 10 planned (28 total).** This is acceptance-item
coverage, not an estimate of engineering effort or feature parity. NC01 records
only the pre-change baseline; NC03 proves headless service extraction and
NC05-NC14 cover native manifest selection/preview/publication and shared
execution/reuse with legacy format adapters. NC16/NC18 add native RGB8 decoding
and training cohort capture. No thin
Python consumer or installed integration is complete.

- [x] NC01: capture compatibility baseline.
- [x] NC03: extract and link the shared headless C++ density service.
- [x] NC02: shared native selection/run/report and control options.
- [ ] NC04: settle importer and density ownership, failures and aggregate limits together.
- [x] NC05-NC08: native manifest loading, shared selection/preview and exports.
- [x] NC09-NC14: shared execution, guards, legacy resume, publication and cancellation.
- [x] NC16 / NC18: native RGB8 PNG decoding and training cohort capture.
- [x] NC15 / NC17: native NuCLS pairing/CSV/geometry and atomic project import.
- [ ] NC19-NC22: bind settled services and reduce Python/ImGui/CLI to controls.
- [ ] NC23-NC28: complete focused behavior/integration checks, boundary audit and source handoff.

Immediate order: settle the shared density/import contract (ownership, aggregate
admission, terminal failures), then wire thin consumers and run the single
combined binding build. Do not count a
passing kernel test as completion of an unfinished consumer migration.

[Incremental native evidence](benchmarks/consolidation/README.md) records the
extraction and strict-manifest checks, their limits and diagnosed failures.

## Order and existing authorities

Execute A -> B -> C -> D -> E -> F. Settle the density and importer interfaces
before the combined binding build; native tests run as each authority settles.
Cohort/manifest and run/resume are parts of the same density service, not separate
runners. Reuse these existing sources:

| Responsibility | Current source / easiest path |
| --- | --- |
| Headless density execution/preview | Extract the service in [density_workflow.cpp](../cpp/density_workflow.cpp) and its [SDK types/job](../cpp/include/qupathxx/density_workflow.hpp); the native target depends on core + JSON without ImGui |
| Density science and reuse admission | Keep SpatialDensityRequest/result and [density_reuse.hpp](../cpp/density_reuse.hpp) as authorities; do not substitute input-hash equality for captured-object compatibility |
| JSON, file integrity and publication | Reuse [strict_json.hpp](../cpp/strict_json.hpp), [json_string.hpp](../cpp/json_string.hpp), [reader_file_guard.hpp](../cpp/reader_file_guard.hpp), existing fingerprinting and [atomic_export.hpp](../cpp/atomic_export.hpp) |
| Python logic to retire | [project_density.py](../examples/project_density.py), [density_manifest.py](../examples/density_manifest.py), [nucls_import.py](../examples/nucls_import.py) and training selection in [raw_classifier_workflow.py](../examples/raw_classifier_workflow.py) |
| Native project/binding patterns | Reuse [project_batch.hpp](../cpp/include/qupathxx/project_batch.hpp), native request/job/result lifetimes and [feature binding registration](../cpp/project_batch_bindings.cpp); do not force density into an incompatible generic batch result |

## A. Contracts and module ownership

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC01 | Record current Python/GUI inputs, output ordering, errors, limits, exit codes and both receipt schemas from existing examples/tests | [Compatibility contract and baseline](consolidation-contract.md) | Complete - baseline only |
| NC02 | Define shared native selection/options/preview/run/report types supporting explicit per-image IDs and class selectors | [Typed native run options and both schema routes](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters); no Python callbacks in workers | Complete |
| NC03 | Separate native service from the panel controller; use a headless core+JSON target and thin GUI job/controller | [Headless build and native tests](benchmarks/consolidation/README.md); no Python/ImGui/OpenGL link dependency | Complete |
| NC04 | Settle native API fields/lifetimes, typed failures and limits for density/import together before bindings | Density snapshots/events/jobs are owned natively; importer contracts, aggregate capture limits and terminal failure mapping remain to settle together | In progress |

## B. Density selection and manifest authority

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC05 | Load qpx.project-density/1 in C++; retain bounded strict JSON, duplicate-key/type/nonfinite/unknown-field checks and relative project paths | [Strict loader, shipped example and generator roundtrip](benchmarks/consolidation/README.md) | Complete |
| NC06 | Resolve image IDs, explicit object IDs, class selectors and all-cells policy in one native selection pass | [Native class/explicit cohorts and generator cases](benchmarks/consolidation/README.md); consumer wiring remains NC20/NC22 | Complete |
| NC07 | Native preview owns calibration, planes, geometry/curve policy and density request admission | [Native request admission, counts/calibration/all issues, source unchanged](benchmarks/consolidation/README.md) | Complete |
| NC08 | Generate/save manifests and preview reports natively using existing atomic exports | [Generated manifest roundtrip, invalid preview and save-new/scratch refusal](benchmarks/consolidation/README.md) | Complete |

## C. Execute, reuse and publish through one service

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC09 | Execute each admitted selection through the existing native density kernel and retained record path | Same numeric rows, area, calibration, hashes and native CSV as current accepted workflow; no Python geometry/statistics; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |
| NC10 | Centralize source guards: closed/saved project, SQLite sidecars, file versions, setup/final fingerprints and checks between images | Source mutation/sidecars fail clearly and preserve inputs; avoid hashing an unchanged whole project once per image; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |
| NC11 | Centralize saved-record reuse using density_reuse_compatible and one engine compatibility policy | Unrelated metadata edits/larger allowances retain allowed reuse; changed geometry/centroids/calibration/engine or smaller limits require fresh admission/computation; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |
| NC12 | Implement explicit native readers for Python qpx.project-density-checkpoint/1 and GUI qpx.gui-density-checkpoint/1, then normalize | Old folders still work with their documented routes; schema/identity/corruption mismatches refuse or recompute as specified; no silent interchangeability; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |
| NC13 | Keep new-directory/no-replace publication, per-image QPX/receipts, contextual combined CSV and final reports in C++ | Existing files survive collisions; native numeric CSV text/ordering is preserved; no partial file is advertised as complete; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |
| NC14 | Native job owns cancellation, joins and partial/failure/completed-image state; expose existing computed/reused/kernel-call observations | Cancel publishes no unfinished image; completed rows/checkpoints remain valid; reuse performs zero new density kernel calls; input project unchanged; [evidence](benchmarks/consolidation/README.md#shared-native-execution-and-legacy-adapters) | Complete |

## D. Remaining domain rules move to native authorities

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC15 | Native NuCLS service owns case-sensitive pairing/selection, CSV interpretation, row validation and bounded diagnostics | Same accepted/missing/malformed pairs and row issues as existing importer; no hidden dependency on the density example; [native evidence](benchmarks/consolidation/README.md#native-nucls-pairing-interpretation-and-publication) | Complete |
| NC16 | Decode strict RGB PNG natively; use one pinned openly licensed decoder, existing source guards and allocation limits | Exact RGB8/filter/Adam7 pixels; modes/corruption/limits/mutation refuse; [open license and evidence](benchmarks/consolidation/README.md#native-training-selection-and-rgb-png-prerequisites) | Complete |
| NC17 | Construct NuCLS geometry, accurate centroids, classes, calibration and representation in C++; preview and atomic save-new use existing project authorities | Accepted objects/geometry/calibration/declared centroid identity match baseline; any blocking issue prevents whole-project publication; no overwrite; [native evidence](benchmarks/consolidation/README.md#native-nucls-pairing-interpretation-and-publication) | Complete |
| NC18 | Move class/area training-cohort eligibility from raw_classifier_workflow.py into the existing native training frontend | Exact native cohort/table/model inputs and empty/invalid/limit checks; [evidence](benchmarks/consolidation/README.md#native-training-selection-and-rgb-png-prerequisites). Thin consumer wiring remains NC21 | Complete |

## E. Thin SDK, binding, CLI and ImGui consumers

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC19 | Bind typed density/import services through focused feature units; release GIL during native work | Argument ownership, progress, terminal failures and cancellation arrive correctly; no per-pixel/object callbacks; public workflow types avoid the general core/GUI header | Planned |
| NC20 | Reduce project_density.py and density_manifest.py to arguments/options/submit/poll/display | Native service owns selection/reuse/receipts/CSV; no copied policy lists, dynamic sibling imports or Python workflow state machine | Planned |
| NC21 | Reduce nucls_import.py/raw classifier controls to the same thin boundary | No Python centroid, CSV/PNG interpretation or cohort eligibility implementation or Pillow/NumPy dependency implementing the NuCLS workflow; existing CLI options/status remain mapped | Planned |
| NC22 | ImGui/native CLI consume the same service/preview/report and expose meaningful units/errors/progress | No duplicated admission/reuse/export rules; selections and visible outcomes match SDK/Python, allowing harmless cosmetic differences | Planned |

## F. Integration and handoff

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC23 | Test domain behavior once at native authority using existing fixtures and focused missing cases | Selection/reuse/legacy/cancel/corruption/limits/source-mutation/no-overwrite cases pass; meaningful scientific tests retained | Planned |
| NC24 | Replace implementation-coupled Python observation hooks with native result/progress observations while retaining their behavior coverage | No-recomputation, mutation refusal and atomic publication still tested; tests do not require removed Python internals | Planned |
| NC25 | Run cheap changed native targets first, then one combined cached consumer/binding build/install after interfaces settle | Compatible expensive objects reused; changed inputs/source identity logged; failed checks repair only affected targets; no per-substep packaging | Planned |
| NC26 | Verify a direct C++ route and a thin installed Python route on the selected saved-project/import fixtures | Authoritative rows/geometry/calibration/identity/export and terminal states agree; standalone native execution needs no Python | Planned |
| NC27 | Complete boundary review and remove superseded product domain implementations | Every shipped Python decision maps to control/display or a native authority; independent diagnostic oracles/fixtures live in tests/tools; no compatibility behavior lost | Planned |
| NC28 | Update API/stubs/guides, receipts, current card and this checklist; push matching source | Evidence links identify tested source and unresolved scope; active cache/releases/fixtures/archives preserved; no new release or speed claim inferred | Planned |

## Progress rules and stop condition

There are **28 planned acceptance items**. Existing native helpers are reuse
inputs, not checked-off migration work. At implementation start, mark the current
row In progress; check it Complete only with a decisive evidence link. If a real
blocker arises, record the concrete condition and next independent work. Amend
this checklist for an agreed scope change instead of inventing a second ledger.

After each coherent task or failed attempt, update the current card with outcome,
check, actual waste/correction and next action. Append only durable new lessons
to compilexx. Documentation changes need link/diff checks, not product builds.
Reuse unchanged-kernel evidence; thin consumers get mapping/ownership/integration
checks, not repeated numerical qualification for every frontend. Native APIs must
still validate data even when Python argparse already validated syntax.

Finish when NC01-NC28 are evidenced: one headless C++ workflow/import authority,
compatible saved data/receipts and thin consumers. No100% QuPath parity, cosmetic
campaign, whole-slide performance acceptance, optional feature expansion or binary
publication is implied. Deferred L3 optimization stays separate from this queue.
