import numpy as np
from numpy.typing import NDArray
import qupathxx as q


def invalid(table: q.RawPixelTiledTrainingTable, result: q.RawPixelTrainingCheckpointResult,
            request: q.RawPixelTrainingRequest, data: q.ScientificRawWindowData,
            model: q.RawPixelClassifier) -> None:
    bytes_instead_of_values: NDArray[np.uint8] = table.values_numpy()
    missing_partial_guard: q.RawPixelTiledTrainingTable = result.table
    q.sample_raw_pixel_tiled_training(request)
    full_instead_of_window: q.RawPixelClassification = q.classify_raw_pixel_window(data, model)
