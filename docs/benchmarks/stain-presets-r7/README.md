# R7 PRESET1 trace

2026-11-05, sourcee6a7d791. Existing ImageAnalysisSettings owns type, full basis,
profile/channel names, white/residual, history and schema20 save/reopen. GUI can
inspect/adopt saved defaults; it renders no authoring controls when defaults are
absent. MODEL1 file reuse is delivered. No native named-default factory or GUI
creation action exists; do not build another settings/result/persistence model.

Pinned QuPath0.7 StainVector/ColorDeconvolutionStains sources have two defaults,
H&E and H-DAB. [Source identity](upstream-sources.json) records exact pinned raw
URLs/SHA256 and cached-byte equality. The web fetch cache missed both URLs;
direct pinned downloads matched the existing copies. The old settings fixture
and probe hashes match their existing manifest. [Reference](reference.json)
selects its two already captured normalized profiles and independently computes
numeric model hashes with Python hashlib/struct. No new QuPath runtime/corpus.

Selected gap: explicit native/SDK named defaults, GUI save-default selection and
CLI preset model selection through current settings/transform/history/batch paths.
The [contract](../../stain-presets.md) keeps old implicit constructors, requests
and numeric CLI/GUI defaults unchanged. Use a separate public factory header;
no new DTO/project schema or common GUI option/report/replay fields are needed.

Waste review: reused saved-settings and MODEL1 behavior/fixtures; no blanket
registry implementation. Cache miss required a direct pinned-source fallback,
not a moving-branch lookup or whole download. Earlier runtime tree.json was not
used as a Git tree. Bounded owner reads avoid repeating the prior broad audit.
Next native factory/golden checks, then thin consumers and one installed route.
