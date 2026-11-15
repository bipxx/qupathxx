# Molecular analysis in Genes

The Genes tool provides one native desktop route through full-source QC and
normalization, regional summaries/pseudobulk, PCA/clustering and replicate-level
differential expression. Computation, identity, persistence and exports use the
same C++ services as the CLI and SDK; Python is optional.

The standalone Windows handoff is described in
[the candidate guide](windows-molecular-desktop-candidate.md).

## Choose the source and values

Open a project containing an admitted indexed expression matrix. The Genes and
QC tabs use the selected spatial map. **Regional summaries** uses its full
matrix, even when the overlay shows only one or two genes. For **PCA / clustering**
and **Differential expression**, choose the **Full analysis matrix**. An already
open matching Genes matrix is reused; otherwise choose **Open analysis matrix**.
These two methods also work on matrices without spatial point associations.

Original matrix values are used unless a kept normalization is selected
explicitly. Fit results show their native value domain and method. Selecting
display genes never changes the analysis universe or normalization denominator.
The matrix and observation coordinates remain unchanged.

## Regional summaries and pseudobulk

Select area annotations on the current plane, choose **Use selected regions** in
**Regional summaries**, and edit the cohort identifiers. Ordinary summaries
reject overlaps by default; allowing overlaps and using accepted registration
are explicit choices. Empty regions have zero sums and undefined means.

For **Pseudobulk**, enter biological sample, replicate and group labels for every
region. Run and inspect the raw results, then choose **Keep aggregation**.
Pseudobulk always uses disjoint memberships and the complete raw-count universe.
All sums remain UInt64; no floating conversion is used for count reduction.
See the [regional guide](expression-aggregation.md) for membership contracts,
summary/membership CSVs and historical cohort replay.

## PCA and clustering

Choose the full matrix in **PCA / clustering**. **Center features (PCA)** selects
centered PCA; clearing it selects uncentered SVD. Set the component count, cluster
count and complete UInt64 random seed. **Advanced fit options** exposes
oversampling, power iterations, the clustering iteration limit and tolerance.
The default native work and operation bounds still apply.

Choose a kept normalization explicitly when required, then **Run PCA /
clustering**. Direct raw integers above2^53 are refused by the native binary64
fit contract. Constant/insufficient-rank inputs, invalid dimensions, changed
parents and budget failures remain visible. The previous preview survives a
failed or cancelled fit.

**Next run settings** remains a draft after opening a kept result. Results show
the stored method, domain, feature/observation counts, component/cluster counts,
complete seed and convergence status. The scatter view shows components1/2; a one-component fit uses zero
vertically. At most the first4096 observations are drawn, with the limit shown
when applicable. Hover shows exact observation ID, cluster and round-trip scores.
**Observation index (0 based)** can inspect every observation and every fitted
component. All observations remain in exports. Cluster numbers are exploratory
labels; they do not identify cell types, biological samples or replicates.

Choose a new `.qpr` **Fitted archive** path and **Export fitted archive**, then
**Keep fitted result**. This explicitly writes the native exact fitted arrays
before the project association is added. Keep performs the existing bounded
native archive validation on the owner thread. Saving the project copies owned
archives beside it. **Open fitted result** opens the retained exact arrays and
checks the parent; it does not refit. **Verify fitted result** explicitly refits
through the native backend and requires exact fitted identity. Numerical
closeness alone is never substituted for identity. **Remove fitted result**
changes project metadata and is undoable.

**Export embedding CSV**, **Export loadings CSV** and **Export scatter SVG** use
the existing native writers. CSVs contain all observation/feature associations
and fitted components with round-trip floating values. The SVG includes the
native method/domain, energy labels and cluster metadata. See the
[method/archive guide](expression-reduction.md) for numerical scope and limits.

## Differential expression

First keep a raw pseudobulk with explicit biological units. In **Differential
expression**, choose the matching full matrix and kept aggregation, then
**Open pseudobulk**. A regional summary or a pseudobulk from another selected
matrix is refused. The opened record is identified separately from the chooser;
**Pseudobulk labels** shows its captured cohort/sample/replicate/group labels.

Enter **Group A**, **Group B** and **Library target** explicitly, then **Run
differential expression**. Repeated disjoint regions of the same replicate are
summed in exact raw counts before normalization. Sample and replicate identifiers
must be one-to-one and assigned to exactly one group. User-supplied labels are
declarations; software cannot establish biological independence. Empty cohorts,
zero libraries, invalid groups, conflicting identities and overflow fail visibly.

The native method is an unpaired, two-sided Welch test on log1p-normalized
replicate pseudobulk, with BH correction over all testable genes in the complete
source universe. The displayed effect is **mean log1p B minus A**, not log2 fold
change. Results show the stored library target, biological replicate counts and
the correction universe, separately from next-run input drafts.
Use the feature index to inspect native status, means, effect,p andq. Missing
statistics stay absent, including insufficient replication, all-zero genes and
zero variance; they are never shown as artificial zero p values. Tail underflow
is labeled explicitly. **Replicate associations** shows contributing biological
units and exact raw libraries/observation counts.

**Keep differential result** records the native recipe against its kept
pseudobulk. Save the project normally. **Reopen differential result** reopens the
parent and recomputes/validates the exact saved result identity. **Remove
differential result** changes metadata and is undoable. **Export differential
genes CSV** and **Export differential replicates CSV** use the native writers;
both preserve result/parent associations. Removing the required parent pseudobulk
makes later DE reopening fail visibly. See the [DE guide](expression-differential.md)
for the precise statistical contract and admitted numerical evidence.

## Jobs, files and delivery scope

Run/open/verify/export use owned worker inputs. **Stop** cancels work; closing the
Genes window hides it and does not cancel. Editing inputs or switching context
does not attach an old preview to a new image. Keep refuses stale context and
duplicate records. Keep and Remove each use the native undo authority; export
files remain external files.

Every export is atomic and new-only. Choose a new path when a file exists;
project/source/recovery paths and aliases are protected. Shared path fields show
the resolved destination and support the common picker/drop behavior. Historical
previews export their captured values rather than silently selecting new inputs.
Requests use256MiB of native work with the existing per-method bounds.

The local Windows frontend candidate, exact sources, evidence and limitations
are in the [end-to-end receipt](benchmarks/molecular-desktop/README.md). This
selected workflow does not imply QuPath parity, biological validation or a new
published SDK/wheel release.
