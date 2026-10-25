import numpy as np
from numpy.typing import NDArray
import qupathxx as q

def sample(document: q.Document, server: q.ImageServer) -> tuple[NDArray[np.float64], NDArray[np.uint8], q.RawInputDigest]:
    grid = q.ScientificGrid(document.metadata.width, document.metadata.height, q.ScientificGridAxis(0, 2, 1), q.ScientificGridAxis(0, 1, 1))
    options = q.ScientificRawOptions()
    sampled = q.read_scientific_raw(server, grid, options)
    values: NDArray[np.float64] = sampled.numpy()
    reasons: NDArray[np.uint8] = sampled.reasons_numpy()
    request = q.RawAnalysisRequest(document, grid, options)
    descriptor = q.analyze_raw(request).grid
    if descriptor is not None:
        original: q.RawImageMetadata = descriptor.source_metadata
        sample_type: q.SampleType = descriptor.derived_sample_type
    return values, reasons, q.raw_analysis_input_digest(request)
