# Project-wide string metadata

Candidate contract for R5-PROJECTMETA1; verification pending. This is independent
of image-entry maps, descriptions/tags and image/ROI facts. Pinned QuPath0.7.0
DefaultProject owns Map<String,String>, persists data/metadata.json and reloads
it separately. The project-only fixture reuses a captured builder without image
server, pixel or ROI reads. Rich/empty/zero-image projects have independent saved
references, including exact UTF8, empty key/value, newline/tab, normalization
forms, integer-looking strings and a same-named entry key with a different value.

Project.metadata is an owning read-only getter; set_metadata replaces atomically,
with omitted/default map clearing all pairs. Equality ignores insertion order;
an exact no-op does not dirty Project. Changes do not change Document revision or
analysis. No implicit typed conversion or Unicode normalization occurs. The
shared string-map validator admits up to4096 pairs,4096 UTF8 bytes per key/value
and1MiB estimated map allocation (map header plus128 bytes per pair plus text).
NUL/invalid UTF8/non-string/excessive input refuses before replacement.

Schema25 adds normalized project_metadata key/value rows. Sparse empty state
writes no pairs. Legacy schemas default empty; bounded streaming load rejects
bad types/UTF8/count/payload/duplicate keys. Save/snapshot/recovery workspace
adoption own the map and count it in budgets. Failed migration/write preserves
the prior project. Metadata remains available without opening any image.

The common plain/assisted importer binds data/metadata.json to the existing file
review. A missing file defaults empty; a present non-object or non-string map
refuses. Truly zero-image source projects can import and retain global state;
a nonempty source project whose images are all unsupported/unavailable still
refuses. The existing editor supports project mode from the project sidebar,
including zero images, with metadata-only bounded drafts, reload, Apply/Cancel,
stale project/map checks and separate entry authority. No duplicate codec/model
or validation authority is introduced.

Tests/candidate results are pending. Do not treat this document or captured
reference as proof that the changed native product is qualified. Broader
versions/builders/records/class hierarchy, biology/resources/hosts/release remain
in the canonical queue.
