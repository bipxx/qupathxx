import qupathxx as q

options = q.SpatialDensityOptions()
options.max_geometry_vertices = 'many'
q.SpatialDensityRequest(q.Document(q.Image.demo()), [1], 'tissue', [], [2], options)
q.measure_spatial_neighborhoods(q.SpatialDensityRequest(q.Document(q.Image.demo()), [1], [2], [], []))
