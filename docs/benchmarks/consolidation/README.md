# Native consolidation evidence

Incremental development evidence, not release or full migration acceptance.
Selection/manifest checkpoint native source tree `d6a2699d4a9e439f2bde308e30d8bd5931382ef061437971c628fd4845c93ae9` has 536 raw compiler inputs, independently
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

## Shared native execution and legacy adapters

Run checkpoint raw source tree `329dd182fbcd08556607acb0a5240cdb59955f2683dc2c0c1b61d426abf6a242` has 540 inputs, independently
recomputed from [input hashes](native-run-source-inputs.sha256) and matched to
the [cached native build manifest](native-run-build-manifest.hpp). Built on
ca4033a8 with the changes recorded at this checkpoint; no binding rebuild or
installed-consumer qualification is implied.

One execution loop handles GUI-v1 admitted previews and project-v1 manifests.
[Private format adapters](../../../cpp/density_formats.cpp) preserve the two
receipt/report schemas, reuse tags, image states, fields and contextual CSV
quoting; native numeric CSV text is unchanged. Each route refuses the other's
checkpoint schema. Native run options carry recompute, stop-on-error, source/
checkpoint/CSV byte limits and optional per-image acknowledgement. Native jobs
own the manifest snapshot, worker, cancellation and joins; their owned JSON image
events are drained/acknowledged on the controlling thread, without callbacks or
Python references in workers. Setup input changes have a typed native exception.

[First combined pass](native-run-tests.log): all five density suites pass (3.35 s).
[Publication closeout](native-run-final-tests.log): desktop and resume pass; the
new manifest suite stops at a test configuration mistake. Adding a later-budget
case changed resume_from, but the nesting test still named the older directory.
The preserved fixture shows native correctly allowed a separate run. Correcting
that setup and adding call-site diagnostics gives a [targeted pass](native-run-repair-tests.log)
(2.51 s). No native assertion/guard was weakened or unaffected test campaign replayed.

[Manifest-run behavior cases](../../../cpp/tests/density_manifest_run_tests.cpp):

- Native result identities/retained records and exact contextual scientific CSV;
  matching JSON on disk and in the native report; no source/manifest mutation.
- Zero-kernel checkpoint reuse/chains without project copies, legacy receipt
  fields retained, missing final report tolerated, explicit cross-schema refusal.
- Source-record hints, stale/missing hints, changed engine/geometry, larger and
  smaller allowances and forced recomputation use the existing reuse authority.
- Per-image failures, stop-on-error/not-run counts, byte limits, complete-image
  CSV rollback with earlier rows retained and both checkpoints reusable.
- Owned events with acknowledgement, cancellation between images and resume;
  existing desktop tests retain active-kernel cancellation coverage.
- Source/manifest/checkpoint changes, SQLite sidecars, typed stale-manifest setup
  refusal, corrupt receipt/checksum, scratch and output/no-overwrite preservation.
- Missing linked images remain unopened; saved source locators remain linked;
  protected missing inputs beneath a proposed output prevent its creation.

Source/manifest checks are constant per image. Retained checkpoint/receipt guards
are checked at final verification and complete-publication boundaries, avoiding
quadratic rescanning of every prior checkpoint between images. Setup/final hashes
remain; final commits recheck metadata after staging. A failed-integrity run may
publish only its partial historical rows/error report. This is native-authority
completion of NC09-NC14; interface/limit settlement with the importer (NC04), thin
consumers, installed integration and the final boundary audit remain required.

## Native training selection and RGB PNG prerequisites

NC18: [training cohort](training-cohort-tests.log) passes (0.14 s). The
[new native capture factory](../../../cpp/training_cohort.cpp) selects the same
labelled area objects/order as the former Python comprehension, including Cells
and Detections. It iterates the object map rather than copying every annotation
and geometry. Existing RawPixelTiledTrainingRequest owns plane/class/source/
resource admission. [Focused cases](../../../cpp/tests/training_cohort_tests.cpp)
prove exact table/input hashes, features, labels, coordinates, owners and class
counts against explicit IDs; no pixels are read during capture. Empty/oversized
cohorts, invalid options and stale capture refuse natively. The initial compile
needed core.hpp in the implementation because the public request header correctly
forward-declares Document. The narrow repair reused completed objects; raw compiler
logs remain in main build/consolidation. Bindings/example wiring remain NC19/NC21.
This check predates the PNG addition; factory/test inputs are unchanged afterward.

