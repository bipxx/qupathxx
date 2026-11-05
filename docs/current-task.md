# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-CALIB1 is locally verified in its
[recorded scope](benchmarks/axis-calibration-r5/README.md).

Current: **R5-ENTRYMETA1 — QuPath project-entry metadata** (contract inspection ready).

- Outcome: retain supported per-image project metadata independently of native
  ImageData properties and historical root/ROI facts, with exact save/reopen.
- Entry/evidence: cpp/qupath_project.cpp reports image_metadata_dropped for
  nonempty metadata/tags/description; cpp/include/qupathxx/project.hpp defines
  ProjectImage, with existing project listing/snapshot/save/restore consumers.
- First action: inspect those existing entry fields/APIs and the pinned QuPath
  metadata-map representation. Check current fixtures before capturing missing
  metadata variants. Keep unsupported tags/description or other representations
  explicitly tracked rather than claiming the whole metadata family complete.
- Contract: settle scope/type/UTF8/size limits, owning copies, update semantics,
  project-versus-document lifecycle, snapshot accounting, old defaults and
  malformed/source-stale atomic refusal. Use Project as authority; no duplicate
  ImageData/root-fact encoding or UI model.
- Reuse: existing project manifest/source binding, ProjectImage/snapshots/store,
  loss review and inspector/Python consumers. CALIB1 and prior native/assisted
  state receipts retain their original source and scope. No reader/math recapture.
- Checks/stop: independent exact metadata values and source-ID binding, immutable
  listing/capture, failed apply/save preservation and round-trip, actual affected
  Python/desktop inspection. Settle interfaces before one cached candidate and
  one necessary stub/install pass. Close only this declared entry-map workflow.
- Dependencies: contract/engineering can proceed now. Research labels, project
  license and release support choices block only their affected claims. Only
  openly licensed readers and required codecs are eligible.

Latest task review (2026-11-04, ENTRYMETA1 candidate preparation):
Outcome/evidence: pinned map/probe/parent identities and independent values pass;
old installed5c179651 reports the four metadata/tag/description losses and lacks
the new entry-map API. The selected cohort collects161 tests; four boundary/
missing-source controls were added afterward. Source now connects ProjectImage,
atomic Project getter/replacement, snapshot accounting, sparse schema23 rows,
common importer and clipped existing inspector. No native acceptance yet.
Waste/action: **reuse** one parent source/probe runtime and current project/native
state authorities. No binary codec or new model/reader. **Retain** real changed
metadata in the GUI save check so reopening cannot pass on the original import.
Inspect actual symbols/fixtures and combine changes before one cached build;
all legacy fixture downgrades remove schema23 state and the native witness now
covers22 older schemas. No raw saved-data recapture or broad numerical replay.
Next: freeze/build one candidate, then new/affected project/import/store/legacy
checks, native roles and actual inspector. Generate stubs/repackage/install once
after behavior settles, with typing and exact source/member identity checks.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
