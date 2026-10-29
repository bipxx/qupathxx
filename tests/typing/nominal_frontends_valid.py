import qupathxx as q

def submit(session: q.ViewerSession, transform: q.StainTransform) -> int:
    sampling = q.NominalCellRoiSampling()
    options = q.GuiOptions()
    options.cell_nominal = sampling
    options.grid_cell_parameters = q.GridCellParameters()
    parameters = q.GridCellParameters()
    parameters.processing = q.CellProcessingPolicy.IMAGEJ_154P
    options.grid_cell_parameters = parameters
    options.cell_nominal = None
    return session.nominal_cells(1, 1, transform, sampling, q.GridCellParameters(), retain_result=True)
