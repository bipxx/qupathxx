"""Thin consumers for the native explicit-grid tissue recipe."""
import csv
import gc
import io
import json
import os
from pathlib import Path
import sqlite3
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_viewer_session import GUI, completed, run


def scene():
    doc = q.Document(q.Image(9, 7, [0] * (9 * 7 * 3)))
    doc.set_calibration(.5, .25)
    obj = doc.add_rectangle(q.Rect(1, 1, 7, 5))
    sampling = q.TissueGridSampling()
    sampling.samples_x, sampling.samples_y = 5, 3
    sampling.context_x, sampling.context_y = 1, 2
    grid = q.ScientificGrid(9, 7, q.ScientificGridAxis(1, 7, 5, 1), q.ScientificGridAxis(1, 5, 3, 2))
    return doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), sampling, grid


def test_sdk_grid_factories_ownership_replay_and_csv4(tmp_path):
    doc, obj, transform, sampling, grid = scene()
    p = q.TissueParameters()
    p.smoothing_sigma, p.median_cleanup = .8, True
    exact = q.TissueRequest(doc, obj, transform, grid, p)
    roi = q.TissueRequest.for_grid_roi(doc, obj, transform, sampling, p)
    result = q.detect_tissue(exact)
    assert result.labels == q.detect_tissue(roi).labels
    assert exact.downsample_x == 1.4 and exact.downsample_y == 5 / 3
    assert q.input_sha256(exact) == result.input_sha256
    with pytest.raises(RuntimeError):
        _ = exact.downsample
    with pytest.raises(RuntimeError):
        _ = exact.level_bounds
    descriptor = exact.scientific_grid
    del exact, roi, grid
    gc.collect()
    assert descriptor.x.samples == 5 and descriptor.y.context == 2
    request = q.TissueRequest.for_grid_roi(doc, obj, transform, sampling, p)
    value = q.detect_tissue(request)
    labels = value.labels_numpy(copy=False)
    expected = np.frombuffer(value.labels, dtype=np.uint32).reshape(value.height, value.width).copy()
    np.testing.assert_array_equal(labels, expected)
    record = doc.keep_tissue(request, value)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'grid.qpxproj'
    project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('SELECT mask FROM tissue_records WHERE id=?', (str(record),)).fetchone()[0][:4] == b'QTM\x05'
    loaded = q.Project.open(str(path)).document(image)
    replay = q.TissueRequest.replay(loaded, record)
    assert loaded.verify_tissue(record, replay, q.detect_tissue(replay))
    rows = list(csv.DictReader(io.StringIO(q.measurements_csv(q.MeasurementTableRequest(loaded, q.MeasurementTableKind.TISSUE, [record])))))
    assert rows[0]['table_schema'] == 'qpx.tissue-table/4'
    assert rows[0]['sampling_kind'] == 'area-rgb' and rows[0]['downsample'] == ''
    assert rows[0]['grid_context_y'] == '2' and float(rows[0]['component_area_um2']) == pytest.approx(value.components[0].pixels * (7 / 5) * (5 / 3) * .5 * .25)
    assert q.TissueRequest.with_grid_component_area_limits(loaded, obj, transform, descriptor, .6, .9).parameters.min_pixels == 3
    loaded.set_calibration(1, .25)
    assert loaded.tissue_status(record) == 'source_changed'
    with pytest.raises(ValueError):
        q.TissueRequest.replay(loaded, record)
    del result, value, request, loaded, project, doc
    gc.collect()
    np.testing.assert_array_equal(labels, expected)


def test_grid_option_properties_are_owning_copies():
    _, _, _, sampling, _ = scene()
    for options in [q.GuiOptions(), q.ProjectBatchOptions()]:
        assert options.tissue_grid is None
        options.tissue_grid = sampling
        sampling.samples_x = 6
        assert options.tissue_grid.samples_x == 5
        copy = options.tissue_grid
        copy.context_x = 77
        assert options.tissue_grid.context_x == 1
        options.tissue_grid = None
        assert options.tissue_grid is None
        sampling.samples_x = 5
    options = q.ProjectBatchOptions()
    options.tissue_grid = sampling
    with pytest.raises(ValueError):
        options.validate()


def test_grid_batch_cli_checkpoint_and_identity(tmp_path):
    doc, obj, transform, sampling, grid = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    options = q.ProjectBatchOptions()
    options.kind, options.tissue_grid = q.ProjectBatchKind.TISSUE, sampling
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['tissue_grid'] == dict(samples_x=5, samples_y=3, context_x=1, context_y=2)
    assert json.loads(request.parameters_json)['tissue']['level'] == 0
    copied = options.tissue_grid
    copied.context_x = 2
    options.tissue_grid = copied
    assert q.ProjectBatchRequest(project, transform, options=options).parameters_sha256 != request.parameters_sha256
    report = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert report.state == 'succeeded' and report.saved == 1
    assert q.run_project_batch(request, str(tmp_path / 'sdk')).reused == 1
    result = q.Project.open(report.images[0].path).document(image).latest_tissue(obj).result
    assert result.scientific_grid.x.samples == 5 and result.scientific_grid.x.context == 1
    assert result.labels == q.detect_tissue(q.TissueRequest(doc, obj, transform, grid)).labels
    rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(report.images[0].path))))
    assert rows[0]['sampling_kind'] == 'area-rgb'
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'tissue', '--tissue-grid', '5', '3', '1', '2', '--stains', '1,0,0,0,1,0']
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    final = json.loads(done.stdout.splitlines()[-1])
    actual = q.Project.open(final['images'][0]['path']).document(image).latest_tissue(obj).result
    assert actual.labels == result.labels and actual.input_sha256 == result.input_sha256
    repeated = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 1
    for tail in [['--kind', 'cells'], ['--tissue-grid', '0', '3', '1', '2'], ['--tissue-grid', '5', '3', '4097', '0']]:
        assert subprocess.run(command + tail, capture_output=True, timeout=30).returncode != 0


