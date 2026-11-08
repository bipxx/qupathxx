"""Thin consumers of retained native region intensity statistics."""
import csv
import gc
import io
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_viewer_session import GUI, completed, run


def scene():
    doc = q.Document(q.Image(2, 2, [0, 16, 32, 64, 128, 255, 255, 32, 16, 16, 64, 128]))
    obj = doc.add_rectangle(q.Rect(0, 0, 2, 2))
    return doc, obj, q.StainTransform([[.65, .70, .29], [.07, .99, .11]])


def same(actual, expected):
    assert actual.samples == expected.samples
    for domain in ('rgb', 'channels'):
        a, b = getattr(actual, domain), getattr(expected, domain)
        for field in ('sum', 'minimum', 'maximum', 'sample_standard_deviation'):
            av, bv = getattr(a, field), getattr(b, field)
            if bv is None:
                assert av is None
            else:
                np.testing.assert_array_equal(av, bv)


@pytest.mark.parametrize('kind', [q.AnalysisKind.OPTICAL_DENSITY_MEANS, q.AnalysisKind.STAIN_MEANS])
def test_sdk_owning_statistics_retention_replay_csv(tmp_path, kind):
    doc, obj, transform = scene()
    old = q.AnalysisRequest(doc, obj, transform, kind)
    assert not old.measure_intensity and q.analyze(old).intensity_statistics is None
    request = q.AnalysisRequest.with_intensity_statistics(doc, obj, transform, kind)
    assert request.measure_intensity
    value = q.analyze(request)
    stats = value.intensity_statistics
    old_id = doc.keep_analysis(old, q.analyze(old))
    request = q.AnalysisRequest.with_intensity_statistics(doc, obj, transform, kind)
    value = q.analyze(request)
    record = doc.keep_analysis(request, value)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'statistics.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    replay = q.AnalysisRequest.replay(restored, record)
    assert replay.measure_intensity and restored.verify_analysis(record, replay, q.analyze(replay))
    same(restored.analysis_record(record).result.intensity_statistics, stats)
    rows = list(csv.DictReader(io.StringIO(q.measurements_csv(q.MeasurementTableRequest(
        restored, q.MeasurementTableKind.ROI, [old_id, record])))))
    assert all(row['table_schema'] == 'qpx.roi-table/2' for row in rows)
    assert rows[0]['intensity_statistics_format'] == ''
    assert rows[1]['intensity_statistics_format'] == 'qpx.roi-intensity-statistics/1'
    rgb = stats.rgb
    del project, restored, replay, value, request, doc, stats
    gc.collect()
    np.testing.assert_array_equal(rgb.sum, [335, 240, 431])
    with pytest.raises(AttributeError):
        rgb.sum = [0, 0, 0]


def test_singleton_statistics_absent_sd():
    doc, _, transform = scene()
    obj = doc.add_rectangle(q.Rect(0, 0, 1, 1))
    stats = q.analyze(q.AnalysisRequest.with_intensity_statistics(doc, obj, transform)).intensity_statistics
    assert stats.samples == 1
    assert stats.rgb.sample_standard_deviation is None
    assert stats.channels.sample_standard_deviation is None


def test_batch_cli_and_resume_share_statistics(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    options = q.ProjectBatchOptions()
    options.roi_intensity_statistics = True
    options.roi_kind = q.AnalysisKind.STAIN_MEANS
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['roi_intensity_statistics'] is True
    report = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert report.state == 'succeeded' and report.saved == 1
    assert q.run_project_batch(request, str(tmp_path / 'sdk')).reused == 1
    expected = q.Project.open(report.images[0].path).document(image).latest_analysis(obj).result
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'stains', '--roi-intensity-statistics']
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    result = json.loads(done.stdout.splitlines()[-1])
    actual = q.Project.open(result['images'][0]['path']).document(image).latest_analysis(obj).result
    same(actual.intensity_statistics, expected.intensity_statistics)
    assert actual.input_sha256 == expected.input_sha256
    again = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert again.returncode == 0 and json.loads(again.stdout.splitlines()[-1])['reused'] == 1
    options.kind = q.ProjectBatchKind.TISSUE
    with pytest.raises(ValueError, match='ROI intensity statistics'):
        q.ProjectBatchRequest(project, transform, options=options)
    bad = subprocess.run(command + ['--kind', 'tissue'], capture_output=True, timeout=30)
    assert bad.returncode != 0


@GUI
def test_managed_region_statistics(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'managed.qpxproj'
    output = tmp_path / 'managed-output.qpxproj'
    project.save(str(path))
    session = q.ViewerSession(str(path))
    expected = q.analyze(q.AnalysisRequest.with_intensity_statistics(doc, obj, transform)).intensity_statistics
    def producer():
        result = completed(session, session.analyze_with_intensity_statistics(image, obj, transform))
        same(result.analysis.intensity_statistics, expected)
        completed(session, session.save(str(output)))
        completed(session, session.close())
    run(session, producer)
    assert q.Project.open(str(output)).document(image).analysis_record_count == 1


@GUI
def test_gui_statistics_report_keep_and_reopen(tmp_path):
    from test_gui_analysis import event
    doc, obj, transform = scene()
    options = q.GuiOptions()
    options.analysis_kind, options.analysis_intensity_statistics = 1, True
    options.analysis_vectors = [.65, .70, .29, .07, .99, .11]
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.ANALYZE_KEY),
                      event(24, q.GuiInputKind.KEEP_ANALYSIS_KEY)]
    expected = q.analyze(q.AnalysisRequest.with_intensity_statistics(doc, obj, transform)).intensity_statistics
    report = q.show(doc, frame_limit=45, options=options)
    assert report.analysis_state == 'ready', report.analysis_error
    same(report.analysis_intensity_statistics, expected)
    same(doc.latest_analysis(obj).result.intensity_statistics, expected)
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'gui.qpxproj'
    project.save(str(path))
    same(q.Project.open(str(path)).document(image).latest_analysis(obj).result.intensity_statistics, expected)


@GUI
def test_gui_batch_statistics_and_resume(tmp_path):
    from test_gui_batch import options as batch_options
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'gui-batch.qpxproj'
    project.save(str(source))
    preset = q.ProjectBatchOptions()
    preset.roi_intensity_statistics = True
    preset.roi_kind = q.AnalysisKind.STAIN_MEANS
    options = batch_options(tmp_path / 'gui-batch-output', preset)
    options.analysis_kind = 1
    options.analysis_vectors = [.65, .70, .29, .07, .99, .11]
    expected = q.analyze(q.AnalysisRequest.with_intensity_statistics(doc, obj, transform)).intensity_statistics
    for attempt in range(2):
        report = q.show(project, frame_limit=40, options=options)
        assert report.batch_state == 'succeeded', report.batch_error
        assert (report.batch_result.saved, report.batch_result.reused) == ((1, 0) if attempt == 0 else (0, 1))
        result = q.Project.open(report.batch_result.images[0].path).document(image).latest_analysis(obj).result
        same(result.intensity_statistics, expected)
