import qupathxx as q

n = q.NominalCellRoiSampling()
n.downsample = '1.5'
n.context_x = 1.5
n.algorithm = 'custom'
q.ProjectBatchOptions().cell_nominal = q.RgbNominalRequest(0, 0, 17, 13)
