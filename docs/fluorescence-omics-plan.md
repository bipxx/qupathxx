# Open-format fluorescence and spatial omics

The completed slice below stays frozen. The owner's subsequent
[eight-gap expansion plan](feature-expansion-plan.md) now schedules the missing
visualization/import/analysis/storage/readers/SDK capabilities, including bounded
H5AD/OME-Zarr additions. Their current delivery state is tracked there, separately from FO00-FO05 completion.
Proprietary and NC material are [kept separately](licensing/restricted-materials.md).

Owner approved a concrete fluorescence workflow first, followed by one spatial-
omics import, restricted to open formats. FO00-FO05 are complete for this finite workflow scope: local native checks,
installed CLI/batch, saved-project viewer routes and the Python-disabled Linux job
pass. [Usage/limits](fluorescence-omics.md) describe the delivered support. Existing native TIFF/raw services are reused; the spatial-count
reader/projection is new C++. Evidence is recorded in
[the implementation receipt](benchmarks/fluorescence-omics/receipt.json).

## Format admission

Both the format/schema and implementation path must be open: specifications
available for reuse, a native open-source reader, open required codecs, preserved
notices and separately established fixture redistribution rights. Publicly readable
vendor documentation or a free SDK is not sufficient. Do not load closed codecs,
execute a vendor conversion service, or introduce a Python reader fallback.

