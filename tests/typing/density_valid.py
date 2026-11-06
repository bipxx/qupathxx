from typing import assert_type
import qupathxx as q

doc = q.Document(q.Image.demo())
options = q.SpatialDensityOptions()
options.polygonize_curves = True
request = q.SpatialDensityRequest(doc, [1], [2], [], [3], options)
result = q.measure_spatial_density(request)
assert_type(result.row(0).count, int)
assert_type(result.row(0).area_mm2, float)
assert_type(result.row(0).density_per_mm2, float | None)
assert_type(result.row(0).footprint, q.Roi | None)
assert_type(doc.keep_density(request, result), int)
job = q.SpatialDensityJob()
job.request(request)
assert_type(job.wait(doc).geometry_operations, int)
gui = q.GuiSpatialOptions()
gui.operation = q.SpatialOperation.TISSUE_DENSITY
gui.tissue = [2]
gui.excluded = []
gui.density = options