NC16: [PNG and shared-input cases](nucls-png-tests.log) pass (2 suites,0.54 s).
The matching PNG build raw tree `be36ab7efd774459c988c0b0423fc456f8be6f738c29fef62aa93f8c10321afd` has 548 inputs, independently
matched from [hashes](nucls-png-source-inputs.sha256) to the [generated manifest](nucls-png-build-manifest.hpp).
CMake now inventories the vendored .h as an actual compiler input.

The [native decoder](../../../cpp/nucls_png.cpp) uses unmodified [pinned LodePNG](../../../cpp/vendor/lodepng/README.md)
under its retained Zlib license; the license is installed with application notices.
No platform codec, Pillow or NumPy performs product decoding. Encoder/disk/C++
convenience/ancillary metadata parsing are disabled. RGB8 bytes remain exact;
there is no gamma/ICC conversion. Palette, gray, RGBA and 16-bit sources refuse
without automatic conversion. This explicitly limits the native import's supported
pixel representation; it does not introduce a 16-bit image authority.

[Independent fixture encoder cases](../../../cpp/tests/nucls_png_tests.cpp) cover
all five PNG filters, split IDAT and Adam7, byte-for-byte RGB values, unsupported
modes/depth, unknown critical chunks, all chunk checksums (including ignored
ancillary metadata), truncated/chunk-length/deflate corruption, pixel/file limits,
cancellation and typed source mutation. Header/pixel admission precedes allocation;
32 MiB encoded,16M aggregate pixels,128 MiB per decoder allocation and bounded
inflate output apply. A shared bounded snapshot reader allocates the admitted
encoded size once and checks metadata before/after I/O. The existing manifest
suite still passes after this shared helper addition.

NuCLS pairing/CSV, geometry/accurate centroid interpretation and final atomic
project import are still NC15/NC17 work. No importer consumer, binding build,
new binary package, release or speed claim follows from these prerequisites.

## Native NuCLS pairing, interpretation and publication

NC15/NC17 native authority is implemented in [nucls_import](../../../cpp/nucls_import.cpp),
with focused [CSV](../../../cpp/nucls_csv.cpp) and [geometry](../../../cpp/nucls_geometry.cpp)
units over the existing RGB decoder, Document/Project and atomic save-new authority.
Native options/limits/reports/jobs own their data; no density-example import,
Python callback, per-pixel or per-object Python processing is required by this
service. Existing Python consumers are still unchanged until NC19-NC21.

Raw tree `ed383c0605068c1d50f2f951fda77e8f983688aa61bb3fe3b6bb55dae476d022` has 557 [compiler inputs](nucls-import-source-inputs.sha256),
independently matched to the [cached build manifest](nucls-import-build-manifest.hpp).
The final importer build uses this source. Earlier numeric/density/PNG results
are scoped below to unchanged inputs rather than retagged as a new binary build.

[Initial six-suite pass](nucls-import-tests.log),4.63 s: native import, numeric,
PNG and three density execution suites after sharing owned workflow events.
A missing ObjectProperties include stopped the first compile; the narrow repair
keeps public headers lean and reuses completed core/decoder objects. The original
compiler log remains in main build/consolidation/nucls-import-build.log.

[Import/numeric closeout](nucls-import-final-tests.log),0.49 s: bound directory
inventory at selection time, retain only requested stems and move point storage
into native geometry validation while moments retain original scalar inputs.
Crowded-directory selection and all existing binary64 oracles still pass.
Publication review then moves report serialization/admission before project
commit; [publication check](nucls-publication-tests.log) passes. A lowerable
native report-byte cap proves exhaustion leaves no project, without huge fixtures:
[final importer check](nucls-report-admission-tests.log),0.43 s.

[Importer behavior cases](../../../cpp/tests/nucls_import_tests.cpp) cover:

- Exact case-sensitive pairing/selection, missing pairs, subset/default limits,
  duplicate headers, wrong row width, Unicode class names, quoted/multiline CSV
  physical line numbers, finite coordinates, types and invalid polygons.
