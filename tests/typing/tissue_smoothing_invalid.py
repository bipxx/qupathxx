import qupathxx as q

parameters = q.TissueParameters()
parameters.smoothing_sigma = "one"
gui = q.GuiOptions()
gui.tissue_smoothing_sigma = [1.]
parameters.smoothing_sigma = None
