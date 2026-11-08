import qupathxx as q

options = q.SpatialNeighborhoodOptions()
options.radius = 'twenty'
options.units = 2
q.SpatialNeighborhoodRequest(q.Document(q.Image.demo()), 'sources', [1], options)
