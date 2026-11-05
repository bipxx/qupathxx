# Current implementation task

Follow the [single queue](api-delivery-plan.md#dependency-lanes-and-immediate-queue)
and [validation policy](api-delivery-plan.md#validation-and-build-policy).
All M0-M7 remain Partial. R5-ASSISTSTATE1 is locally verified in its
[recorded scope](benchmarks/assisted-state-r5/README.md).

Current: **R5-CALIB1 — saved Z/time calibration** (contract inspection ready).

- Outcome: retain supported saved physical Z/time calibration without changing
  source plane indices, explicit analysis recipes or measurement units silently.
- Entry/evidence: cpp/qupath_saved_data.cpp currently retains saved XY but emits
  image_settings_dropped for multi-Z/time saved data; cpp/qupath_project.cpp uses
  source raw metadata for plane compatibility. Existing source-axis adoption,
  reader units and native/v2 state routes provide the implementation base.
- First action: inspect saved calibration fields and existing reader/Document
  axes, override ownership, snapshots and store. Determine what is already
  representable and what smallest missing operation is needed before coding.
- Contract to settle: units, unknown/nonfinite values, regular/irregular spacing,
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

Latest task review (2026-11-04, ASSISTSTATE1 closeout):
Outcome/evidence: explicit exchange2 native/GeoJSON/mixed routes retain native
facts/defaults/properties/TMA using one complete native authority. Original
staged76 passes/1 observer failure/5 GUI skips combine with targeted repairs to
77 unique passes; four native roles, actual desktop import/save, installed mixed
workflow plus5 stub checks and three producer controls pass. See the linked
receipt; all461 compiler inputs and65 installed hashed wheel members match.
Waste/action: **reuse** six successful captures and parent binaries/references;
**merge** geometry/state import at the existing native decoder. One cached build
45.55s, one successful install, zero stub generations. Two observer assumptions
were repaired with targeted nodes; passing cohorts/product were retained. Missing
pip required the available uv installer. All failed attempts remain recorded.
**Remove** nested command quoting and wildcard path assumptions; use literal
here-strings and directory/-g searches. No campaign, alternate state codec or
rebuild for these repairs. Screenshot inspected; full R5/science/release stay open.
Next: inspect CALIB1's saved versus source-axis contract before implementation;
reuse accepted route evidence without extending its claims to calibration.

After each bounded task or failed/stopped attempt, perform the
[waste review](api-delivery-plan.md#task-closeout-and-waste-review) and update state,
next action, dependencies, reuse, checks and stop condition before continuing.
