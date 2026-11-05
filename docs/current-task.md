# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ENTRYMETA1 is locally verified in its
[recorded scope](benchmarks/entry-metadata-r5/README.md).

Current: **R5-ENTRYDETAILS1 — project-entry tags and description** (contract inspection ready).

- Outcome: retain both remaining known entry attributes on ProjectImage/Project
  with exact strings and tag-set semantics, independently of short metadata,
  ImageData properties and historical root/ROI facts.
- Entry/evidence: cpp/qupath_project.cpp still reports nonempty tags/description
  as image_metadata_dropped. The entry-metadata fixture's with-losses case already
  captures both through actual QuPath save/reload. Existing project map/state,
  snapshot/schema23 and inspector/SDK APIs provide the implementation pattern.
- First action: inspect pinned tags/description serialization and current entry
  APIs, especially null/empty defaults, tag uniqueness/order and text limits.
  Settle both attribute contracts together before editing the shared header.
- Contract: owning copies, atomic replacement/clear/no-op, project dirty lifecycle,
  bounded exact UTF8/tag values, snapshot accounting and transactional old-default
  migration. Keep unsupported fields and global metadata explicitly tracked.
- Reuse: current capture/readout and parent source, ProjectImage/Project, source
  review/publication and existing store/inspector/SDK. No new model/codec or
  metadata/ROI/reader/math recapture; capture only a demonstrated missing form.
- Checks/stop: independent exact values/sets and source-ID binding, absence/empty
  defaults, invalid and stale/refusal atomicity, update/snapshot/save/reopen and
  actual saved-geometry import/Python/desktop use. Settle interfaces before one
  cached build, one necessary stub/install pass and a scoped waste handoff.
- Dependencies: engineering can proceed now. Research labels, project license
  and release support decisions block only their claims. Only openly licensed
  readers and required codecs are eligible. Broader R4/R5 and R6-R16 stay queued.

Latest task review (2026-11-04, ENTRYDETAILS1 candidate preparation):
Outcome/evidence: source integration and44 new product controls are ready;
selected new/affected project/import/legacy tests collect with bounded semantic
IDs. Pinned references/parent/probe identity pass. Old b4abea1b reports four losses,
including empty description, and lacks the pair API. No product build/run yet.
Waste/action: **merge** both attributes before one header edit; **reuse** captured
builder/source and current Project authority/snapshot/store/inspector. A baseline
count assumption and glob matching its own receipt were corrected without any
native or capture retry; diagnostics retained. Short IDs prevent1MiB payloads
inflating logs. Reuse the trimmed backend's four selected native roles.
Next: freeze/build once, execute the selected metadata/details/import/store/
legacy cohort, four native roles and actual changed-state inspector/save. Then
one stub/repackage/install/typing pass and exact native/member identity checks.
No new model/codec, raw geometry/math recapture or per-attribute build cycle.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
