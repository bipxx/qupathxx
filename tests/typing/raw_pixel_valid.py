import numpy as np
from numpy.typing import NDArray
import qupathxx as q


def arrays(table: q.RawPixelTiledTrainingTable, data: q.ScientificRawWindowData,
           output: q.RawPixelWindowClassification) -> NDArray[np.float32]:
    values: NDArray[np.float32] = table.values_numpy()
    coordinates: NDArray[np.int64] = table.coordinates_numpy()
    owners: NDArray[np.uint64] = table.annotation_ids_numpy()
    labels: NDArray[np.uint32] = output.labels_numpy()
    support: NDArray[np.uint8] = output.valid_numpy()
    means: NDArray[np.float64] = data.numpy()
    return values


def delivery(document: q.Document, request: q.RawPixelTiledTrainingRequest,
             archive: q.RawPixelArchive, file: q.RawPixelStreamFile) -> None:
    job = q.RawPixelTrainingCheckpointJob()
    job.request(request, "training.qtc")
    result: q.RawPixelTrainingCheckpointResult | None = job.take(document)
    if result is not None and result.table is not None:
        table: q.RawPixelTiledTrainingTable = result.table
        values: NDArray[np.float32] = table.values_numpy()
    historical: q.RawPixelTiledTrainingTable | None = archive.tiled_table
    reader = q.RawPixelStreamFileReadJob()
    reader.read(file, 0)
    block: q.RawPixelWindowClassification | None = reader.take_block()
    if block is not None:
        votes: NDArray[np.float32] = block.vote_fractions_numpy()


def stream(request: q.RawPixelTiledPredictionRequest) -> q.ScientificRawWindow:
    geometry: q.ScientificRawLattice = request.lattice
    window: q.ScientificRawWindow = geometry.window(0, 0, 1, 1)
    axis: q.ScientificRawWindowAxis = window.x
    producer = q.RawPixelPredictionStream(request)
    block: q.RawPixelWindowClassification | None = producer.next()
    return window


def prediction(data: q.ScientificRawWindowData, model: q.RawPixelClassifier) -> q.RawPixelWindowClassification:
    return q.classify_raw_pixel_window(data, model)
