# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. PROJECTMETA1 is locally verified; CLASS1 remains open.

Current: **R4/R5-CLASS1 — class ancestry and project defaults** (in progress).
Phase: reference/default-color preflight verified; native/public integration next.

- Outcome: import, inspect/edit and save/reopen exact recognized class ancestry
  and ordered project defaults, including zero images, through one class authority.
- First action: implement the [class contract](class-hierarchy.md): extend existing
  ObjectClass values with explicit parent/raw ARGB and a resolved RGB view; use
  the tested internal resolver for absent colors. Carry that state through native
  staged history and registry edits/subtree rename before binding/GUI work.
- Reuse: [captured class references and primitive](benchmarks/class-hierarchy-r4/README.md),
  retained builder/ROI, existing Document classes/history, Project snapshots/store,
  native binary decoder and common GeoJSON reader. No display-name heuristic,
  parallel hierarchy, second decoder, new pixel oracle or repeated preflight.
- Contracts: registry definitions are unique; ordered palette keys may repeat.
  Native flat keys retain flat identity. Only the pinned project wire format has
  a defined delimiter decoder; explicit names arrays/binary parents carry other
  ancestry. Refuse identity collisions/orphans/cycles atomically. Keep raw missing
  color separate from resolved default; project palette precedence is scoped to
  project loading rather than a new mutable process-wide singleton cache.
- Route: native definition/registry commands and history, owning Project registry/
  ordered defaults, one schema/snapshot/recovery extension, thin Python and shared
  existing class controls with explicit project-default application to images.
- Checks/stop: exact parent/color/association and palette ordering through
  import/edit/undo/save/reopen, subtree rename and used/child removal refusal,
  namespace/ownership, old schemas/corruption/failed-write/source guards and actual
  installed Python/GUI/SDK flows. Primitive success does not close this workflow.
- Cost/dependencies: settle class constructor/method signatures and native/foreign
  wire versioning with consumers before one grouped cached candidate; one stub/
  install after APIs settle. Repeat only for changed input or unresolved failure
  with diagnosis. Engineering is ready independently of owner biology/license/
  support choices; only openly licensed readers/codecs. R4/R5 and R6-R16 stay open.

Latest task review (2026-11-05, CLASS1 reference/color primitive):
Outcome/evidence: five fixture/provenance/semantics tests and39 independent native
color cases plus existing flat-class history pass. Source46358eb4 is a targeted
classes observer/internal primitive, distinct from installed408f383e. References
prove palette order/repeats, NULL omission, explicit ancestry/ARGB, project color
precedence, fresh fallback after clear and upstream colorless-GeoJSON failure.
Old-product refusals/flattening are preserved; public hierarchy remains missing.
Waste/action: **reuse** retained ROI/builder with one class-only producer, one
fresh process to avoid cache bias and one small default-color oracle prompted by
that finding. One targeted classes build; no wheel/stubs/install/math campaign.
A guessed adapter path and unsupported web tree request failed; use discovered
upstream paths. Preserve scopes/core provenance separately; do not repeat the
preflight or relabel it as an installed product result.
Next: integrate the native class model/history and Project registry/defaults,
then decoder/wire/store/SDK/GUI contracts before the next expensive ABI build.

After every bounded task or failed/stopped attempt, record its scoped outcome,
waste action and changed next step here before continuing or retrying.

CLASS1 native foundation checkpoint: explicit parent/raw ARGB/resolved RGB values,
shared graph resolution, sparse class history/subtree rename, ordered Project
registry/defaults and schema26 sparse document semantics are implemented locally.
Default edits are Project-only; explicit application is one image journal step
and deduplicates affected definitions without changing stored palette repeats.
Twenty legacy downgrade helpers and the all25-older-schema native witness are
updated. Native regression controls are written; execution is pending.
Waste/action: validate incoming definitions once and reuse trusted ancestor state;
no cloned UI hierarchy or default-color copy. The next affected core build runs
classes/project/save/spatial roles only. Bindings, wire/import and GUI integration
remain pending, so no wheel/stub/install or full-workflow acceptance is claimed.
