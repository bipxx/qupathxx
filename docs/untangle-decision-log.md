# Untangle decision log

Owner explicitly invoked [untangle-codebase](https://github.com/caldoai/skills/blob/main/skills/untangle-codebase/SKILL.md)
and selected both Comments and Tests. Inventory precedes implementation. The active
checkout remains scientific-grid-159; exact scientific/persistent identities and
C++ application ownership are required. This is the prerequisite to L1-L7.

## Inventory and redesign (before edits)

A local include inventory covers511 C++ source/header files and1,503 resolvable
local includes; public headers do not include private app headers. Independent
core, frontend and workflow inventories supplement the dependency scan. Source
size alone is not a finding; generated reference arrays are deliberate oracles.

### U1 Put JSON string encoding below checkpoint/application modules

- batch_checkpoint.cpp owns batch_json_quote, yet independent CLI reports and the
  density service include checkpoint internals solely to serialize strings.
- source_identity.cpp, file_fingerprint.cpp and provenance.cpp copy the same
  escape mechanism (the last is embedded in a streaming writer method).
- Introduce one neutral private JSON-string encoder. Preserve exact lower-case
  escapes/UTF-8 bytes. Keep caller validation where its accepted text contract
  differs; remove the batch-named wrapper and update all callers explicitly.
- Share the pinned nlohmann-json dependency setup currently copied by density and
  interchange CMake paths. Preserve options, hash, target and license install.

### U2 Make watershed headers independently understandable

- log_response_regions.hpp and log_sampled_nuclei.hpp are injected inside a
  namespace in log_watershed.hpp and rely on types/helpers declared before them.
- Put shared types and actually shared region primitives in self-contained
  private headers; give each header explicit dependencies and namespace.
- Remove stale candidate/never-production comments; retain numerical rationale,
  precision/topology and external ImageJ attribution. Change no algorithms.

### U3 Give cell contour construction one module and explicit loading policy

- cells.cpp owns common tracing/concurrency/repair/integer proof used by fresh
  scientific detection and both archive readers, coupling legacy detector edits
  to shared reconstruction policy.
- Both archive readers duplicate identical catch/reset policy for numeric-only
  historical archives. Fresh detection must stay strict; cancellation must fail.
- Move the shared authority to cell_contours.cpp with narrow private access and
  an explicit archive-tolerant entry point. Preserve exact geometry/order/budgets.

### U4 Keep density reuse policy with density and make async tests decisive

- project_density_panel.cpp contains same_recipe domain comparisons; UI naming
  hides their independent use and Python repeats them in project_density.py.
- Extract the native comparison into a density-specific authority with the exact
  current captured-input and admission semantics; later L5 will use it from the
  shared headless service/binding rather than invent another comparator.
- Two density-panel test waits have no deadline. Bound behavioral observation
  and join/cancel on failure. Preserve stale-input, unrelated-edit, smaller-limit,
  engine-change and atomic collision assertions. No meaningful test is removed.

### U5 Keep replay readiness vocabulary in one place

- GuiInputWait, its long validation allow-list, binding names and dispatch are
  manually synchronized. Remove duplication of the vocabulary, preserving every
  existing numeric value and behavior; keep dispatch with its domain owners.
- Broad physical relocation of all GUI/core public declarations is not itself
  accepted as a fix. Determine concrete include/caller impact before choosing
  that separate structural change; avoid a wrapper that leaves coupling intact.

### U6 Derive panel handles from domain-owned target membership

- gui_spatial.hpp, gui_tma.hpp, gui_pixel_classifier.hpp and
  gui_object_classifier.hpp and the pixel-stream browser size/index widget-handle arrays using enum ranges
  and appended-target exceptions. Unrelated enum edits can change membership.
- Put explicit target lists beside each panel's storage and derive lookup/storage
  from those lists, preserving all current members and behavior. The surgical
  gui.cpp routing uses the same membership instead of copied enum ranges. No generic
  command/dispatcher framework. This was identified in the frontend inventory;
  implement and exercise existing panel/replay consumers with the combined build.

### Existing native migration requirements (L5-L7)

- Python density/manifest/NuCLS use dynamic example-to-example imports for file
  guards/serialization, and Python owns reuse, selection and centroid rules.
- The shared native workflow/import authorities in the existing L5-L7 plan are
  the fix; no interim Python helper library or third density runner. Preserve
  both old receipt formats through native adapters. These remain open until
  migrated; they are not declared untangled by a header move.

## Required special cases retained

Archive numeric-only fallback; unsupported/sparse geometry fallback; historical
recipe/schema/numeric precision behavior; platform file sharing recovery; worker
joins; source mutation guards and no-replace publication. These implement actual
contracts and must not be deleted merely for reducing branch counts.

## Execution and acceptance

Implement U1-U6 after this recorded redesign. Run only affected native targets,
standalone header checks and relevant serialization/CLI/record tests. Shared
interfaces settle before one combined cached build. Independent sub-agents review
for remaining worthwhile links/special cases and behavior regressions. Keep
comments that record external facts and mathematical/compatibility requirements;
remove only redundant/stale ones in edited modules. Test cleanup runs last.
Record results/failures/waste here and in the current card; no broad benchmark,
platform campaign or release. Work is complete only when actionable inventory
items are resolved and remaining migration issues are honestly accounted for.

## Implementation/review outcome - U1-U6 complete

U1-U6 are implemented. Independent core/workflow/frontend reviewers found no
remaining worthwhile structural problem in these addressed areas. Moved numerical
and contour bodies compare exactly; enum numeric values, names/registration order
and five panel target/index maps match the original. Ten standalone private/public
header checks and15 affected native suites pass. The changed module passes direct enum and frozen real-output checks: all66
members/order and all2,237 labels/ordered geometries agree in four workflows.
The final provenance/source checks pass, and all37 binding objects were reused.
[Source-scoped evidence](benchmarks/untangle-codebase/README.md) owns receipts/logs.

Test cleanup retained meaningful scientific, archive, quota and compatibility
checks. One old replay acceptance subset was superseded by full-domain behavioral
coverage. Density wait failures cancel/join, and the collision fixture verifies
its intended phase and uses no-replace creation; CTest supplies the outer timeout.
Comment cleanup removed stale candidate/never-production text, a narrating Python
comment and a pass-through JSON lambda; mathematical/external constraints remain.

Waste: one wrong guessed save-test target stopped the initial command after the
core/first five targets built. Corrected to qupathxx_save_tests and reused all
objects, retaining both logs. No numerical thresholds or stack limits changed.
The public replay header/binding change requires rebuilding its actual consumers;
this cost is justified here, not a qualification-refresh campaign. Future edits
should avoid guessing paths/targets and settle new tests/CMake wiring up front.

Final repeat scan found one encoder copy inside provenance.cpp's streaming writer,
missed by function-name searches. It now consumes the same neutral encoder;
validated metadata and serialized bytes remain unchanged. This requires only a
provenance/core relink and its affected tests after the in-flight binding build,
reusing all binding objects. Search for encoding operations, not just helper names.
