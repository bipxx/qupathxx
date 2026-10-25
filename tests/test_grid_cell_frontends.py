"""Installed binding checks for explicit-grid cell capture and consumers."""
import csv
import gc
import io
import json

import numpy as np
import pytest
import qupathxx as q


def scene():
    rgb = np.full((9, 6, 3), 255, dtype=np.uint8)
    rgb[3:6, 2:4] = 32
    doc = q.Document(q.Image(6, 9, rgb.ravel().tolist()))
    doc.set_calibration(.5, .25)
    obj = doc.add_rectangle(q.Rect(0, 0, 6, 9))
    sampling = q.GridCellSampling()
    sampling.samples_x = sampling.samples_y = 3
    sampling.context_x = sampling.context_y = 1
    p = q.GridCellParameters()
    p.units = q.GridCellUnits.MICROMETRES
    p.sigma, p.log_prominence = 0, .2
    p.min_nucleus_area, p.cell_expansion = .75, .9
    contours = p.contours
    contours.work_bytes = 1024 * 1024
    p.contours = contours
    return doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), sampling, p


def test_grid_request_metadata_is_owned_and_legacy_scalar_is_guarded():
    doc, obj, transform, sampling, p = scene()
    request = q.CellRequest.for_grid_roi(doc, obj, transform, sampling, p)
    sampling.samples_x, p.min_nucleus_area = 1, 10000
    request.grid_parameters.min_nucleus_area = 20000
    result = q.detect_cells(request)
    assert result.algorithm == 'qpx.grid-log-watershed-bodies/1'
    assert result.scientific_grid.x.samples == 3
    assert result.grid_parameters.min_nucleus_area == .75
    assert result.resolved_grid_parameters.step_x == 1
    assert result.resolved_grid_parameters.step_y == .75
    assert len(result.features) == 1
    assert result.features[0].nucleus_pixels == 1
    assert result.features[0].cell_pixels == 3
    assert result.calibrated_feature(1).cell_sample_area_um2 == 2.25
    assert result.nuclei.downsample_x == 2 and result.nuclei.downsample_y == 3
    assert result.nuclei.sample_edge_x(1) == 0  # context index zero precedes core
    for value, attribute in [(request, 'parameters'), (result, 'parameters'), (result.nuclei, 'downsample')]:
        with pytest.raises(RuntimeError):
            getattr(value, attribute)
    descriptor = result.scientific_grid
    resolved = result.resolved_grid_parameters
    del request, result, doc
    gc.collect()
    assert descriptor.y.extent == 9 and resolved.step_y == .75


def test_grid_gui_options_copy_parameters_without_a_window():
    _, _, _, sampling, p = scene()
    options = q.GuiOptions()
    options.cell_grid = sampling
    options.grid_cell_parameters = p
    sampling.samples_x, p.sigma = 1, 7
    options.cell_grid.samples_x = 2
    options.grid_cell_parameters.sigma = 9
    assert options.cell_grid.samples_x == 3
    assert options.grid_cell_parameters.sigma == 0
    options.cell_grid = None
    assert options.cell_grid is None


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_grid_batch_snapshot_reopens_and_resumes_with_typed_csv(tmp_path):
    doc, _, transform, sampling, p = scene()
    project = q.Project()
    image = project.add_document(doc, 'Grid, "typed"')
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.CELLS
    options.cell_grid, options.grid_cells = sampling, p
    request = q.ProjectBatchRequest(project, transform, options=options)
    options.grid_cells.min_nucleus_area = 10000
    assert json.loads(request.parameters_json)['grid_cells']['min_nucleus_area'] == .75
    report = q.run_project_batch(request, str(tmp_path))
    assert report.state == 'succeeded' and report.saved == 1, q.project_batch_report_json(report)
    assert doc.cell_record_count == 0
    checkpoint = report.images[0].path
    reopened = q.Project.open(checkpoint).document(image)
    record = reopened.cell_records[0]
    assert record.result.grid_parameters.units == q.GridCellUnits.MICROMETRES
    assert record.result.features[0].cell_pixels == 3
    replay = q.CellRequest.replay(reopened, record.id)
    assert q.detect_cells(replay).nuclei.input_sha256 == record.result.nuclei.input_sha256
    rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(checkpoint))))
    assert len(rows) == 1
    assert rows[0]['sampling_grid'] == 'explicit'
    assert rows[0]['grid_parameter_units'] == 'um'
    assert rows[0]['downsample'] == ''
    resumed = q.run_project_batch(request, str(tmp_path))
    assert resumed.state == 'succeeded' and resumed.reused == 1 and resumed.saved == 0


