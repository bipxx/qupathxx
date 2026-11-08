# Reusable stain model files — MODEL1 contract

Status: locally verified for the bounded file-reuse workflow; [installed evidence](benchmarks/stain-model-r7/README.md#m3-installed-file-reuse-closeout--2026-11-05). This is model
exchange using the current native StainTransform, not a new estimator, model
registry, QuPath file-format promise or scientific-parity claim.

## User outcome and existing authority

Save an estimated or supplied stain basis, reopen it in another session, and
apply its exact normalized three-vector basis and white values through SDK,
ImGui and project batch/CLI. Invalid files fail without changing the current
model or published results. Analysis and saved project recipes retain the same
model digest. Reuse the native transform, stain_model_sha256, immutable captures,
existing GUI estimated_basis/full-basis path and batch transform argument.

Current evidence: stain estimation, bounded reservoirs, cancellation/stale jobs,
JSON estimate export, manual Python exact reload, GUI estimate/apply, model digests
and saved ImageAnalysisSettings already exist. Neither native public headers nor
bindings provide a model-file loader; CLI accepts numeric vectors/white and GUI
accepts manual/estimated/saved-image vectors. Do not redeliver those mechanisms.

## File/API contract

- Add a small public stain-model I/O header returning StainTransform values.
  Expose stain_model_json(transform), stain_model_from_json(text),
  save_stain_model(transform, path) and load_stain_model(path). Bind the same
  functions directly to Python; no second Python JSON interpretation.
- Canonical model JSON uses schema qpx.stain-model/1 and members white (three
  numbers), stains (three vectors of three numbers), model_sha256 (existing
  digest). Preserve normalized doubles and signed third-vector orientation;
  use from_normalized without re-normalization. Emit with round-trip precision.
- Also accept existing qpx.stain-estimate/1 exports as model input. Extract only
  their white/stains/model_sha256 after verifying the supported schema/algorithm.
  This loads a model, not an authenticated/reconstructed estimator result;
  historical source/provenance/diagnostics are not adopted as current analysis.
- Require finite numeric components, supported RGB8 white, a valid normalized
  nonsingular basis, exact member shapes and matching model digest. Reject
  duplicate keys, invalid JSON, unsupported schema, malformed hashes and
  mismatches. Reject unexpected fields in the minimal model format; estimate
  exports keep their defined extra metadata. Do not silently normalize a corrupt
  basis, drop the third vector, replace white or invent a fallback model.
- Bound serialized input/file reads at16MiB (the existing estimate-export bound),
  including files that grow while reading. Use UTF-8/filesystem-safe paths and
  existing atomic export. Failed writes preserve the prior destination; temporary
  files are cleaned by the existing writer. A transform holds no input file or
  document references after load.
- Reuse strict_json.hpp and nlohmann JSON under the existing optional interchange
  capability. Preserve minimal builds: writer/transform/estimator remain usable;
  parsing/loading report the established capability error when interchange is
  disabled. No unconditional decoder/library dependency or new reader.

## Consumers and retained behavior

Python uses the native loaded transform with existing AnalysisRequest and managed
ViewerSession operations; a new managed estimator/model authority is unnecessary.

CLI adds --stain-model FILE beside existing --white/--stains/--normalized inputs.
Reject combinations explicitly rather than making order select the winner.
Read once before batch capture. Batch identity/resume continues to use the exact
transform hash; path spelling is not a new model identity. Source/model files
are not reread in workers. CLI has no --use-image-settings flag; its model inputs are numeric. The GUI
batch checkbox uses the current inspector settings, so imported models flow
through that existing authority. Do not introduce another precedence mechanism.

ImGui adds one model path and Load/Save actions beside stain controls. Load must
commit basis/white atomically only after successful decoding, retain the existing
full-basis mechanism, select stain analysis and invalidate pending analyses whose
model changed. Save uses the exact active full transform. Show an actionable
error and retain the old model on failure. Reuse existing image-source output
alias guards for GUI saves. Estimation, saved recheck and saved-image adoption
must continue to preserve their own exact bases; numeric edits retain existing
explicit behavior. No new per-project model table/schema is needed for this slice.

Settle option/report/replay fields and actual ownership before the first native
build, to avoid another umbrella-header recompilation at consumer closeout.
A named library/favorites/project registry remains on R7; a file loader does not
close that wider requirement or QuPath stain-profile interchange.

## Delivery and decisive checks

1. Native foundation: shared codec/load/save plus independent exact-basis/hash,
   existing-estimate import, signed third-vector/white, malformed/duplicate/shape/
   nonfinite/hash/schema/size errors, UTF-8 paths and failed atomic-write cases.
   Check real optional-capability branches. Reuse native transform arithmetic;
   no new biological cohort or estimator accuracy campaign.
2. Thin consumers: settle all Python/GUI/CLI/batch interfaces together; one cached
   bindings/stub pass. Check owning values after file/result removal, conflicting
   flags and actual GUI load/save/error/analysis. Match full transform/hash across
   SDK/CLI/GUI, retain/reopen/replay, and unchanged default numeric recipes.
3. Installed closeout: one settled wheel/install, source/member checks and the
   installed save -> load -> apply -> retain/export -> reopen route. Reuse all
   compatible native/kernel/typing evidence. No automatic full qualification.

After each bounded task or failed/stopped attempt, review actual waste and update
the current card before the next task/retry. Existing REGION1/GRID1 receipts and
all M0-M7/R2-R16 gaps retain their scope. Openly licensed dependencies only.

M1 public-layout decision: GuiOptions.stain_model_path initializes the existing
session-owned text-buffer pattern; GuiReport stain_model_state/error/sha256
report the most recent file operation, not a new model authority. Semantic Load/
Save targets append to GuiInputTarget for existing replay targeting. Known fields
are declared before native compilation. GUI async loading/saving reuses the
existing future/owned-result model-I/O pattern; changes commit on the owner only.

GUI save selection: serialize make_transform(true), the displayed supplied-stain
basis/white including any exact adopted third vector. OD feature selection uses
an identity separation basis internally and must not silently replace the saved
stain model. Loading selects stain analysis. Existing GUI batch image-settings
mode consumes that same current inspector transform. Most-recent file-operation
report fields do not replace current captured analysis recipes.

## SDK, CLI and GUI usage

```python
import qupathxx as q
model = q.load_stain_model("estimated-stains.json")
q.save_stain_model(model, "reusable-model.json")
request = q.AnalysisRequest.with_intensity_statistics(doc, object_id, model)
record = doc.keep_analysis(request, q.analyze(request))
```

CLI: `qupathxx_batch source.qpxproj output --kind stains --stain-model model.json`.
Explicit white/stains/normalized flags conflict in either order. Batch reuse
uses the captured model identity, not its path spelling. Managed viewer callers
load the same native value and pass it to existing analysis commands.

ImGui provides Model file, Load stain model and Save stain model controls. I/O
runs on native workers; adoption checks the current document/image/project and
model settings. Failed/stale loads preserve the prior model. Saves protect captured active
image/companion/project paths before writing and committing. Saved-analysis
recheck restores the complete basis for later model saves. Status/digest/errors
wrap within the inspector. GuiInputWait.STAIN_MODEL_FINISHED supports replay
without fixed I/O delays; it includes loaded, saved, failed and stale outcomes.

These are installed-verified routes within the recorded Windows scope. Wider named registry and QuPath profile-format
support remain separate gaps; no new project schema or estimator was introduced.
