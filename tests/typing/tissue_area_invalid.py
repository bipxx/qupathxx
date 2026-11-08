import qupathxx as q

def invalid(doc: q.Document, session: q.ViewerSession, transform: q.StainTransform) -> None:
    q.TissueRequest.with_component_area_limits(doc, 1, transform, minimum_um2="ten")
    session.tissue_with_component_area_limits(1, 1, transform, maximum_um2=[1.])
    options = q.ProjectBatchOptions()
    options.tissue_min_component_area_um2 = "ten"
