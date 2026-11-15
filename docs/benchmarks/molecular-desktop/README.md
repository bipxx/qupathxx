# Molecular desktop workflow, end to end

Delivered locally on Windows x64,2026-11-15. Main starts at
`cee52459b15c9da57117742705a7823cfb59e2ba`; the tested candidate changes are
identified in `build/gui-aggregation/results.json` and its source snapshot.
The [user guide](../../molecular-desktop.md) covers the complete selected outcome:
full-source QC/normalization, ROI summaries/pseudobulk, PCA/clustering and
replicate-level differential expression in Genes.

## Completion evidence

| Requirement | Decisive evidence |
| --- | --- |
| Shared native authorities and full source universe | Controllers call existing typed C++ services on owned worker inputs. The connected3-feature/4-observation fixture displays one overlay gene but fits all3 features; actual QC totals are1000 for every observation. Native A02/A03/A04 scientific services remain the authorities. |
| Explicit normalization and method | Actual QC/log-normalization/Keep controls feed the new normalization checkbox. The fit retains that exact full-source recipe/domain. Explicit uncentered SVD is exercised, then the original centered raw fit identity is restored. |
| PCA options and readable results | Native components/clusters/complete UInt64 seed and advanced controls are exposed. Authored means and sign-invariant score magnitude match analytical expectations. Results show actual stored parameters separately from next-run drafts. |
| Exact fitted lifecycle | Actual archive export, Keep/one-undo, Remove/Undo, project save/open, exact stored-array open and explicit refit verification pass. No open operation silently refits. |
| Pseudobulk to DE | Actual selected-ROI capture/Run/Keep creates4 explicit independent declared subjects. DE opens that kept pseudobulk; empty group inputs refuse, then explicit A/B groups and target1000 produce2 replicates each. |
| Native statistics and missing values | Analytical gene0 effect is ln2, with p=`1-1/sqrt(2)`. The full-source correction universe has2 testable genes; gene2 is all-zero with absent p/q. Native A04 numerical/status/replicate-consolidation evidence is reused. |
| DE lifecycle | Actual Keep/one-undo, Remove/Undo, native saved-recipe reopen and project save/open preserve exact identity. |
| All outputs | Actual embedding/loading CSV, scatter SVG and DE gene/replicate CSV bytes match native writer outputs. Existing destinations and a protected nonexistent path refuse without replacement dialogs. A02 summary/membership exports pass their existing GUI test. |
| Context, jobs and persistence | Both new workflows exercise blocked/stale Keep and cancellation without stamping old previews fresh. Actual Genes window closing/reopening preserves the fit. Cross-image retirement with an active export leaves no dangling watcher; nonspatial retained-matrix PCA and DE routes pass. |
| Scientific input preservation | Authored original matrix bytes remain exact. Existing Genes tests retain native point registration/colors and unchanged raw counts; A02 retains exact UInt64 values above2^53 and up toUINT64_MAX. |
| Usability/localization | Normal/French200 captures inspected; no horizontal layout overflow. Stored seed/target are drawn exactly despite different next-run drafts.130 new captions across the combined A02/A03/A04 work each have15 translations. Vertical scrolling at200% is expected. |
| Native application | Final app opens the saved workflow project in smoke mode with PATH restricted to runtime/System32 and Python environment variables removed. Input project bytes remain exact. |

Final connected fixture/captures:
`build/gui-aggregation/gui-molecular-438133752242600/`.
The related final A02 fixture is
`build/gui-aggregation/gui-aggregation-437522405444300/`.
`molecular-final.log`, `gui-tests.json`, `native-reduction-verified.log`,
`high-seed-compatibility.log` and `app-smoke.json` hold the local results.

Four GUI test executables pass: `gui_molecular_workflow`,
`gui_expression_aggregation`, `gui_normalization` and `gui_gene`.
The affected native `expression_reduction` regression suite passes, including
its independent oracle, cancellation/bounds, seeded exact archives and native
CLI/CSV/SVG routes.58 design/translation/layout/export/notification/vocabulary
source contracts pass. The connected test skips explicitly with CTest code77
when required optional native capabilities are disabled; this tested candidate
has project/matrix/algebra/statistics enabled and executed every assertion.

## Native correctness repair and frozen products

The connected test found a pre-existing archive-boundary bug: fitting accepts
UINT64_MAX as a seed, but mixed signed/unsigned JSON comparisons rejected its
saved archive. The native integer helper now compares explicitly typed values.
Negative, float, string, boolean and over-bound inputs still refuse. New native
regressions cover0,INT64_MAX,INT64_MAX+1,UINT64_MAX and exact high-seed archive/
refit identity. The previously failing project also opens and refits with its
original exact fitted identity under the corrected candidate.

Frozen SDK core SHA-256 remains
`4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb`.
The distinct corrected candidate core is
`c6467b5c3580dee5282565af1abd597750eef7d9cb0749235bad2d89efde25b1`.
Exactly five reduction members were rebuilt with the retained native compiler
commands and replaced in a new library; all other members were retained.
`native/build.json` records sources, objects, members and both library identities.
No public ABI, archive schema, scientific tolerance or fitted identity changed.
All71 protected frozen/current product hashes remain exact. Frozen earlier SDK/
wheel packages were not rebuilt; this fix is scoped to the new source/candidate.

## Build, corrections and handoff

MSVC19.44.35225.0,Release `/MD`,C++17,UTF-8,ImGui193/32-bit glyphs. The local
development controllers and exact response files are under
`build/gui-aggregation/`:

```powershell
.venv\Scripts\python.exe build/gui-aggregation/build_native_boundary.py
.venv\Scripts\python.exe build/gui-aggregation/build_frontend.py
build\gui-aggregation\runtime\Release\qupathxx_gui_molecular_workflow_tests.exe
build\gui-aggregation\runtime\Release\qupathxx_expression_reduction_tests.exe
```

The standalone app is
`build/gui-aggregation/runtime/Release/qupathxx_app.exe`; its matching runtime
DLLs/assets are alongside it. Development Python orchestrates compilation/tests;
the app and scientific services do not require Python. No new dependency,
restricted material, model or reader was introduced.

Costs/corrections are preserved in the current card and failure logs: a Windows
`near` test-helper macro collision; caption vocabulary/helper lint corrections;
the newly exposed native high-seed decode bug (the first test dereferenced the
failed open, now guarded); a retired forward-slash oracle path fixed in compiler
command remapping; one Windows executable-lock failure from an overlapping
relink, then sequential relink; and a render-driven stored-parameter clarification.
There were two combined GUI compilations plus scoped native/test repairs and
relinks. Existing native/binding objects and accepted broad qualifications were
not rebuilt/replayed. The A02 passing source/products were preserved before
extension in `a02-before-end-to-end/`.

The selected desktop plan is complete locally. It makes no broader QuPath
parity, biological independence/discovery, platform, performance or new package
publication claim. The owner's follow-up continuation selects the source commit;
Git history records its identity. Before that handoff, all22 source files matched
the passing checkpoint, all six executables/corrected native candidate and all71
protected product hashes were exact. Only these handoff notes then changed; no
product build was repeated. Push, SDK/wheel repackaging and release remain
unselected.
