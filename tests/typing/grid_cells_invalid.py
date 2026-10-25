import qupathxx as q

q.CellRequest.for_grid_roi(None, 1, None)
q.GridCellParameters().units = 'um'
q.GridCellSampling().samples_x = 1.5
q.ResolvedGridCellParameters()
q.GuiOptions().cell_grid = q.CellParameters()
q.ProjectBatchOptions().grid_cells = q.CellParameters()