| Format | Decision | Evidence / easiest implementation path |
| --- | --- | --- |
| OME-TIFF with OME-XML | Selected fluorescence input | [OME specification](https://docs.openmicroscopy.org/ome-model/6.3.1/ome-tiff/specification.html), [OME specification/schema licensing](https://www.openmicroscopy.org/licensing/); reuse existing pinned libtiff/tinyxml2/native raw services and [codec inventory](reader-license-policy.md). First workflow uses supported lossless uint16/float32 inputs, one series and explicit channel/Z/T selection. No new decoder. |
| Matrix Market coordinate integer general + TSV | Selected first spatial-count input | [NIST format specification](https://math.nist.gov/MatrixMarket/formats.html) and [information reuse terms](https://www.nist.gov/copyrights-disclaimers). Implement our own AGPL native parser; no NIST code copied or third-party runtime. The TSV association profile below is documented here under the repository's [license](../LICENSE). |
| AnnData/H5AD | Open candidate, deferred from this phase | [AnnData on-disk specification](https://anndata.readthedocs.io/en/stable/fileformat-prose.html), [BSD-3-Clause reference implementation](https://github.com/scverse/anndata/blob/main/LICENSE), [HDF5 library terms](https://github.com/HDFGroup/hdf5/blob/develop/LICENSE). A later task must pin exact native HDF5 sources, admit selected filters and define an encoding subset. Do not add Python/anndata/h5py as a runtime. |
| OME-Zarr/NGFF | Deferred | Open OME specification family; not required to deliver this workflow. A later task must select a version, native chunk reader and exact open codecs. |
| Closed vendor containers, schemas or required codecs | Excluded | No direct proprietary microscope/omics platform adapter in this phase. Open export files can enter only through the explicitly supported generic profile; this does not establish vendor compatibility. |

This is stricter than the legacy rule allowing vendor formats through open-source
readers. It governs new fluorescence/omics admission; existing supported readers
are not silently removed. A dataset's availability or a format's openness does
not grant redistribution rights to the dataset itself.

## First delivery: fluorescence ROI measurements

User flow: open supported OME-TIFF, select channels and one Z/T plane, inspect
display windows/LUTs, measure existing rectangular/body/nucleus ROIs on original
typed values, save a new project, reopen and replay the retained measurements.
Missing calibration stays unknown. Do not apply RGB optical density/stain
deconvolution to fluorescence channels.

| Step | Reuse first / smallest addition | Acceptance |
| --- | --- | --- |
| Source and axes | open_tiff, TiffOpenOptions, RawImageMetadata, existing project source associations | Exact selected channels/names/Z/T/dtype; invalid or unavailable axes refuse |
| Measurement | RawAnalysisRequest, analyze_raw, keep_raw_analysis; existing bounded native tiling | Per-channel finite/nonfinite counts, means, minima/maxima; no preview quantization or inferred calibration |
| Display | Existing TIFF view, windows/LUT/gamma and overlay controls | Changing display settings changes presentation only; raw inputs and numeric result identities remain unchanged |
| Retention | Project::save_new, RawAnalysisRequest::replay, verify_raw_analysis and existing CSV writer | Input project unchanged; saved values/context survive reopen; existing output refuses; changed sources/ROI invalidate reuse |
| Entry points | One shared native recipe if sequencing is genuinely missing; otherwise use existing services/CLI | C++ caller works with Python disabled; UI/CLI and any optional Python controller call native authorities |

The existing batch consumer already exposes `--kind raw`, `--raw-channel`,
`--raw-channels`, `--raw-z`, `--raw-time` and `--raw-explicit-axes`. Exercise that
route first instead of inventing a parallel fluorescence command or recipe.

Start with tiny deterministic uint16 and float32 fluorescence-like fixtures made
by native test utilities. Include values above255, a zero channel, distinct channel
patterns and float nonfinites; verify the relevant counts explicitly. Use a small
openly redistributable real image only after its license is recorded. Synthetic
checks establish engineering behavior, not biological segmentation accuracy.

No new spectral unmixing, compensation, illumination correction, automatic
fluorescence cell detector, marker-positivity rules or broad multiplex claim is
included. Select those separately for a named experiment.

## Second delivery: spatial-count projection

User flow: associate explicit spot/cell positions and sparse counts with an
existing project image; select genes; inspect those positions and gene counts;
save a new project and reopen it. This imports measured observations and a
selected feature projection, not a complete transcriptomics analysis platform.

The initial open association profile is intentionally explicit:

- counts.mtx: Matrix Market `matrix coordinate integer general`, features as rows,
  observations as columns, one-based matrix indices and nonnegative counts.
- features.tsv: header `feature_id` followed by one unique stable ID per matrix
  row, in row order. Display symbols can be added later; never merge distinct IDs.
- positions.tsv: header `observation_id\tx_px\ty_px`, one unique observation per
  matrix column in column order. Coordinates are finite level-zero pixel positions
  on one explicitly selected image/plane, with 0≤x<width and 0≤y<height.
- User-selected feature IDs are explicit. Absent sparse entries mean zero;
  malformed/missing associations do not mean zero. Duplicate coordinates in the
  sparse matrix, negative values, mismatched dimensions, invalid numeric fields,
  duplicate IDs, extra/truncated records and unsupported encodings refuse.
- No inferred units, axis swap, scaling, registration or nearest-cell assignment.
  Micrometre/vendor coordinate transforms require a later explicit contract.

The native reader should own bounded immutable input snapshots and sparse
validation. Apply file/line/feature/observation/nonzero/selected-value budgets before
large allocations, support cancellation and never densify the full expression
matrix. Preserve counts as integers internally; if the existing double-valued
object measurements are used for selected genes, reject values exceeding exact
integer representation instead of rounding them.

Reuse Roi::points, Document::add_detection and ObjectProperties to retain selected
counts as named imported measurements, with observation/feature IDs and source
digests. Reuse project metadata, save_new and the existing object inspector/table
and export paths. Label this a selected-gene projection: the project must not
claim to contain the complete original matrix. Avoid a new project database
schema or expression engine unless these existing services fail this contract.

Keep matrix parsing, association, provenance and publication in C++. Add a thin
CLI entry over the shared service; optional Python only forwards options/results.
Initial tests use a hand-calculable tiny sparse matrix with a zero-expression
observation, an unselected gene, an exact integer boundary and shuffled input
entry order. Verify IDs, positions, selected values, save/reopen and no-overwrite,
plus native rejection/cancellation/budget cases. Do not imply clustering,
normalization, differential expression, protein/genome analysis or platform-native
Visium/Xenium/CosMx import.

## Delivery checklist and waste controls

- [x] FO00: select open formats, verify primary license/specification sources,
  document native-only ownership and the explicit sparse association profile.
- [x] FO01: exercise the existing native fluorescence source/ROI/retention paths;
  identify the actual missing consumer step before adding any service.
- [x] FO02: implement only the missing fluorescence recipe/controls; verify raw
  values and display independence, save/reopen/replay and Python-disabled use.
- [x] FO03: implement bounded native Matrix Market/TSV profile reading and
  association with explicit IDs/coordinates; verify sparse numeric/refusal cases.
- [x] FO04: retain a selected-gene spatial projection using existing objects and
  measurements; verify source digests, persistence, cancellation and no overwrite.
- [x] FO05: expose the native services through existing CLI/UI consumers; add an
  optional Python wrapper only if a named caller needs it. Run one focused native
  integration check, publish the actual support limits and update this checklist.

One active task at a time: FO01/FO02 first, then FO03/FO04. Reuse the existing
integration cache, reader stack, raw measurement code, sparse/native utilities and
passing scientific receipts. Settle interfaces before any binding build; no new
HDF5/Zarr dependency, duplicate workflow state machine, full-platform campaign or
binary release is needed for this plan. After each task/failure, update the current
card with its result, actual waste/correction and next action. Stop at these finite
contracts; do not expand into all fluorescence/omics methods.
