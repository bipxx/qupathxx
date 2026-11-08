# Native feature-expansion progress — 2026-11-08

The [22-item roadmap](../../feature-expansion-plan.md) remains the completion
authority. This receipt covers S01/V01/V02/I01/I02/R01 locally, plus an incomplete L01
guard foundation. GUI claims cover only the named gene workflows below; no new
release, speed or biological-accuracy claim.

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

R01 continuation: native CoordinateFrame/Affine2D/RegistrationRequest and a
versioned typed registration definition preserve original coordinates/counts.
Explicit matrices cover translation/rotation/scale/reflection, inverse/composition
and calibrated anisotropic pixel/µm conversion. Ill-conditioned/nonfinite transforms
and unknown physical calibration refuse conversion. Linked source identity reuses
verified primary/companion versions and series; embedded RGB hashes exact bytes.
Neither display windows nor process generations enter persistent frame identity.
Acceptance recaptures the point cohort and target frame before the existing
undoable image-data-properties command. Separate transformations retain outside
points and save/reopen without rewriting source observations.

Registration/expression CTest:2 cases pass (0.21s); the admitted2,702-spot real
project also passes mapping/acceptance with unchanged coordinate/count identity.
Fixtures check analytical transforms/inverse/composition, unit scale, numerical
limits, budgets, cancellation, stale counts/geometry/image/calibration, immutable
metadata, native undo/redo, persisted record validation and linked/lazy reopen.
No interactive landmark-registration or GUI claim is made from this native check.
The companion OME-TIFF real example additionally passes a display-window change
with identical frame identity, checking the raw/display boundary on the real
two-channel source. Original real projects were opened read-only and not saved.

V02 continuation: Analyze → Gene expression (also Workflow → Tools) opens the
native gene selector. Search imported IDs/symbols and page the catalogue; enable
spot-center colors and choose linear/log1p, minmax/percentile/manual ranges,
palette/opacity and glyph size. The legend reports the actual transformed units,
range, measured zeros, missing values and clipping. Hover gives exact raw counts.
Compare opens two image canvases with one shared navigation state; shared
automatic percentiles use pooled measured values, never the extrema of separate
percentiles. Constant/missing fields retain their semantics. Presentation/source
choices live in workspace preferences, without rewriting project measurements.

Native GeneViewSession owns immutable snapshots and one worker with latest-
generation coalescing/cancellation. UI uses native fields/palette colors. New
overlay fields draw outside the geometry cache, so cached geometry cannot retain
an old gene color. Comparison tile/context ownership stays on the GUI thread.

Final native expression/gene_view CTest:2 cases pass (0.55s). Independent pooled
percentile oracle is [0,2,10,100,200,300,400] → P25=6/P75=250; checks include
constant/all-missing, raw integer preservation, rapid switches, stale generations,
cancel/owner thread, catalogue paging and exact native preference/source round trip.
Native GUI replay passes exact colors at scientific point locations, zero/missing,
source persistence with two lineages and a40-screen-pixel drag measured in both
comparison maps while the main camera/project remain unchanged. Real Aqp4/Gad1
comparison covers2,702 spots with a shared log1p scale and unchanged source identity.
Evidence: workspace-root build/feature-expansion/gui-v02-r3; first failed oracle
capture remains in gui-v02 and the intermediate passing comparison in gui-v02-r2.

The first oracle incorrectly treated Viewport::origin as image coordinates;
native image_to_screen resolved it, and comparison drag now uses screen translation
too. Source persistence/drag checks were added for named final-review gaps.
Only affected cached targets compiled; Python bindings, packages and historical
science qualification were not rebuilt. The tool reuses the completed layout's
bounded-window/collection helpers; other concurrent UI source is preserved.

I02 continuation: Analyze → Import spatial expression (also Workflow → Tools)
opens a four-step native wizard. Choose an image in a saved project or a PPM/TIFF
file, choose local Matrix Market/TSV inputs and a new output, declare ID/X/Y column
names and units, select genes from the paged native catalogue, then inspect mapped
spot centers over the image before import. Position rows follow matrix-column
order and feature rows follow matrix-row order; no ID/axis/unit guessing. Known
image calibration is required for µm conversion; explicit scaling/offsets apply
after that conversion. Original coordinates, units, source hashes and the full
recipe are preserved separately from image-coordinate point geometry.

Catalogue and import reuse the same native feature parser; UI uses the same
native search/page function as gene display. SpatialImportJob owns catalogue,
preview and publication work. Preview owns its image/coordinates and never mutates
the live study. Its verified source identities refuse stale files/catalogues.
Import requires a completed image preview and a new output; Open delegates to the
existing app loader/unsaved-work protection. Local file choice scans bounded pages,
does no decoding/download and introduces no proprietary reader. This first storage
route embeds selected genes; full backed matrices remain the D01 work item.

Final native expression/spatial_counts/CLI checks:3 cases pass (1.37s); gene_view
passes separately for shared search compatibility. Added cases cover catalogue
identity, explicit header permutation, independently expected anisotropic µm/px
mapping, raw-coordinate persistence, direct PPM input and unknown-calibration
refusal. Existing exact-count/strict-profile/no-overwrite/stale/cancel cases remain.
Repeated imports now have recipe-scoped cohorts selectable independently; old
objects without that optional scope retain their exact previous identity. Scope
distinguishes import occurrences, not biological replicates.

Native GUI replay clicks the actual Import and Open controls, verifies exact
retained counts and mapped/original coordinates, unchanged prior project/live
state, and the canonical new-project path handed to the loader. It also imports
the admitted real2,702-spot/six-marker demo without a shell. Captured overlay/values
are readable. Final evidence: workspace-root build/feature-expansion/gui-i02-final;
initial passing GUI evidence remains in gui-i02. A missing /utf-8 option on the new
MSVC test target caused the initial compiler failure; the translated sources were
unchanged. No binding build, new dependency, new engine cache or package campaign.
