# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-TMASAVED1 is locally verified in its recorded scope.

Current: **R5-ASSISTSTATE1 — assisted saved-state retention** (ready for contract preparation).

- Outcome: carry recognized image facts, analysis defaults, portable ImageData
  properties and TMA state through the existing assisted-exchange route as well
  as direct native import. Preserve source/geometry/ID binding and explicit losses
  for unsupported values; do not silently change existing bundle behavior.
- Entry/evidence: tools/upstream/export_qupath_project.groovy still reports image
  type/stain/custom properties as not migrated, with no TMA-state handling. The
  assisted branch in cpp/qupath_project.cpp imports GeoJSON and XY calibration
  only. The four native state authorities are now delivered and reusable.
- Next action: trace assisted bundle version, source/hash guards, geometry/type
  normalization and route priority. Define a single state authority for each
  supported route before adding serialization or decoding. Prefer reusing pinned
  native saved-state parsing where correct; avoid decoding geometry twice or
  inventing parallel state/codec models. Inspect compatibility with old bundles
  and unsupported saved-data records before choosing the smallest correct route.
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

Latest closeout (2026-11-04, R5-TMASAVED1):
Outcome/evidence: [native saved TMA grids](benchmarks/saved-tma-r5/README.md)
retain core slots/identities, analytic bounds, missing/case and legacy fallback,
facts/planes and child annotation/detection/cell/nucleus hierarchy through native
import, TMA edits/history, immutable snapshots, store/reopen and ImGui export.
Native9627e545:64 staged passes/four declared skips (20 new headless), one desktop,
one additional installed multi-image namespace/case-pooling case and five
packaged-stub checks. Three native roles pass on original products; repaired
QuPath native witness passes with its distinct regenerated dirty provenance.
All product source inputs/65 hashed installed members match; only the C++ witness
source differs. Both source manifests and scope boundaries are recorded.
Waste/action: **reuse** TmaCodec/native metadata edits rather than new state/API/
schema. One46.05s product build, one install, zero stub generation. **Retain** two
Groovy parse failures, old installed refusal and the native Unicode witness/
identity-postcheck failures. **Fix** ternary/list ambiguity and zero-prefix
literals; use an ASCII Unicode escape in the C++ test. Rebuild only that witness;
CMake refreshes provenance after test edits, so record its distinct identity
instead of claiming an unchanged archive. Do not rebuild the passing product.
Read actual headings/paths before documentation edits; corrected a stopped
heading-context patch without another build. No math/text-codec campaign.
Plan change/next: TMASAVED1 closes for declared recognized grids; unsupported
representations refuse. Prepare ASSISTSTATE1 above to carry the delivered state
across the other import route. Broader interchange, science/resources/platforms,
speed and release remain open. No milestone or overall-goal completion claimed.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse and necessary checks before continuing/retrying.
Only openly licensed readers and required codecs are eligible.
