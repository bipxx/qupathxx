"""Pre-threshold Gaussian tissue filtering, frozen recipes and shared consumers."""
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import subprocess
import time

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_viewer_session import GUI, completed, run

FIXTURE = Path(__file__).parent / 'fixtures/tissue-smoothing-v1'


def scene():
    yy, xx = np.indices((29, 37))
    gray = np.where((xx % 17 < 5) | (yy % 11 < 3), 20, 225).astype(np.uint8)
    rgb = np.repeat(gray[..., None], 3, axis=2)
    doc = q.Document(q.Image.from_numpy(rgb))
    doc.set_calibration(.5, .25)
    obj = doc.add_rectangle(q.Rect(0, 0, 37, 29))
    p = q.TissueParameters()
    p.smoothing_sigma, p.threshold = 1.7, .66
    return doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), p, rgb


def direct_gaussian(values, domain, sigma):
    radius = math.ceil(3 * sigma)
    h, w = values.shape
    sums, weights = np.zeros_like(values), np.zeros_like(values)
    for dy in range(-radius, radius + 1):
        y0, y1 = max(0, -dy), min(h, h - dy)
        for dx in range(-radius, radius + 1):
            x0, x1 = max(0, -dx), min(w, w - dx)
            coefficient = math.exp(-.5 * ((dx / sigma)**2 + (dy / sigma)**2))
            support = domain[y0+dy:y1+dy, x0+dx:x1+dx]
            sums[y0:y1, x0:x1] += values[y0+dy:y1+dy, x0+dx:x1+dx] * support * coefficient
            weights[y0:y1, x0:x1] += support * coefficient
    return np.divide(sums, weights, out=np.zeros_like(sums), where=weights > 0)


@pytest.mark.parametrize('sigma', [.6, 1.7, 2.5])
@pytest.mark.parametrize('tile', [16, 31, 256])
def test_direct_two_dimensional_reference_and_decode_seams(sigma, tile):
    doc, obj, transform, p, rgb = scene()
    p.smoothing_sigma, p.tile_size = sigma, tile
    request = q.TissueRequest(doc, obj, transform, p)
    result = q.detect_tissue(request)
    values = (-np.log10(np.maximum(rgb.astype(float), 1.) / 255.)).astype(np.float32).sum(axis=2, dtype=np.float64)
    expected = direct_gaussian(values, np.ones(values.shape, bool), sigma) >= p.threshold
    actual = np.frombuffer(result.labels, dtype=np.uint32).reshape(values.shape)
    np.testing.assert_array_equal(actual != 0, expected)
    assert result.algorithm == 'qpx.threshold-components/4'
    assert result.threshold_pixels == int(expected.sum()) == result.foreground_pixels
    assert result.samples == expected.size
    assert request.parameters.smoothing_sigma == sigma
    copy = request.parameters
    copy.smoothing_sigma = 0.
    assert request.parameters.smoothing_sigma == sigma


def test_negative_stain_features_are_filtered_without_clamping():
    yy, xx = np.indices((11, 17))
    rgb = np.empty((11, 17, 3), dtype=np.uint8)
    rgb[:] = [20, 225, 225]
    rgb[(xx % 7 < 3) | (yy % 5 < 2)] = [225, 20, 225]
    doc = q.Document(q.Image.from_numpy(rgb))
    obj = doc.add_rectangle(q.Rect(0, 0, 17, 11))
    transform = q.StainTransform([[1, 0, 0], [1, 1, 0]])
    p = q.TissueParameters()
    p.feature, p.threshold, p.smoothing_sigma = q.TissueFeature.STAIN_1, -.1, 1.7
    od = -np.log10(rgb.astype(float) / 255.)
    feature = (od[..., 0] - od[..., 1]).astype(np.float32).astype(float)
    assert feature.min() < 0 < feature.max()
    expected = direct_gaussian(feature, np.ones(feature.shape, bool), p.smoothing_sigma) >= p.threshold
    result = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    np.testing.assert_array_equal(np.frombuffer(result.labels, dtype=np.uint32).reshape(feature.shape) != 0, expected)


def test_largest_sigma_and_subnormal_sigma_on_small_crops():
    doc = q.Document(q.Image(3, 3, [0] * 27))
    obj = doc.add_rectangle(q.Rect(0, 0, 3, 3))
    transform = q.StainTransform([[1, 0, 0], [0, 1, 0]])
    p = q.TissueParameters()
    for sigma in (16., math.nextafter(0., 1.)):
        p.smoothing_sigma = sigma
        assert q.detect_tissue(q.TissueRequest(doc, obj, transform, p)).foreground_pixels == 9
        p.threshold = 8.
        assert q.detect_tissue(q.TissueRequest(doc, obj, transform, p)).foreground_pixels == 0
        p.threshold = .3


