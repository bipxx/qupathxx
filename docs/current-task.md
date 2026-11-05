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

Latest task review (2026-11-04, CALIB1 first staged results and repair):
Outcome/evidence: candidate e5d39e44 built in687.96s; staged266 cases yielded
252 passes/12 declared GUI skips/2 observer failures (guessed create_annotation
and Project.revision APIs). Malformed header probe refused8/9; absent timeUnit
with nonempty timestamps was incorrectly interpreted as seconds. Logs/results
are retained. The native spatial migration witness still omits tables19-22.
Waste/action: **reuse**252 passing results within candidate1 scope; **remove**
guessed observer APIs, using existing add_rectangle and saved clean/dirty/source
state to prove atomic refusal. **Retain** a targeted decoder repair and cached
rebuild because ambiguous units change behavior. Merge the migration-witness fix
and its native target into that same rebuild; do not recompile for each observer
repair or regenerate stubs before the final contract passes.
Both repaired observer nodes now pass at unchanged candidate1; combined scope
is254 unique passes/12 skips. Candidate1 source/wheel is preserved separately.
Nine permanent malformed-header controls and the extended native migration
witness are ready. No stubs/install have been generated yet.
Next: freeze the repaired missing-unit refusal and observer/
legacy controls, freeze one repaired candidate and build from cache. Rerun changed
state/import/downgrade paths plus new refusal controls at the repaired identity;
then native/desktop/package/typing acceptance. No CALIB1 acceptance yet.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
