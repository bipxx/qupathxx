# Project-entry tags and description (ENTRYDETAILS1, candidate pending)

Source implementation is connected; product build/verification is pending. Both
attributes extend existing ProjectImage/Project state, independently of entry
metadata, ImageData properties and root/ROI facts. The
[pinned supplement](../tests/fixtures/qupath-entry-details-0.7.0/manifest.json)
reuses a captured builder without a server/pixel/ROI read. It proves null/empty
description and first-occurrence insertion order for unique tags after QuPath
save/reload, complementing the earlier captured ordinary description/tag case.

`ProjectImage.description` and `Project.image_description(id)` are owning
`str | None` values. None and an explicit empty string remain distinct. Tags are
owning lists from `ProjectImage.tags`/`Project.image_tags(id)`: unique membership,
first insertion order, exact case/UTF8/normalization forms and empty tags are
preserved. `Project.set_image_details(id, description=None, tags=[])` atomically
replaces both fields; omissions clear both. Duplicate input tags keep the first
occurrence. Equal normalized fields are no-ops; changing order is a state change.
Updates dirty the project, without changing document revisions or analysis state.

Descriptions allow up to1MiB UTF8; tags accept up to4096 inputs, each4096 UTF8
bytes, with1MiB estimated unique-tag payload. NUL, invalid UTF8, non-string
values, excessive input/count/payload and unknown IDs refuse atomically. Source
absence/null map to None/empty tags; explicit empty description stays empty.
Malformed source shapes or members refuse before publication. A null tag member
is outside the supported string-tag contract and refuses; no coercion/drop occurs.

Schema24 adds sparse image_details and ordered image_tags rows. None plus no
tags omits details; empty description is a real row. Loading validates nullable
TEXT description, bounded UTF8, unique contiguous tag positions/membership,
counts/payload, duplicate details and parent identities. Legacy schemas open with
None/empty tags and upgrade transactionally. Existing snapshots copy and account
both attributes. No binary codec, extra native model or document history is added.

Plain manifests, v1 GeoJSON and v2 GeoJSON/native geometry routes share the same
entry import authority/source guards. Existing sidebar sections show a bounded
read-only description viewport and clipped ordered tag rows/full hover values.
Python supplies atomic editing for this slice. Metadata-map values keep their
own namespace and getter/setter behavior.

Verify captured/independent values, optional defaults and duplicate order, owning
getters/listings, no-op/clear and failed pair updates, exact limits, each attribute's
snapshot budget/ownership, corrupt/orphan store rows, legacy23 rollback/upgrade,
actual geometry and stale/missing-source behavior, changed-state desktop save,
installed SDK/stubs/typing and affected project/import/native migrations before
scoped acceptance. Broader global metadata/formats/research/resources/support/
release remain queued; no general speed or full QuPath parity claim is implied.
