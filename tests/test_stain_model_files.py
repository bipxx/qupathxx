"""Native model-file reuse through thin SDK/CLI/managed/ImGui consumers."""
import gc
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native
from test_region_statistics import scene
from test_viewer_session import GUI, completed, run

pytestmark = pytest.mark.skipif(not q.has_project or not q.has_interchange, reason="model-file workflow requires project and interchange support")


def model():
    return q.StainTransform.from_normalized([[.6, .8, 0], [.8, -.6, 0], [0, .6, -.8]], [250, 245, 230])


def result_model(result):
    return q.StainTransform.from_normalized(result.stains, result.white)


def test_sdk_exact_file_json_ownership_and_old_export(tmp_path):
    expected = model()
    digest = q.stain_model_sha256(expected)
    text = q.stain_model_json(expected)
    path = tmp_path / 'model-色.json'
    q.save_stain_model(expected, str(path))
    assert path.read_text(encoding='utf-8') == text
    loaded = q.load_stain_model(str(path))
    parsed = q.stain_model_from_json(text)
    path.unlink()
    del expected
    gc.collect()
    assert q.stain_model_sha256(loaded) == q.stain_model_sha256(parsed) == digest
    doc, obj, _ = scene()
    request = q.AnalysisRequest.with_intensity_statistics(doc, obj, loaded)
    record = doc.keep_analysis(request, q.analyze(request))
    project = q.Project()
    image = project.add_document(doc)
    project_path = tmp_path / 'project.qpxproj'
    project.save(str(project_path))
    restored = q.Project.open(str(project_path)).document(image)
    replay = q.AnalysisRequest.replay(restored, record)
    assert q.stain_model_sha256(replay.transform) == digest
    assert restored.verify_analysis(record, replay, q.analyze(replay))
    fixture = Path(__file__).parent / 'fixtures/stain-model-v1'
    manifest = json.loads((fixture / 'manifest.json').read_text(encoding='utf-8'))
    assert q.stain_model_sha256(q.load_stain_model(str(fixture / 'pre-loader-estimate.json'))) == manifest['model_sha256']
    with pytest.raises(ValueError):
        q.stain_model_from_json('{')
    broken = json.loads(text)
    broken['model_sha256'] = '0' * 64
    with pytest.raises(ValueError, match='digest'):
        q.stain_model_from_json(json.dumps(broken))


