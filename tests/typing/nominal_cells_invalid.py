import qupathxx as q

n = q.NominalCellSampling(q.RgbNominalRequest(0, 0, 17, 13, 1.5))
n.context_x = 0
n.source_request.downsample = 2
q.NominalCellSampling(q.RegionRequest(0, 0, 0, 17, 13))
q.CellRequest.for_nominal_roi(None, 1, None, '1.5')
