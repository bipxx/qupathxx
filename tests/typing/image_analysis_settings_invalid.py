import qupathxx as q


def invalid(doc: q.Document) -> None:
    doc.set_image_analysis_settings({"image_type": "OTHER"})
    value: q.ImageAnalysisSettings = doc.image_analysis_settings
    doc.image_analysis_settings = None
