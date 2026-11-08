# Eight-gap native feature expansion plan

Owner request: plan all eight remaining areas, keeping proprietary and
noncommercial (NC) material separate. This document owns the new implementation
checklist: **22 pending work items**. It does not reopen completed FO00–FO05 or
claim any of these additions implemented. Current desktop-layout work in
[current-task.md](current-task.md) continues; this plan queues subsequent work.

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

## Checklist: shared services and useful visualization

- [ ] **L01 — material inventory and separation guard.** Extend the existing
  inventories instead of creating another dependency database. Keep proprietary
  and NC records physically distinct. Check source/package asset membership,
  derivative lineage and notices offline. Exit: admitted open examples pass;
  restricted and unreviewed fixture/model records refuse staging. No restricted
  binaries or datasets need downloading to test the guard.
- [ ] **S01 — expression dataset/value contract.** Add a small native
  `ExpressionDataset`/`FeatureSelection` interface with stable observation/feature
  IDs, count versus continuous-value domain, validity, source identity, plane and
  coordinate-frame reference. Start with an adapter over existing imported
  ObjectProperties; retain old projects and the current CLI. Zero is distinct from
  a missing/unimported gene. Keep raw counts integer-exact; legacy double-valued
  projections retain their2^53 admission limit. No new full-matrix schema yet.
- [ ] **V01 — native gene display field.** `GeneDisplayRequest` captures a dataset,
  gene, cohort and linear/log1p display policy; an owned `GeneDisplayField` supplies
  values/validity/colors to overlays. Native min/max or percentile range, explicit
  zero/missing colors, opacity, legend units and optional shared range for gene
  comparisons. Cache by dataset/gene/cohort/display identity; cancel stale work.
  Display transformations never rewrite counts or masquerade as normalization.
- [ ] **V02 — gene selector, heatmaps and comparison UI.** After the current UI
  closeout, reuse the RmlUi shell's typed-command bridge and existing inspectors.
  Search/paginate gene IDs and symbols, render position-based expression overlays,
  show the exact range/transform/legend and support synchronized two-gene views.
  Label spots versus cells accurately and persist presentation choices. Exit:
  real mouse-brain markers change the correct point colors/legend; missing genes,
  constant/all-zero values and rapid switching behave correctly; raw/project
  measurements remain unchanged by navigation/display.

## Checklist: import and registration

- [ ] **I01 — shared native import preview/job.** Wrap current sparse parsing and
  project publication in `SpatialImportPreview` and an owned cancellable job;
  CLI and UI call the same services. Preview IDs/dimensions/genes, source hashes,
  coordinate frame, counts domain, selected projection and work/storage limits.
  Extend only actual missing header/column mappings through explicit options.
  Reject ambiguous IDs/units, missing pairs and unsafe output collisions before
  publication. Retain the current strict profile and no-overwrite behavior.
- [ ] **I02 — guided import UI.** Steps: choose image/files → map IDs/axes/units →
  select genes or backed storage → inspect overlay/limits → import a new project.
  Preview does not mutate the current study; progress/cancel/errors come from the
  native job. Save the explicit import recipe and source identities. Exit: complete
  the real open demo without shell commands; cancel/error leaves prior state and
  output files intact. Do not parse matrices in RML/JavaScript/Python.
- [ ] **R01 — native coordinate-frame and affine contract.** Add versioned
  `CoordinateFrame`, source→image-level-zero `Affine2D` and `RegistrationRequest`.
  Reuse native geometry/calibration. Store original coordinates, units, image/plane
  identity and transform separately; apply scale/translation/rotation/reflection
  only through explicit matrices. Unknown calibration prevents a micrometre/pixel
  conversion. Test analytical transforms, inverse/round-trip and stale source checks.
- [ ] **R02 — interactive landmark registration.** Reuse canvas point editing,
  overlays and undo. Fit a translation/similarity/affine transform from sufficient
  nondegenerate landmarks using native least squares; show residuals and held-out
  error in declared units. Preview, accept, undo and save/reopen; do not rewrite raw
  coordinates or gene counts. Exit: recover a known transform and align the real
  demo without swapping axes implicitly. Nonlinear deformation and unrestricted
  automatic registration are not required for this first registration contract.

## Checklist: fluorescence analysis

- [ ] **F01 — raw-channel nucleus/cell workflow.** Use selected original channel,
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
- [ ] **F02 — marker positivity and phenotype rules.** Reuse per-compartment raw
  measurements and native classification publication. Versioned per-marker gates
  and AND/OR rules include channel/compartment, threshold, units and measurement
  domain. Missing values are Unknown, not Negative. Show counts/cohorts, preview
  assignments and retain/replay the exact rule. Exit: threshold-boundary,
  multi-marker, missing-data, stale-input and save/reopen cases pass.
- [ ] **F03 — dark/flat-field correction.** Add an immutable native derived-channel
  recipe using explicit dark/flat sources and declared normalization. Reuse typed
  tile IO and masks; first put TIFF-specific raw-source checks behind a shared
  typed capability reused later by O02, preserving existing TIFF identities.
  Validate compatible axes/calibration, zero denominators and
  nonfinites. Version the corrected domain/source provenance instead of changing
  original samples or reusing a raw record's identity. Raw measurements remain the
  default unless a derived domain is explicitly selected. Test known fields,
  seams, cancellation and persistence of recipe/source associations.
