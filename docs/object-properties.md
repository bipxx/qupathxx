# Imported object metadata and measurements

R5-PROP1 implementation is in progress; completion stays in [current task](current-task.md).
`ObjectProperties(metadata={}, measurements={})` owns bounded UTF-8 string metadata
and named binary64 measurements. `Annotation.properties` returns an immutable native
owner or None; returned Python dictionaries are detached copies. Shared owners stay
valid after edits, removal and project close. Total entries are limited to4096,
text fields to4096 UTF-8 bytes and estimated native payload to1MiB per object.
Names are nonempty; NUL/invalid UTF-8 reject. NaNs are canonicalized, signed zero
and infinities retained. These facts are historical values, never recomputed or
promoted to verified results by changing geometry/calibration.

```python
props = q.ObjectProperties({"note": "imported"}, {"Synthetic": 3.5})
document.set_object_properties(object_id, props)
document.execute([q.ObjectEdit.properties(object_id, None)])  # undoable clear
```

Ordinary geometry/name/class edits preserve properties. Property edits share the
existing atomic command/history authority; invalid compound edits publish nothing.
Merging objects with nonempty historical properties refuses until explicitly
cleared: there is no generally valid aggregation of imported measurements.
Snapshots retain immutable properties and include their payload in memory budgets.

Schema16 adds `object_properties(image_id,object_id,payload)` with an object foreign
key. QOP1 stores sorted maps and binary64 bits under a SHA-256 trailer. Bounds,
checksum, duplicate keys, trailing bytes, noncanonical forms and orphan rows reject.
Schemas1–15 stay readable without modification; save upgrades transactionally and
failed migration/write preserves the previous committed project.

GeoJSON reads/writes `properties.metadata` string maps and `properties.measurements`
numeric maps, with `NaN`, `Infinity` and `-Infinity` strings for special values.
Legacy measurement arrays of name/value pairs remain readable; duplicate names
reject. Unsupported fields/locks/custom structures retain explicit loss/refusal.
The [pinned QuPath adapter](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/io/QuPathTypeAdapters.java)
is the format reference. The native saved-data reader retains supported metadata
and scalar measurement lists from the pinned synthetic0.7.0 representation; it
executes no Java. Root-only properties have no image-object counterpart and retain
a loss report. Unsupported representations fail explicitly.

CSV appends sorted `metadata:<key>` and `imported:<name>` columns when applicable;
existing computed columns and the no-properties CSV shape stay unchanged. ImGui
shows imported facts separately in the selected-object inspector, marked historical.
Python and GUI delegate to the same native object/storage authority.

SDK review: AIP-180 compatibility and AIP-203 field behavior are adapted to this
local additive API. Existing signatures/defaults and enum ordinals stay intact.
Resources/ownership, command methods, field limits, errors and SDK exports are
reviewed in the header, binder and named workflows. HTTP/auth/retry semantics are
not applicable; no HTTP/proto API lint configuration is present for this SDK.
Generated stubs/typing and actual installed workflow checks are pending the settled
build. This contract is not a release, biological-validation or performance claim.
