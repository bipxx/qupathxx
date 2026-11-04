# Imported object metadata and measurements

R5-PROP1 is [locally verified](benchmarks/object-properties-r5/README.md); wider
property/plane support stays in the [queue](api-delivery-plan.md).
`ObjectProperties(metadata={}, measurements={})` owns bounded UTF-8 string metadata
and named binary64 measurements. `Annotation.properties` returns an immutable native
owner or None; returned Python dictionaries are detached copies. Shared owners stay
valid after edits, removal and project close. Total entries are limited to4096,
text fields to4096 UTF-8 bytes and estimated native payload to1MiB per object.
Names are nonempty; NUL/invalid UTF-8 reject. NaNs are canonicalized, signed zero
and infinities retained. These facts are historical values, never recomputed or
promoted to verified results by changing geometry/calibration.

Python construction uses native shared ownership. The
[image-facts workflow](benchmarks/root-facts-r5/README.md) verifies that final
image/ROI fact release on save workers needs no Python cleanup, including disabled
history and a failed writer followed by retry. It also fixes borrowed encoded-BLOB
lifetimes for both image and ROI storage. Wider controller lifecycle remains R11.

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
Generated stubs/typing and the installed inspect/save/export workflows pass
within the linked Windows scope. C++ clients rebuild for changed native layouts and include object_properties.hpp
when inspecting the owner. C++ extension stability remains R15. The wheel does
not ship headers. Existing Python arguments/defaults remain compatible. This contract is not a release, biological-validation or performance claim.

## Object editing state (EDITSTATE1 locally verified)

ObjectProperties adds optional color_rgb (24-bit RGB), a locked flag and optional
class_probability; existing constructor arguments/defaults remain valid. Annotation
exposes the same read-only state. with_edit_state replaces those typed fields while
sharing immutable metadata/measurement maps. Document.set_object_edit_state uses
the existing atomic property command; clear is explicit and undoable. A default
state request for a plain object is a no-op. C++ users include object_properties.hpp
when inspecting the owner; core.hpp forward-declares it to reduce rebuild coupling.
Bindings live in their own module with explicit map/optional/shared-pointer casters.

Locks are interactive ROI-editing hints, matching the pinned
[QuPath ROI object](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-core/src/main/java/qupath/lib/objects/PathROIObject.java)
and [move tool](https://github.com/qupath/qupath/blob/04ccfa4fb7d43e9b566393e08e83690b72248d44/qupath-gui-fx/src/main/java/qupath/lib/gui/viewer/tools/handlers/MoveToolEventHandler.java).
ImGui blocks vertex/hole/inspector/Boolean/merge geometry changes while locked;
selection, properties, naming, classification and explicit deletion remain usable.
Native scripted geometry commands still operate. Lock/unlock, display-state Apply
and explicit Clear/Unlock controls use the same native history authority.

Reported class probability is historical source state, not calibrated/recomputed
confidence. NaN means unknown (None); other binary64 values, including infinities
and signed zero, remain representable. Explicit reclassification clears the old
probability and a named classification clears the color override. Taxonomy rename
retains state; undo/redo restores it exactly, including the optimized classification
transaction and saved immutable snapshot blocks.

Schema17 retains the schema16 object_properties table. QOP1 remains canonical for
metadata/measurements without editing state; QOP2 adds bounded typed flags/color/
probability. Schemas1–16 load without upgrading until save. Schema16 rejects QOP2;
save migration is transactional with rollback. Editing state does not require new
per-frontend properties or a second persistence model.

GeoJSON uses standard color/isLocked properties. Reported probability is retained
in properties.qupathxx.classProbability, with an informational extension notice:
foreign readers may omit it. Both flat and nested native exchange preserve it.
CSV adds editing-state columns only when needed, and limits the selected metadata/
measurement union to4096 columns before materializing rows. This closes that export
admission gap; whole-slide/process budgets remain R12. Per-object color overrides
class color when unselected; selection highlighting and missing-TMA styling retain
their existing precedence. The existing render revision invalidates overlay caches.

API review applies AIP-180 compatibility and AIP-203 field behavior to this local
SDK: resources/ownership, fields, commands, codec migration and errors are reviewed;
Python/stub/typing and changed installed UI checks pass in the
[local evidence](benchmarks/object-edit-state-r5/README.md). HTTP/auth/retry
semantics remain inapplicable. The original PROP1 evidence retains its own scope.
