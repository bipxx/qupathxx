# Sparse PCA and deterministic clustering

A03 fits a low-rank embedding over an existing native expression matrix, then
clusters the fitted scores with an explicit number of clusters. The delivered
consumers are the native batch CLI and installed C++ SDK, with CSVs and a native
SVG scatter view.
Original counts, observation IDs, image coordinates and spatial registration
are preserved. Cluster numbers are exploratory labels and carry no inferred
cell-type, sample, replicate or biological meaning.

## Create and inspect a result

Use a matrix association already retained on the selected project image. Write
a specification such as:

```json
{
  "schema": "qpx.expression-reduction-spec/1",
  "method": "centered-pca",
  "components": 2,
  "clusters": 4,
  "seed": 42,
  "oversampling": 8,
  "power_iterations": 1,
  "max_iterations": 100,
  "cluster_tolerance": 1e-8
}
```

```powershell
qupathxx_batch --reduce-expression input.qpx fitted.qpx --image 1 --matrix MATRIX_SHA256 --spec reduction.json
qupathxx_batch --verify-expression-reduction fitted.qpx --image 1 --record RECORD_SHA256
qupathxx_batch --export-expression-reduction fitted.qpx embedding.csv --image 1 --record RECORD_SHA256
qupathxx_batch --export-expression-reduction fitted.qpx loadings.csv --image 1 --record RECORD_SHA256 --loadings
qupathxx_batch --export-expression-reduction fitted.qpx scatter.svg --image 1 --record RECORD_SHA256 --plot
```

The creation report supplies `record`. Verify/export open the exact stored fitted
arrays; they do not recompute PCA or clustering. Outputs are atomic and new-only.
An explicit refit check is available when needed:

```powershell
qupathxx_batch --replay-expression-reduction fitted.qpx --image 1 --record RECORD_SHA256 --work-bytes 67108864
```

The SVG shows the first two components, labeled energy fractions, source value
domain, cluster legend, seed and convergence status. Point tooltips retain exact
observation IDs, cluster labels and binary64 scores formatted with 17 significant
digits. A one-component fit explicitly uses zero vertically. Axis ticks use a
compact display format; exact scores remain in tooltips/CSVs. XML-incompatible
identifier control characters refuse SVG export rather than publish an unreadable
file; the native matrix and fitted result remain unchanged.

`embedding.csv` associates source observation indices/IDs, cluster labels and all
requested component scores with the exact fitted ID and source/value identities.
`loadings.csv` associates stable feature indices/IDs, feature means and all fitted
loading coefficients with that same result. All numeric exports use binary64
round-trip formatting.

## Choose values and the feature universe explicitly

Omitting `features` uses the complete source feature universe. An optional
nonempty `features` array names stable source feature IDs; native validation joins
them exactly and records the selected source-axis order. It does not select
variable genes automatically or infer an omitted feature set from a display.
All source observations participate, including measured sparse zeros.

Without `normalization`, analysis uses the matrix's declared original value
domain. To select an existing [A01 normalization](fluorescence-omics.md) record,
add `"normalization": "NORMALIZATION_RECORD_SHA256"`. The native service verifies
the exact parent and recomputes its full-source QC once before selected-feature
analysis. A selected gene list never becomes its own library denominator.
An already-normalized H5AD layer is not normalized implicitly.

PCA/SVD is binary64 analysis. Direct raw integers above 2^53 refuse instead of
silently rounding; selecting a suitable explicit derived-value recipe is a
separate scientific choice. The raw integer matrix always remains intact.
Continuous values must be finite. Overflowing arithmetic, insufficient numerical
rank and constant centred input fail visibly.

`centered-pca` subtracts recorded feature means implicitly in sparse block
products; it never materializes an observations-by-features centred matrix.
`uncentered-svd` deliberately omits that subtraction and is labeled separately.
There is no implicit unit-variance feature scaling.

## Numerical method and limits

