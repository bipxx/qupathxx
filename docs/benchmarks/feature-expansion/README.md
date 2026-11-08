# Native feature-expansion progress — 2026-11-08

The [22-item roadmap](../../feature-expansion-plan.md) remains the completion
authority. This receipt covers S01/V01/I01 locally, plus an incomplete L01 guard
foundation. No expanded GUI, release, speed or biological-accuracy claim.

S01 adds an immutable native ExpressionDataset over the existing imported point
projection, with stable IDs/positions/frame/plane/source identity, exact uint64
counts or declared continuous values and explicit validity. Legacy projection
admission remains bounded by2^53. Captures own their buffers and survive live
document edits. Read-only content identity is not live publication authority.

V01 adds shared gene-display requests/fields, linear/log1p display, native minmax/
interpolated percentile/manual ranges, explicit zero/missing colors, native
palettes/opacity and a bounded owner-thread LRU. Raw buffers remain exact and
unchanged; transformed values are presentation only. Manual ranges support a
shared scale. Cache identity includes dataset, feature, row order and applicable
display policy. Cancellation is checked even on a cache hit. Capacity accounting
is conservative buffer/work accounting, not process-RSS qualification.

I01 adds read-only owned import previews and a cancellable native job. Existing
CLI import calls the same preview/publication authority and reuses the parsed
projection. Preview checks the strict pixel/ID profile, dimensions, selected genes,
plane, image bounds, output and resource limits. Native file-version guards reject
stale matrix/ID/position/project inputs before atomic no-overwrite publication.
Exact source hashes, locators/options and recipe lineage survive project reopen;
successive imports retain prior recipes. No second parser or Python workflow.

Decisive local checks in the existing gridfrontends167-package Release cache:

- `qupathxx_expression_tests`: analytical counts, full uint64 ownership, measured
  zero/missing/continuous domains, stable versus edited identity, legacy boundary,
  malformed cohorts, source ambiguity, wrong planes, ranges/colors/log1p, empty/
  constant/extreme/all-missing fields, budgets, cache eviction, cancellation,
  controller ownership and native project save/reopen.
- Its real-data argument opens the admitted mouse-brain project:2,702 spots,
  six markers, all measured; all marker fields compute and counts retain identity.
- `ctest -C Release --output-on-failure --no-tests=error -R
  '^(expression|spatial_counts|spatial_counts_cli)$'`: three affected cases pass.
  Import cases cover read-only/async preview, publication, retained recipe lineage,
  changed source/project refusal, collision, cancellation, strict parsing and the
  existing CLI/save/reopen route. No GUI or bindings compiled for these changes.
- `python -m pytest tests/test_material_staging.py -q`: two cases pass, one Windows
  symlink-creation case unavailable. The test covers exact existing font bytes,
  missing inventory, restricted/unreviewed/NC declarations, restricted source/path,
  restricted-derived lineage, missing/cyclic parents and excluded pools.

L01 reads added material metadata in existing inventories rather than a second
license database. Unknown files/terms refuse admission. Original font lineage and
CC0 downloaded fixture records are annotated. The offline checker is development
tooling, with no product Python dependency. Complete enabled-dependency/source-kit/
package membership and the actual release-stage integration remain pending;
historical archives are preserved. Restricted datasets/weights were not downloaded.

Waste review: native-only targets reused the existing cache; an extra affected
build covered newly added persistence/recipe checks. Source provenance intentionally
regenerates CMake when compiler inputs change, so settle each batch before build.
No binding rebuild, second engine cache, full scientific campaign or packaging.
