import qupathxx as q

def tissue_workflow(doc: q.Document, object_id: int, session: q.ViewerSession) -> None:
    transform = q.StainTransform([[1., 0., 0.], [0., 1., 0.]])
    request: q.TissueRequest = q.TissueRequest.with_component_area_limits(
        doc, object_id, transform, minimum_um2=10., maximum_um2=20.)
    result: q.TissueResult = q.detect_tissue(request)
    doc.keep_tissue(request, result)
    ticket: int = session.tissue_with_component_area_limits(1, object_id, transform,
        minimum_um2=10., retain_result=True)
    options = q.ProjectBatchOptions()
    options.tissue_min_component_area_um2 = 10.
    gui = q.GuiOptions()
    gui.tissue_max_component_area_um2 = None
