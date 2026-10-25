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


def producer(document: q.Document, endpoint: q.ViewerSession, image: int, obj: int) -> int:
    counts = q.RawGridSampling()
    counts.samples_x, counts.samples_y = 3, 2
    request: q.RawAnalysisRequest = q.RawAnalysisRequest.for_grid_roi(document, obj, counts)
    gui = q.GuiOptions()
    gui.raw_grid, gui.raw_grid_work_bytes = counts, 1048576
    options = q.ProjectBatchOptions()
    options.raw_grid = counts
    options.raw_grid_work_bytes = 1048576
    return endpoint.analyze_raw_grid(image, obj, counts, retain_result=True)
