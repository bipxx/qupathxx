import qupathxx as q


def batch(project: q.Project, model: q.RawPixelClassifier, lattice: q.ScientificRawLattice) -> q.ProjectBatchReport:
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.RAW_PIXELS
    options.raw_pixel_model = model
    options.raw_pixel_x = lattice.x
    options.raw_pixel_y = lattice.y
    stream = q.RawPixelStreamOptions()
    stream.tile_width, stream.tile_height = 32, 16
    options.raw_pixels = stream
    maybe_model: q.RawPixelClassifier | None = options.raw_pixel_model
    maybe_axis: q.ScientificGridAxis | None = options.raw_pixel_x
    options.raw_pixel_file_work_bytes = 16 * 1024**2
    options.validate()
    request = q.ProjectBatchRequest(project, q.StainTransform([[1, 0, 0], [0, 1, 0]]), options=options)
    return q.run_project_batch(request, "output")