@pytest.mark.skipif(not q.has_gui or not q.has_geos, reason='requires geometry desktop')
def test_grid_gui_detects_renders_and_publishes_with_axis_geometry(tmp_path):
    import os
    if os.environ.get('QUPATHXX_TEST_GUI') != '1':
        pytest.skip('opt-in desktop replay')
    from test_gui_replay import event
    doc, obj, transform, sampling, p = scene()
    expected = q.detect_cells(q.CellRequest.for_grid_roi(doc, obj, transform, sampling, p))
    options = q.GuiOptions()
    options.cell_grid, options.grid_cell_parameters = sampling, p
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY),
                      event(8, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.KEEP_CELLS_KEY, wait=q.GuiInputWait.CELLS_READY)]
    capture = tmp_path / 'grid-cells.ppm'
    report = q.show(doc, frame_limit=50, capture_path=str(capture), options=options)
    assert report.cell_state == 'ready' and not report.cell_error
    assert report.cell_result.nuclei.labels == expected.nuclei.labels
    assert report.cell_result.cells.labels == expected.cells.labels
    assert report.cells_published and report.cell_annotations_added == 1
    assert report.cell_preview_ready and 0 < report.cell_gpu_bytes <= 32 * 1024 * 1024
    assert report.selected_ids == [obj]
    assert doc.object_count == 2
    cell = doc.annotation(doc.children(obj)[0])
    assert cell.geometry.area == 18 and cell.nucleus.area == 6
    assert doc.cell_object_feature(cell.id).cell_pixels == 3
    header, dimensions, maximum, pixels = capture.read_bytes().split(b'\n', 3)
    width, height = map(int, dimensions.split())
    assert header == b'P6' and maximum == b'255' and len(pixels) == width * height * 3


@pytest.mark.skipif(not q.has_geos, reason='requires cell geometry publication')
def test_grid_classifier_source_metadata_survives_snapshot_lifetime():
    doc, obj, transform, sampling, p = scene()
    request = q.CellRequest.for_grid_roi(doc, obj, transform, sampling, p)
    result = q.detect_cells(request)
    record = doc.keep_cells(request, result)
    ids = doc.publish_cell_record(record)
    options = q.ObjectFeatureOptions()
    options.cell_rgb = True
    table = q.extract_object_features(q.ObjectFeatureRequest(doc, ids, options))
    source = table.cell_origin(0).source
    assert source.scientific_grid.x.samples == 3
    assert source.grid_parameters.units == q.GridCellUnits.MICROMETRES
    assert source.resolved_grid_parameters.step_y == .75
    for attribute in ('parameters', 'downsample'):
        with pytest.raises(RuntimeError):
            getattr(source, attribute)
    source.grid_parameters.min_nucleus_area = 999
    assert source.grid_parameters.min_nucleus_area == .75
    del table, doc, result, request
    gc.collect()
    assert source.scientific_grid.y.extent == 9


@pytest.mark.skipif(not q.has_gui or not q.has_project, reason='requires managed project desktop')
def test_grid_python_producer_retains_exports_and_saves(tmp_path):
    import os
    if os.environ.get('QUPATHXX_TEST_GUI') != '1':
        pytest.skip('opt-in managed desktop')
    from test_viewer_session import completed, run
    doc, obj, transform, sampling, p = scene()
    project = q.Project()
    image = project.add_document(doc, 'Managed grid')
    source, saved, table = tmp_path/'source.qpxproj', tmp_path/'saved.qpxproj', tmp_path/'cells.csv'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    held = {}

    def producer():
        completed(session, session.catalog())
        ticket = session.grid_cells(image, obj, transform, sampling, p, retain_result=True)
        sampling.samples_x, p.min_nucleus_area = 1, 10000
        event = completed(session, ticket)
        assert event.committed and event.record_id and event.cells.features[0].cell_pixels == 3
        assert event.cells.scientific_grid.x.samples == 3
        held['result'] = event.cells
        assert session.stats.retained_bytes == 0
        assert completed(session, session.export_cells(image, str(table), True)).committed
        assert completed(session, session.save(str(saved))).committed
        completed(session, session.close(True))

    run(session, producer)
    assert session.stats.state == 'closed' and session.stats.tickets == 0
    assert doc.cell_record_count == 0
    rows = list(csv.DictReader(table.open(newline='')))
    assert len(rows) == 1 and rows[0]['sampling_grid'] == 'explicit'
    reopened = q.Project.open(str(saved)).document(image)
    assert reopened.cell_record_count == 1
    assert reopened.cell_records[0].result.grid_parameters.min_nucleus_area == .75
    assert held['result'].calibrated_feature(1).cell_sample_area_um2 == 2.25
