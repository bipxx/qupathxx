import qupathxx as q

request = q.RgbNominalRequest(x_px=3, y_px=5, width_px=24, height_px=20, downsample=1.1)
affine = q.RgbNominalRequest(7, 11, 241, 173, 1.5, q.RgbNominalSampling.AFFINE_INVERSE)
policy: q.RgbNominalSampling = affine.sampling
algorithm: str = request.algorithm
source_x: int = request.x_px
server: q.ImageServer = q.Document(q.Image.demo()).server
pixels: q.PixelBuffer = q.read_rgb_nominal_region(server, request)
width: int = pixels.width
