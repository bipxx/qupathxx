# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-TMASAVED1 is locally verified in its recorded scope.

Current: **R5-ASSISTSTATE1 — assisted saved-state retention** (implementation drafted; six captures complete; importer acceptance pending).

- Outcome: carry recognized image facts, analysis defaults, portable ImageData
  properties and TMA state through the existing assisted-exchange route as well
  as direct native import. Preserve source/geometry/ID binding and explicit losses
  for unsupported values; do not silently change existing bundle behavior.
- Entry/evidence: tools/upstream/export_qupath_project.groovy still reports image
  type/stain/custom properties as not migrated, with no TMA-state handling. The
  assisted branch in cpp/qupath_project.cpp imports GeoJSON and XY calibration
  only. The four native state authorities are now delivered and reusable.
- Contract: new exchange schema2 explicitly selects native-saved-data or GeoJSON
  per entry. Recognized pinned native entries use one complete native geometry/
  state authority and bind the exact .qpdata hash; they carry no alternate file,
  calibration or loss fields. No automatic fallback after a native decode error.
  Schema1 keeps its current GeoJSON behavior; schema2 GeoJSON retains that route
  with explicit representation. Existing version1 exporter/probe bytes stay frozen.
- Producer: add a version2 exporter that identifies supported producer/header
  versions with a bounded data-only probe. Native entries emit bindings rather
  than redundant geometry/state serialization. Auto/native/geojson modes and an
  optional separate output directory are explicit; unknown legacy producers use
  GeoJSON through the existing QuPath APIs and retain reported state losses.
- Next action: bind the six successful captures to exact producer/parent/input
  hashes and explicit derived/synthetic provenance; add affected route/state/
  refusal tests. Freeze one candidate, build from cache and verify Python and
  actual desktop import/save/reopen. Do not replay successful producer captures.
  No native build or importer acceptance has been recorded for this draft.
- Dependencies/reuse: existing export script, pinned runtime, captured native
  fixtures and assisted import evidence; native image facts/settings/properties,
  TmaCodec/commands, private preview publication and current store. Capture only
  the missing assisted workflow using those sources; no detector/math recapture.
- Checks/stop: source-bound route/bundle compatibility, retained state/IDs/children
  and round-trip, explicit unsupported/refusal behavior, atomic failed apply, and
  actual Python/desktop consumers. Settle interfaces before one cached candidate;
  avoid API/schema changes when existing state suffices. Z/time calibration
  overrides, other geometry/classes/versions/builders and source races remain R5;
  R4/R6-R16 and owner research/license/release decisions stay in the same queue.

Latest task review (2026-11-04, ASSISTSTATE1 candidate preparation):
Outcome/evidence: all retained capture/producer/parent hashes and six fixture
materializations pass; native9627 rejects schema2 as expected.26 public nodes
collect, including independent state checks, strict native refusal, reviewed
source changes, unused alternate geometry and actual desktop bundle open/save.
Waste/action: **reuse** parent binaries/references and one cached native build;
**retain** compatibility/source/state/refusal and consumer checks because route
selection changed. No new binding/model/store schema or stub generation. The
failed nested quoting and wildcard-path probes were repaired without rebuilding.
Next: freeze the candidate, build once from cache, run new headless and affected
legacy native-import cohorts, native project/save roles and actual desktop flow.
Retain failures and update this review before any repair/retry. Product acceptance
and feature-branch publication are pending; all M0-M7 remain Partial.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
