# R6-CONTEXT1: signed nominal support and retained replay

Locally verified at **faa71c1fa6eb9e5bb6eab1ce11cb8389497d47d7** on the reused
Windows Release/GEOS/Project/GUI/Python/OpenSlide/TIFF configuration. This closes
the named context-boundary/replay workflow, not full R6, biological accuracy,
whole-slide resources, other hosts or speed superiority. M0-M7 remain Partial.

The [installed baseline](baseline-white.json) at2892db74 finds one false cell on
opaque white8x8 data with ROI(0,0,8,8), explicit nominal extent(-4,0,12,8), step1.6,
context0/2. Nominal binary64 center eligibility treats a boundary point as valid;
the RGB fixed-point phase places it outside. Black opaque padding entered OD
analysis. Source alpha cannot identify padding; nonopaque source rejection stays.

The fix shares the RGB point/phase authority with cell support, without changing
RGB bytes, detector arithmetic, area grids or ROI geometry. A support-only phase
has no window scan; per-point queries reuse it. New requests yield zero cells and
20 eligible core samples; step.4/origin-1 yields zero/380, both context0/2. Fresh
nominal input identity is `/2`, with QCL9. Legacy `/1` requests keep their original
policy, hash, masks and replay. QCA9 origin kind5 retains corrected support per
origin; mixed old/new classifiers preserve their distinct sources. The owning
native manifest gains one flag; public Python signatures/stubs are unchanged.
See the [contract](../../cell-context-support.md) and
[retained legacy fixtures](../../../tests/fixtures/cell-context-legacy-v1/manifest.json).
Known-wrong historical results are compatibility evidence only.

| Boundary | Evidence |
| --- | --- |
| Native correctness/persistence | [Five final candidate roles](candidate-native.xml): resampling, cells, object_classifier, project_batch, project_save. Independent boundary bitmap/counts, no false white-data cells, old QCL6 replay, cross-policy verification refusal, QCL9 roundtrip and new QCA9 origin roundtrip pass; original RGB references remain intact |
| Native mixed-origin assertion | [Observer](mixed-observer.xml) checks legacy false/corrected true per row after QCA9 decode, plus hashes/canonical archive. Only nominal_cell_tests.cpp differs from the frozen installed source; separate provenance is in integrity.json |
| Staged SDK | [105 passes](staged.xml): corrected and legacy replay, new save/reopen, mixed old/new classifier origins, actual owning SDK flow and retained nominal/grid reference comparisons |
| Installed consumers | [85 passes/23 explicit desktop skips](installed.xml): CLI, batch/checkpoint/resume, retained records and unchanged public stubs. Skips are not desktop evidence |
| Selected actual ImGui flows | [Five passes](installed-gui.xml): both legacy rechecks, quantitative nominal detect/archive/publish/export at context0 and3/4, and desktop batch resume |
| Source/package | [Integrity](integrity.json):476 compiler inputs match runtime source hash;65 wheel/installed hashed members match RECORD. Stubs unchanged; no generation/repackage, one cached wheel and one install |

Retained failed attempts: [support-native.xml](support-native.xml) passed the new
primitive then hit an existing raw observer's mismatched ROI Z/T. Its authoring
setup now binds the requested plane and starts metadata counting after authoring;
[repair check](support-observer.xml) passes without weakening guards/pixel hashes.
[integration-build.log](integration-build.log) retains a misnamed final save target;
CMake's actual qupathxx_save_tests target was then built, without repeating the
successful core/cells/classifier/batch work. The later [mixed observer failure](mixed-observer-missing-id.xml) guessed record ID2 instead of the patterned manifest's ID3; the repair obtains the actual restored CellRecord ID. No product policy changed. Final candidate-native.xml is the
source-consistent result after all required relinks.

Waste review: **reuse** captured pixels, old byte/halo evidence and existing caches/
environments; **merge** codec/request/consumer changes before one wheel/install;
**remove** guessed targets, wildcard filenames and excessive document dumps by
using exact returned paths and bounded reads. Only a missing patterned legacy
archive was captured from installed2892, reusing existing pixels to exercise
mixed classifier origins. No second sampler, reader, cell model, general audit,
host campaign or duplicated numerical comparison per frontend.

Reproduction: the executed backend is retained as [text](executed-backend.py.txt);
it ran from root/build/ci/cell_context_r6_backend.py using root/.venv Python only
as build backend. Runtime tests use isolated project-metadata-r5 probe/installed
environments, never the editable root Python. Build/test commands and source
receipts are in the linked logs/XML. A later native mixed-origin observer adds the explicit per-row support-flag
assertion that Python cannot inspect. Its separate source scope is recorded in
integrity.json and mixed-observer.xml; only the cell test target is rebuilt.
Installed faa71c1f stays frozen, with no second wheel/install. Preserve failures
and earlier source scopes.

Next: R6-SAMPLING1, a named lossless pyramid/reader equivalence workflow using
existing adapters, nominal authority and source evidence. First inspect retained
coverage; select only an uncovered case. Broader context/vendor/feature/split-merge
and biological acceptance remain in R6; resource claims remain R12.
