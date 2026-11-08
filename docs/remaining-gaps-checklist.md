# Remaining gaps checklist

**G01-G05: 5/5 complete for the selected Windows research scope.**
Closed2026-11-08 with [candidate7](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.7).
This checklist owns the five acceptance targets; linked receipts own detailed
checks. Completion does not mean100% QuPath parity or universal biological,
hardware, format or platform qualification.

## Accepted scope and evidence

The owner's close-all request selected finite targets before execution in
[current-task.md](current-task.md): a matched ROI comparison; real coarse
whole-slide tissue plus dense/sustained resource checks; a separate second-slide
scientific panel; a matching Windows binary handoff; and the actual saved-data
compatibility failure. No panel cases were replaced after observing outputs.

| Done | ID | Closed target | Decisive evidence |
| --- | --- | --- | --- |
| [x] | G01 | Current-source selected-ROI comparison | Six alternating nonconcurrent pairs: native305.6ms /QuPath313.1ms, exact RGB/partitions for2,237 cells. Ranges overlap under load, so no general speed claim. [Comparison](benchmarks/current-source-speed/README.md). Candidate7 changes only private import code/tests/CMake source registration; all scientific authorities match the measured inputs. [Source delta](benchmarks/all-five-gaps/source-scope.json). |
| [x] | G02 | Representative whole-slide, dense and sustained resource acceptance | Canonical CMU-1 tile256/512 masks/labels/input hashes and independent threshold agree; cancellation and8 stale/cancel/publication controls pass. Ten-minute run:2,768 identical results,1.07MiB post-warmup private range within32MiB bound.100,001-object/600-frame viewer passes; actual process GPU counter peak87.88MB committed is recorded separately from native payload. [Resource evidence](benchmarks/all-five-gaps/README.md#g02-dense-and-sustained-whole-slide-resource-acceptance). |
| [x] | G03 | Independent second-slide scientific/feature panel | Frozen CC0 CMU-2 three-crop panel:2,258/2,266/2,208 cells (6,732 total), exact RGB/counts/canonical nucleus-body partitions/calibration versus QuPath.20,196 nucleus/cell/cytoplasm domains pass independent RGB/float32-OD means, pixel counts and calibrated sample areas at1e-10. [Plan/results](benchmarks/all-five-gaps/README.md#g03-independent-second-slide-scientific-panel). |
| [x] | G04 | Matching supported engineering binary handoff | Candidate7 wheel/native/source and open dependency/notices inventory published;14 asset sizes/SHA256 digests match before/after publication and tag points to0ff8c5c3.69 immutable wheel members and actual DLL origins verified; installed/standalone density/import/reuse and558-cell preservation pass. Uninstall preserves all33 generated files. [Publication](benchmarks/all-five-gaps/publication.json), [installation](benchmarks/all-five-gaps/installation.json), [removal](benchmarks/all-five-gaps/removal.json). |
| [x] | G05 | Retained QuPath0.7.0 saved-data compatibility failure | C++ vertex-touch decomposition preserves coordinates/nonzero-winding fill, reports2 normalizations and requires explicit loss admission. All558 cells/559 objects and22,880 bit-exact stored measurements, names/classes/parents/calibration and reopen agree with actual upstream data. Native crossing/spike/quota/cancel controls and5 importer regressions pass. Unnamed source objects stay unnamed. [Preservation](benchmarks/all-five-gaps/import-preservation.json), [contract](annotation-migration.md#saved-polygonroi-vertex-touches). |

Current native tree:
`b633f9924589299be1a4900f3f4afc37aa12d270251822f0bcaffe04c67869fc`
(**571 exact raw compiler inputs**). The public source tag targets
`0ff8c5c3257c9084a8f77cad0126f6104f6a5cb1`. Native embeds its historical
`f110c183+dirty` build label; exact raw source/runtime receipts are authoritative.
Candidate6 and earlier releases remain frozen. All38 costly binding objects were
reused byte-for-byte; no Python product/domain logic was introduced.

## Completed baseline to preserve

- [x] NC01-NC28 native application/workflow consolidation:28/28 complete.
  [Acceptance](consolidation-coverage.md), [boundary](consolidation-python-boundary.md).
- [x] Delivered density/neighborhoods/signed distances/project-density resume,
  native NuCLS import and training cohort controls stay delivered.
- [x] Earlier exact contour optimization and its observed11.2% local reduction
  retain their own source scope; do not add percentages across batches.
  [Contour evidence](benchmarks/contour-optimization/README.md).
- [x] Published candidate5/6 packaging retains its historical qualification;
  candidate7 owns the newer source handoff.

## Support boundaries and optional expansion

Owner planning update2026-11-08: the [eight-gap expansion checklist](feature-expansion-plan.md)
now owns22 pending tasks selected after the real fluorescence/omics demonstration.
This does not reopen G01-G05. The older optional categories below remain a
reference: ICC/cosmetic parity, external model ecosystems and distributed execution
are not automatically selected by the eight-gap plan. Proprietary and NC records
remain [separate from the open implementation](licensing/restricted-materials.md).

[Candidate7 limits](benchmarks/all-five-gaps/KNOWN-LIMITS.md) remain explicit:
local Windows x64/CPython3.12 engineering support, requiring MSVC x64 runtime and
OpenGL3.2. The selected whole-slide route is bounded coarse tissue, not level-zero
whole-slide cell streaming. Ten-minute stability and one host do not prove all
lifetimes/hardware/drivers. Exact second-slide comparisons do not prove
pathologist ground-truth biological accuracy. General speed superiority,
fresh-host/signing/stable release and Linux/macOS support are not claimed.
Unsupported arbitrary polygon crossings/spikes and exceeded limits still refuse;
only the reviewed vertex-touch representation is admitted through explicit losses.
These are declared boundaries, not unfinished checks for the five selected exits.

The following remain **optional, unselected categories**, not parity obligations:

| Selected | ID | Optional research need | Reuse first |
| --- | --- | --- | --- |
| [ ] | O01 | Advanced fluorescence/ICC/monitor-DPI/international text | Current channel/display/font/tile pipeline; preserve measurement pixels and overlay registration. R3. |
| [ ] | O02 | Additional models/schemas or Cellpose/StarDist/OMERO/ImageJ exchange | Native model/import/export services and one named feature/prediction or round-trip contract. R8/R15. |
| [ ] | O03 | Tiled spatial aggregation or additional TMA study workflows | Delivered spatial index/density/distances and TMA proposal/editor/batch APIs. R9/R10. |
| [ ] | O04 | General multi-stage dependencies/retries/pipeline replay | Current native requests/jobs/persistence; density resume/import are already delivered. R11. |
| [ ] | O05 | Distributed execution or broader extension/platform infrastructure | Select only for a measured workload/user need; reuse native jobs/archives and qualify the actual host/package. R14-R16. |

The [workflow inventory](feature-matrix.md) and R0-R16 retain finer possibilities.
Cosmetic equivalence, proprietary readers and100% feature parity remain outside
scope. Do not automatically reopen completed checks or start an optional category.

## Waste review and stop rule

Reuse passed kernel/route evidence at its actual source boundary. The new code is
private C++; unchanged scientific inputs are enumerated, not retimed repeatedly.
The first OD observer used the wrong per-pixel precision; its successful saved
archive supplied the corrected oracle without redetection or tolerance relaxation.
A strict name check found a real null-name bug, fixed natively. Final review
replaced cancellation-prone area orientation with a robust predicate; one local
preparation was superseded and preserved, never published.38 binding objects
survived all narrow corrections. Release lookup uses UTF8 and the existing draft
ID, with no duplicate upload. Raw failures remain local; detailed evidence is in
[the closeout](benchmarks/all-five-gaps/README.md).

**Stop this batch.** New work requires a concrete user workflow or demonstrated
blocker, one finite acceptance target and source-appropriate checks. Keep this
checklist and current-task updated rather than starting another ledger/campaign.