- [ ] **F04 — linear compensation/spectral unmixing.** Consume an explicitly
  supplied, labeled finite mixing matrix and native channel blocks. Start with
  small full-rank linear systems and residual reporting; reuse the existing
  OpenCV core SVD/solve backend behind a native optional processing capability.
  Pin/license its exact dependency configuration; no Python/model download.
  Refuse ill-conditioned or incompatible matrices, preserve signed results unless
  an explicit rule says otherwise, and never silently clamp. Save matrix/order/
  domain/algorithm provenance. Exit: analytical mixtures, noisy residuals, axis
  permutations and stale/cancel cases pass. Nonlinear/learned unmixing is separate.

## Checklist: storage, scale and open readers

- [ ] **D01 — disk-backed expression matrices.** Implement `ExpressionMatrixSource`
  with bounded gene/cohort reads. Stream large Matrix Market files once into an
  optional indexed sparse sidecar using the existing SQLite stack; keep raw counts
  uint64, stable IDs and content/chunk identities. Prefer a gene-oriented index
  for heatmaps; avoid re-reading the whole source on each gene switch. Project
  associations support relative paths, verified relink and owned packaged data.
  Do not force old selected-gene projects through a migration. Exit: import the
  real197MB source directly, query unprojected genes and reopen without full
  matrix densification; interruption never advertises an incomplete store as valid.
- [ ] **D02 — bounded interactive and whole-slide processing.** Reuse tile/cache,
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
  frame-rate or hardware promise.
- [ ] **O01 — native H5AD adapter.** Use the pinned HDF5 C API behind an optional
  CMake feature; feed D01/S01 without importing Python/anndata/h5py at runtime.
  Initial subset: AnnData0.1.0; CSR/CSC0.1.0; dataframe/array/string encodings0.2.0;
  obs/var IDs, a selected dense/CSR/CSC X/layer, and Nx2 spatial coordinates with explicit units/
  frame. Counts versus already-normalized continuous values must be declared;
  never assume X is raw counts. Check sparse pointers/indices/shape/duplicates,
  string/metadata/chunk limits and cancellation. Disable unreviewed filter plugins;
  refuse external/virtual storage until supported. Report ignored metadata rather
  than pretending a complete AnnData round trip. Exit: independent CSR/CSC/dense
  fixtures and one small openly licensed real dataset with count/ID/axis equality.
- [ ] **O02 — native OME-Zarr image adapter.** Initial declared target is local
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

## Checklist: molecular methods

- [ ] **A01 — QC and normalization.** Native requests compute observation totals,
  detected features and optional explicitly defined feature groups, then library-
  size normalization and log1p as versioned derived results. Use the full intended
  feature universe; a six-gene projection is not a whole-transcriptome library.
  Preserve raw integers, zero-library and missing-value semantics; never normalize
  an already-normalized H5AD layer implicitly. Exit: analytical sparse examples
  and independent expected values, source/parameter identity and native replay.
- [ ] **A02 — regional summaries and pseudobulk.** Reuse ROI/spatial-index cohorts
  and D01 block reads. Report per-gene sums/means, observation counts, group IDs and
  normalization domain; distinguish spot-center membership from cell/spot area.
  Handle overlaps explicitly and overflow-check integer totals. Add sample/
  replicate group metadata and pseudobulk matrices for A04. Exit: hand-calculated
  overlapping/empty cohorts, exact totals and exported/save/replayed associations.
- [ ] **A03 — sparse PCA and deterministic clustering.** Use bounded native block
  matrix products and a fixed-seed truncated PCA/SVD; only the small sketch may
  be dense. Center implicitly using recorded feature means for PCA; distinguish
  it from uncentered SVD. Reuse the admitted native linear algebra backend. Start clustering
  with explicit-k, bounded-iteration k-means and job-local randomness, not a global
  RNG. Retain embedding/cluster IDs, feature universe, preprocessing and seed.
  Exit: reproducibility on named fixtures, bounded work/cancel, independent
  numerical checks and persistence. Additional graph clustering methods are optional.
- [ ] **A04 — named differential-expression workflow.** First method: two-group
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

## Checklist: SDK and release

- [ ] **P01 — installable native SDK.** Export `qupathxx::core` and available
  component targets through `find_package(qupathxx CONFIG ...)`. Install public
  headers, version/feature metadata and every required transitive native library/
  runtime/notice; remove build-tree paths. Reuse current static libraries first,
  documenting compiler/CRT/ABI compatibility and optional backends. A tiny external
  consumer must configure/build/run after relocating the installation and without
  Python or the source checkout. That named consumer build is not a second engine
  cache. Do not claim an installed SDK from an in-tree example alone.
- [ ] **P02 — matching engineering release.** Freeze the completed source and
  publish matching app/CLI/SDK, optional Python artifacts and source/notices for
  the actual supported platform. Windows x64 GUI/SDK is the first stated target;
  Linux native/headless CI remains distinct until a Linux package is qualified.
  Claim macOS architectures only after their actual changed dependency/ABI/runtime
  checks. Reuse current install/remove/runtime inventory tools, include HDF5/
  codec/statistics dependencies actually enabled and verify all package hashes.
  Perform one frozen install/use/uninstall handoff preserving user data. Existing
  candidate7 and historical evidence remain unchanged; no stable/signing claim
  without those deliverables being implemented.

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
parity. Start next with S01/V01 using already admitted data, after the active UI
handoff allows consumer changes.
