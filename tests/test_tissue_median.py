"""Binary post-threshold median with pinned ImageJ and frozen native recipes."""
import csv
import hashlib
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

FIXTURE = Path(__file__).parent / 'fixtures/tissue-median-v1'


def scene():
    yy, xx = np.indices((29, 37))
    mask = (xx * 7 + yy * 11) % 13 < 6
    rgb = np.repeat(np.where(mask, 0, 255).astype(np.uint8)[..., None], 3, axis=2)
    doc = q.Document(q.Image.from_numpy(rgb))
    doc.set_calibration(.5, .25)
    obj = doc.add_rectangle(q.Rect(0, 0, 37, 29))
    p = q.TissueParameters()
    p.median_cleanup = True
    return doc, obj, q.StainTransform([[1,0,0],[0,1,0]]), p, mask


def sorted_median(mask):
    padded = np.pad(mask, 1, mode='edge')
    neighborhoods = [padded[dy:dy+mask.shape[0], dx:dx+mask.shape[1]] for dy in range(3) for dx in range(3)]
    return np.sort(np.stack(neighborhoods), axis=0)[4]


def test_pinned_imagej_rank_outputs_and_independent_majority():
    contract = json.loads((FIXTURE / 'median-contract.json').read_text())
    reference = json.loads((FIXTURE / 'median-reference.json').read_text())
    assert reference['imagej_version'] == contract['imagej_version'] == '1.54p'
    assert len(reference['cases']) == len(contract['cases']) == 524
    transform = q.StainTransform([[1,0,0],[0,1,0]])
    p = q.TissueParameters()
    p.median_cleanup = True
    for case in reference['cases']:
        shape = case['height'], case['width']
        raw = np.array([c == '1' for c in case['bits']]).reshape(shape)
        expected = np.array([c == '1' for c in case['median']]).reshape(shape)
        np.testing.assert_array_equal(sorted_median(raw), expected, err_msg=case['name'])
        rgb = np.repeat(np.where(raw, 0, 255).astype(np.uint8)[..., None], 3, axis=2)
        doc = q.Document(q.Image.from_numpy(rgb))
        obj = doc.add_rectangle(q.Rect(0, 0, case['width'], case['height']))
        result = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
        actual = np.frombuffer(result.labels, dtype=np.uint32).reshape(shape) != 0
        np.testing.assert_array_equal(actual, expected, err_msg=case['name'])
        assert result.threshold_pixels == int(raw.sum())
        assert result.cleaned_pixels == result.foreground_pixels == int(expected.sum())


@pytest.mark.parametrize('tile', [16, 31, 256])
@pytest.mark.parametrize('sigma', [0., 1.7])
def test_threshold_gaussian_median_order_and_tile_seams(tile, sigma):
    doc, obj, transform, p, _ = scene()
    p.tile_size, p.smoothing_sigma = tile, sigma
    before = q.TissueParameters()
    before.tile_size, before.smoothing_sigma = tile, sigma
    baseline = q.detect_tissue(q.TissueRequest(doc, obj, transform, before))
    mask = np.frombuffer(baseline.labels, dtype=np.uint32).reshape((29, 37)) != 0
    request = q.TissueRequest(doc, obj, transform, p)
    result = q.detect_tissue(request)
    np.testing.assert_array_equal(np.frombuffer(result.labels, dtype=np.uint32).reshape(mask.shape) != 0, sorted_median(mask))
    assert result.algorithm == 'qpx.threshold-components/5' and result.parameters.median_cleanup
    assert result.threshold_pixels == baseline.threshold_pixels
    copy = request.parameters
    copy.median_cleanup = False
    assert request.parameters.median_cleanup


