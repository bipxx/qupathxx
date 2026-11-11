# Eight-gap native feature expansion plan

Owner request: plan all eight remaining areas, keeping proprietary and
noncommercial (NC) material separate. This document owns the new implementation
checklist: **22 work items: 22 complete, 0 pending**. It does not reopen completed
FO00–FO05. S01/V01/V02/I01/I02/R01/R02/F01/F02/F03/F04/D01/D02/O01/O02/A01/A02/A03/A04/P01/L01 are delivered under their
stated contracts. F01's train-selected recipe passes unchanged pilot gates and
native publication/replay; the failed original pilot is retained and validation
reuse is disclosed. F02 marker rules, F03 correction and F04 explicit linear
unmixing, D01 indexed expression storage/import/view and D02 bounded interaction/
tiled processing are delivered. O01/O02 adapters and A01 QC/normalization are
delivered; A02 summaries/pseudobulk, A03 PCA/clustering and A04 replicate DE are delivered through native services/CLI. P01 SDK/L01 membership and P02 matching native candidate8 publication are delivered. See the
[source-scoped progress receipt](benchmarks/feature-expansion/README.md).
Preserve independent desktop work recorded in [current-task.md](current-task.md).

The retained native, SDK, reader-plugin and format-catalog sources are now selected
for integration with the completed main UI. This source integration has its own
consumer evidence; published candidate8 and its original qualification stay
frozen. The [current task](current-task.md) records the active integration result.

Planning update: retain this single checklist for all eight requested areas;
do not restart delivered items. A04 closes with admitted independent mice and native Welch/BH. P01/L01/P02 close with the matching Windows native engineering release; no further phase is selected.
F02/F03/F04 native, CLI and desktop completion are recorded below. Proprietary and NC
material remain
separate excluded references, not optional product backends.

All application/scientific interpretation, transforms, statistics, storage and
export remain C++. Python is optional control only. RmlUi/ImGui present owned
native state and issue typed commands; neither UI contains a second algorithm.
Small cosmetic differences remain acceptable; result identity and interpretation
do not. No proprietary reader/model or NC dependency is introduced by this plan.

## Material separation

Use three distinct material categories, with an additional unreviewed state:

| Category | Location and handling | Included in open builds/releases? |
| --- | --- | --- |
| Admitted open code, formats, data and weights | Existing dependency/fixture inventories and source-scoped receipts; reusable licenses, exact versions/hashes and required notices | Yes, subject to the applicable distribution terms |
| Proprietary / permission-restricted | Metadata references in [the proprietary register](licensing/restricted-materials.md#proprietary-material); any later authorized local artifacts live outside the repository in a separate proprietary pool | No |
| Noncommercial / research-only | Separate [NC register](licensing/restricted-materials.md#noncommercial-material); any later authorized artifacts use a different external pool | No |
| Unreviewed terms or provenance | Metadata-only pending record; no automatic download/admission | No |

These pools are distinct: an example external layout is
`../qupathxx-restricted/proprietary/`, `../qupathxx-restricted/noncommercial/` and
`../qupathxx-restricted/unreviewed/`. Do not create/download them for this planning
task. The open product must not discover, link, copy, train on, benchmark against,
or package their assets implicitly. Separation is not authorization to implement
closed SDK support; the earlier open-format/open-reader policy remains in force.

Apply classification separately to **code, format/schema, codec, model weights,
training data, evaluation data and derived assets**. An open model format does not
license its weights; open software does not license its sample data. Crops,
converted images, screenshots and derived weights retain their provenance and
restrictions. Conversion to OME-TIFF/Matrix Market does not remove NC restrictions.
GPL/LGPL/AGPL are open licenses, not NC licenses; preserve their applicable terms.

Reuse the admitted OME tubhiswt and 10x mouse-brain CC BY4.0 examples and their
[existing receipts](benchmarks/real-data-demo/README.md). Vendor ownership alone
does not make those openly licensed exports proprietary. Private user-owned
research input stays local under its own permissions; users are not required to
publish their data. This separation controls project-supplied fixtures/models,
dependencies, evaluation claims and distribution, without adding a runtime upload
or licensing service for users' files.

L01 extends existing inventories with category, resource kind, exact source/hash,
license evidence, attribution and derivative/source IDs. Add offline staging checks
that every bundled asset/model/fixture has an admitted record and that no restricted
pool/path/reference is resolved into the build, source kit or package. Preserve
historical archives/receipts; classify unknown legacy assets before the next
release rather than deleting or relabeling old evidence. Restricted references
are not default demos, CI fixtures or acceptance evidence for the open product.

## Coverage and delivery order

| Missing area | Work items | First useful delivered behavior |
| --- | --- | --- |
| Omics visualization | S01, V01, V02 | Select a gene; show spatial expression, a meaningful legend and comparable scales |
| Guided import | I01, I02 | Preview an image/count/coordinate association and import it without a CLI |
| Spatial registration | R01, R02 | Explicit coordinate transforms and interactive landmark alignment |
| Fluorescence analysis | F01–F04 | Raw-channel segmentation, marker rules, illumination correction and linear unmixing |
| Large datasets | D01, D02 | Read full sparse matrices and large image regions with bounded work and responsive cancellation |
| Additional open formats | O01, O02 | Native H5AD expression and OME-Zarr image adapters for declared encoding subsets |
| Molecular analysis | A01–A04 | QC/normalization, regional summaries, PCA/clustering and a named DE method |
| SDK and distribution | P01, P02 | Relocatable CMake SDK, external C++ consumer and matching engineering release |

Suggested sequence, one active user outcome at a time:

1. L01 + S01 → V01/V02: establish admission/contracts and make existing real data useful.
2. I01/I02 → R01/R02: guided import and explicit alignment.
3. F01/F02 → F03/F04: fluorescence measurements become reproducible analysis workflows.
4. D01/D02 → O01/O02: scale the storage/source interfaces before adding readers.
5. A01/A02 → A03/A04: molecular methods on correctly identified full matrices.
6. P01 → P02: freeze interfaces, package once and qualify the stated release.

L01 need not block native scaffolding using already admitted fixtures. New
dependencies/materials cannot enter until their own admission is complete. SDK
layout can be designed early, but export/packaging happens after the APIs settle.
Do not build a general workflow engine or distributed service for these tasks.

## Implementation packets for the remaining work

This table makes the existing checklist executable; it is not another progress
ledger. Checkboxes below remain authoritative. Finish one packet before starting
the next, except that admission of a new input belongs to the packet consuming it.
Preserve concurrent desktop edits. R02 is delivered with native fit, actual UI,
registered-gene rendering and persistence checks. F01 is delivered under its
selected pilot contract; F02/F03/F04/D01/D02/O01/O02 consumers and A01/A02/A03/A04 and P01 are delivered; final L01 is next.

| Packet | Prerequisites | Easiest implementation path and consumer integration | Decisive completion check |
| --- | --- | --- | --- |
| L01: admission, completed incrementally before P02 | Existing inventories/guard | Annotate actual enabled dependencies, fixtures and derivatives in their current inventories; connect the offline guard to source-kit and package staging. Use metadata-only negative fixtures, without restricted downloads. Review license compatibility and notices as well as open status. | Every actual staged file has admitted provenance; proprietary, NC, unreviewed and restricted-ancestor records refuse staging. Final membership coverage closes L01. |
| R02: interactive alignment, delivered | Delivered R01/I02/V02 | Native landmark fit/report and atomic registration command; compiled private canvas controller using existing viewport/tile/undo services. Preview draft coordinates, accept one command, and consume the accepted transform in gene display. Retain source coordinates/counts and exact stored coefficients. | Analytical translation/similarity/affine and degenerate cases; drag, preview, accept, undo, save/reopen and stale refusal; real open demo with known landmark displacement. |
| F01: raw-channel segmentation | Existing fluorescence source and segmentation kernels | Capture channel/plane/ROI once; add only the raw-domain pipeline and missing compartment reducer. Reuse object publication and recipe jobs; expose a small tool panel/CLI recipe over that service. | Independent masks/intensities, invalid/empty compartments and a licensed annotated example with declared tolerances; no RGB-preview measurement. |
| F02: marker rules, delivered | F01 measurements | Add typed gates and a small AND/OR evaluator over immutable measurement snapshots. Reuse classification preview/publication and stored recipes rather than a new classifier framework. | Threshold edges, Unknown values, multichannel rules, stale refusal and exact saved-rule replay. |
| F03: correction and typed raw-source capability, delivered | Existing TIFF raw IO | Extract the minimum source capability needed for original/derived channel blocks; keep legacy TIFF identity unchanged. Implement explicit dark/flat correction as a separate derived source and recipe, shared by later readers. | Known correction fields, zero/invalid denominators, tiles/seams, cancellation and source/recipe persistence. |
| F04: linear unmixing, delivered | F03 blocks; admitted existing linear algebra backend | Feed labeled channel blocks to a small native solve; emit signed derived channels and residuals. UI selects an explicit matrix and channel order; no estimated spectra or model download. | Known mixtures, rank/conditioning refusal, residuals, channel permutations, stale/cancel and saved provenance. |
| D01: indexed sparse storage, delivered | Delivered S01/I01 | Add an ExpressionMatrixSource adapter and stream Matrix Market into a transactional SQLite sidecar with a gene-oriented sparse index. Extend the existing import wizard with backed storage after the native path works. | Import the existing full197MB source, switch to previously unprojected genes without rescanning/densifying, reopen/relink and refuse incomplete sidecars. |
| D02: bounded interaction and tiling, delivered | D01; F01/F03; existing jobs/cache | Page gene metadata and bound value/color caches first. Then schedule existing raw-cell processing by tile with fixed global parameters, halo, deterministic ownership and resumable records. Do not introduce another scheduler. | Declared sparse fixture/work bound plus a named-host cancellation/responsiveness check; independent seam comparison and interrupted/resumed tiled ROI equality. |
| O01: H5AD subset, delivered | D01/S01; exact HDF5 dependency admitted | Implement one native adapter over the HDF5 C API and feed the existing matrix source/import UI. Start with dense/CSR/CSC X or selected layer and explicit value domain; report unsupported encodings. | Independent dense/CSR/CSC malformed and valid fixtures plus one open real dataset with exact IDs/counts/axes; native-only runtime. |
| O02: local OME-Zarr subset, delivered | F03 capability; D02; exact codecs admitted | Implement declared NGFF0.4/Zarr2 metadata/chunk reads behind the existing image-source interface. Start uncompressed/zlib; add another codec only for a selected admitted fixture. | Chunk boundaries, axes, transforms, missing/failing IO and raw values; one complete open real store in viewport and measurement routes. |
| A01: QC and normalization, delivered | D01 full intended feature universe | Add streaming native totals/detection and an immutable normalized/log1p result source. Let gene display choose raw or explicitly named derived values; never replace the raw matrix. | Hand-calculated sparse values, zero libraries/missing data, exact raw preservation and persisted source/parameter identity. |
| A02: regions and pseudobulk, delivered | D01; R01/R02 when transformed coordinates selected | Reuse ROI cohorts and block reductions; add explicit coordinate-domain choice, overlap policy and sample/replicate metadata. Export exact sums and associations through existing native writers. | Analytical empty/overlapping cohorts, overflow, registered membership and exact totals/save/replay. |
| A03: PCA and clustering, delivered | A01; admitted native algebra backend | Add bounded block products and fixed-seed truncated PCA, then explicit-k k-means. Persist preprocessing/means/features/seed with owned result arrays; add one results view. | Independent small numeric oracle, deterministic seeded replay, work/cancel bounds and stored embedding/cluster association. |
| A04: replicate-level DE | A01/A02; admitted replicated dataset and numerical distribution dependency | Aggregate raw counts per biological replicate, normalize/log pseudobulk, then native Welch/effect/BH output. Reuse typed result export; single-mouse spots cannot satisfy the replicate contract. | Analytical/independent statistics, insufficient/degenerate groups and one openly licensed replicated dataset. If absent, keep the task open and continue independent work; do not substitute pseudo-replicates. |
| P01: native SDK | Shared interfaces settled | Export existing native component targets/headers and exact transitive dependencies through relocatable CMake config. Add thin nanobind exposure in one settled batch where needed; retain Python-free app/CLI/SDK routes. | Relocated installation consumed by a separate tiny C++ project with no Python or checkout access, matching headers/libraries/notices. |
| P02: one release handoff | All feature checks and final L01/P01 | Freeze source, stage matching Windows app/CLI/SDK and optional Python once, apply admission/hash checks and existing install/use/uninstall tools. Publish only actual qualified support. | Frozen source/package identities, no restricted inputs/lineage, clean install/use/removal preserving data, matching native/optional-Python results and notices. |

Before each packet, record changed inputs and one decisive check in the existing
current-task card. Settle the native request/result/persistence contract, implement
the native service, then wire thin consumers. Extend an existing request when its
meaning is unchanged; version persisted interpretation when it changes. Shared
jobs own snapshots, cancellation and stale-result refusal. Do not add per-object
Python calls or reimplement validation in the UI.

Each completion updates the canonical checkbox and the single evidence receipt,
then records actual waste/correction and the next packet in current-task.md.
Use affected cached builds/tests; defer packaging and broad qualification to P02.
An unavailable external fixture blocks its acceptance claim, not unrelated work.
No calendar or speed-superiority estimate is asserted before measured work.

## Next execution slices and shared API boundaries

D01 is delivered: bounded indexed storage, native project references/relink/owned
copies, explicit spatial joins and actual desktop index/import/paged gene display.
Reuse its source-scoped real-data and legacy-route evidence. D02 is now delivered under the checks below. These retained steps explain its
source-scoped evidence; next execution is final L01, without replaying D02/O01/O02/A01/A02/A03/A04/P01.

1. Inspect existing value/color/tile caches and generation-driven controllers.
   Reuse the opened matrix and current selected-value snapshot for display-only
   changes; bound retained pages/values/colors and coalesce obsolete viewport work.
   Selected-value reuse is implemented and verified: palette/opacity/transform
   edits share the immutable exact-count buffer; changed selected genes or spatial/
   runtime sources invalidate it. Source currentness remains checked before work
   and publication. Shared comparisons now prepare each transformed field once,
   resolve the pooled range once and paint once; no unused per-gene percentile
   sort. Radius-only changes reuse colors without a native job. Five affected
   suites pass with final-field identity/color/clipping equivalence. Existing
   per-viewer tile services already cap decoded/GPU caches, active work, requests
   and viewport plans and coalesce/reject obsolete generations; reuse them.
2. Generate one native100000-observation/30000-feature/5000000-entry sparse fixture
   covering the intended axes. Declare256MiB accounted work before running it;
   qualify indexed reads, cache invalidation and cancellation on a named host.
   Keep allocator/process/GPU observations separate from accounted work.
   Native fixture now passes on HEQPC:135276000-byte accounted import peak and
   194971982-byte conservative retained display bound; exact selected counts,
   recolor reuse and active-read/latest-generation cancellation pass. Reuse
   build/feature-expansion/d02-scale/counts.qmx rather than regenerate. The measured
   1855ms coordinate join exposed a desktop-owner stall. It now resolves immutable
   spatial requests in the existing GeneViewSession worker; desktop budget is256MiB
   with retained request storage reserved. Reopen of the same fixture passes with
   6.926ms immutable request creation,0.083ms dispatch,2025ms worker join/comparison
   and202073662-byte conservative retention. All five affected native/GUI suites
   pass. Native dispatch timing does not prove full100k desktop frame responsiveness.
   Continue tiled-cell work. This qualifies the declared native interaction route
   and bounded services, not total process/GPU memory or100k desktop FPS.
3. Extend existing raw-cell jobs with bounded tiles, one captured global threshold/
   normalization, declared halo, deterministic seam ownership/global IDs and source-
   bound checkpoint/resume. Detect unresolved boundary objects explicitly.
   Existing fluorescence context is smoothing-only: process an expanded
   segmentation domain, then assign complete cells to tile cores. Propagate
   boundary dependence from thresholded connected components before watershed
   filtering so an interior child of a cut component cannot be silently accepted.
   Keep the existing untiled run identity/recipe contract intact. Resolve global
   Otsu from the declared whole ROI rather than recomputing thresholds per tile.
   Bounded native tile kernel is delivered: expanded-domain segmentation, half-
   open anchor ownership, source/plane/recipe-bound global IDs, global nucleus/body
   RLE and exact raw moments. Cut-parent dependence is propagated before watershed/
   area filtering; unresolved pixels/owned labels prevent complete status. Three
   synthetic layouts/reversed order match untiled geometry/moments exactly; all
   four affected native suites pass. Source-backed original-channel orchestration
   now captures source/ROI/calibration once, lazily reads halos and streams bounded
   tile results through the existing cell job. Global Otsu reuses untiled smoothing/
   256-bin/lowest-tie semantics with unique-core counting and per-tile raw hash
   agreement across passes; constants skip the histogram pass. Native tests match
   untiled threshold/geometry/moments across two layouts and cover mutation/cancel/
   stale/constant behavior. Native checkpoint/resume is delivered: atomic bounded
   threshold/tile/completion records, exact source/options/raw-input verification,
   skipped completed segmentation and byte-identical interrupted/resumed masks/
   IDs/raw moments. Quotas include orphan staging; startup scans existing members
   once. Raw source profile/2 excludes TIFF display-view descriptors and survives
   fresh-reader reopen. Corrected/unmixed adapters now use retained native channel
   sources, fixed normalization/model and explicit measurement/parent identities;
   derived-read peaks include checkpoint reservations. Corrected, unmixed and
   corrected-unmixed tests match untiled masks/Otsu/all raw moments and independent
   subtraction/diagonal-mixing equations; interrupted/resumed records are byte-
   identical and fresh recipe replay reuses checkpoints. All five affected native
   suites pass. Desktop tiled Run/resume, folder/halo/storage controls and visible
   native statuses now pass actual actions; correction/unmix GUI routes stay passing.
   Real CC0 BBBC039 field:104 certified candidates match independent whole-field
   geometry/moments across two layouts and interruption/resume;26 candidates stay
   boundary/dependency-uncertified. Complete and explicitly partial tile records
   preserve all certified output; partial records are reprocessed, never reused as
   complete. Same-layout persisted records/digest agree byte-for-byte. This is a
   696x520 field; the separate actual whole-slide ROI exit below now passes.
4. Admitted CC-BY4 OME LuCa-7color_Scan1.ome.tiff (24960x34560x5,
   2085508976 bytes) uses existing native BigTIFF/LZW. Level-zero ROI
   (12000,16000,768,512), DAPI segmentation/all5-channel measurement, has1374
   certified cells of1527 reference labels;153 boundary-dependent labels remain
   explicitly uncertified. All certified masks/moments/IDs match the independent
   untiled reference across24/20-tile layouts; interrupted/resumed complete and
   partial records match byte-for-byte,4 reused/20 segmented. Maximum accounted
   work240760252 bytes <256MiB; tile-boundary cancellation unwinds0.4172ms on
   HEQPC. Preserve scope: no full-slide throughput, RSS/GPU or biology claim.
5. D02 closes with this WSI/scale evidence and delivered thin desktop controls.
   O01/O02 are also delivered. Follow remaining methods/SDK/release packets:18/22 complete.
   Optional bindings/installable SDK remain P01; package once at P02.

Delivered F01-F04 instructions are retained through their checklist progress and
source-scoped receipt; they are not execution steps to repeat. The remaining
remaining-item breakdown below owns subsequent order and completion boundaries.

Use these API boundaries for every remaining packet:

| Boundary | Minimal shared responsibility | Thin consumer responsibility |
| --- | --- | --- |
| Request/capture | Validate typed options, domains, IDs, source/calibration and work limits; freeze inputs before work | CLI/UI/nanobind construct options and show native validation errors |
| Execution/job | Native algorithm, owned buffers, progress, cancellation and bounded work; reuse existing jobs | Submit, poll, cancel and render reports; no per-pixel/per-object Python loops |
| Result/preview | Immutable values, validity, units, associations and result/source identity | Present the native values and meaningful legends without recomputing science |
| Publication | One native authority checks currentness and atomically publishes or refuses | Invoke Keep/Accept and expose Undo; never create partial result objects in a frontend loop |
| Persistence/replay/export | Version interpretation; retain recipe/source IDs and exact result identity; use existing project/store/writers | Select destination or saved record; display missing-source/relink errors |

Expose optional nanobind controls in one settled API batch before P01/P02;
standalone native UI/CLI remain usable before that batch. No external credential,
cloud account or API key is required for these local workflows. Reader subset and
statistical method limits remain those explicitly listed below.

## Separation acceptance throughout implementation

The existing proprietary, NC and unreviewed register sections remain distinct
metadata records. Actual restricted artifacts, if ever separately authorized,
must use their corresponding external pools; none is downloaded or integrated
by this roadmap. There is no proprietary/NC build option or default discovery path.

At each dependency/fixture addition, record resource kind, exact revision/hash,
license evidence, attribution and complete source/derivative lineage. Review code,
format, codecs, weights and data separately. Open status alone does not establish
compatibility with the project's AGPL-3.0-only distribution or discharge notices.
Keep user-private research inputs outside public fixtures, logs and packages.

L01/P02 must test both source-kit and binary/package membership, including model
assets, screenshots and generated derivatives. Negative tests use synthetic bytes
and restricted metadata to prove exclusion; restricted data are not needed as test
inputs. A restricted or unresolved source ancestor excludes its derivative from
the open staging path. Converting a file or wrapping a decoder does not change
admission. If no admissible evaluation data exist, retain analytical evidence and
report the missing real-data acceptance rather than borrowing an NC fixture.

## Checklist: shared services and useful visualization

- [x] **L01 — material inventory and separation guard.** Extend the existing
  inventories instead of creating another dependency database. Keep proprietary
  and NC records physically distinct. Check source/package asset membership,
  derivative lineage and notices offline. Exit: admitted open examples pass;
  restricted and unreviewed fixture/model records refuse staging. No restricted
  binaries or datasets need downloading to test the guard.
  Delivered locally: exact finite native/SDK/source-kit membership, fifteen
  reviewed dependency source artifacts, separate embedded-component/font notices,
  strict path/hash archive checks and guarded native staging/source collection.
  Restricted ancestry, unknown assets, unsafe members, carrier fallback and stale
  source identities refuse.25 focused tests pass (one host-link privilege skip);
  the actual native/SDK/source/dependency archives and source configure checks pass.
  [Guide](native-release-materials.md),
  [receipt](benchmarks/feature-expansion/README.md#l01-final-native-material-membership).
  Historical unknown assets remain excluded; P02 deployment/publication has its own receipt below.
- [x] **S01 — expression dataset/value contract.** Add a small native
  `ExpressionDataset`/`FeatureSelection` interface with stable observation/feature
  IDs, count versus continuous-value domain, validity, source identity, plane and
  coordinate-frame reference. Start with an adapter over existing imported
  ObjectProperties; retain old projects and the current CLI. Zero is distinct from
  a missing/unimported gene. Keep raw counts integer-exact; legacy double-valued
  projections retain their2^53 admission limit. No new full-matrix schema yet.
- [x] **V01 — native gene display field.** `GeneDisplayRequest` captures a dataset,
  gene, cohort and linear/log1p display policy; an owned `GeneDisplayField` supplies
  values/validity/colors to overlays. Native min/max or percentile range, explicit
  zero/missing colors, opacity, legend units and optional shared range for gene
  comparisons. Cache by dataset/gene/cohort/display identity; cancel stale work.
  Display transformations never rewrite counts or masquerade as normalization.
- [x] **V02 — gene selector, heatmaps and comparison UI.** After the current UI
  closeout, reuse the RmlUi shell's typed-command bridge and existing inspectors.
  Search/paginate gene IDs and symbols, render position-based expression overlays,
  show the exact range/transform/legend and support synchronized two-gene views.
  Label spots versus cells accurately and persist presentation choices. Exit:
  real mouse-brain markers change the correct point colors/legend; missing genes,
  constant/all-zero values and rapid switching behave correctly; raw/project
  measurements remain unchanged by navigation/display.

## Checklist: import and registration

- [x] **I01 — shared native import preview/job.** Wrap current sparse parsing and
  project publication in `SpatialImportPreview` and an owned cancellable job;
  CLI and UI call the same services. Preview IDs/dimensions/genes, source hashes,
  coordinate frame, counts domain, selected projection and work/storage limits.
  Extend only actual missing header/column mappings through explicit options.
  Reject ambiguous IDs/units, missing pairs and unsafe output collisions before
  publication. Retain the current strict profile and no-overwrite behavior.
- [x] **I02 — guided import UI.** Steps: choose image/files → map IDs/axes/units →
  select genes or backed storage → inspect overlay/limits → import a new project.
  Preview does not mutate the current study; progress/cancel/errors come from the
  native job. Save the explicit import recipe and source identities. Exit: complete
  the real open demo without shell commands; cancel/error leaves prior state and
  output files intact. Do not parse matrices in RML/JavaScript/Python.
- [x] **R01 — native coordinate-frame and affine contract.** Add versioned
  `CoordinateFrame`, source→image-level-zero `Affine2D` and `RegistrationRequest`.
  Reuse native geometry/calibration. Store original coordinates, units, image/plane
  identity and transform separately; apply scale/translation/rotation/reflection
  only through explicit matrices. Unknown calibration prevents a micrometre/pixel
  conversion. Test analytical transforms, inverse/round-trip and stale source checks.
- [x] **R02 — interactive landmark registration.** Reuse canvas point editing,
  overlays and undo. Fit a translation/similarity/affine transform from sufficient
  nondegenerate landmarks using native least squares; show residuals and held-out
  error in declared units. Preview, accept, undo and save/reopen; do not rewrite raw
  coordinates or gene counts. Exit: recover a known transform and align the real
  demo without swapping axes implicitly. Nonlinear deformation and unrestricted
  automatic registration are not required for this first registration contract.
  Delivered: native normalized fitting/residuals, compiled canvas tools, atomic
  accept/undo and exact saved-evidence reload. Accepted transforms place native
  gene overlays; generic object geometry remains explicitly unchanged. Analytical
  and actual GUI checks pass, including a controlled shift of all2,702 real spots.
  [Evidence](benchmarks/feature-expansion/README.md).

## Execution breakdown for the remaining 10 items

This breakdown refines the existing checklist; it adds no new completion count.
Use the smallest end-to-end native slice first, then extend it to the declared
contract. Do not restart the twelve completed items. No calendar or speed estimate
is promised before measuring the affected workload.

| Item | Ordered implementation steps | Deliverable and boundary |
| --- | --- | --- |
| D02 | 1. Page metadata and bound decoded/value/color caches. 2. Coalesce viewport work using existing jobs/generations. 3. Add tiled cell requests with fixed global parameters, sufficient halo and deterministic seam ownership. 4. Persist checkpoints and resume only matching inputs/options. | Keep interactive matrix and tiled-image checks separate. Prove work limits and seam/resume equality; report unresolved boundary objects. No new scheduler. |
| O01 | 1. Admit one exact HDF5 configuration and notices. 2. Implement bounded encoding/shape/string/sparse validation. 3. Adapt dense/CSR/CSC reads to D01 with an explicit counts or continuous domain. 4. Expose the declared subset through existing import controls. | Native H5AD subset, unsupported metadata report and exact independent/real-fixture comparison. No Python reader or automatic filter-plugin loading. |
| O02 | 1. Admit the chosen complete open store and only required codecs. 2. Parse declared NGFF0.4/Zarr2 axes/transforms/dtypes/chunk metadata. 3. Implement bounded chunk decoding behind the existing raw-source interface. 4. Connect viewport and quantitative reads. | Local declared subset only; distinguish valid fill/missing chunks from IO/decode failures. Preserve TIFF identities and exclude unsupported geometry rather than guessing. |
| A01 | 1. Stream totals/detected features over the full declared feature universe. 2. Add immutable library-normalized and log1p sources with explicit zero/missing semantics. 3. Persist parameters/source identity. 4. Let display/export select the named domain. | Raw counts remain exact and unchanged; analytical values and native replay establish completion. |
| A02 | 1. Capture ROI cohorts with explicit coordinate frame and overlap policy. 2. Reduce sparse blocks with checked totals/counts. 3. Add explicit sample/replicate groups and raw pseudobulk aggregation. 4. Save/export/replay cohort and result associations. | Exact regional and replicate totals, including empty/overlapping cohorts; registration changes membership only when explicitly selected. |
| A03 | 1. Implement bounded block products and recorded feature means. 2. Add fixed-seed truncated PCA with implicit centering. 3. Add bounded explicit-k k-means with job-local randomness. 4. Persist embedding, preprocessing, seed and cluster associations; connect one result view. | Independently checked numerical results and repeatable named-fixture replay. Record tolerance and component sign/subspace convention; do not require bit identity across unspecified backends. |
| A04 | 1. Admit a replicated open fixture and exact native distribution dependency. 2. Validate independent replicate groups. 3. Aggregate raw counts, normalize/log replicate libraries, then compute the named Welch/effect/BH outputs. 4. Save/export method, groups and correction universe. | Do not substitute spots for biological replicates. If fixture admission is unresolved, keep A04 open while completing independent SDK work. |
| P01 | 1. Audit actual public targets and transitive requirements. 2. Export/install component targets, headers, feature/version metadata and notices. 3. Remove build-tree paths. 4. Relocate the install and build/run a tiny external C++ consumer. | Python-free SDK on the stated compiler/CRT/ABI; optional nanobind interfaces use the same services and are batched after APIs settle. No extra engine cache. |
| L01 | At each packet, admit only its actual inputs and derivative lineage in existing inventories. Before release, enumerate all staged source/binary/data/model/generated assets, connect the offline guard and resolve every unmatched member. | Incremental prerequisite for new inputs; final closure requires actual complete stage membership. An allowlist of license names alone is insufficient. |
| P02 | 1. Freeze completed source/features and close final L01 membership. 2. Stage matching app/CLI/SDK and selected optional Python artifacts once. 3. Verify hashes, sources/notices and clean install/use/uninstall. 4. Publish the exact qualified platform/support statement. | One engineering release handoff after P01 and feature acceptance; preserve user data and old receipts. No implied signing, stable status or untested platform support. |

All22 selected contracts are closed. No further execution phase is selected.
Reuse delivered A01/A02/A03/A04 and the O01/O02 adapters and their source-scoped
evidence. A04 uses an admitted original-author replicated Kidney dataset; the
single-mouse spatial demo remains outside DE acceptance.

For every item: record changed inputs and the decisive check in current-task,
settle native types and persistence before consumer edits, run affected checks
once, then update this checkbox and the shared evidence receipt. Review actual
waste in the same card: repeated reads/solves, per-frame parsing, copies, cache
invalidation, duplicated frontend logic and unnecessary build scope. Fix concrete
waste at its owning boundary; do not open another audit campaign or repeat passing
biological/whole-platform validation without a changed risk.

### Material handling at each implementation step

Before a packet consumes a new dependency, format, codec, dataset or model, record
its exact identity, evidence of terms, intended use, notices and parent/derivative
IDs. Reuse admitted inputs where possible. Keep the three excluded categories
physically distinct if later authorized external copies exist:

- Proprietary: metadata in the proprietary register; external proprietary pool.
- NC/research-only: metadata in the NC register; external noncommercial pool.
- Unknown terms: metadata in the unreviewed register; external unreviewed pool.

None of these pools enters build search paths, automatic downloads, fixtures,
training, open-product performance/accuracy acceptance, screenshots or packages.
Derived crops/conversions/weights retain their parents' restrictions. Rejection
checks use synthetic records, not restricted downloads. Do not create external
pools or move private/historical assets merely to satisfy this plan; preserve them
and classify actual release inputs under L01. See the existing
[separation register](licensing/restricted-materials.md) for handling details.

## Checklist: fluorescence analysis

- [x] **F01 — raw-channel nucleus/cell workflow.** Use selected original channel,
  series/Z/T and ROI. Reuse existing native Gaussian/mask/morphology/distance/
  watershed/contour kernels where their contracts apply; do not route fluorescence
  through RGB optical density. Start with explicit threshold or native Otsu,
  bounded nucleus watershed and optional calibrated cell expansion. Preserve
  finite/invalid masks, physical units, context/tile seams and work limits. Keep
  raw body/nucleus/cytoplasm domains explicit; add a missing cytoplasm reducer
  over body-minus-all-nuclei masks instead of measuring RGB preview values. Empty
  compartments have absent measurements, not invented zero intensity. Keep
  recipe/record identity and publish through existing native object commands.
  Exit: independent tiny masks plus an admitted annotated fluorescence panel with
  declared split/merge/measurement tolerances; no blanket biological-accuracy claim.
  Progress: the native raw-raster service and arbitrary-channel compartment
  reducer pass independent threshold/watershed/expansion/moment tests, including
  physical units and boundary flags. Source-bound requests/runs/jobs now capture
  explicit channel/Z/T/ROI/calibration, reserve work before decoding, read once,
  refuse stale inputs and test finite Gaussian context agreement. Native typed
  contours, atomic cell/fact publication and exact recipe/result-identity records
  (including zero-cell outcomes) now pass undo, replay and project save/reopen.
  Native CLI and desktop run/preview/compartment inspection, cell/record keep,
  remove and exact replay are implemented and pass focused consumer checks.
  The real CC0 panel delivers/publishes/replays130 labels through the UI.
  Finite same-core context/ROI/source-edge checks now preserve body/nucleus masks,
  moments, boundary flags and absent cytoplasm. Whole-slide seam/resume remains D02.
  The original predeclared BBBC039 pilot fails F1=0.683; retain that report.
  A separately preselected CC0 training panel and frozen nine-recipe selection
  choose sigma2px/prominence0.5px, keeping Otsu/min-area5/expansion3/pixel units.
  Freeze728aae2a precedes follow-up validation: F1=0.7193, Dice0.9225, split17/110,
  merge4/118 and matched mean relative-error median0.03056 pass unchanged gates;
  every original-sample moment passes the independent1e-12 oracle. This is reused
  single-panel pilot evidence, not unseen-held-out or population accuracy.
  All118 selected cells trace/publish/batch-verify and survive native record/
  object save/reopen. The actual CLI gives the exact selected run hash and replay.
  Desktop generic option/job/Keep behavior reuses its passing source-scoped checks.
  No default, gate, scientific kernel, NC/proprietary material or model changed.
  [Evidence and limitations](benchmarks/feature-expansion/README.md).
- [x] **F02 — marker positivity and phenotype rules.** Reuse per-compartment raw
  measurements and native classification publication. Versioned per-marker gates
  and AND/OR rules include channel/compartment, threshold, units and measurement
  domain. Missing values are Unknown, not Negative. Show counts/cohorts, preview
  assignments and retain/replay the exact rule. Exit: threshold-boundary,
  multi-marker, missing-data, stale-input and save/reopen cases pass.
  Progress: native typed gates and bounded topological AND/OR rules, scalar-only
  snapshots, flat gate states/counts/cohorts, exact typed rule storage and existing
  compound classification publication pass focused checks. Unknown stays distinct
  from Negative; no pixels are read. Geometry/raw facts remain intact and saved
  rules/observations replay exactly after project reopen. Strict native JSON
  IO/reporting, native cohort selection and CLI preview/apply/list now pass,
  including exact saved replay on the admitted118-cell project. Private native
  desktop controls now load/edit rules, preview root/gate counts and assignments,
  apply/Undo, retain/load recipes and refuse stale captures. Focused native GUI
  checks and real118-cell save/reopen replay pass, with original inputs unchanged.
  Standalone Windows-only-PATH smoke passes; Python remains optional. This proves
  literal rule workflow behavior, not biological phenotype accuracy.
- [x] **F03 — dark/flat-field correction.** Add an immutable native derived-channel
  recipe using explicit dark/flat sources and declared normalization. Reuse typed
  tile IO and masks; first put TIFF-specific raw-source checks behind a shared
  typed capability reused later by O02, preserving existing TIFF identities.
  Validate compatible axes/calibration, zero denominators and
  nonfinites. Version the corrected domain/source provenance instead of changing
  original samples or reusing a raw record's identity. Raw measurements remain the
  default unless a derived domain is explicitly selected. Test known fields,
  seams, cancellation and persistence of recipe/source associations.
  Progress: additive native ChannelSource adapter and immutable dark/flat block
  source/kernel pass analytical values, signed/zero/missing/nonfinite semantics,
  original channel/Z/T selection, exact tiled seams, pre-read work admission,
  integer precision refusal and file/metadata/cancel guards. Sources have distinct
  original/corrected domains; no legacy TIFF/F01 identity code changed. Initial
  correction subset is level zero with identical spatial/C/Z/T axes, channel names
  and calibration, explicit positive per-channel normalization and no broadcasting.
  Typed retained recipes/source associations, owner capture and native block reports
  now support Keep/Remove Undo and exact project/source/block save/reopen replay.
  Native CLI Keep/list/verify/preview pass source-byte, protected-path, cancellation
  and save-budget checks. Initial saved-source reopening is native TIFF only;
  missing/changed fields refuse, with no implicit relink. Explicit corrected cell
  analysis now reuses the fluorescence kernel, contours, jobs and atomic publication;
  version2 recipes/profiles identify corrected values and reference retained field
  associations. Native/CLI saved replay and domain-specific marker rules pass.
  Referenced correction recipes cannot be removed. Original F01 real run retains
  its exact digest and118 cells. Desktop field load/preview, recipe Undo/Redo and
  saved load, explicit corrected/original selection, negative cell measurements,
  publication Undo/Redo, exact project replay and corrected marker actions pass.
  Native opening/reading is asynchronous and cancelled/stale/replaced inputs refuse.
  Standalone Windows-only-PATH handoff preserves project/source hashes. F03 closes
  under this declared subset; no automatic field estimation or biological claim.
- [x] **F04 — linear compensation/spectral unmixing.** Consume an explicitly
  supplied, labeled finite mixing matrix and native channel blocks. Start with
  small full-rank linear systems and residual reporting; reuse the existing
  OpenCV core SVD/solve backend behind a native optional processing capability.
  Pin/license its exact dependency configuration; no Python/model download.
  Refuse ill-conditioned or incompatible matrices, preserve signed results unless
  an explicit rule says otherwise, and never silently clamp. Save matrix/order/
  domain/algorithm provenance. Exit: analytical mixtures, noisy residuals, axis
  permutations and stale/cancel cases pass. Nonlinear/learned unmixing is separate.
  Native foundation: explicit matrix/model and component/residual ChannelSource
  are implemented. A prepared thin SVD is reused across blocks; adjacent inputs
  read directly and sparse groups skip gaps. Requested outputs alone allocate
  rasters. Strict typed matrix records preserve exact coefficients/order/domain
  and refuse malformed schema/types. Native retained original/corrected source
  recipes now pass exact project/operator replay, Undo, correction-parent reference
  protection and stale/cancel checks. Bounded strict native matrix JSON/file IO
  and CLI Keep/list/verify now pass, including standalone original/corrected replay
  and new-project save with input hashes unchanged. Native cell/marker integration
  now passes output-axis capture, signed compartment values, version3 cell/run
  references, exact project replay and recipe-scoped version2 marker rules. Other
  matrices/orders cannot silently share component indices. Native/CLI detection,
  verification and phenotype application agree; retained real F01 identity and118
  cells are unchanged. Desktop matrix JSON load/typed coefficient edit/Apply,
  worker preparation and field opening, signed component/residual preview,
  Keep/Remove/Undo/load, explicit original/corrected/unmixed selection and native
  cell/marker actions now pass. Saved unmixed Replay opens its source asynchronously
  and reuses one prepared operator. Wrong scope, referenced deletion, changed
  fields, cancellation and replacement refuse. Standalone app/CLI handoff preserves
  source/project hashes; inspected signed preview is readable. F04 closes under
  the declared full-column-rank, level-zero, saved-native-TIFF source subset.
  [Evidence and limits](benchmarks/feature-expansion/README.md).

## Checklist: storage, scale and open readers

- [x] **D01 — disk-backed expression matrices.** Implement `ExpressionMatrixSource`
  with bounded gene/cohort reads. Stream large Matrix Market files once into an
  optional indexed sparse sidecar using the existing SQLite stack; keep raw counts
  uint64, stable IDs and content/chunk identities. Prefer a gene-oriented index
  for heatmaps; avoid re-reading the whole source on each gene switch. Project
  associations support relative paths, verified relink and owned packaged data.
  Do not force old selected-gene projects through a migration. Exit: import the
  real197MB source directly, query unprojected genes and reopen without full
  matrix densification; interruption never advertises an incomplete store as valid.
  Progress: native streaming SQLite import, paged stable-ID catalogues, exact
  uint64 sparse/cohort reads and source/gene integrity checks pass focused tests.
  The retained full source imports32285 genes /2702 observations /16031101 entries;
  all six-gene reference counts agree and an additional gene reopens exactly.
  Original text can be offline in native fixtures. Foundation only: project
  associations, owned copies and verified project relink now pass native
  Keep/remove/Undo, relative tree relocation, Save As and async snapshot checks.
  Project copy conflicts and changed source identities refuse publication. D01
  now has explicit ID/frame/plane spatial snapshots and selected-gene native
  GeneViewSession integration, including exact counts, persisted point associations
  and stale/coalesced result checks. Native backed preview/publication reuses the
  import job, image-base rules and position parser. First-save owned copies and
  unsaved recovery pass; the real owned project retains all32285 genes and2702
  exact coordinates/counts. Desktop full/selected storage choice, existing-store
  opening or native raw indexing, prepared-source preview, actual Import/Open,
  bounded ID/symbol pages and gene selection now pass. The gene panel keeps opened
  sources, coalesces catalogue requests and follows exact owned Save As after the
  old locator goes offline. Changed backing data refuse instead of showing stale
  values. Actual real import/comparison and standalone Windows-only-PATH handoff
  pass, with inspected aligned maps/shared legends and unchanged inputs/bindings.
  Native API provides exact matrix relink/Undo; optional bindings/SDK wait for P01.
  D01 closes under local indexed uint64 counts/declared TSV profiles and explicit
  level-zero coordinate imports. See the shared receipt.
- [x] **D02 — bounded interactive and whole-slide processing.** Reuse tile/cache,
  spatial index, native jobs and generation-based publication. Page/filter metadata,
  bound value/color/decoded-tile caches and coalesce rapid viewport requests.
  Add tiled raw-cell processing with declared context, deterministic seam ownership,
  stable global IDs and checkpoint/resume rather than one full-slide raster.
  Capture any global threshold/normalization once; validate halo sufficiency and
  detect unresolved boundary objects rather than silently dropping/clipping them.
  Exit: native100,000-observation/30,000-feature/5-million-entry fixture within a
  declared256MiB accounted work budget, plus one admitted whole-slide ROI/tiled
  cell workflow with independent seam checks. Record process/GPU observations
  separately and qualify responsiveness/cancellation on a named host; no universal
  frame-rate or hardware promise. Delivered native scale/cache/worker, actual
  desktop controls and admitted LuCa nonzero-origin ROI seam/resume checks are
  recorded in the shared receipt; unresolved candidates retain explicit status.
- [x] **O01 — native H5AD adapter.** Use the pinned HDF5 C API behind an optional
  CMake feature; feed D01/S01 without importing Python/anndata/h5py at runtime.
  Initial subset: AnnData0.1.0; CSR/CSC0.1.0; dataframe/array/string encodings0.2.0;
  obs/var IDs, a selected dense/CSR/CSC X/layer, and Nx2 spatial coordinates with explicit units/
  frame. Counts versus already-normalized continuous values must be declared;
  never assume X is raw counts. Check sparse pointers/indices/shape/duplicates,
  string/metadata/chunk limits and cancellation. Disable unreviewed filter plugins;
  refuse external/virtual storage until supported. Report ignored metadata rather
  than pretending a complete AnnData round trip. Exit: independent CSR/CSC/dense
  fixtures and one small openly licensed real dataset with count/ID/axis equality.
  Progress: pinned native decoding and shared indexed import now pass dense/CSR/
  CSC count/continuous, exact uint64/ID/spatial axes, cohorts, source-offline reopen,
  composed work limits and existing import-job/owned-project publication checks.
  Schema3 retains explicit value domain, coordinates/frame and selected H5AD
  matrix/spatial keys; reopen verifies the saved recipe. Schema1 output bytes stay
  frozen and a frozen schema2 fixture remains readable. Native physical-coordinate
  alignment and continuous gene projections survive owned project reopen without
  the original H5AD/store. Six affected suites pass; native Windows-only PATH works.
  Desktop H5AD selection/indexing, embedded-coordinate continuous preview and actual
  publish/open/stale-input controls now pass. Full admitted CC-BY4 10x raw-count
  derivative comparison verifies all16031101 entries,32285 gene IDs,2702 observation
  IDs and exact coordinates; source bytes remain unchanged. Actual real desktop
  preview/publication also passes using that sidecar and the retained explicit TSV.
  See the [source-scoped receipt](benchmarks/feature-expansion/README.md#o01-desktop-and-real-count-closure).
  No legacy AnnData/all-metadata, biological, speed or released-package claim.
- [x] **O02 — native OME-Zarr image adapter.** Initial declared target is local
  NGFF0.4/Zarr2 image stores, selected C/Z/T, scalar numeric dtypes and supported
  isotropic multiscales. Use JSON + existing native tile scheduling; admit only
  the exact native compression libraries needed by the selected real fixture
  (uncompressed/zlib first; pinned Blosc + required subcodecs if needed).
  Reuse F03's generalized raw-source capability while keeping existing TIFF
  identities/records unchanged. Preserve axes, units, scale/
  translation, endian/order and fill-value semantics. IO/decompression failure
  is never a zero-filled chunk. Exit: chunk/seam/axes tests plus viewport/raw
  measurements on one complete open store. Zarr3, remote stores and unsupported
  anisotropic geometry remain explicit later extensions, not guessed support.
  Progress: first native ChannelSource implementation covers bounded local metadata/
  inventory and typed chunk decoding;169 independently authored fixture files
  cover scalar extremes, C/F/LE/BE, C/Z/T, transforms, fill and raw/zlib/gzip.
  Native decoder/ImageServer, generic raw view/planes and versioned directory
  provenance pass six affected suites2.78s. Full CC-BY4 tubhiswt-2D512x512x2
  derivative matches all524288 samples/validity, axes/calibration, native raw
  statistics and explicit RGB pixels against TIFF; Windows-only PATH works.
  See the [adapter receipt](benchmarks/feature-expansion/README.md#o02-native-image-adapter-and-complete-real-store).
  Project links now retain view, limits and expected directory identity, including
  offline inspect/save and native QRA5/6 measurement replay. Source review detects
  changed chunks with unchanged metadata. Actual native GUI channel/plane/measure/
  keep/save/reopen controls pass; the complete real saved project displays native
  tiles, channel metadata and its retained record. App source/project startup
  succeeds with Windows-only PATH. See the
  [project/desktop closure receipt](benchmarks/feature-expansion/README.md#o02-project-and-desktop-closure).
  No biological, release, general NGFF or performance claim.

## Checklist: molecular methods

- [x] **A01 — QC and normalization.** Native requests compute observation totals,
  detected features and optional explicitly defined feature groups, then library-
  size normalization and log1p as versioned derived results. Use the full intended
  feature universe; a six-gene projection is not a whole-transcriptome library.
  Preserve raw integers, zero-library and missing-value semantics; never normalize
  an already-normalized H5AD layer implicitly. Exit: analytical sparse examples
  and independent expected values, source/parameter identity and native replay.
  Native foundation delivered locally: full-source exact uint64 QC/group totals,
  detected features, lazy library-size/log1p source, checked overflow and
  versioned project recipe/replay. Four native suites pass1.50s; complete real
  32285-feature/2702-observation QC matches independent original-MTX totals,
  detection and explicit groups for every observation. Different256/64MiB
  replay batches retain exact QC/recipe/value identity. See the
  [native QC receipt](benchmarks/feature-expansion/README.md#a01-native-full-universe-qc-and-normalization).
  Desktop closure delivered through the existing native GeneViewSession worker:
  actual QC/raw/normalized/log and recipe Keep/Apply/Remove controls pass, with
  exact raw preservation, coordinate registration, QC/source cache reuse and
  saved replay on the complete real source. Native controller refuses double
  display log and clears failed current work. See the
  [desktop closure receipt](benchmarks/feature-expansion/README.md#a01-desktop-qc-and-normalization-closure).
  Optional explicit feature-group creation remains native API; desktop presents
  groups retained in recipes. P01 installs the native APIs; optional Python
  controls remain separately selected work.
- [x] **A02 — regional summaries and pseudobulk.** Reuse ROI/spatial-index cohorts
  and D01 block reads. Report per-gene sums/means, observation counts, group IDs and
  normalization domain; distinguish spot-center membership from cell/spot area.
  Handle overlaps explicitly and overflow-check integer totals. Add sample/
  replicate group metadata and pseudobulk matrices for A04. Exit: hand-calculated
  overlapping/empty cohorts, exact totals and exported/save/replayed associations.
  Delivered through shared native services and the thin batch CLI. Exact uint64
  sums, optional means, explicit source/registered spot-centre membership,
  independent/reject overlaps and full-universe disjoint raw pseudobulk pass.
  Captured sample/replicate/group and observation associations persist as chunked
  recipes; analytical/overflow/cancel/stale/budget, actual native CLI/CSV and
  full real32285-feature/2702-spot replay checks pass. See the
  [A02 receipt](benchmarks/feature-expansion/README.md#a02-regional-expression-summaries-and-pseudobulk)
  and [guide](expression-aggregation.md). P01 subsequently installs the native
  API. Desktop/Python controls remain separate; this aggregation slice alone
  makes no DE/biological-replicate claim.
- [x] **A03 — sparse PCA and deterministic clustering.** Use bounded native block
  matrix products and a fixed-seed truncated PCA/SVD; only the small sketch may
  be dense. Center implicitly using recorded feature means for PCA; distinguish
  it from uncentered SVD. Reuse the admitted native linear algebra backend. Start clustering
  with explicit-k, bounded-iteration k-means and job-local randomness, not a global
  RNG. Retain embedding/cluster IDs, feature universe, preprocessing and seed.
  Exit: reproducibility on named fixtures, bounded work/cancel, independent
  numerical checks and persistence. Additional graph clustering methods are optional.
  Delivered locally through shared native services/CLI and a native SVG scatter
  view. Implicit centering, orthogonalized seeded subspace iteration, pinned
  OpenCV thin SVD and bounded explicit-k clustering retain exact fitted arrays,
  means/features/preprocessing/seed/axis associations. Independent LAPACK and
  analytical rank/subspace/inertia, exact seeded replay across work sizes, owned
  Save As and actual CLI/plot checks pass. Real512-of32285-feature/2702-observation
  engineering check converges and preserves exact fitted identity; no biological
  cluster assignment. [Guide](expression-reduction.md),
  [receipt](benchmarks/feature-expansion/README.md#a03-sparse-pca-and-deterministic-clustering).
  Result view is native SVG; P01 installs the native API. Desktop/Python controls
  remain separate.
- [x] **A04 — named differential-expression workflow.** First method: two-group
  Welch testing on explicitly defined replicate-level log-normalized pseudobulk,
  effect sizes and Benjamini–Hochberg correction. Use a pinned native numerical
  distribution implementation (Boost.Math is an open candidate), never scipy/R at
  runtime. Aggregate raw counts per replicate before normalizing/logging the
  pseudobulk libraries. Require valid independent sample/replicate groups; do not treat thousands
  of spots from one mouse as biological replicates. Flag degenerate/insufficient
  cases. The current single-sample demo is not DE acceptance data. Exit: analytical
  and independent statistical oracles plus one admitted replicated dataset; record
  feature universe, sample groups, normalization, method and correction scope.
  This is not a claim to implement every DE method or DESeq2 compatibility.
  Delivered through native services/CLI, exact canonical saved-recipe replay and
  gene/replicate CSV. Same-replicate cohorts merge raw counts before normalization;
  sample bijection, insufficient/degenerate/underflow flags and stale direct-retain
  refusal are native. Analytical/frozen independent Welch/BH and original-author
  CC BY4.0 Tabula Muris Kidney23433-gene/865-cell/six-mouse checks pass.
  [Guide](expression-differential.md),
  [receipt](benchmarks/feature-expansion/README.md#a04-replicate-level-differential-expression).
  P01 subsequently installs the native API. This method slice adds no GUI,
  Python binding, package publication or biological-discovery claim.

## Checklist: SDK and release

- [x] **P01 — installable native SDK.** Export `qupathxx::core` and available
  component targets through `find_package(qupathxx CONFIG ...)`. Install public
  headers, version/feature metadata and every required transitive native library/
  runtime/notice; remove build-tree paths. Reuse current static libraries first,
  documenting compiler/CRT/ABI compatibility and optional backends. A tiny external
  consumer must configure/build/run after relocating the installation and without
  Python or the source checkout. That named consumer build is not a second engine
  cache. Do not claim an installed SDK from an in-tree example alone.
  Delivered locally: Windows x64/MSVC19.44 Release /MD SDK exports core, density
  workflow and NuCLS targets, all public headers/features and pinned native link/
  runtime/notices.180 installed files match build inputs; relocated external
  CMake consumer compiles every public header, calls native services and preserves
  exact saved identities without Python/source/dependency checkout inputs. ABI/
  component refusal checks pass. Private JPEG CRT mismatch is corrected and eight
  independent native JPEG cases pass; no warning suppression.
  [Guide](native-sdk.md),
  [receipt](benchmarks/feature-expansion/README.md#p01-relocatable-native-sdk).
  Local engineering SDK only; bindings/GUI/release membership remain separate.
- [x] **P02 — matching engineering release.** Freeze the completed source and
  publish matching app/CLI/SDK, optional Python artifacts and source/notices for
  the actual supported platform. Windows x64 GUI/SDK is the first stated target;
  Linux native/headless CI remains distinct until a Linux package is qualified.
  Claim macOS architectures only after their actual changed dependency/ABI/runtime
  checks. Reuse current install/remove/runtime inventory tools, include HDF5/
  codec/statistics dependencies actually enabled and verify all package hashes.
  Perform one frozen install/use/uninstall handoff preserving user data. Existing
  candidate7 and historical evidence remain unchanged; no stable/signing claim
  without those deliverables being implemented.
  Delivered as [Windows native candidate8](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.8):
  matching app/CLI/SDK/application source/dependency source plus validation receipts,
  release manifest and checksums. All seven uploaded SHA256 digests match; public
  checksum/manifest downloads verify. Current installed SDK builds120 headers;
  native saved replay, exports, pixels and GUI startup pass.251 package members
  removed with18 user outputs unchanged; removal preflight negative tests pass.
  [Support/use](windows-native-candidate8.md),
  [deployment receipt](benchmarks/windows-native-candidate8/README.md).
  Local fresh-prefix Windows x64/MSVC19.44 Release /MD engineering support only;
  optional Python artifacts remain frozen. No main merge or new platform,
  biological, signing/stable acceptance is implied.

## Source/dependency decisions before implementation

These are candidates and specification targets, not new dependency admission.
At the owning task, select an exact release/commit/archive digest and capture its
license/notices once; CI must not fetch a moving main/develop/latest reference.

| Item | Primary reference | Intended use |
| --- | --- | --- |
| AnnData encoding specification | [On-disk specification](https://anndata.readthedocs.io/en/stable/fileformat-prose.html) and [reference implementation license](https://github.com/scverse/anndata/blob/main/LICENSE) | O01 encoding/layout reference; no Python runtime |
| HDF5 | [Upstream source/license](https://github.com/HDFGroup/hdf5/blob/develop/LICENSE) | O01 native container; admit selected filters separately |
| NGFF0.4 | [Versioned specification](https://ngff.openmicroscopy.org/0.4/) and [OME licensing](https://www.openmicroscopy.org/licensing/) | O02 image/axis/multiscale interpretation |
| Blosc | [Source/license](https://github.com/Blosc/c-blosc/blob/main/LICENSE.txt) | O02 only if the chosen open fixture needs it; inspect bundled codecs independently |
| Native linear algebra | Existing pinned OpenCV core source/notices inventory | F04/A03 small solves/SVD; keep backend optional and native |
| Boost.Math | [Source license](https://github.com/boostorg/boost/blob/master/LICENSE_1_0.txt) | A04 candidate distribution functions; do not import Boost wholesale unnecessarily |

## Validation, progress and stop rules

For each item, update its single checkbox only with a linked source-scoped result.
Put outcome, actual waste/correction and next task in the existing current card;
append only durable new lessons to compilexx. Do not create one report per API.

Native algorithms get analytical/independent expected results and affected
stale/cancel/seam/persistence checks. UI/CLI/Python consumers get mapping/ownership
checks over those same results, not separate numerical campaigns. New floating
methods specify tolerances in their method contracts; raw source/count identity
remains exact. Python reference fixtures/oracles may remain development tools.

Reuse one active checkout/cache/runtime, the real open demo receipts and existing
native-only CI. Settle native/consumer interfaces before one affected build and
optional binding/stub pass; preserve compatible expensive objects. Do not build
bindings for docs or native-only additions. Keep new matrix APIs batched with owned
buffers and GIL release; no Python per-gene/per-pixel loops or subprocess fallbacks.

Package once after the SDK/interface boundary settles. No full platform/corpus
run until P02 or a named correctness/platform risk requires it. Cleanup only
verified disposable outputs after preserving fixtures/failures/source. Completion
means these22 finite contracts are met; it does not imply all statistical methods,
nonlinear registration, deep pretrained models, distributed execution or100% QuPath
parity. O01/O02/A01/A02/A03/A04/P01/L01 and the P02 native engineering release are
delivered under their stated contracts. Stop; preserve the
independent desktop layout work and its source-scoped evidence.
