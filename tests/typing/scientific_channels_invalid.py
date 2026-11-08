import qupathxx as q

def invalid(source: q.ScientificRgbGrid, transform: q.StainTransform) -> None:
    q.ScientificChannels()
    result = q.scientific_deconvolve(source, transform)
    result.mode = q.ScientificChannelMode.OPTICAL_DENSITY
    result.grid.x.samples = 9
    q.scientific_optical_densities(source, [255, 255, 255])
    q.scientific_deconvolve(source, transform, work_bytes="small")
