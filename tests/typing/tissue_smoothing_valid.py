import qupathxx as q

def filtered(doc: q.Document, obj: int, transform: q.StainTransform) -> q.TissueResult:
    parameters = q.TissueParameters()
    parameters.smoothing_sigma = 1.7
    request = q.TissueRequest(doc, obj, transform, parameters)
    gui = q.GuiOptions()
    gui.tissue_smoothing_sigma = 1.7
    batch = q.ProjectBatchOptions()
    batch.kind = q.ProjectBatchKind.TISSUE
    batch.tissue = parameters
    return q.detect_tissue(request)