@pytest.mark.skipif(os.name != 'nt', reason='recorded MSVC/x64 logical metadata boundaries')
def test_older_unfiltered_and_gaussian_metadata_limits_still_fit():
    manifest = json.loads((FIXTURE / 'manifest.json').read_text())
    path = FIXTURE / manifest['file']
    assert hashlib.sha256(path.read_bytes()).hexdigest() == manifest['sha256']
    project = q.Project.open(str(path))
    doc = project.document(manifest['image'])
    transform = q.StainTransform([[1,0,0],[0,1,0]])
    for case in manifest['cases']:
        result = doc.tissue_record(case['record']).result
        assert not result.parameters.median_cleanup and result.parameters.smoothing_sigma == case['sigma']
        assert result.retained_bytes == case['retained_bytes']
        assert doc.tissue_record(case['record']).payload_bytes == case['payload_bytes']
        table_options = q.MeasurementTableOptions()
        table_options.snapshot_budget = case['table_snapshot_bytes']
        table = q.MeasurementTableRequest(doc, q.MeasurementTableKind.TISSUE, [case['record']], table_options)
        assert table.snapshot_bytes == case['table_snapshot_bytes']
        options = q.ProjectBatchOptions()
        options.kind, options.tissue, options.snapshot_budget = q.ProjectBatchKind.TISSUE, result.parameters, case['batch_snapshot_bytes']
        batch = q.ProjectBatchRequest(project, transform, options=options)
        assert batch.snapshot_bytes == case['batch_snapshot_bytes']
        assert len(batch.parameters_json.encode()) == case['batch_json_bytes']
        replay = q.TissueRequest.replay(doc, case['record'])
        assert replay.estimated_work_bytes == case['work_bytes']
        assert q.detect_tissue(replay).labels == result.labels


def test_pre_median_qtm1_qtm3_replay_and_canonical_blobs(tmp_path):
    manifest = json.loads((FIXTURE / 'manifest.json').read_text())
    source = FIXTURE / manifest['file']
    project = q.Project.open(str(source))
    doc = project.document(manifest['image'])
    for case in manifest['cases']:
        request = q.TissueRequest.replay(doc, case['record'])
        assert not request.parameters.median_cleanup
        value = q.detect_tissue(request)
        assert doc.verify_tissue(case['record'], request, value)
    output = tmp_path / 'old-canonical.qpxproj'
    project.save(str(output))
    def masks(path):
        with sqlite3.connect('file:' + path.resolve().as_posix() + '?mode=ro', uri=True) as db:
            return dict(db.execute('SELECT id,mask FROM tissue_records'))
    original = masks(source)
    assert masks(output) == original
    assert [original[str(case['record'])][3] for case in manifest['cases']] == [1, 3]


@pytest.mark.parametrize('winding', [False, True])
def test_qtm4_mixed_csv3_save_replay_and_canonical_blob(tmp_path, winding):
    doc, obj, transform, p, _ = scene()
    ids = []
    for sigma, median in [(0., False), (1.7, False), (0., True), (1.7, True)]:
        p.smoothing_sigma, p.median_cleanup = sigma, median
        if winding:
            p.sampling_policy = q.RoiSamplingPolicy.POLYGON_WINDING
        request = q.TissueRequest(doc, obj, transform, p)
        ids.append(doc.keep_tissue(request, q.detect_tissue(request)))
    project = q.Project()
    image = project.add_document(doc)
    table = q.measurements_csv(q.MeasurementTableRequest(project, q.MeasurementTableKind.TISSUE))
    rows = list(csv.DictReader(io.StringIO(table)))
    assert rows and all(None not in row and row['table_schema'] == 'qpx.project-tissue-table/3' for row in rows)
    assert {(float(row['smoothing_sigma_sample_px']), int(row['median_cleanup'])) for row in rows} == {(0.,0),(1.7,0),(0.,1),(1.7,1)}
    path = tmp_path / 'median.qpxproj'
    project.save(str(path))
    loaded = q.Project.open(str(path))
    restored = loaded.document(image)
    for record in ids:
        replay = q.TissueRequest.replay(restored, record)
        result = q.detect_tissue(replay)
        assert result.labels == restored.tissue_record(record).result.labels
        assert restored.verify_tissue(record, replay, result)
    again = tmp_path / 'canonical.qpxproj'
    loaded.save(str(again))
    def blobs(source):
        with sqlite3.connect('file:' + source.resolve().as_posix() + '?mode=ro', uri=True) as db:
            return dict(db.execute('SELECT id,mask FROM tissue_records'))
    assert blobs(path) == blobs(again)
    assert blobs(path)[str(ids[-1])][:4] == b'QTM\x04'
    assert blobs(path)[str(ids[-2])][:4] == b'QTM\x04'


