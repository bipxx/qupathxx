import qupathxx as q

sampling = q.TissueGridSampling()
sampling.samples_x = "five"
options = q.GuiOptions()
options.tissue_grid = q.TissueParameters()
batch = q.ProjectBatchOptions()
batch.tissue_grid = 5
