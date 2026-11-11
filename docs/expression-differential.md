# Replicate-level differential expression

A04 compares two explicit groups with a native, two-sided Welch test on
log-normalized replicate pseudobulk. The batch CLI and installed C++ SDK use the
shared native services. They use the retained [A02 pseudobulk](expression-aggregation.md),
including its exact raw counts, full source feature universe and captured
sample/replicate/group/observation associations. No Python or R runtime is used.

## Run and export

Create a raw, disjoint, full-universe pseudobulk record with A02 first. Its cohorts
must carry explicit biological sample, biological replicate and group labels.
Use the reported pseudobulk record ID:

```powershell
qupathxx_batch --differential-expression pseudobulk.qpx differential.qpx --image 1 --pseudobulk PSEUDOBULK_SHA256 --group-a control --group-b treated
qupathxx_batch --verify-expression-differential differential.qpx --image 1 --record DE_RECORD_SHA256 --work-bytes 67108864
qupathxx_batch --export-expression-differential differential.qpx genes.csv --image 1 --record DE_RECORD_SHA256
qupathxx_batch --export-expression-differential differential.qpx replicates.csv --image 1 --record DE_RECORD_SHA256 --replicates
```

Creation supplies the new record ID. Every output is atomic and new-only.
`--library-target N` defaults to10000 and accepts finite values in1..1e12.
`--work-bytes` accepts32..1024MiB; output uses `--csv-bytes` (default256MiB,
maximum1GiB). Work budget does not change the scientific identity. The existing
project/save-snapshot limits also apply. No desktop or optional Python exposure
is added by this slice.

## Biological units and statistics

The source feature universe is never reduced for this method. All A02 cohorts
must belong to one of the two named groups. Disjoint cohorts with the same
replicate ID are summed in exact checked uint64 arithmetic before normalization.
Their sample and group must agree. A biological sample may map to only one
replicate ID, and a replicate may map to only one sample and group. The method
does not accept paired or repeated subjects across groups. Labels are explicit
user declarations; software cannot establish independence from invented labels.
Empty cohorts and zero-library replicates are refused, with no silent subject
exclusion. Cells, spots, regions and technical plates are not independent units
when they belong to the same biological replicate.

For gene f and replicate r, compute the full-universe raw library total Lr, then
`y[f,r] = log1p(double(raw[f,r]) / double(Lr) * library_target)`.
Raw counts and Lr remain exact integers; only this declared transform enters
floating arithmetic. Group means and unbiased sample variances are over
biological replicates with equal weight. The signed effect is
`mean(y[B]) - mean(y[A])`, in natural-log1p units. It is not log2 fold change.