def test_batch_cli_checkpoint_identity_and_csv3(tmp_path):
    doc, obj, transform, p, _ = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'input.qpxproj'
    project.save(str(source))
    options = q.ProjectBatchOptions()
    options.kind, options.tissue = q.ProjectBatchKind.TISSUE, p
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['tissue_median_cleanup'] is True
    report = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert report.state == 'succeeded' and report.saved == 1
    assert q.run_project_batch(request, str(tmp_path / 'sdk')).reused == 1
    result = q.Project.open(report.images[0].path).document(image).latest_tissue(obj).result
    assert result.parameters.median_cleanup
    rows = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(report.images[0].path))))
    assert rows and all(row['median_cleanup'] == '1' for row in rows)
    p.median_cleanup = False
    options.tissue = p
    changed = q.ProjectBatchRequest(project, transform, options=options)
    assert changed.parameters_sha256 != request.parameters_sha256
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'tissue', '--tissue-median']
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    final = json.loads(done.stdout.splitlines()[-1])
    assert final['saved'] == 1
    actual = q.Project.open(final['images'][0]['path']).document(image).latest_tissue(obj).result
    assert actual.parameters.median_cleanup and actual.labels == result.labels
    repeated = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert repeated.returncode == 0 and json.loads(repeated.stdout.splitlines()[-1])['reused'] == 1
    assert subprocess.run(command + ['--kind', 'cells'], capture_output=True, timeout=30).returncode != 0


def test_median_scratch_budget_and_stale_request():
    doc, obj, transform, p, _ = scene()
    p.level = 0
    request = q.TissueRequest(doc, obj, transform, p)
    result = q.detect_tissue(request)
    p.work_bytes = request.estimated_work_bytes - 1
    with pytest.raises(ValueError, match='budget'):
        q.TissueRequest(doc, obj, transform, p)
    doc.rename_annotation(obj, 'Changed')
    with pytest.raises(ValueError):
        doc.keep_tissue(request, result)


@GUI
def test_managed_owner_retains_median_recipe(tmp_path):
    doc, obj, transform, p, _ = scene()
    project = q.Project()
    image = project.add_document(doc)
    source, output = tmp_path / 'input.qpxproj', tmp_path / 'output.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    def producer():
        event = completed(session, session.tissue(image, obj, transform, p, retain_result=True))
        assert event.committed and event.tissue.parameters.median_cleanup
        completed(session, session.save(str(output)))
        completed(session, session.close(True))
    run(session, producer)
    assert q.Project.open(str(output)).document(image).latest_tissue(obj).result.parameters.median_cleanup


@GUI
def test_visible_median_control_and_mask(tmp_path):
    doc, obj, transform, p, _ = scene()
    expected = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    options = q.GuiOptions()
    options.tissue_median_cleanup = True
    inputs = []
    for frame, kind in [(4,q.GuiInputKind.SELECT_ALL_KEY),(8,q.GuiInputKind.DETECT_TISSUE_KEY)]:
        event = q.GuiInput();event.frame,event.kind = frame,kind;inputs.append(event)
    wheel = q.GuiInput();wheel.frame,wheel.kind = 15,q.GuiInputKind.WHEEL
    wheel.target,wheel.x,wheel.y,wheel.wheel = q.GuiInputTarget.WINDOW,.1,.5,-24
    inputs.append(wheel)
    options.inputs = inputs
    report = q.show(doc, frame_limit=60, capture_path=str(tmp_path/'median-controls.ppm'), options=options)
    assert report.tissue_state == 'ready', report.tissue_error
    assert report.tissue_foreground_pixels == expected.foreground_pixels
    assert report.tissue_components == expected.component_count and report.tissue_preview_ready