def test_cli_model_matches_sdk_and_resumes_with_explicit_conflicts(tmp_path):
    doc, obj, _ = scene()
    value = model()
    project = q.Project()
    image = project.add_document(doc)
    source = tmp_path / 'source.qpxproj'
    project.save(str(source))
    model_path = tmp_path / 'model.json'
    q.save_stain_model(value, str(model_path))
    options = q.ProjectBatchOptions()
    options.roi_kind = q.AnalysisKind.STAIN_MEANS
    options.roi_intensity_statistics = True
    request = q.ProjectBatchRequest(project, value, options=options)
    sdk = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert sdk.state == 'succeeded'
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    base = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'stains', '--roi-intensity-statistics']
    file_option = ['--stain-model', str(model_path)]
    done = subprocess.run(base + file_option, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    report = json.loads(done.stdout.splitlines()[-1])
    assert report['parameters_sha256'] == request.parameters_sha256
    result = q.Project.open(report['images'][0]['path']).document(image).latest_analysis(obj).result
    expected = q.Project.open(sdk.images[0].path).document(image).latest_analysis(obj).result
    assert q.stain_model_sha256(result_model(result)) == q.stain_model_sha256(value)
    np.testing.assert_array_equal(result.means, expected.means)
    assert result.input_sha256 == expected.input_sha256
    again = subprocess.run(base + file_option, capture_output=True, text=True, timeout=30)
    assert again.returncode == 0 and json.loads(again.stdout.splitlines()[-1])['reused'] == 1
    for numeric in [['--white', '255,255,255'], ['--stains', '1,0,0,0,1,0'], ['--normalized']]:
        for tail in [numeric + file_option, file_option + numeric]:
            bad = subprocess.run(base + tail, capture_output=True, text=True, timeout=30)
            assert bad.returncode != 0 and 'conflicts' in bad.stderr
    for tail in [file_option + file_option, ['--stain-model', '']]:
        assert subprocess.run(base + tail, capture_output=True, timeout=30).returncode != 0


def gui_inputs(action, scroll=18):
    from test_gui_tiles import replay
    from test_gui_analysis import event
    down = replay(10, q.GuiInputKind.LEFT_DOWN, target=action)
    down.wait_for = q.GuiInputWait.TARGET_READY
    barrier = replay(14, q.GuiInputKind.MOVE, .1, .5, target=q.GuiInputTarget.WINDOW)
    barrier.wait_for = q.GuiInputWait.STAIN_MODEL_FINISHED
    return [event(4, q.GuiInputKind.SELECT_ALL_KEY),
            replay(7, q.GuiInputKind.WHEEL, .1, .5, wheel=-scroll, target=q.GuiInputTarget.WINDOW),
            down, replay(11, q.GuiInputKind.LEFT_UP, target=action), barrier]


@GUI
def test_gui_load_apply_keep_save_and_reopen(tmp_path):
    from test_gui_tiles import replay
    from test_gui_analysis import event
    doc, obj, _ = scene()
    expected = model()
    path = tmp_path / 'model.json'
    q.save_stain_model(expected, str(path))
    options = q.GuiOptions()
    options.stain_model_path = str(path)
    options.inputs = gui_inputs(q.GuiInputTarget.LOAD_STAIN_MODEL)
    save = replay(34, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_STAIN_MODEL)
    save.wait_for = q.GuiInputWait.TARGET_READY
    finish = replay(38, q.GuiInputKind.MOVE)
    finish.wait_for = q.GuiInputWait.STAIN_MODEL_FINISHED
    options.inputs = options.inputs + [event(16, q.GuiInputKind.ANALYZE_KEY), event(26, q.GuiInputKind.KEEP_ANALYSIS_KEY),
        replay(30, q.GuiInputKind.WHEEL, .1, .5, wheel=-6, target=q.GuiInputTarget.WINDOW),
        save, replay(35, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_STAIN_MODEL), finish]
    report = q.show(doc, frame_limit=55, options=options)
    assert report.stain_model_state == 'saved', report.stain_model_error
    assert report.stain_model_sha256 == q.stain_model_sha256(expected)
    saved = doc.latest_analysis(obj).result
    assert q.stain_model_sha256(result_model(saved)) == q.stain_model_sha256(expected)
    assert q.stain_model_sha256(q.load_stain_model(str(path))) == q.stain_model_sha256(expected)
    project = q.Project()
    image = project.add_document(doc)
    project_path = tmp_path / 'gui.qpxproj'
    project.save(str(project_path))
    assert q.stain_model_sha256(result_model(q.Project.open(str(project_path)).document(image).latest_analysis(obj).result)) == q.stain_model_sha256(expected)
    assert report.replay_complete and report.ui_overflow_items == 0


@GUI
def test_gui_failed_load_retains_prior_model(tmp_path):
    from test_gui_analysis import event
    doc, obj, transform = scene()
    path = tmp_path / 'broken.json'
    path.write_text('{', encoding='utf-8')
    options = q.GuiOptions()
    options.analysis_kind = 1
    options.stain_model_path = str(path)
    options.inputs = gui_inputs(q.GuiInputTarget.LOAD_STAIN_MODEL) + [event(16, q.GuiInputKind.ANALYZE_KEY), event(26, q.GuiInputKind.KEEP_ANALYSIS_KEY)]
    report = q.show(doc, frame_limit=38, options=options)
    assert report.stain_model_state == 'failed' and report.stain_model_error
    assert q.stain_model_sha256(result_model(doc.latest_analysis(obj).result)) == q.stain_model_sha256(transform)


@GUI
def test_gui_save_uses_supplied_stains_in_od_feature_mode(tmp_path):
    doc, _, _ = scene()
    options = q.GuiOptions()
    options.analysis_kind = 0
    options.analysis_vectors = [.6, .8, 0, .8, -.6, 0]
    options.analysis_white = [250, 245, 230]
    expected = q.StainTransform([options.analysis_vectors[:3], options.analysis_vectors[3:]], options.analysis_white)
    path = tmp_path / 'supplied.json'
    options.stain_model_path = str(path)
    options.inputs = gui_inputs(q.GuiInputTarget.SAVE_STAIN_MODEL)
    report = q.show(doc, frame_limit=28, options=options)
    assert report.stain_model_state == 'saved', report.stain_model_error
    assert q.stain_model_sha256(q.load_stain_model(str(path))) == q.stain_model_sha256(expected)


@GUI
def test_gui_model_save_preserves_active_source_file(tmp_path):
    source = tmp_path / 'source.ppm'
    data = b'P6\n2 2\n255\n' + bytes([16, 32, 64]) * 4
    source.write_bytes(data)
    linked = q.Project()
    image = linked.add_image(str(source), reader="ppm")
    doc = linked.document(image)
    doc.add_rectangle(q.Rect(0, 0, 2, 2))
    options = q.GuiOptions()
    options.stain_model_path = str(source)
    options.inputs = gui_inputs(q.GuiInputTarget.SAVE_STAIN_MODEL)
    report = q.show(doc, frame_limit=28, options=options)
    assert report.stain_model_state == 'failed' and report.stain_model_error
    assert 'model path' in report.stain_model_error
    assert source.read_bytes() == data
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@GUI
def test_loaded_model_uses_existing_managed_capture(tmp_path):
    doc, obj, _ = scene()
    value = model()
    model_path = tmp_path / 'managed-model.json'
    q.save_stain_model(value, str(model_path))
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path / 'managed.qpxproj'
    output = tmp_path / 'managed-output.qpxproj'
    project.save(str(path))
    session = q.ViewerSession(str(path))
    def producer():
        loaded = q.load_stain_model(str(model_path))
        model_path.unlink()
        result = completed(session, session.analyze_with_intensity_statistics(image, obj, loaded))
        assert q.stain_model_sha256(result_model(result.analysis)) == q.stain_model_sha256(value)
        completed(session, session.save(str(output)))
        completed(session, session.close())
    run(session, producer)
    result = q.Project.open(str(output)).document(image).latest_analysis(obj).result
    assert q.stain_model_sha256(result_model(result)) == q.stain_model_sha256(value)


@GUI
def test_gui_model_save_preserves_open_project(tmp_path):
    doc, _, _ = scene()
    project = q.Project()
    project.add_document(doc)
    path = tmp_path / 'open-project.qpxproj'
    project.save(str(path))
    original = path.read_bytes()
    options = q.GuiOptions()
    options.stain_model_path = str(path)
    options.inputs = gui_inputs(q.GuiInputTarget.SAVE_STAIN_MODEL, scroll=24)
    report = q.show(project, frame_limit=28, options=options)
    assert report.stain_model_state == 'failed' and report.stain_model_error
    assert 'model path' in report.stain_model_error
    assert path.read_bytes() == original
    assert len(q.Project.open(str(path)).images) == 1


@GUI
def test_gui_rechecked_record_saves_full_basis(tmp_path):
    from test_gui_tiles import replay
    from test_gui_analysis import event
    doc, obj, _ = scene()
    value = model()
    request = q.AnalysisRequest(doc, obj, value)
    record = doc.keep_analysis(request, q.analyze(request))
    project = q.Project()
    image = project.add_document(doc)
    project_path = tmp_path / 'record.qpxproj'
    project.save(str(project_path))
    restored = q.Project.open(str(project_path)).document(image)
    path = tmp_path / 'rechecked-model.json'
    options = q.GuiOptions()
    options.stain_model_path = str(path)
    down = replay(16, q.GuiInputKind.LEFT_DOWN, target=q.GuiInputTarget.SAVE_STAIN_MODEL)
    down.wait_for = q.GuiInputWait.TARGET_READY
    finished = replay(20, q.GuiInputKind.MOVE)
    finished.wait_for = q.GuiInputWait.STAIN_MODEL_FINISHED
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, q.GuiInputKind.RECHECK_ANALYSIS_KEY),
        replay(11, q.GuiInputKind.WHEEL, .1, .5, wheel=-24, target=q.GuiInputTarget.WINDOW),
        down, replay(17, q.GuiInputKind.LEFT_UP, target=q.GuiInputTarget.SAVE_STAIN_MODEL), finished]
    report = q.show(restored, frame_limit=35, options=options)
    assert report.stain_model_state == 'saved', report.stain_model_error
    assert q.stain_model_sha256(q.load_stain_model(str(path))) == q.stain_model_sha256(value)
    assert restored.analysis_status(record) == 'current'


