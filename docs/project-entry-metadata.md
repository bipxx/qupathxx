# Project-entry metadata

R5-ENTRYMETA1 is [locally verified](benchmarks/entry-metadata-r5/README.md). Metadata lives on the existing ProjectImage/Project authority, separate
from ImageData properties, root/ROI facts, tags and description. The
[pinned capture](../tests/fixtures/qupath-entry-metadata-0.7.0/manifest.json)
contains three QuPath0.7 entries reloaded through ProjectIO, sharing an existing
parent source TIFF with no saved-data/ROI recapture.

`ProjectImage.metadata` and `Project.image_metadata(id)` return owning string-map
copies. `Project.set_image_metadata(id, metadata={})` atomically replaces the
whole map; omission/empty map clears it. Equal maps are no-ops. Entry metadata
changes project dirty state, without altering Document revisions or analysis
recipes. Earlier listing values remain independent after edits/project teardown.

Keys and values allow empty text and retain exact UTF8, case, line breaks and
normalization forms; numeric-looking strings remain strings. Both are bounded
at4096 UTF8 bytes, with4096 pairs and1MiB estimated payload per map. NUL, invalid
UTF8, non-string fields and excessive collections/payloads refuse before live
mutation. Source import accepts absent/null/empty maps as empty; other shapes or
non-string values refuse. Unknown entry IDs refuse.

Schema23 uses normalized sparse image_metadata rows with parent identity,
non-null TEXT key/value and a composite primary key. Loading checks types,
UTF8, count/payload incrementally, duplicates and orphaned images. Old schemas
open with empty maps and upgrade transactionally on save. Existing snapshots
copy entry values and account their map payload before publication. No new
binary metadata codec or document undo history is introduced; this follows the
existing project entry/name lifecycle.

Manifest metadata uses the same import path for plain projects and exchange1/2.
Existing manifest/source/hash guards cover changes between review and apply.
[Tags/description](project-entry-details.md) now have separate scoped retention;
global project metadata remains a distinct loss.
The existing image source/name panel provides a clipped read-only entry list;
full text is available on hover, with line breaks flattened only in row previews.
Python remains the replacement-edit route for this slice.

The receipt records exact values/IDs, namespace separation, owning listings/
getters, no-op/clear, invalid-input atomicity, snapshot ownership/budget,
malformed stored rows, schema22 rollback/upgrade, missing-source inspection,
source-stale/existing-destination preservation, actual changed-metadata desktop
save/inspection, installed API/stubs/typing and affected native/project/import
migrations. Rich maps also accompany retained saved geometry on actual v1
GeoJSON, v2 GeoJSON and v2 native routes. Optional absent/null maps retain empty
legacy defaults. Final observer hashes are distinct from compiled product inputs.

One cached candidate and one stub/install pass were used. This closes the short
entry-map engineering workflow only. Tags/description have their separate receipt. Subsequent
[GUI authoring](benchmarks/entry-editor-r5/README.md) uses the settled map/pair
setters with bounded drafts and stale/refusal guards. Global project metadata,
broader R5 formats/records/races and release/scientific/resource work remain open.