- Both representations save/reopen exact RGB/calibration, object IDs/Cell kinds,
  body/nucleus geometry, areas, source class metadata and declared centroids.
  Outside outlines refuse; valid centroid markers use the original midpoint and
  never silently clip the source geometry.
- Aggregate pixel/row/vertex limits, bounded50 retained diagnostics with full issue
  counts, unchanged inputs, existing/competing output preservation, source mutation,
  native cancellation and joins. No invalid/partial import publishes a project.

[Independent oracle](../../../tools/reference_nucls_numeric.py) and [frozen fixture](../../../tests/fixtures/nucls-import-v1/numeric.json)
are development-only; [hashes](nucls-numeric-oracle.sha256) authenticate them.
Regeneration is byte-identical. [Native numeric cases](../../../cpp/tests/nucls_numeric_tests.cpp)
match12 decimal/Unicode/underflow binary64 values,29 accurate-sum cases and32
local-frame polygons (orientation/closure and offsets through1e15) exactly.
Separate centroid moment terms retain floating-point error components and final
half-way rounding; strict FP/no contraction preserves the legacy scalar order.
No tolerance was widened, and no general GEOS centroid replaces that calculation.

A shared owned workflow event queue now serves density and import progress:
maximum1000 events and16 MiB queued JSON, optional acknowledgement, cancellation
and controller-side drain. Source guards hash at setup/final verification and
recheck metadata before atomic publication. Native reports are admitted before
commit; schema/event/native ownership and aggregate admission/terminal failure
mapping still need joint NC04 closeout before bindings. No binary release,
installed integration, broad parity campaign or speed claim follows.

## Settled native workflow contract

NC04 [contract](../../consolidation-native-api.md) is implemented for both services.
Raw tree `b48b777085b905800c9eef68e37ab334abbb494d75ecb374ed59da1597d1a4e5` has 558 [inputs](workflow-contract-source-inputs.sha256),
independently matched to the [cached build manifest](workflow-contract-build-manifest.hpp).
The final contract/import binaries use this source; prior narrower evidence is
retained for unchanged inputs, without another full campaign.

[Initial pass](workflow-contract-tests.log): seven suites pass and the new contract
suite fails only at its aggregate-budget fixture. The fixture's131072-byte property
value correctly hits the existing4096-byte ObjectProperties limit first. Preserve
that assertion; use300 valid4096-byte rows to exercise aggregate capture refusal.
The [repair and affected async routes](workflow-contract-repair-tests.log) pass
(3.30 s). Explicit movable captures also remove const-lambda deep-copy repeats.

[Path admission closeout](workflow-contract-final-tests.log),3.27 s, verifies
operational strings are refused before future copies. [Final catalog cancellation
check](workflow-contract-cancel-tests.log),1.13 s, adds a cooperative stop per entry
for potentially long missing-subset scans. No geometry/kernel assertions or
scientific tolerances were weakened; passing loader/generator/PNG/density evidence
was reused where its inputs were unchanged.

[Contract cases](../../../cpp/tests/workflow_contract_tests.cpp) prove owner-only
controls, cross-thread cancel and paused destruction/join, exact filesystem/output/
CSV/cancel exception types after worker completion, job reset, copied argument
lifetime and native manifest generation. Invalid/overlong requests do not launch
workers. Caller-control errors produce partial density reports. Valid metadata
exhausts a lower aggregate capture cap and publishes no import.

Native services now preserve exception_ptr for owner-thread rethrow. Source guards
and workflow checkpoints distinguish setup cancellation; controllers never borrow
Python state. Native NuCLS documents disable transient undo history, progressively
admit RGB/object/property payloads and retain Project.save_new as publication
admission authority. Density recipes/previews are charged before async copies;
128/256 MiB allowances retain the existing16 MiB manifest envelope. Manifest report
serialization/admission precedes optional commit, and generation has a native job
with the same progress/cancel/join ownership rules.

Remaining: focused bindings and Python GIL/type/immutable-result mappings, thin
examples/shared frontends and final installed evidence (NC19-NC28). No new package,
release, feature-parity or speed claim follows from native contract completion.