@GUI
def test_gui_image_switch_rejects_pending_model_adoption(tmp_path):
    from test_gui_analysis import event
    first, _, prior = scene()
    second, obj, _ = scene()
    project = q.Project()
    first_id = project.add_document(first)
    project.add_document(second)
    project.active_image = first_id
    project.save(str(tmp_path / 'switch.qpxproj'))
    # A valid large estimate export exercises real asynchronous parsing while
    # replay switches images. Escaped metadata stays below the native16MiB bound.
    data = json.loads(q.stain_model_json(model()))
    data['schema'], data['algorithm'] = 'qpx.stain-estimate/1', 'qpx.od-plane-stains/1'
    text = json.dumps(data)[:-1] + ', "historical_note":"' + '\\u0061' * (2 * 1024 * 1024) + '"}'
    assert len(text.encode()) < 16 * 1024 * 1024
    path = tmp_path / 'large-estimate.json'
    path.write_text(text, encoding='utf-8')
    options = q.GuiOptions()
    options.analysis_kind = 1
    options.stain_model_path = str(path)
    options.inputs = gui_inputs(q.GuiInputTarget.LOAD_STAIN_MODEL, scroll=24) + [
        event(12, q.GuiInputKind.NEXT_IMAGE_KEY), event(16, q.GuiInputKind.SELECT_ALL_KEY),
        event(18, q.GuiInputKind.ANALYZE_KEY), event(26, q.GuiInputKind.KEEP_ANALYSIS_KEY)]
    report = q.show(project, frame_limit=45, options=options)
    assert report.stain_model_state == 'stale', report.stain_model_error
    assert report.active_image != first_id
    assert q.stain_model_sha256(result_model(second.latest_analysis(obj).result)) == q.stain_model_sha256(prior)
