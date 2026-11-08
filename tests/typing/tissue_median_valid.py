import qupathxx as q

def median(doc: q.Document, obj: int, transform: q.StainTransform) -> q.TissueResult:
    parameters = q.TissueParameters()
    parameters.median_cleanup = True
    gui = q.GuiOptions()
    gui.tissue_median_cleanup = True
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.TISSUE
    options.tissue = parameters
    return q.detect_tissue(q.TissueRequest(doc, obj, transform, parameters))
