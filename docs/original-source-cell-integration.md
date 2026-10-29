# Original-source cell compatibility integration

This work extends the existing C++ engine, nanobind SDK and shared ImGui/managed
controllers. The private candidate304 detector matches 13 captured processing
workflows. That evidence does not yet prove original-slide request parity.

The local Windows affine implementation now passes the resampling suite,
including all 112 captured RGB reads, tile/context assembly and signed/large
origin admission. The 13 reference-generator checks and 14 installed collection
checks pass. This is an uncommitted integration checkpoint, with logs and the
retained executable in `build/original-source-305` of the root workspace. The
first build's header declaration error and a stale development-environment
collection failure are retained; the corrected build and installed-environment
checks pass. Subsequent local edits now carry both choices through public
requests, QCL8/QCA8, replay/equality, batch identity, CSV11/12, nanobind, CLI,
shared desktop controls and managed capture. Those integrated edits are awaiting
their first application build and installed validation; they inherit no acceptance
from the earlier private sampler. GCC verification remains open.

## Contract decisions

The existing SDK and persisted recipes are treated as stable. Following AIP-180
(compatibility) and AIP-185 (versioning), preserve the default nominal sampler,
the quantitative recipe and the shipped ImageJ154p recipe. Add independent
sampling and processing choices; changing a detector must not silently change
its source request's sampler identity. No migration is required for old requests.

The affine choice uses the binary64 inverse of a diagonal scale followed by
translation, then a split whole/fractional 32.32 lattice with the requested
downsample stride. It must not simplify the inverse translation to an integer,
fuse the first-center arithmetic, or restart the lattice for each tile/context
window. Public requests expose the independent `sampling` choice. Origins outside the exact binary64 integer range are
refused for this choice; the old sampler keeps its admission behavior.

Following AIP-141 (quantities) and AIP-203 (field behavior), document requested
distances/areas and resolved processing units separately. Do not average
anisotropic calibration into a scalar. Candidate304 currently accepts processing
sample units; its public integration must resolve or explicitly reject unsupported
metrics before source IO, while preserving existing per-axis workflows. The
new `QUPATH_070` recipe requires equal resolved axis steps, sigma at most4096
processing samples, expansion at most1000000 processing samples, and zero
`min_peak_radius`/`min_seed_distance`. Unsupported combinations refuse during
capture; the existing quantitative/ImageJ recipes retain their own behavior.

## Python invocation

```python
parameters = q.GridCellParameters()
parameters.processing = q.CellProcessingPolicy.QUPATH_070
request = q.CellRequest.for_nominal_roi(
    document, object_id, transform, 1.5, 32, 32, parameters,
    q.RgbNominalSampling.AFFINE_INVERSE,
)
result = q.detect_cells(request)
```

`RgbNominalRequest(..., sampling=q.RgbNominalSampling.AFFINE_INVERSE)` selects
the same sampling contract for independent RGB reads. Batch/managed options
copy `NominalCellRoiSampling.sampling`. CLI choices are
`--cell-processing qupath070 --cell-nominal-sampling affine-inverse`; the region
reader accepts `--sampling affine-inverse` after its nominal output path.
Sampling and processing remain independent opt-ins. QCL8/QCA8 retain both;
older archives retain their old defaults and byte layouts. CSV11/12 append
`sampling_algorithm` alongside the processing column when affine runs occur.

## Delivery order and required evidence

1. Finish the native affine sampler and compare captured pixels, signed/large
   origins, source edges, context and tile assembly. The 13 new golden vectors
   are generated solely from authenticated public PNG bytes; the earlier 99
   vectors remain unchanged. Verify compiler arithmetic on Windows and GCC.
2. Add an immutable sampling choice to `RgbNominalRequest` and copied ROI
   sampling options. Carry it through native factories, read planning, source
   hashing and algorithm identity. Bind it in nanobind with the old default.
3. Add an explicit detector processing choice using candidate304's common native
   pipeline. Avoid retaining diagnostic stage copies for ordinary jobs. Preserve
   common features, contours, source authority and atomic result publication.
4. Extend canonical history (planned QCL8/QCA8), replay, equality, classifier
   provenance, batch JSON/resume and CSV. Old archive versions must decode to
   their old defaults and retain their canonical bytes. Unknown values refuse;
   different numerical policies must produce different recipe/input identities.
5. Carry both independent choices through Python/stubs, region and batch CLI,
   direct/batch ImGui and managed viewer. Reuse the existing native controller
   paths and exercise real installed consumers, including save/reopen/resume.
6. Compare original-slide requests against all 13 new and nine original cell
   workflows with unchanged masks/features/tolerances. Exercise ROI/holes,
   real context, source support/alpha, budgets, cancellation, stale results and
   ownership. Captured processing planes alone cannot satisfy this step.
7. Freeze the complete implementation once, build the installed application and
   qualify its own source across the required platform profiles. Keep earlier
   qualification failures and source identities distinct.

## Review coverage

| Aspect | Coverage and evidence |
| --- | --- |
| Resources | Not applicable: native/SDK image requests, no remote resource API |
| Methods | Reviewed: existing factory/read/job methods remain the entry points |
| Fields/units | Reviewed: immutable explicit choices; requested versus resolved units above |
| Compatibility | Reviewed: old defaults, enum values and canonical history preserved |
| Documentation/examples | In progress: consumer examples follow delivered public signatures |
| Errors | Reviewed: validate choices/precision/units before IO; existing cancellation and atomic publication |
| Auth/retry/HTTP | Not applicable to this local API change |
| SDK | In progress: nanobind bindings, generated stubs and consumer checks remain required |

This is a local native API design adaptation, not a claim of Google API
conformance. No proto/OpenAPI artifact is introduced. Broader biological,
whole-slide resource/seam, representative speed/hardware, license and supported
release requirements remain in the full project plan.
