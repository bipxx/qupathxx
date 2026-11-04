# Historical image facts

`Document.image_properties` retains metadata and measurement values associated
with the whole image. These are imported/user-supplied historical facts, separate
from ROI properties and live measurements. The native model is shared by Python,
project import/storage and the **Image facts (historical)** desktop inspector.

```python
import qupathxx as q

doc = project.document(image_id)
doc.set_image_properties(q.ObjectProperties(
    metadata={"specimen": "synthetic"},
    measurements={"Imported score": 12.5},
))
facts = doc.image_properties
if facts is not None:
    print(facts.metadata, facts.measurements)
doc.set_image_properties()  # Explicitly clear; one undoable edit.
```

The value is immutable and owning. Native shared construction keeps image and
ROI facts independent of Python wrapper lifetime during save-worker cleanup. Python map getters return independent copies;
use set_image_properties to change document state. Passing None or empty maps
means no image facts. Equal values are a no-op. Nonempty set/clear operations
use one **Set image properties** history entry; history admission/refusal and
undo/redo share the ordinary document authority. No fake ROI or object ID is
created. Image facts do not change calibration, ROI facts, selection or computed
pixel values; image-only edits leave ROI/render revisions unchanged.

Reuse ObjectProperties limits: at most4096 total metadata/measurement entries,
at most4096 UTF-8 bytes in a string, no NULs and at most1 MiB estimated payload.
Measurements preserve finite values, canonical NaN, infinities and signed zero;
no implicit units or geometric/calibration interpretation is applied. ROI locks,
object colors and class probability are invalid at image level and refuse before
mutation. Getter-held values survive document closure and subsequent replacement.

Pinned QuPath v0.7.0 native version3 saved-data import preserves root metadata and
measurements, excluding the reserved serialized Object ID used for identity.
These facts stay distinct from child ROI maps and ImageData settings. Import
preview remains nonmutating and source guarded. Unsupported ImageData properties,
stain defaults, image types and other format/version limitations retain their
existing explicit loss reports; this workflow does not execute saved scripts.
Annotation GeoJSON/CSV/clipboard operations still concern selected objects, not
whole-image facts or settings.

The inspector shows retained values with a historical label and a scrollable
area. Project schema19 stores one sparse checksummed row per populated image;
legacy schemas1-18 default empty and migrate transactionally on save. Canonical
facts survive save/reopen, while editing history remains session state. Async
save captures an owning immutable value and accounts for its bounded payload
before IO, so later replacements do not alter the captured save. Corrupt or
inapplicable payloads refuse rather than disappearing silently.
