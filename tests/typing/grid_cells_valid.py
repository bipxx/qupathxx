"""Typed entry points for explicit-grid cell workflows."""
import qupathxx as q


def grid_workflow(doc: q.Document, object_id: int, transform: q.StainTransform,
                  session: q.ViewerSession, image_id: int) -> q.CellResult:
    sampling = q.GridCellSampling()
    sampling.samples_x, sampling.samples_y = 256, 128
    parameters = q.GridCellParameters()
    parameters.units = q.GridCellUnits.MICROMETRES
    request = q.CellRequest.for_grid_roi(doc, object_id, transform, sampling, parameters)
    result = q.detect_cells(request)
    grid = result.scientific_grid
    resolved = result.resolved_grid_parameters
    if grid is not None and resolved is not None:
        x_step: float = resolved.step_x
        count: int = grid.x.samples
        assert x_step > 0 and count > 0
    options = q.GuiOptions()
    options.cell_grid, options.grid_cell_parameters = sampling, parameters
    options.cell_grid = None
    batch = q.ProjectBatchOptions()
    batch.kind = q.ProjectBatchKind.CELLS
    batch.cell_grid, batch.grid_cells = sampling, parameters
    batch.cell_grid = None
    ticket: int = session.grid_cells(image_id, object_id, transform, sampling, parameters, retain_result=True)
    assert ticket > 0
    return result


def decoded_digest(value: q.ScientificRgbGrid) -> str:
    return value.decoded_source_sha256


def nucleus_grid(value: q.CellResult) -> q.ScientificGrid | None:
    return value.nuclei.scientific_grid
