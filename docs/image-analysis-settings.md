# Saved image analysis defaults

`Document.image_analysis_settings` retains image type and stain defaults as an
immutable, owning native value. Loading defaults leaves explicit analysis
requests, retained recipes and historical results unchanged. Python and the
**Saved analysis defaults** desktop inspector use the same document state.

```python
import qupathxx as q

doc = project.document(image_id)
doc.set_image_analysis_settings(q.ImageAnalysisSettings(
    "BRIGHTFIELD_H_E",
    stains=[[.65, .70, .29], [.2159, .8012, .5581]],
    white=[255, 255, 255],
    profile_name="H&E",
    stain_names=["Hematoxylin", "Eosin"],
))
saved = doc.image_analysis_settings
if saved is not None and saved.rgb8_supported:
    request = q.AnalysisRequest(doc, object_id, saved.transform())
    result = q.analyze(request)
doc.set_image_analysis_settings()  # Explicit clear; undo restores the value.
```

Accepted type strings are `UNSET`, `OTHER`, `FLUORESCENCE`, `BRIGHTFIELD_H_E`,
`BRIGHTFIELD_H_DAB` and `BRIGHTFIELD_OTHER`. `UNSET` without stains means no
settings (`None`). A type alone is valid; stain vectors require a brightfield
type. Setting a type alone does not invent a stain profile.

Two vectors use the existing native normalization and signed cross-product
residual. A third explicit vector is retained as a third stain. `basis` always
contains all three normalized vectors when `has_stains` is true;
`residual_stain` identifies a derived third vector. Names are returned for all
three channels; two supplied names receive `Residual` as the third name.
Singular/nonfinite vectors, unknown types and inconsistent names refuse before
document mutation. Names/profile text use the existing 4096-byte bounded UTF-8
rules. Python sequence getters return copies; replace the value through the
document setter to change saved state.

White references retain finite positive values, including values above255.
The current `StainTransform` applies to RGB8 and requires each white channel in
1..255. `rgb8_supported` exposes this boundary; `transform()` refuses unsupported
values explicitly. No clipping or reinterpretation of quantitative white occurs.
`model_sha256` uses the existing numeric stain-model identity for applicable
RGB8 transforms and is empty when no applicable transform exists. Type and
profile/channel labels are descriptive state, separate from numeric model identity.

Set/clear uses one **Set image analysis defaults** history command. Equal values
are a no-op. History admission failure preserves the old state. Settings change
the document revision without changing object IDs, ROI/render revisions,
selection, geometry, calibration or root/ROI historical facts. Getter-held values
survive replacement and document closure. Save snapshots copy pure C++ values;
worker cleanup does not depend on a Python wrapper or the GIL.

The desktop inspector shows the saved type, profile, names, full basis and white.
**Use saved stains for analysis** deliberately copies a supported profile into
the existing analysis controls. The full third vector is preserved until a
vector or white control is edited. Opening an image does not apply defaults.
Unsupported white disables application while keeping the stored values visible.
Existing completed recipes/results are not rewritten by adoption.

Pinned QuPath0.7 native version3 saved-data import recognizes the six image types
and version1 `ColorDeconvolutionStains` text records. Missing third-vector fields
mean a residual. Unknown/incomplete/malformed settings refuse; unimplemented
ImageData properties and saved executable workflow history retain explicit loss
reports. Import preview remains nonmutating and source guarded. This contract
does not imply support for other saved-data versions or execute saved scripts.

Schema20 stores one sparse `image_analysis_settings(image_id, payload)` row.
It reuses the checksummed ObjectProperties encoding with the typed
`qpx.image-analysis-settings/1` marker and a32768-byte load limit. Loading validates
the marker, fields, numeric basis and owning image before publication. Schemas1-19
load without defaults and migrate within the existing save transaction. Failed
migration leaves the original database unchanged. Settings survive save/reopen;
undo history remains session state. See [project storage](project-storage.md).
