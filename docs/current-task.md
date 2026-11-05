# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial; [ENTRYEDIT1](benchmarks/entry-editor-r5/README.md) is
locally verified within its stated GUI-authoring scope.

Current: **R5-PROJECTMETA1 — project-level string metadata** (in progress).

- Outcome: import, inspect/edit, save/reopen a project-wide string map independent
  of entry metadata, descriptions/tags and image/ROI facts, including zero images.
- Evidence/first action: cpp/qupath_project.cpp156 explicitly drops nonempty
  data/metadata.json. Pinned QuPath DefaultProject owns Map<String,String>, saves
  it with syncChanges and reloads separately. Capture one project-only supplement
  with the retained builder; no server, pixel or ROI recapture.
- Contract: owning Project.metadata getter and atomic set_metadata replacement/
  default clear; no-op leaves dirty state unchanged, edits affect Project only.
  Reuse exact UTF8/4096-byte strings,4096 pairs and1MiB estimated-map limits from
  entry metadata. No implicit typed conversion, Unicode normalization or namespace
  merging. Legacy absence defaults to empty; malformed values refuse atomically.
- Route: existing Project state/snapshot/budget/normalized SQLite migration,
  common manifest/assisted import source guards, thin nanobind exposure, existing
  draft editor extended for project mode (metadata only), usable without images.
- Checks/stop: pinned exact save/reload reference; actual manifest/v1/v2 geometry
  routes; namespace separation/owning copies/no-op/clear/refusal; empty project;
  snapshot ownership/budget/failed-write preservation; old schema migration and
  malformed rows; actual GUI draft/stale/Apply/save/reopen; installed SDK/typing.
  Close only this vertical workflow and hand off the next demonstrated gap.
- Cost: group header/binding/store/consumer contracts before one cached candidate;
  one stub generation/install after APIs settle. Reuse entry fixtures, validators,
  store/snapshot/editor and unchanged numerics. Repeat only for changed inputs or
  an unresolved failure, recording its cause before retry.
- Dependencies: ready independently of biology/license/support decisions. Only
  openly licensed readers/codecs. Broader R4/R5 and R6-R16 remain scheduled.

Latest task review (2026-11-05, one-time R4/R5 reconciliation):
Outcome/evidence: source confirms a separate project string map is dropped;
derived class-parent semantics are flattened in qupath_saved_data.cpp. Existing
entry/facts/defaults/properties/TMA/geometry/recovery/source workflows retain
scoped receipts. Additional builders/versions/Java records and network/power-loss
claims need specific fixtures/fault contracts, not a new general audit.
Waste/action: **reuse** current implementation/receipts and source-only review;
**remove** another broad import audit, build or recapture of accepted fields.
A guessed project_snapshot.hpp path was absent; inspect known project.cpp and
project.hpp, using discovered paths only. No build/install was run for review.
Next: PROJECTMETA1 supplement capture, then grouped native/consumer implementation.

After every bounded task or failed/stopped attempt, record the scoped outcome,
waste action and changed next step here before continuing or retrying.

PROJECTMETA1 capture review: one2.77s project-only QuPath process saved/reloaded
rich, empty and zero-image projects; nine captured files reuse the old builder
and source identity. Installed db90985d baseline confirms nine global pairs are
reported as dropped and zero-image import is refused even after loss approval.
Waste/action: reuse that baseline/runtime/source; no pixels/ROI/biology capture.
Implement genuine empty-project import without permitting all-unavailable image
projects, and group global map/store/snapshot/editor changes before building.

PROJECTMETA1 edit-attempt review: bulk patch stopped on a schema-constant spacing
assertion before binding/import/editor edits. Header/native map/schema25 changes
are present; Python schema constant is CURRENT_SCHEMA_VERSION=24 without spaces.
Waste/action: preserve this diagnosis and resume only the unapplied edits against
observed source. No build/retry/capture/install occurred; keep grouped candidate
pending until APIs, consumer and meaningful tests are complete.

PROJECTMETA1 legacy-helper review: first replacement matched no downgrade helpers
because they use separate db.execute calls rather than SQL semicolon batches.
Updated the observed execute-call form in19 modules, keeping corruption tests
intact. Another nonexistent optional test-path lookup was attempted; subsequent
searches must target the tests directory with filename globs, not guessed files.
No build has started. Next: focused project-metadata contracts, then grouped GUI
observer extension and one cached candidate after interfaces/tests settle.

PROJECTMETA1 pre-build review:48 focused cases (two opt-in desktop), existing
entry/affected project controls, five native roles including actual GUI and all
24 older schemas are selected. New capture identity test passes on the old safe
environment; functional changes remain unverified until candidate execution.
Grouped native map/schema25/snapshot/recovery adoption, owning SDK and shared
editor project mode are settled. Empty-source import retains metadata; all-
unavailable nonempty projects still refuse. Zero-image project class definitions
receive an explicit loss notice, since the registry is a separate remaining gap.
Waste/action: reuse source/capture/validators/editor and old affected selection;
no numerical campaign or new observer framework. Next: freeze and run the cached
candidate once, preserve and diagnose any failures before retrying.
