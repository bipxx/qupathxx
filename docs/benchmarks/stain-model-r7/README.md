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