@pytest.mark.parametrize('sigma', [-1., 17., math.nan, math.inf])
def test_invalid_smoothing(sigma):
    doc, obj, transform, p, _ = scene()
    p.smoothing_sigma = sigma
    with pytest.raises(ValueError, match='sigma'):
        q.TissueRequest(doc, obj, transform, p)
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.TISSUE
    options.tissue = p
    with pytest.raises(ValueError):
        q.ProjectBatchRequest(q.Project(), transform, options=options)


@pytest.mark.parametrize('winding', [False, True])
def test_saved_mixed_table_recipe_and_stale_publication(tmp_path, winding):
    doc, obj, transform, p, _ = scene()
    if winding:
        p.sampling_policy = q.RoiSamplingPolicy.POLYGON_WINDING
    raw = q.TissueParameters()
    raw.threshold = p.threshold
    old_request = q.TissueRequest(doc, obj, transform, raw)
    old_id = doc.keep_tissue(old_request, q.detect_tissue(old_request))
    old_table = q.measurements_csv(q.MeasurementTableRequest(doc, q.MeasurementTableKind.TISSUE))
    assert 'smoothing_sigma_sample_px' not in old_table and 'qpx.tissue-table/1' in old_table
    request = q.TissueRequest(doc, obj, transform, p)
    result = q.detect_tissue(request)
    record = doc.keep_tissue(request, result)
    table = q.measurements_csv(q.MeasurementTableRequest(doc, q.MeasurementTableKind.TISSUE))
    rows = list(csv.DictReader(io.StringIO(table)))
    assert all(None not in row and row['table_schema'] == 'qpx.tissue-table/2' for row in rows)
    assert {float(row['smoothing_sigma_sample_px']) for row in rows} == {0., 1.7}
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'filtered.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    replay = q.TissueRequest.replay(restored, record)
    assert replay.parameters.smoothing_sigma == 1.7
    assert q.detect_tissue(replay).labels == result.labels
    assert restored.tissue_record(old_id).result.parameters.smoothing_sigma == 0.
    doc.rename_annotation(obj, 'Changed')
    assert not request.matches(doc, obj)
    with pytest.raises(ValueError):
        doc.keep_tissue(request, result)


@pytest.mark.skipif(os.name != "nt", reason="recorded MSVC/x64 metadata boundary")
def test_old_archive_exact_snapshot_work_and_metadata_boundary():
    manifest = json.loads((FIXTURE / 'manifest.json').read_text())
    source = FIXTURE / 'legacy.qpxproj'
    assert hashlib.sha256(source.read_bytes()).hexdigest() == manifest['project_sha256']
    doc = q.Project.open(str(source)).document(manifest['image'])
    result = doc.tissue_record(manifest['record']).result
    assert result.parameters.smoothing_sigma == 0.
    assert result.retained_bytes == manifest['result_retained_bytes']
    assert doc.tissue_record(manifest['record']).payload_bytes == manifest['record_payload_bytes']
    options = q.MeasurementTableOptions()
    options.snapshot_budget = manifest['table_snapshot_bytes']
    request = q.MeasurementTableRequest(doc, q.MeasurementTableKind.TISSUE, options=options)
    assert request.snapshot_bytes == options.snapshot_budget
    replay = q.TissueRequest.replay(doc, manifest['record'])
    p = replay.parameters
    p.work_bytes = manifest['work_bytes']
    exact = q.TissueRequest(doc, manifest['object'], q.StainTransform([[1,0,0],[0,1,0]]), p)
    assert exact.estimated_work_bytes == manifest['work_bytes']
    assert q.detect_tissue(exact).labels == result.labels
    assert result.input_sha256 == manifest['input_sha256']


@pytest.mark.skipif(os.name != "nt", reason="recorded MSVC/x64 batch snapshot boundary")
def test_old_unfiltered_batch_exact_snapshot_budget():
    manifest = json.loads((FIXTURE / 'manifest.json').read_text())
    project = q.Project.open(str(FIXTURE / 'legacy.qpxproj'))
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.TISSUE
    options.snapshot_budget = manifest['batch_snapshot_bytes']
    request = q.ProjectBatchRequest(project, q.StainTransform([[1,0,0],[0,1,0]]), options=options)
    assert request.snapshot_bytes == manifest['batch_snapshot_bytes']
    assert len(request.parameters_json.encode()) == manifest['batch_parameters_json_bytes']


def test_filtered_workspace_is_admitted_before_execution():
    doc, obj, transform, p, _ = scene()
    p.level = 0
    cost = q.TissueRequest(doc, obj, transform, p).estimated_work_bytes
    p.work_bytes = cost - 1
    with pytest.raises(ValueError, match='budget'):
        q.TissueRequest(doc, obj, transform, p)
    p.work_bytes = cost
    assert q.detect_tissue(q.TissueRequest(doc, obj, transform, p)).samples == 37 * 29


