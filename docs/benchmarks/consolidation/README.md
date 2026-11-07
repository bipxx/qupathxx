# Native consolidation evidence

Incremental development evidence, not release or full migration acceptance.
Native source tree `d6a2699d4a9e439f2bde308e30d8bd5931382ef061437971c628fd4845c93ae9` has 536 raw compiler inputs, independently
recomputed from [input hashes](native-source-inputs.sha256) and matched against
the cached generated build manifest. Built on cb87a56e with local changes;
final installed consumer evidence remains required by NC25-NC28.

| Checklist | Evidence | Meaning and limit |
| --- | --- | --- |
| NC01 | [Contract](../../consolidation-contract.md), [baseline hashes](legacy-baseline.sha256) | Frozen legacy behavior/comparators; does not qualify changed source |
| NC02 (partial), NC03 | [Two native suites](native-extraction-tests.log) | Class and explicit-ID routes produce equal result identities; resume cases pass. Headless test links workflow + core and native dependencies, without Python, ImGui or OpenGL |
| NC05 | [Three native suites](workflow-guard-tests.log), [test source](../../../cpp/tests/density_manifest_tests.cpp) | Strict JSON/types/duplicates/limits, canonical paths, cancellation and source mutation pass; existing fingerprint/source-identity suites pass after shared metadata formatting refactor |

CMake target `qupathxx_density_workflow` owns the service and manifest loader.
`qupathxx_density_manifest_tests` consumes the public SDK header and links this
target without a UI/runtime language dependency. The generated Release link
references were inspected in the existing gridfrontends167-package cache.
GUI consumer source was adapted; its final build remains NC22/NC25 work.

Failures retained in main `build/consolidation`: extraction initially lacked an
explicit export_path import; manifest diagnostics exposed relative-path resolution
before joining the manifest directory and a persistent Windows decoder lock that
prevented mutation. Repairs reused completed objects. The final metadata snapshot
permits external edits and detects them; no assertion was weakened. Passing test
logs above are copied verbatim from that cache. No binding build, package,
release or speed claim follows from these checks.

## Native selection and generation checkpoint

[Final loader/generator checks](manifest-contract-tests.log): 2 suites pass,
0.46 s. Loader now checks the unchanged shipped example's exact normalized IDs,
options and relative source path, overflow, unknown fields, cohort caps and
invalid resource types. Generator proves exact case-sensitive/all-image and
explicit image selection, duplicate class normalization, legitimate zero Cells,
all missing-class issues, non-Cell refusal, calibration/curve admission, policy
roundtrip, source bytes unchanged, cancellation and save-new/scratch preservation.
These prove NC05-NC08 at the native authority; bindings/consumer compatibility
and final installed integration are still required, not implied by this result.

[First generation check](manifest-generation-tests.log) retained 3 passing legacy
suites and one failure: the test expected an invalid preview for an invalid
resource budget, but native options correctly rejected it at setup. Only the
test expectation changed; [targeted repair](manifest-generation-repair-tests.log)
passes. The later loader additions and canonical export-parent correction were
checked only in the two affected manifest suites. The shared selection/preview
implementation is the same one already checked by density_panel/density_resume;
no need to replay their asynchronous execution cases for serialization changes.

Preview and generation each fingerprint the source at setup/final verification,
with cheap metadata checks between images. A metadata-only guard permits external
mutation on Windows; decoder guards retain their original lock semantics. Final
native source-mutation/admission coverage and consumer observation replacement
remain NC23-NC24 work. Python's existing contextual CSV explicitly uses LF, not
CRLF; the compatibility note is corrected from source inspection before adapters.
