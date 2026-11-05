# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ASSISTSTATE1 is locally verified in its
[recorded scope](benchmarks/assisted-state-r5/README.md).

Current: **R5-CALIB1 — saved Z/time calibration** (capture verified; native value/codec drafted; integration pending).

- Outcome: retain supported saved physical Z/time calibration without changing
  source plane indices, explicit analysis recipes or measurement units silently.
- Entry/evidence: cpp/qupath_saved_data.cpp currently retains saved XY but emits
  image_settings_dropped for multi-Z/time saved data; cpp/qupath_project.cpp uses
  source raw metadata for plane compatibility. Existing source-axis adoption,
  reader units and native/v2 state routes provide the implementation base.
- First action: integrate the drafted owning ImageAxisCalibration value/codec
  through Document staging/history/restore, snapshots/store and source adoption;
  then native import, nanobind and the existing raw inspector. Follow the settled
  [contract](image-axis-calibration.md); no candidate build until interfaces and
  affected tests are ready.
- Contract: units, unknown/nonfinite values, regular/irregular spacing,
  source versus saved precedence, immutable ownership, replacement compatibility,
  legacy defaults, loss/refusal and direct/v2 native versus v1/GeoJSON behavior.
- Reuse: accepted source-axis/unit fixtures, current reader and retained state,
  source guards/private publication and installed assisted-state25b342fc within
  its unchanged scope. Capture only missing saved calibration variants; no
  repeated raw decoder/state/TMA captures or second frontend sampler/model.
- Checks/stop: independent physical coordinate expectations plus affected native,
  Python and desktop inspection/use; history/snapshot/save/reopen if state changes;
  invalid units/values and incompatible replacement refuse atomically. Freeze
  one candidate after interfaces settle. Stop at the declared supported contract,
  explicit unsupported limits and evidence/waste handoff; broader R5 stays open.
- Dependencies: source metadata and calibration contract can be inspected now.
  Biological labels, project license and release support block only their claims.
  Only openly licensed readers and required codecs are eligible.

Latest task review (2026-11-04, CALIB1 candidate preparation):
Outcome/evidence: shared Document/history/snapshot/store22/native-import/Python/
raw-inspector implementation is connected.44 initial product test nodes collect;
retained-record and independent valid-wire/bound controls were added afterward.
Thirteen fixture/source controls remain the only passing CALIB1-specific evidence.
Waste/action: **merge** physical-axis access at Document and reuse source raw
metadata unchanged; **retain** old-format and affected artificial-downgrade checks
because the sparse table changes schema. Select touched downgrade nodes instead
of rerunning their entire unrelated numerical suites. No interim product builds.
Native save-test filename probing failed; existing cache/backend targets were
reused and actual document-edits target was confirmed from CMake.
Next: freeze/build one cached candidate including affected native roles, stage
new/import/state/source and downgrade cohorts, then one stub pass and installed/
desktop checks. Preserve failures before targeted repair. Source-axis APIs and
schema22 have not yet been compiled or accepted.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