def test_batch_cli_resume_and_sigma_identity(tmp_path):
    doc, obj, transform, p, _ = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'input.qpxproj'
    project.save(str(source))
    options = q.ProjectBatchOptions()
    options.kind, options.tissue = q.ProjectBatchKind.TISSUE, p
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['tissue_smoothing_sigma'] == 1.7
    first = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert first.state == 'succeeded' and first.saved == 1
    assert q.run_project_batch(request, str(tmp_path / 'sdk')).reused == 1
    restored = q.Project.open(first.images[0].path).document(image)
    assert restored.latest_tissue(obj).result.parameters.smoothing_sigma == 1.7
    rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(first.images[0].path))))
    assert rows and all(float(row['smoothing_sigma_sample_px']) == 1.7 for row in rows)
    p.smoothing_sigma = 2.5
    options.tissue = p
    changed = q.ProjectBatchRequest(project, transform, options=options)
    assert changed.parameters_sha256 != request.parameters_sha256
    changed_result = q.run_project_batch(changed, str(tmp_path / 'sdk'))
    assert changed_result.state == 'succeeded' and changed_result.reused == 0
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'tissue', '--tissue-sigma', '1.7', '--threshold', '.66']
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    report = json.loads(done.stdout.splitlines()[-1])
    assert report['saved'] == 1
    value = q.Project.open(report['images'][0]['path']).document(image).latest_tissue(obj).result
    assert value.parameters.smoothing_sigma == 1.7
    assert value.labels == restored.latest_tissue(obj).result.labels
    repeated = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 1
    assert subprocess.run(command + ['--kind', 'cells'], capture_output=True, timeout=30).returncode != 0


@pytest.mark.parametrize('action', ['cancel', 'stale'])
def test_gaussian_worker_phase_discards_cancelled_or_stale_output(action):
    doc = q.Document(q.Image.from_numpy(np.zeros((1024, 1024, 3), dtype=np.uint8)))
    obj = doc.add_rectangle(q.Rect(0, 0, 1024, 1024))
    p = q.TissueParameters()
    p.smoothing_sigma, p.max_components = 16., 1000
    request = q.TissueRequest(doc, obj, q.StainTransform([[1,0,0],[0,1,0]]), p)
    job = q.TissueJob()
    assert job.request(request)
    deadline = time.monotonic() + 10
    while True:
        status = job.poll(doc, obj)
        if status.phase == 'smoothing':
            break
        assert status.state == 'running' and time.monotonic() < deadline, (status.state, status.phase)
        time.sleep(.0005)
    if action == 'cancel':
        job.cancel()
        expected = 'cancelled'
    else:
        doc.rename_annotation(obj, 'Changed during smoothing')
        expected = 'stale'
    assert job.wait(doc, obj).state == expected
    assert job.result is None and not doc.tissue_records


@GUI
def test_managed_viewer_retains_smoothing(tmp_path):
    doc, obj, transform, p, _ = scene()
    project = q.Project()
    image = project.add_document(doc)
    source, output = tmp_path / 'input.qpxproj', tmp_path / 'output.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    def producer():
        event = completed(session, session.tissue(image, obj, transform, p, retain_result=True))
        assert event.committed and event.record_id and event.tissue.parameters.smoothing_sigma == 1.7
        completed(session, session.save(str(output)))
        completed(session, session.close(True))
    run(session, producer)
    assert q.Project.open(str(output)).document(image).latest_tissue(obj).result.parameters.smoothing_sigma == 1.7


@GUI
def test_desktop_smoothing_options_and_controls(tmp_path):
    doc, obj, transform, p, _ = scene()
    expected = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    options = q.GuiOptions()
    options.tissue_smoothing_sigma, options.tissue_threshold = 1.7, p.threshold
    inputs = []
    for frame, kind in [(4, q.GuiInputKind.SELECT_ALL_KEY), (8, q.GuiInputKind.DETECT_TISSUE_KEY)]:
        event = q.GuiInput(); event.frame, event.kind = frame, kind; inputs.append(event)
    wheel = q.GuiInput(); wheel.frame, wheel.kind = 15, q.GuiInputKind.WHEEL
    wheel.target, wheel.x, wheel.y, wheel.wheel = q.GuiInputTarget.WINDOW, .1, .5, -24
    inputs.append(wheel)
    options.inputs = inputs
    report = q.show(doc, frame_limit=60, capture_path=str(tmp_path / 'smoothing-controls.ppm'), options=options)
    assert report.tissue_state == 'ready', report.tissue_error
    assert report.tissue_foreground_pixels == expected.foreground_pixels
    assert report.tissue_components == expected.component_count and report.tissue_preview_ready
