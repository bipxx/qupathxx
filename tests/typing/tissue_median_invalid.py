import qupathxx as q

parameters = q.TissueParameters()
parameters.median_cleanup = "yes"
gui = q.GuiOptions()
gui.tissue_median_cleanup = [True]
parameters.median_cleanup = None
