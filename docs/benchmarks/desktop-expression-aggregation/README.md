# Desktop regional summaries and pseudobulk

Local Windows x64 frontend candidate,2026-11-15. Source starts at main
`cee52459b15c9da57117742705a7823cfb59e2ba` plus the uncommitted frontend changes
listed below. This is a selected desktop workflow, not a new scientific kernel,
SDK/wheel release, biological acceptance or QuPath-parity claim.

The Genes tool now captures explicitly selected area annotations and cohort
labels, invokes native A02 regional/pseudobulk services on owned worker inputs,
displays exact results and provides Keep/reopen/remove and both protected CSV
exports. GUI, CLI and SDK share membership, normalization/domain, identity,
persistence and export authorities. See the [guide](../../expression-aggregation.md)
and retained [native A02 evidence](../feature-expansion/README.md#a02-regional-expression-summaries-and-pseudobulk).

## Build and retained products

Frontend-only MSVC19.44.35225.0,Release `/MD`,C++17,UTF-8,ImGui193 with32-bit
glyphs. `build/gui-aggregation/build_frontend.py` remaps the retained compiler
commands, compiles current local GUI/app/test units, reuses unchanged GUI backend
objects and links the retained FP04 SDK dependency closure. It does not configure
or rebuild core, bindings or packages. Commands, response files and compiler logs
remain under `build/gui-aggregation/`.

```powershell
.venv\Scripts\python.exe build/gui-aggregation/build_frontend.py
build\gui-aggregation\runtime\Release\qupathxx_gui_expression_aggregation_tests.exe
build\gui-aggregation\runtime\Release\qupathxx_gui_normalization_tests.exe
build\gui-aggregation\runtime\Release\qupathxx_gui_gene_tests.exe
```

The runnable local candidate is
`build/gui-aggregation/runtime/Release/qupathxx_app.exe`, with matching GUI assets
and runtime DLLs alongside it. The retained FP04 core library remains SHA-256
`4bf957c9ea6e46294c11b9bfb7d9c2496cf4fca607a9c28ba837768d301eb6bb`.
All71 frozen/current products from the storage-cleanup protection manifest retain
their exact hashes; see `build/gui-aggregation/frozen-products-after.json`.

## Decisive results

- `gui_expression_aggregation`: passed through actual Genes tab/capture/run/Keep/
  reopen/remove/export buttons. Authored full indexed3-feature/5-observation
  matrix, while the overlay displays only one gene. Literal expected memberships
  and sums include `9007199254740994` and `18446744073709551615`; empty means,
  missing pseudobulk labels, overlapping memberships, cancellation, blocked and
  stale Keep, cross-image reset, one-undo metadata and saved exact replay pass.
- Both actual CSV exports match native writer bytes; exact large integers are
  present. Existing destinations refuse without replacement dialogs. A protected
  nonexistent path refuses. Original matrix bytes remain unchanged.
- Existing `gui_normalization` and `gui_gene`: passed, including native overlay
  coordinates/colors, raw/QC/mode reuse, workspace persistence and saved replay.
-58 source-contract tests passed across design, translation, layout, export,
  notification and vocabulary checks.38 added captions each have15 translations;
  duplicates, conversion signatures and shared path wiring are checked.
- New app project startup smoke: passed against the saved authored project,
  with its own `build/gui-aggregation/app-smoke.ini`. No native Python dependency.
- Normal and French200 rendered captures were inspected: exact count values and
  translated controls are readable; no horizontal layout overflow. Vertical
  scrolling at200% is expected. These check usability, not pixel equivalence.

Authored inputs, saved project, CSV references and screenshots remain in
`build/gui-aggregation/gui-aggregation-434356136847600/`. Other GUI regression
fixtures remain beside that directory. The source diff and original HEAD are
preserved in `in-progress-before.diff` and `before.json`; final source/product
identities are recorded in `results.json`.

The first cheap test compile caught a missing mutable worker closure; fixed
before the large GUI unit compiled. One source lint initially assumed path fields
always draw in their owning file; it now verifies the explicit persistent-owner
to plane-view wiring while retaining exactly one draw per field. No broad native
or platform campaign was repeated. PCA/clustering and DE desktop controls are
outside this delivery. No commit, push, package or publication was requested.