For nA,nB>=2, positive standard-error variance gives
`t = effect / sqrt(sA²/nA + sB²/nB)` and the Welch–Satterthwaite degrees of freedom.
The two-sided p value is `2 * cdf(complement(StudentT(df), abs(t)))`, avoiding
subtraction of a near-one CDF. See the pinned
[Boost.Math distribution documentation](https://www.boost.org/doc/libs/1_88_0/libs/math/doc/html/math_toolkit/dist_ref/dists/students_t_dist.html).
BH adjusts **every testable gene in the complete retained source universe**,
with no expression/effect/significance prefilter. Stable source-index ordering
settles p-value ties; the reverse cumulative minimum bounds q in[0,1].

| Status | Interpretation |
| --- | --- |
| `tested` | Both groups have at least2 independent replicates and positive standard-error variance; t,df,p,q are present |
| `all-zero` | All replicate raw counts are zero; t,df,p,q absent |
| `insufficient-replicates` | Either group has fewer than2; t,df,p,q absent; its sample variance is also absent when n<2 |
| `zero-variance` | Both computed group variances are zero, including different constants; t,df,p,q absent |
| `tail-underflow` | A positive-variance finite test rounded its tail to zero in double arithmetic; p=q=0 is explicitly flagged and included in BH |

Insufficient replication takes precedence over per-gene degeneracy. Nonfinite
statistics and raw count/library overflow refuse the operation. The method is
an exploratory unpaired test on this declared transform. It has no count-model,
paired-study, covariate-adjustment, DESeq2-compatibility or biological-validation
claim.

## Identity, persistence and native boundaries

`qpx.expression-differential/1` records method `welch-log1p-pseudobulk/1`,
`Boost.Math-1.88.0-standalone/double`, ordered groups, target, preprocessing,
effect definition, correction universe, degenerate policies, source/pseudobulk
identities and an exact result digest. Recipes are checksummed canonical ASCII,
chunked under `qupathxx.expression-differentials/1` in native image properties;
retention/removal participates in native undo. The retained A02 record is required.

Results use recipe replay: verify/export reopen the native source, replay A02,
then recompute A04 and require the exact stored result/recipe identity. They do
not silently accept numerically close replacements after backend/source changes.
Unlike A03 fitted models, this contract does not store duplicate floating arrays
or introduce another sidecar format. Owned source relocation follows A02/project
storage. Missing/changed parents, modified source bytes, malformed records,
budget exhaustion and cancellation refuse publication. IDs, means, variances,
t/df/p/q, native method/backend, group sizes and BH universe accompany gene CSVs;
replicate CSVs retain raw libraries and contributing cohort associations. Floating
CSV values use17 significant digits; absent statistics are empty fields.

The public headers are `qupathxx/expression_differential.hpp` and
`expression_differential_workflow.hpp`. CLI parsing only constructs typed native
options and displays native reports. All validation, merging, statistics,
identity, project persistence and export are in the shared native services.
Accounted work includes the parent result, merged count buffer, result rows and
BH ordering workspace; this is an allocation reservation, not a process-RSS claim.
Existing A02 limits bound cohorts to64 and total memberships to65536.

## Dependency and real-data admission

`QUPATHXX_BUILD_STATISTICS=OFF` is the default. Disabled builds provide an explicit
native fit refusal and require no Boost. Enabled builds use only the standalone
Math include tree from tag `boost-1.88.0`, archive SHA256
`bb753392c3908292da46034a5435107f9e0546fda54da4d47b438aff5e4d9c48`.
The [standalone contract](https://www.boost.org/doc/libs/1_88_0/libs/math/doc/html/math_toolkit/standalone.html)
avoids the rest of Boost. [Third-party notices](../third_party/README.md) record
BSL-1.0 terms, exact license hash and installation path. No new DLL or statistical
runtime is introduced. Header notices and corresponding sources must accompany
the eventual SDK/release; final L01 staging remains separate.

The real check uses original-author **Tabula Muris Consortium / Chan Zuckerberg
Biohub** [FACS deposition5829687 version8](https://api.figshare.com/v2/articles/5829687/versions/8),
explicitly CC BY4.0. The original study is
[Single-cell transcriptomics of20 mouse organs creates a Tabula Muris](https://www.nature.com/articles/s41586-018-0590-4).
The admission is for the authors' deposited gene-count/metadata files, not a
third-party wrapper license. FACS.zip (file10700143,303966623B) SHA256 is
`7048a634d7beed307d5d6a77c2d38eb0a21e9006fd592f98f5a4eb6a8da56157`;
metadata_FACS.csv (file10842785) SHA256 is
`031ecca1ef8230006f0c0ef44a0d37ae2b4356a14c44253417e14fd40d1ecd6e`.

Only the complete **Kidney** table is used:23433 genes,865 cells, with no row,
cell or count filtering. The authors' plate metadata identifies female mice
`3_38_F`, `3_39_F` and male mice `3_8_M`, `3_9_M`, `3_10_M`, `3_11_M`.
F is group A and M is group B. This whole-tissue comparison includes cell
composition differences and only2 female subjects; it establishes numerical and
identity behavior, not a cell-type effect, biological discovery or power claim.

The development-only reference tool losslessly transcodes the original CSV to
the existing open Matrix Market/TSV input route, preserving every integer and
original gene/cell ID. Plate/mouse/sex labels are joined from author metadata,
with cell-ID mouse tokens checked. This is fixture preparation, not a shipped
CSV adapter or product analysis fallback. Native import/A02/A04 interpret and
validate their own input boundaries. A separate NumPy2.5.4/SciPy1.16.2 oracle
re-reads the original CSV and independently computes raw replicate sums,
[Welch tests](https://docs.scipy.org/doc/scipy-1.16.2/reference/generated/scipy.stats.ttest_ind.html)
and [BH adjustment](https://docs.scipy.org/doc/scipy-1.16.2/reference/generated/scipy.stats.false_discovery_control.html).
The authored small frozen JSON oracle makes native tests Python-free.
Original/derivative hashes and lineage are in
[the fixture inventory](../tests/fixtures/manifest.json). Large data and oracle
packages remain development evidence outside shipped package/fixture inputs.

See [the source-scoped receipt](benchmarks/feature-expansion/README.md#a04-replicate-level-differential-expression)
for decisive checks and exact product/source identities.
