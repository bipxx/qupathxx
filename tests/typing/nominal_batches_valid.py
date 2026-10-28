import qupathxx as q


def capture(project: q.Project, transform: q.StainTransform) -> q.ProjectBatchRequest:
    sampling = q.NominalCellRoiSampling()
    sampling.downsample = 1.5
    sampling.context_x = sampling.context_y = 0
    sampling.validate()
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.CELLS
    options.cell_nominal = sampling
    options.grid_cells.units = q.GridCellUnits.MICROMETRES
    request = q.ProjectBatchRequest(project, transform, options=options)
    options.cell_nominal = None
    return request