@pytest.mark.skipif(os.name != "nt", reason="recorded MSVC/x64 logical metadata boundaries")
def test_pre_grid_exact_quotas_remain(tmp_path):
    fixture = Path(__file__).parent / 'fixtures/tissue-grid-v1'
    manifest = json.loads((fixture / 'manifest.json').read_text())
    project = q.Project.open(str(fixture / manifest['file']))
    doc = project.document(manifest['image'])
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    for case in manifest['cases']:
        record = doc.tissue_record(case['record'])
        assert record.result.retained_bytes == case['retained_bytes'] and record.payload_bytes == case['payload_bytes']
        options = q.MeasurementTableOptions()
        options.snapshot_budget = case['table_bytes']
        assert q.MeasurementTableRequest(doc, q.MeasurementTableKind.TISSUE, [case['record']], options).snapshot_bytes == case['table_bytes']
        batch = q.ProjectBatchOptions()
        batch.kind, batch.tissue, batch.snapshot_budget = q.ProjectBatchKind.TISSUE, record.result.parameters, case['batch_bytes']
        captured = q.ProjectBatchRequest(project, transform, options=batch)
        assert captured.snapshot_bytes == case['batch_bytes'] and len(captured.parameters_json.encode()) == case['batch_json_bytes']
        assert q.TissueRequest.replay(doc, case['record']).estimated_work_bytes == case['work_bytes']


@GUI
def test_managed_grid_capture_retention_and_exact_descriptor(tmp_path):
    doc, obj, transform, sampling, grid = scene()
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'managed.qpxproj'
    project.save(str(path))
    session = q.ViewerSession(str(path))
    output = tmp_path / 'managed-output.qpxproj'
    def producer():
        a = completed(session, session.grid_tissue(image, obj, transform, sampling, retain_result=True))
        assert a.record_id and a.tissue.scientific_grid.x.samples == 5
        b = completed(session, session.tissue_on_grid(image, obj, transform, grid, retain_result=True))
        assert b.tissue.labels == a.tissue.labels and b.tissue.input_sha256 == a.tissue.input_sha256
        assert b.record_id != a.record_id
        completed(session, session.save(str(output)))
        completed(session, session.close())
    run(session, producer)
    assert q.Project.open(str(output)).document(image).tissue_record_count == 2


@GUI
def test_gui_grid_report_preview_and_retention(tmp_path):
    from test_gui_tissue import event
    doc, obj, transform, sampling, _ = scene()
    options = q.GuiOptions()
    options.tissue_grid = sampling
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.DETECT_TISSUE_KEY), event(35, q.GuiInputKind.KEEP_TISSUE_KEY)]
    expected = q.detect_tissue(q.TissueRequest.for_grid_roi(doc, obj, transform, sampling))
    report = q.show(doc, frame_limit=65, capture_path=str(tmp_path / 'grid-preview.ppm'), options=options)
    assert report.tissue_state == 'ready', report.tissue_error
    assert report.tissue_preview_ready and not report.tissue_error
    assert report.tissue_grid.x.samples == 5 and report.tissue_grid.y.context == 2
    assert report.tissue_downsample == 0 and report.tissue_downsample_x == 1.4 and report.tissue_downsample_y == 5 / 3
    assert report.tissue_input_sha256 == expected.input_sha256
    assert report.tissue_samples == expected.samples and report.tissue_foreground_pixels == expected.foreground_pixels
    assert report.saved_tissue_id and report.saved_tissue_status == 'current'
    saved = doc.tissue_record(report.saved_tissue_id).result
    assert saved.scientific_grid.x.origin == 1 and saved.labels == expected.labels


@GUI
def test_gui_grid_batch_matches_captured_recipe(tmp_path):
    from test_gui_batch import options as batch_options
    doc, obj, transform, sampling, _ = scene()
    project = q.Project()
    image = project.add_document(doc)
    project.save(str(tmp_path / 'gui-batch-source.qpxproj'))
    preset = q.ProjectBatchOptions()
    preset.kind, preset.tissue_grid = q.ProjectBatchKind.TISSUE, sampling
    expected = q.ProjectBatchRequest(project, transform, options=preset)
    options = batch_options(tmp_path / 'gui-batch-output', preset)
    report = q.show(project, frame_limit=45, options=options)
    assert report.batch_state == 'succeeded', report.batch_error
    assert report.batch_result.parameters_sha256 == expected.parameters_sha256
    output = q.Project.open(report.batch_result.images[0].path).document(image)
    result = output.latest_tissue(obj).result
    assert result.scientific_grid.x.samples == 5 and result.scientific_grid.y.context == 2
    assert result.input_sha256 == q.detect_tissue(q.TissueRequest.for_grid_roi(doc, obj, transform, sampling)).input_sha256
