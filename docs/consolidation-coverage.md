# Native logic consolidation checklist

Detailed acceptance checklist for L5-L7 of the [delivery plan](api-delivery-plan.md#c-logic-optimization-and-thin-python-control-plan).
[Current task](current-task.md) owns the execution queue; [phase summary](speed-gap-coverage.md)
links here rather than maintaining a second detailed list. This is the single acceptance checklist for consolidation. Status records only
evidenced work; all28 items, including installed migration verification, are
complete. Remaining scope is indexed in the [gap checklist](remaining-gaps-checklist.md).

The outcome is one C++ authority for shipped selection, workflow state, reuse,
import interpretation, scientific values, persistence and export. Python parses
arguments, constructs native options, submits/polls/cancels and displays native
reports. ImGui and CLI use those same services. Independent test/reference code
may remain Python. Scientific/persistent identities stay exact; cosmetic parity,
new segmentation features and a binary release are outside this consolidation.

## Current coverage and next tasks

**28 complete, 0 in progress, 0 planned (28 total).** This is acceptance-item
coverage, not an estimate of engineering effort or QuPath parity. Native
contracts/services, focused bindings, thin primary controls, frontend wiring,
behavior/test migration and installed route agreement are evidenced. The
[consumer checkpoint](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair)
records source identity and limits. [NC27 boundary closeout](benchmarks/consolidation/README.md#shipped-python-boundary-closeout)
adds native preview ownership/calibration and the remaining example cleanup.
Final API/stub/guide/evidence/source handoff closes NC28; the selected
consolidation is complete. Published binary releases remain frozen.

- [x] NC01-NC04: compatibility baseline, native API and headless service contracts.
- [x] NC05-NC14: native selection/manifest, execution/reuse, guards and publication.
- [x] NC15-NC18: native NuCLS/RGB8/geometry/import and training cohort capture.
- [x] NC19-NC22: focused bindings, thin primary controls and shared frontend wiring.
- [x] NC23-NC26: behavior evidence, test migration, cached build/install and route agreement.
- [x] NC27: finish the [shipped-example boundary cleanup](consolidation-python-boundary.md).
- [x] NC28: final API/evidence/current-card/source handoff.

Stop this phase. The [final boundary receipt](benchmarks/consolidation/boundary-receipt.json)
and compiler/control manifests identify the matching installed source. Unchanged
scientific evidence is reused explicitly; broader optional features, a binary
release and general speed superiority are not part of these28 acceptance items.

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
| NC04 | Settle native API fields/lifetimes, typed failures and limits for density/import together before bindings | [Native ownership/error/admission contract](consolidation-native-api.md) and [decisive tests](benchmarks/consolidation/README.md#settled-native-workflow-contract); binding mappings remain NC19 | Complete |

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
| NC19 | Bind typed density/import services through focused feature units; release GIL during native work | Argument ownership, progress, terminal failures and cancellation arrive correctly; no per-pixel/object callbacks; public workflow types avoid the general core/GUI header | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC20 | Reduce project_density.py and density_manifest.py to arguments/options/submit/poll/display | Native service owns selection/reuse/receipts/CSV; no copied policy lists, dynamic sibling imports or Python workflow state machine | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC21 | Reduce nucls_import.py/raw classifier controls to the same thin boundary | No Python centroid, CSV/PNG interpretation or cohort eligibility implementation or Pillow/NumPy dependency implementing the NuCLS workflow; existing CLI options/status remain mapped | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC22 | ImGui/native CLI consume the same service/preview/report and expose meaningful units/errors/progress | No duplicated admission/reuse/export rules; selections and visible outcomes match SDK/Python, allowing harmless cosmetic differences | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |

## F. Integration and handoff

| ID | Requirement / easiest implementation path | Decisive completion check | State |
| --- | --- | --- | --- |
| NC23 | Test domain behavior once at native authority using existing fixtures and focused missing cases | Selection/reuse/legacy/cancel/corruption/limits/source-mutation/no-overwrite cases pass; meaningful scientific tests retained | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC24 | Replace implementation-coupled Python observation hooks with native result/progress observations while retaining their behavior coverage | No-recomputation, mutation refusal and atomic publication still tested; tests do not require removed Python internals | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC25 | Run cheap changed native targets first, then one combined cached consumer/binding build/install after interfaces settle | Compatible expensive objects reused; changed inputs/source identity logged; failed checks repair only affected targets; no per-substep packaging | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC26 | Verify a direct C++ route and a thin installed Python route on the selected saved-project/import fixtures | Authoritative rows/geometry/calibration/identity/export and terminal states agree; standalone native execution needs no Python | Complete - [consumer evidence](benchmarks/consolidation/README.md#installed-native-controls-and-legacy-archive-repair) |
| NC27 | Complete boundary review and remove superseded product domain implementations | Every shipped Python decision maps to control/display or a native authority; independent diagnostic oracles/fixtures live in tests/tools; no compatibility behavior lost | Complete - [boundary evidence](benchmarks/consolidation/README.md#shipped-python-boundary-closeout) |
| NC28 | Update API/stubs/guides, receipts, current card and this checklist; push matching source | Evidence links identify tested source and unresolved scope; active cache/releases/fixtures/archives preserved; no new release or speed claim inferred | Complete - [final source/installed receipt](benchmarks/consolidation/boundary-receipt.json) and source handoff on feature/scientific-grid-159 |

## Execution checklist for consumer integration and handoff

These subchecks make NC19-NC28 actionable; they do not add acceptance items or
replace the parent-item coverage count. Mark a parent Complete only when its decisive evidence
is linked in the tables above. Keep one active outcome and update this checklist
and the current card after each coherent task. Unchecked means not yet evidenced.

### NC19 - focused native bindings

- [x] Register density manifest/options/reports/jobs and NuCLS options/reports/jobs in focused binding units; link the existing native targets.
- [x] Map typed input-change, output-exists, budget, cancellation and filesystem failures to the declared Python exceptions.
- [x] Copy mutable options under the GIL; release it for waits and synchronous work after capture; expose immutable results without mutable nested aliases.
- [x] Bind the existing native labelled-training capture factory; keep unavailable-feature builds explicit without a Python domain fallback.
- [x] Check argument ownership, retained results, progress, cancellation and worker exception delivery through Python.

### NC20 - thin density controls

- [x] Replace project_density.py selection, reuse, checkpoint, CSV and publication logic with native manifest/options/job calls.
- [x] Replace density_manifest.py class scans and manifest assembly with native generation; preserve existing public arguments and report schemas.
- [x] Keep callbacks on the controller thread; forward callback failure and cancellation through native controls before acknowledging paused events.
- [x] Preserve CLI exit statuses and legacy resume behavior; remove dynamic sibling imports and superseded Python policy helpers.

### NC21 - thin import and training controls

- [x] Replace NuCLS pairing, CSV/PNG decoding, geometry, centroids and publication with the native import job.
- [x] Preserve requested-name selection, calibration, representation, class-column arguments and native diagnostics; remove product Pillow/NumPy decoding dependencies.
- [x] Replace raw-classifier cohort filtering with the native labelled-training factory.
- [x] Check existing CLI options, terminal statuses and interruption handling without reimplementing native admission in Python.

### NC22 - shared frontend integration

- [x] Confirm ImGui and native CLI call the shared density selection/preview/execution/report authority.
- [x] Remove any remaining frontend admission, reuse or export implementation superseded by that authority.
- [x] Verify meaningful units, progress, errors and completed/cancelled outcomes; accept harmless cosmetic differences.

### NC23 - native behavior coverage

- [x] Map required selection, reuse, legacy-format, corruption, limit, mutation, cancellation and no-overwrite behavior to existing source-scoped native evidence.
- [x] Add or run only missing cases or cases invalidated by consumer-driven native changes; retain independent scientific oracles.
- [x] Record exact affected source and checks; do not repeat accepted numerical qualification merely for frontend wiring.

### NC24 - migrate Python tests

- [x] Replace density/manifest tests that monkeypatch retired Python internals with native result, event or progress observations.
- [x] Replace NuCLS implementation hooks with supported native limits/events and fixture outcomes; retain malformed-input and atomic-publication checks.
- [x] Retain meaningful no-recomputation, source-mutation, cancellation, CLI and compatibility coverage; remove only redundant implementation assertions.

### NC25 - one settled build and development install

- [x] Settle known consumer/API edits and record changed inputs plus the unresolved risks before the expensive build.
- [x] Build cheap changed native/frontend targets first, then affected focused bindings using the existing cache and preserved binding objects.
- [x] Generate matching stubs and perform the development install after interfaces settle; repair only affected targets after a diagnosed failure.
- [x] Record source/runtime identity and build results; preserve active source, cache, fixtures, failure evidence, archives and frozen products.

### NC26 - installed route agreement

- [x] Run direct headless C++ and installed thin Python routes on the selected saved-project density and NuCLS fixtures.
- [x] Compare authoritative rows, geometry, calibration, stored identities, exports and terminal states using existing compatibility expectations.
- [x] Confirm standalone native execution needs no Python; use focused frontend checks for any remaining integration risk.

### NC27 - boundary audit and retirement

- [x] Review shipped Python workflow code: each operation must be argument/control/display work or invoke a named native authority.
- [x] Remove superseded Python domain implementations and unused product dependencies; keep independent references in tests/tools.
- [x] Confirm no compatibility fallback, duplicate state machine or frontend-only scientific policy remains in the selected workflows.

### NC28 - source handoff

- [x] Update API/stubs/guides for the installed native controls, supported inputs, exceptions and known limits.
- [x] Link source-scoped receipts and decisive evidence; reconcile this checklist and current-task.md with actual results.
- [x] Record actual waste/correction in the current card; add only new durable lessons to compilexx's existing notebook.
- [x] Commit and push matching source; report remaining scope without inferring a binary release, full parity or general speed superiority.

Execution order: prepare NC19-NC22 and their NC24 test changes together; reuse
NC23 native evidence and fill named gaps; perform NC25 once interfaces settle;
complete NC26-NC28 against that installed source. Documentation and boundary-review
edits can accompany each task. Do not create a second checklist or rebuild for a
status-only update.

[Shipped Python boundary review](consolidation-python-boundary.md) maps every
example/package module to its native authority and decisive checks.

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


## Native-first optional-interface follow-up

Owner-requested boundary follow-up, separate from the completed28 historical
acceptance items. **7/7 complete**; see [boundary review](consolidation-python-boundary.md)
and [source-scoped evidence](benchmarks/native-optional/README.md).

- [x] P01: audit all3 package modules/11 examples; distinguish product authority
  from optional caller controls and independent development oracles.
- [x] P02: share C++ application startup between native executable/nanobind;
  Python CLI forwards only argv/result. Validate actual launch/recovery/error paths.
- [x] P03: move raw-classifier recipe/pipeline/retention to C++ service/CLI;
  preserve baseline scientific identity and no-overwrite behavior.
- [x] P04: default ordinary CMake to Python OFF; explicit wheel opt-in verified
  with the installed backend settings reader.
- [x] P05: Python discovery disabled, no binding targets/files/imported DLLs;
  restricted-PATH native GUI/train/save/verify/reopen and headless entry pass.
- [x] P06: focused controls/stubs/tests pass;36/38 binding objects reused for new
  APIs; all38 preserved through native-only and restoration configurations.
- [x] P07: update policy/audit/build guide/current task; source handoff with no
  intermediate wheel, new release or repeated scientific qualification campaign.

## Native C++ usability follow-up

- [x] U01: optional native project example and synthetic fixture; open/analyze/
  retain/save/reopen/replay and no-overwrite checks pass.
- [x] U02: native build defaults, reader/training/topology SDK requirements and
  source-consumer linking documented in [the build guide](native-build.md).
- [x] U03: bounded Python-disabled Linux CI guard added, including installation
  and native runtime dependencies; local Windows check and shell syntax pass.
  Hosted job execution tracked separately in [evidence](benchmarks/native-usability/README.md).

## Open-format fluorescence and spatial counts

- [x] FO00: open format/reader/codec scope and explicit association profile.
- [x] FO01/FO02: reuse native raw measurement/batch services; uint16/float32 OME
  axes/values/display/CSV/save/replay/stale/no-overwrite checks pass.
- [x] FO03/FO04: bounded C++ Matrix Market/TSV reader and selected-gene point
  projection; exact counts, sparse zeros, associations, budgets/cancel/persistence.
- [x] FO05: installed native CLI/batch and existing viewer checks pass; hosted
  Python-disabled Linux CI passes on source99e75706. [Usage](fluorescence-omics.md), [evidence](benchmarks/fluorescence-omics/README.md).
