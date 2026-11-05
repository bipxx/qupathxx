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

Latest task review (2026-11-04, ENTRYMETA1 closeout):
Outcome/evidence:155 staged passes/six declared GUI skips, four native roles,
actual changed-metadata inspector/save, installed snapshot/budget, five public
stubs and positive/three-negative typing pass. Two optional-map and three actual
v1-GeoJSON/v2-GeoJSON/v2-native geometry controls pass on the same product. Exact
maps, namespaces, ownership/bounds/faults/missing-source and schema23 persist.
All465 compiler inputs and65 installed hashed members match; screenshot inspected.
Waste/action: **reuse** one parent TIFF and capture, plus current project authority;
**remove** codec/model duplication and interim product builds. One553.51s cached
ABI build, one stub generation and one uv install. Two inherited native targets
were unneeded: executed backend retained and future target list trimmed. A
closeout audit found empty-bundle tests too narrow for saved geometry; only the
three missing routes were added, without repeating the passing cohort/build.
**Merge** upcoming tags/description contracts before one public-header edit and
reuse their captured case, avoiding separate capture/build cycles per attribute.
Next: inspect ENTRYDETAILS1's upstream defaults/set semantics and settle both
attributes before implementation. This closes only the entry-map engineering
slice, not the full R5, scientific/resource/host/release scope.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
