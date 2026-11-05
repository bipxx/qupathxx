# R7 MODEL1 trace

2026-11-05, source32ec0f90, read-only trace; no build, runtime recapture, stub or
install. Inspected stain-estimation/transforms documentation, stains.hpp,
stain_estimation.hpp/.cpp and bindings/tests, ImageAnalysisSettings, batch CLI,
GUI make_transform and existing strict_json.hpp/CMake capability.

Existing estimator validation/export and test_model_identity_json_atomic_export_
and_exact_transform_reload cover manual Python JSON reload. GUI make_transform
already preserves an estimated/adopted full basis when its displayed first two
vectors and white still match. CLI accepts numeric white/stains/normalized.
No native model-file loader or GUI/CLI file selection/import route exists.

Selected outcome: native validated model-file reuse through those same native
transform/GUI/batch authorities. [Contract](../../stain-model-files.md) preserves
exact full-basis/hash/white, rejects corruption and retains optional interchange
build behavior. Model input can be an existing estimate export; it does not
reconstruct estimator results or adopt historical source provenance. A broader
named registry and QuPath profile formats remain R7 work.

Waste review: broad combined searches again hit long unrelated model/digest lines.
Subsequent inspection used discovered files and exact short source windows. Reused
existing scientific/export/settings tests rather than creating another estimator
campaign or model/result authority. Dependency inspection caught that JSON is
optional interchange, so the contract avoids an unconditional core dependency.
Pinned cached ColorDeconvolutionStains source was inspected only for context;
MODEL1 claims this project's own format, no new QuPath format/parity acceptance.
Next settle all consumer/public fields and implement one shared native codec.

## M1 native foundation — 2026-11-05

[Build](m1-build.log), [four focused suites](m1-tests.log) and
[semantic output](m1-test-details.log) pass. [Candidate identity](m1-candidate-identity.json)
records source32ec0f90 plus the uncommitted native patch, actual input/configuration
hashes and test product hashes. No MODEL1 bindings, wheel, installation or stub
pass has occurred. Preserved installed REGION1 remains at16470ac0.

Native stain_model_io.hpp/.cpp implement canonical model JSON, existing estimate-
file model extraction, exact normalized full signed basis/white/digest, strict
schema/numeric/duplicate validation,16MiB bounded file reads and existing atomic
writing. JSON parsing stays under optional interchange. Tests include the
[2289-byte prior-format export](../../../tests/fixtures/stain-model-v1/pre-loader-estimate.json)
from installed16470ac0, pinned by its [manifest](../../../tests/fixtures/stain-model-v1/manifest.json).
Its model hash matches exactly; no scientific estimator accuracy recapture.

Known GUI path/report/replay fields and heap text-buffer storage were settled
before this compile. They are declarations only; M2 still must wire their
behavior. Native tests cover exact doubles/signed third vector/white, old export,
invalid/duplicate/schema/hash/numeric/shape/size/file and failed-write boundaries.
A separate parser-disabled implementation translation unit verifies writers and
clear import capability errors without JSON includes. This is not evidence for
a full new minimal-platform package or host.

Waste review: no failed build/test or duplicate campaign. One guessed classifier
header was absent; discovered the real gui_object_classifier.hpp async-I/O owner.
Broad model/digest searches were narrowed to owning files and short windows.
Atomic UTF-8 writes and retained capacity lessons prevented another truncated
edit. Reused transform/estimator authorities, strict JSON, saved full-basis GUI
adoption and old estimate export rather than inventing a registry/result model.
Next M2 settles and wires native bindings, async GUI file operations and explicit
CLI conflicts before one combined binding/stub pass; M3 installation remains open.

## M2 staged consumer closeout — 2026-11-05

The [combined candidate](m2-build.log) compiled successfully, including native
guard tests, bindings, GUI, batch and app. [Two native model suites](m2-native-guards.log)
pass with a new precommit-guard failure assertion; older unchanged format/transform
checks retain their M1 scope. All known consumers were wired before the binding
build. One newly identified async completion wait marker was added before this
candidate; future contract/layout reviews must include readiness conditions.

Source review found open-project protection and saved-recheck full-basis adoption
gaps. Bundled those into [one incremental GUI repair](m2-gui-repair-build.log) after
the original live build finished. No binding source/interface recompile was needed.
Model workers own only strings/immutable native transforms; owner delivery checks
document/image/project and current model settings before adoption. Saves recheck
primary/companion/project aliases through the shared native atomic writer.

[First consumer run](m2-python-first.log):7 passed,2 observer failures. A C++ PPM
factory was assumed bound; the actual Python entry point is Project.add_image.
The project-window sidebar required a deeper scroll than a bare document; its
[position capture](m2-project-position.png) identified the correction. [Both corrected
cases](m2-observers-corrected.log) pass without another product build or repeating
seven valid passes. These cover active-image and open-project preservation.

The ten workflow cases across retained runs cover native JSON/file ownership and
old-export input, exact full basis/white/hash and saved replay, CLI matching/resume/
conflicting numeric flags, managed capture, GUI load/apply/keep/save/reopen,
failed-load preservation, OD-mode supplied-basis save, source/project aliases,
full-basis replay-to-save and [stale image-switch rejection](m2-stale-adoption.log).
The latter uses a valid bounded large estimate file to exercise pending async
parsing, switches images, observes stale rejection and verifies the new image's
prior model. This is one Windows replay, not a general timing/resource claim.
[Six existing analysis/estimation GUI cases](m2-existing-gui.log) also pass.

One [stub generation](m2-stubs.log), [valid typing](m2-typing-valid.log), exactly
[three intended negative typing errors](m2-typing-invalid.log) and
[14 public-stub/signature checks](m2-stub-checks.log) pass. Signatures were unchanged
by later GUI-only corrections, so these results are reused without regeneration.

The [actual loaded controls/digest](m2-controls-loaded.png) are readable, with
zero reported overflow. The [first save-error view](m2-save-error.png) exposed
fingerprint terminology from the shared alias guard. Translated that alias error
at the model UI boundary; [incremental build](m2-error-context-build.log) and
[two affected cases](m2-alias-context.log) pass. The
[corrected view](m2-save-error-corrected.png) is readable and its [proof](m2-error-control-proof.json)
confirms the project bytes are unchanged. Repeated capture initially tried a new
Project identity over the first retained project fixture; its native save guard
refused it before GUI execution. Used a distinct corrected fixture path instead.

[Final staged identity](m2-candidate-identity.json) matches actual compiler-input
bytes to the loaded source-tree hash and records shipping product hashes. It is
86fdf6b0 plus the consumer patch, distinct from installed REGION1. Source stubs
match the staged package. Earlier success-path/typing/control checks keep their
original candidate scope; final alias/error and stale checks use this final stage.
No MODEL1 wheel/install or full qualification campaign has occurred.

Waste review: retained the original live bindings build, reused unchanged
bindings/stubs/numerics for GUI repairs, and reran only affected failed/changed
observers. Addressed repeated API-exposure assumptions with actual Python exports;
used phase-specific capture paths and observed project scroll depth. Preserved
failure logs/images. [Visual cleanup](m2-visual-cleanup.json) proves decoded pixel
equality before removing duplicate temporary PPMs; PNG evidence remains. Atomic
UTF-8 edits and actual capacity checks remained in use. Next M3: one settled
wheel/install and exact installed user route/source/member checks. Named registry,
QuPath formats/science, other hosts/resources/release and all M0-M7 remain open.
