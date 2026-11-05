import qupathxx as q


def defaults(doc: q.Document) -> q.StainTransform | None:
    value = q.ImageAnalysisSettings("BRIGHTFIELD_H_E", [[.65, .7, .29], [.27, .57, .78]])
    doc.set_image_analysis_settings(value)
    saved: q.ImageAnalysisSettings | None = doc.image_analysis_settings
    return saved.transform() if saved is not None and saved.rgb8_supported else None
