# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. CLASS1 remains open; reference/primitive/native foundation
are verified in their [recorded scopes](benchmarks/class-hierarchy-r4/README.md).

Current: **R4/R5-CLASS1 — class ancestry and project defaults** (in progress).
Phase: native foundation and GeoJSON class transport pass; binary/project context next.

- Outcome: import, inspect/edit and save/reopen recognized class ancestry and
  ordered project defaults, including zero images, through the current authority.
- First action: extend qupath_saved_data.cpp to emit explicit class definitions
  into the existing versioned GeoJSON route; replace parent flattening only once
  exact parent/raw-color retention is checked. Add project-context palette
  precedence in qupath_project.cpp using the same parser/definitions. Legacy native flat keys stay flat; only the pinned
  project class-list wire has a defined delimiter decoder.
- Reuse: [class contract](class-hierarchy.md), retained reference/ROI/builder,
 39-case resolver and new ObjectClass/Document/Project foundation. No new class
  model, decoder, edit journal, pixel capture or repeated source preflight.
- Settled native surface: ObjectClass::definition, explicit parent and optional
  color_argb()/resolved color; Document set_class_definition/replace_class_registry/
  class_path and subtree rename; Project classes/class_defaults/atomic replacement
  and explicit image application. Python values/getters must remain owning;
  expose parent/raw ARGB/leaf information with the existing ObjectClass type.
- Continue: reuse the passing GeoJSON v1/v2 transport and finish binary/project
  palette integration, then thin nanobind/GUI consumers against
  those authorities. Extend existing class controls; drafts are not another
  validator. Keep unique definitions separate from ordered/repeated palette keys.
  Do not silently reinterpret an existing flat root as derived during import.
- Checks/stop: actual pinned native/GeoJSON/project imports, default precedence,
  class edit/undo/save/reopen and empty projects; new snapshot/recovery/budget/
  corrupt-row/failed-write guards; actual installed Python/GUI and SDK/typing.
  Primitive/core checks alone do not close this vertical workflow.
- Cost/dependencies: group remaining compiler inputs/consumer contracts before
  one wheel candidate; one stub/install when interfaces settle. Reuse passing
  checks within their source/configuration scope; repeats need changed input or
  an unresolved risk with diagnosis. Engineering is ready independently of
  biology/license/support choices. Only openly licensed readers/codecs; full
  R4/R5 and R6-R16/milestone exits remain in the canonical queue.

Latest task review (2026-11-05, CLASS1 native GeoJSON substep):
Outcome/evidence: de8f9889 passes the affected classes/interchange roles; see
[wire results and input scope](benchmarks/class-hierarchy-r4/README.md). Explicit
ancestry, raw ARGB/absence, selected-export ancestor closure, flat/native leaf
identity, geometry, undo and atomic conflict/corruption refusal are covered.
Legacy flat RGB remains version1; richer native metadata uses version2. Binary
and project imports, SDK/editor and new-field integrity checks remain pending.
Waste/action: **reuse** captures, native class definitions, resolver and cached
build; **retain** the first compiler failure and fix with existing C++17 equality.
The final repeat covered a changed native-leaf parser guard. No wheel/stub/install
or broad qualification was run. **Merge** ancestor traversal cleanup into the
next necessary wire edit; do not build separately for that cleanup. Use directory
-g searches on Windows and bounded manifest output to avoid failed/noisy probes.
Next: finish [CLASS1 step 1](api-delivery-plan.md#class1-remaining-execution-steps)
with binary ancestry/raw-color definitions and shared project palette precedence,
including ordered repeats/empty projects. Reuse the verified GeoJSON authority;
exercise retained native/project fixtures at the affected native boundary.

After every bounded task or failed/stopped attempt, record scoped outcome,
waste action and changed next step here before continuing or retrying.
