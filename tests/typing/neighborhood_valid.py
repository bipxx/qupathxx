from typing import assert_type
import qupathxx as q

doc = q.Document(q.Image.demo())
options = q.SpatialNeighborhoodOptions()
options.units = q.SpatialDistanceUnits.MICROMETRES
options.radius = 25.0
request = q.SpatialNeighborhoodRequest(doc, [1], [2], options)
result = q.measure_spatial_neighborhoods(request)
assert_type(result.row(0).count, int)
assert_type(result.options.radius, float)
assert_type(doc.keep_neighborhood(request, result), int)
gui = q.GuiSpatialOptions()
gui.operation = q.SpatialOperation.RADIUS_COUNT
gui.radius = options.radius
