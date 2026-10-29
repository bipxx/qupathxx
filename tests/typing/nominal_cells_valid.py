import qupathxx as q


def workflow(doc: q.Document, obj: int, transform: q.StainTransform) -> q.CellResult:
    parameters = q.GridCellParameters()
    parameters.processing = q.CellProcessingPolicy.QUPATH_070
    sampling = q.NominalCellSampling(q.RgbNominalRequest(0, 0, 256, 192, 1.5), 0, 0)
    direct = q.CellRequest(doc, obj, transform, sampling, parameters)
    request = q.CellRequest.for_nominal_roi(doc, obj, transform, 1.5, 0, 0, parameters, q.RgbNominalSampling.AFFINE_INVERSE)
    assert direct.nominal_sampling is not None
    result = q.detect_cells(request)
    descriptor: q.NominalCellSampling | None = result.nominal_sampling
    if descriptor is not None:
        width: int = descriptor.core_width
        step: float = descriptor.source_request.downsample
        assert width > 0 and step > 0
    mask_descriptor: q.NominalCellSampling | None = result.nuclei.nominal_sampling
    assert mask_descriptor is not None
    return result


def classifier_descriptor(origin: q.ObjectCellFeatureSource) -> q.NominalCellSampling | None:
    return origin.nominal_sampling
