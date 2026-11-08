# Saved ImageData properties

`Document.image_data_properties` retains portable custom image properties as
immutable native values. These values are separate from root/ROI historical
facts, saved stain defaults, executable workflow history and analysis recipes.
Keys have no implicit calibration, source-relink or measurement semantics.

```python
import qupathxx as q

doc = project.document(image_id)
doc.set_image_data_properties(q.ImageDataProperties({
    "case": "synthetic",
    "exact_count": 9007199254740993,
    "reviewed": True,
    "details": {"optional": None, "scores": [1.25, 3.5]},
}))
value = doc.image_data_properties
if value is not None:
    copy = value.values  # Independent nested dict/list copies.
doc.set_image_data_properties()  # One undoable clear.
```

Python values may be `None`, `bool`, signed64 `int`, `float`, UTF-8 `str`, `list`
or a string-keyed `dict`. Booleans stay distinct from integers, and integers above
2^53 remain exact. Binary64 values retain infinities and signed zero; NaNs use one
canonical representation. Tuple, set, bytes, arbitrary Python objects, cycles,
out-of-range integers and non-string map keys refuse at the boundary. Empty keys
are allowed; keys/text cannot contain NUL or invalid UTF-8.

Bounds are4096 entries per collection,16384 total value nodes,4096 UTF-8 bytes per
key/text and1 MiB estimated retained native payload. The root map is depth0;
direct values are depth1 and values deeper than16 refuse. Shared input lists
become independent portable values and count at each occurrence. These limits
bound retained state, not total import process memory or whole-slide resources.

An empty bag normalizes to `None`; `{"key": None}` is a retained null value.
Equal values are a no-op, including independently created canonical NaNs. Changing
`True` to `1`, or negative to positive zero, is a real edit. Nonempty set/clear
uses one **Set ImageData properties** history command. Admission failure leaves
the document unchanged. Object IDs, selection, geometry, calibration, ROI/render
revisions and explicit recipes remain unchanged by a property-only edit.

Native shared construction owns the complete immutable tree. Save snapshots
share and charge this value before IO; subsequent replacement leaves an in-flight
snapshot intact. Worker final release has no Python callback/GIL dependency.
Getter-held values survive replacement and document closure. The desktop
**ImageData properties (historical)** inspector displays scalar types and exact
integers, with expandable nested values and scrolling.

Pinned QuPath0.7 version3 saved-data import recognizes strings/nulls, Java boxed
boolean/integer/real/character values, primitive/object arrays, and the captured
ArrayList/LinkedList/Arrays/immutable/singleton/empty/unmodifiable list/map forms.
Integer wrappers normalize to signed64; Float widens exactly to binary64;
characters become text and arrays become lists. Java implementation classes,
numeric wrapper widths and shared-reference identity are not a portable data
contract. The reserved `PROJECT_ENTRY_ID` association is omitted during import;
the existing manifest/native identity authority handles project association.

Unknown Java classes, cycles, non-string-keyed maps and isolated UTF-16 character
surrogates emit `image_property_dropped` with the top-level property key/reason.
An unsupported nested leaf drops its entire top-level property, so a partial list
or map cannot appear complete. Review loss admission is required to import the
remaining supported values. Malformed framing, duplicate keys and exceeded bounds
refuse before publication even when losses are allowed. No Java classes, saved
scripts or deserialization callbacks are executed by the native decoder.

Schema21 adds sparse `image_data_properties(image_id, payload)` rows. `QDP` plus version byte1
stores sorted map keys, typed values and a SHA-256 checksum. Loading enforces
types/bounds, canonical numeric encoding, checksum, owning image and no trailing
data before returning a project. Schemas1-20 load with no custom properties and
migrate inside the existing save transaction. Failed migration leaves the old
database unchanged. Canonical values survive reopen; undo history remains session
state. See [project storage](project-storage.md).
