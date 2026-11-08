import numpy as np
from numpy.typing import NDArray
import qupathxx as q
def invalid(server: q.ImageServer, grid: q.ScientificGrid, result: q.RawAnalysisResult) -> None:
    values: NDArray[np.uint8] = q.read_scientific_raw(server, grid).numpy()
    descriptor: q.RawAnalysisGrid = result.grid
    q.read_scientific_raw(server, grid, 17)
