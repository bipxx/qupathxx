# Selected native COCO annotation workflow

The [COCO plugin](../../../plugins/coco/README.md) imports polygon unions and
compressed/uncompressed RLE into one exact pixel-cell geometry per source
annotation. A separate ABI5 symbol carries a full QBM1 canvas; native frame
validation rejects mismatches even when foreground bounds would fit. Existing
native label/grid/contour, class, preview, atomic apply, undo and project services
remain the scientific authorities. Source identities/crowd/area/bbox/metadata
persist independently; no file/URL fetch, resampling or calibration inference.

Source-scoped local Windows x64 evidence is under
`build/gui-aggregation/catalogue-closure/coco/`. It selects this workflow, not
full COCO-schema/dataset, QuPath, biological, performance or platform acceptance.
The broader formats AND application/integration goal remains active.

The primary `validation-02/validation.json` passes32 supported/65 refused cases
and19 native/CLI/persistence commands.24 independent authored polygon/compressed
RLE masks agree with the pinned original COCO C oracle. Native checks compare
every unit-cell intersection and pixel center, exact area/holes/parts, IDs above
2^53 through UINT64_MAX, category mapping, whole-canvas matching, source metadata,
plane choices, budgets, parse cancellation and altered FP-control refusal.
Stored geometry/properties survive apply/undo/redo and exact save/reopen.

`compatibility-01/validation.json` passes50 legacy controls, including the exact
original ABI1 preview, ABI2 geometry and28 ABI3 malformed-source controls, native
lock semantics and frozen WKT/ImageJ results. `host-faults-v2/validation.json`
adds21 controlled ABI3/4/5 mask cases: old ABIs refuse the encoding; ABI5 checks
malformed coverage, canvas, flags and locked-result persistence.

The staged source builds the independent COCO module and installed public SDK
consumer offline. Its separately archived module passes12 supported/8 refused
profiles and19 workflow checks;17 installed SDK/legacy/coexistence checks pass
with five loaded providers. Root optional targets configure with Python OFF and
without Python/nanobind discovery. Exact source/module/native/package/ZIP/removal
identities and owned-file/user-output checks are in the final `results.json`.

The FP04 base core and runtime remain frozen. Its archive predates the delivered
label bridge, so the overlay reuses the existing exact PNG-label LabelRaster and
cleanup contour objects, after checking all12/55 recorded source inputs. Their
identities/source receipts are in `mask-object-reuse.json`. No new contour kernel,
core, bindings, wheel, GUI or published package is built. The new host/CLI has its
own source/binary identity; earlier qualification is not transferred to it.

Retained failures: the first link exposed the missing frozen-SDK label symbols;
two compile attempts corrected new-consumer API/header/profile spellings; one
test baseline wrongly expected no undo entry after intentionally editing a class.
Compatibility staging needed owned output directories; mock providers needed
distinct format IDs. Logs, source/available binaries and previous passing products
are preserved. Corrections did not relax numeric, source or stored-identity checks.

Code terms: native/application/shared JSON AGPL-3.0-only, bounded COCO mask port
and pinned code references BSD-2-Clause. Native GEOS/interface dependencies retain
their existing separate licenses/notices and source availability. Only independently
authored fixtures are used. API/code terms do not license COCO images, datasets,
models or their derivatives. Each component keeps its actual notices, source
availability and source/distribution obligations.
