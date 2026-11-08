import qupathxx as q

request = q.RgbNominalRequest(0, 0, 2, 2)
request.width_px = 4
q.RgbNominalRequest(0, 0, 2, 2, downsample="bad")
server = q.Document(q.Image.demo()).server
q.read_rgb_nominal_region(server, q.RegionRequest(0, 0, 0, 2, 2))
