import qupathxx as q


def grid(doc: q.Document, obj: int, transform: q.StainTransform, session: q.ViewerSession) -> q.TissueResult:
    sampling = q.TissueGridSampling()
    sampling.samples_x, sampling.samples_y = 5, 3
    options = q.GuiOptions()
    options.tissue_grid = sampling
    batch = q.ProjectBatchOptions()
    batch.kind, batch.tissue_grid = q.ProjectBatchKind.TISSUE, sampling
    request = q.TissueRequest.for_grid_roi(doc, obj, transform, sampling)
    exact: q.ScientificGrid | None = request.scientific_grid
    if exact is not None:
        session.tissue_on_grid(1, obj, transform, exact)
    session.grid_tissue(1, obj, transform, sampling)
    return q.detect_tissue(request)