The implementation uses a fixed-seed Gaussian range sketch, two-pass modified
Gram–Schmidt, orthogonalized power/subspace iterations, and a thin SVD of the
small projected matrix. It follows the randomized subspace framework in
[Halko, Martinsson and Tropp](https://tropp.caltech.edu/papers/HMT11-Finding-Structure.pdf)
and reuses the admitted [OpenCV 4.11 thin-SVD backend](https://docs.opencv.org/4.11.0/df/df7/classcv_1_1SVD.html).
Only low-rank sketches, loadings, embeddings and bounded result buffers are dense.
Sparse reads are bounded feature batches; means include implicit/explicit zeros.

The fitted directions approximate leading PCA/SVD directions. Stored singular
values come from the small sketch. Scores are direct projections onto the fitted
loadings, and `component_energy` measures their actual sums of squares. SVG energy
fractions divide this by total analysis energy. Centred PCA uses centred sums of
squares; uncentred SVD uses uncentred energy. This distinction avoids presenting
uncentred energy as explained covariance. The numerical rank threshold is 1e-12
relative to the sketch scale; a requested component beyond that rank is refused.

Each loading's largest absolute coefficient is made positive, with source-axis
order resolving exact pivot ties. Repeated singular values can rotate their
component basis across numerical backends. Numerical validation therefore compares
singular values and sign/subspace-invariant Gram/projection quantities with the
declared fixture tolerance; it does not equate close coefficients with identical
stored models. The small independent oracle uses absolute 1e-8 plus relative
1e-9 tolerances. Same-backend seeded repeat/replay additionally requires exact
fitted identity.

Clustering uses job-local seeded k-means++ and bounded Lloyd iterations, without
changing a process-wide RNG or OpenCV thread configuration. Distance ties choose
the earlier current centre; final labels are renumbered by lexicographic centre
order. Empty clusters use a deterministic farthest donor when possible; too few
distinct fitted observations fail. Stored centres are the means of their assigned
observations. Inertia and iteration/convergence status are retained. An iteration
limit can yield a valid, explicitly unconverged result; it is never labeled as
converged merely because work stopped.

Initial limits are 1–32 components, at most 32 oversamples and 64 sketch columns,
0–4 power iterations, 1–32 explicit clusters and 1–1000 Lloyd iterations. Source
axes retain their existing million-item bounds, but a requested shape must fit
the 32–1024 MiB accounted work budget. The default is 256 MiB; some shapes and
preprocessing/opening paths need more than the minimum. `--max-operations` and
`--max-distance-tests` bound accounted matrix/orthogonalization work and cluster
distance evaluations. Cancellation and source currentness are checked between
bounded blocks and during native loops. The small backend SVD has checks before
and after its bounded solve. Accounted buffers/operations are not process RSS,
wall-time or throughput guarantees.

Fitting requires `QUPATHXX_BUILD_LINEAR_ALGEBRA=ON` or the existing classifier
training backend, using the pinned native OpenCV core. Plain builds keep Python
optional and this algebra backend optional. Without it, fitting reports an
explicit capability error; stored native arrays can still be opened and exported.
SDK/Python exposure remains P01.

## Exact fitted persistence and native API

`expression_reduction.hpp` exposes typed options, immutable owned results,
`reduce_expression`, archive save/open, explicit replay, project association and
native workflow/export services. CLI parsing calls those same services. GUI
integration should submit the expensive service work to an existing native
worker and present the returned snapshot.

The open native `.qpr` format starts with eight magic/version bytes
`51 50 58 52 45 44 01 00`, followed by a little-endian uint64 canonical-JSON header
length and the ASCII JSON header. Binary arrays follow in order: feature means,
observation-major scores, feature-major loadings, sketch singular values,
component energies, cluster centres (all little-endian IEEE binary64), then
one little-endian uint32 cluster label per observation. A 64-byte lowercase
ASCII SHA256 of the preceding bytes terminates the archive and is the fitted ID.
The header retains source/file/value identities, backend, selected indices and
axis IDs, preprocessing/seed/method parameters and clustering statistics.
Execution work limits and telemetry do not change fitted identity. Header and
archive bounds are 16 MiB and 256 MiB, within the caller's work allowance.

One small undoable image-data association binds the fitted ID, exact archive
file digest and raw parent. Owned archives live beside the project under
`.qpx-expression-reductions/<file-sha256>.qpr`. Existing native project save/open
boundaries copy/verify exact bytes for Save As and relocate association paths.
Managed links/reparse paths and output aliases refuse. Removing a record or
undoing metadata changes does not delete a shared archive. A failed project
publication may leave a verified archive for reuse; it does not publish a partial
project.

Capture `expression_reduction_open_request(project, image, id)` on the owner,
then call `open()` on a worker. Opening validates the exact archive, shapes,
finite arrays, labels and source-axis associations; it preserves fitted bits and
does not solve again. Explicit replay refits using recorded parameters and the
supported backend, and refuses a different fitted identity. A backend change
does not silently replace the stored result with numerically close coefficients.

The [A03 receipt](benchmarks/feature-expansion/README.md#a03-sparse-pca-and-deterministic-clustering)
records the analytical/independent oracle, exact owned replay, native CLI and
meaningful plotted-coordinate checks. The real check uses an explicit 512-gene
selection from the retained 32,285-feature/2,702-observation source. It does not
establish a recommended feature set, cell type or biological clustering method.
