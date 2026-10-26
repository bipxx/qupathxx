"""Desktop batches share native checkpoints, failure policies and cancellation."""
import csv
import io
import json
import os
from pathlib import Path

import pytest
import qupathxx as q

from test_gui_tiles import replay
from test_project_batch import scene

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='opt-in desktop batch workflow',
)


def options(path, preset=None):
    o = q.GuiOptions()
    o.batch_output_directory = str(path)
    o.batch_options = preset
    o.replay_timeout_seconds = 30
    ready = replay(7, q.GuiInputKind.MOVE)
    ready.wait_for = q.GuiInputWait.BATCH_FINISHED
    start = replay(4, q.GuiInputKind.RUN_BATCH_KEY)
    start.wait_for = q.GuiInputWait.STARTUP_FINISHED
    o.inputs = [start, ready]
    return o


@pytest.mark.parametrize('kind', [q.ProjectBatchKind.ROI, q.ProjectBatchKind.TISSUE, q.ProjectBatchKind.CELLS])
def test_gui_batch_checkpoints_match_native_workflows_and_resume(tmp_path, kind):
    project, docs, transform = scene()
    preset = q.ProjectBatchOptions()
    preset.kind = kind
    preset.publish_cells = kind == q.ProjectBatchKind.CELLS and q.has_geos
    o = options(tmp_path/'output', preset)
    native = q.ProjectBatchRequest(project, transform, options=preset)
    source = tmp_path/'source.qpxproj'
    project.save(str(source))
    for attempt in range(2):
        report = q.show(project, frame_limit=40, options=o)
        result = report.batch_result
        assert report.batch_state == 'succeeded' and not report.batch_error
        assert result.parameters_sha256 == native.parameters_sha256
        assert (result.saved, result.reused, result.failed) == ((2, 0, 0) if attempt == 0 else (0, 2, 0))
        assert json.loads(Path(result.report_path).read_text(encoding='utf8')) == json.loads(q.project_batch_report_json(result))
        for image, (image_id, original, obj) in zip(result.images, docs):
            restored = q.Project.open(image.path).document(image_id)
            assert image.objects == image.records == 1
            table = list(csv.DictReader(io.StringIO(q.batch_checkpoint_csv(image.path))))
            assert table and all(row['status_at_capture'] == 'current' for row in table)
            if kind == q.ProjectBatchKind.ROI:
                direct = q.analyze(q.AnalysisRequest(original, obj, transform))
                assert restored.analysis_records[0].result.means == direct.means
                assert restored.analysis_records[0].result.input_sha256 == direct.input_sha256
            elif kind == q.ProjectBatchKind.TISSUE:
                assert len(restored.tissue_records) == 1
            else:
                assert len(restored.cell_records) == 1
                if preset.publish_cells:
                    assert len(restored.annotations) == 1 + len(restored.cell_records[0].result.features)
            assert not original.analysis_records and not original.tissue_records and not original.cell_records
    assert not list((tmp_path/'output').glob('.qupathxx-batch-work-*'))


def test_gui_batch_selected_scope_and_workspace_switch(tmp_path):
    project, docs, _ = scene()
    o = options(tmp_path/'selected')
    o.batch_selected_objects = True
    o.inputs = [replay(2, q.GuiInputKind.SELECT_ALL_KEY), o.inputs[0],
                replay(5, q.GuiInputKind.NEXT_IMAGE_KEY), o.inputs[1]]
    report = q.show(project, frame_limit=40, options=o)
    assert report.active_image == docs[1][0] and report.batch_state == 'succeeded'
    assert len(report.batch_result.images) == 1
    image = report.batch_result.images[0]
    assert image.image_id == docs[0][0] and image.objects == 1
    assert not docs[0][1].analysis_records and not docs[1][1].analysis_records


@pytest.mark.parametrize('policy,expected', [(q.ProjectBatchFailure.STOP, 'failed'),
                                           (q.ProjectBatchFailure.CONTINUE, 'partial')])
def test_gui_batch_failure_policy(tmp_path, policy, expected):
    project, docs, _ = scene()
    docs[0][1].add_roi(q.Roi.polyline([q.Point(1, 1), q.Point(5, 5)]), 'Not a source')
    # A removed linked source fails independently of the valid embedded images.
    image = tmp_path/'missing.ppm'
    image.write_bytes(b'P6\n8 8\n255\n'+bytes([16])*8*8*3)
    missing = project.add_image(str(image))
    project.document(missing).add_rectangle(q.Rect(0, 0, 8, 8))
    image.unlink()
    final = q.Document(q.Image(8, 8, [24]*8*8*3))
    final.add_rectangle(q.Rect(0, 0, 8, 8))
    project.add_document(final, 'After failure')
    preset = q.ProjectBatchOptions()
    preset.failure = policy
    report = q.show(project, frame_limit=40, options=options(tmp_path/'failure', preset))
    assert report.batch_state == expected
    result = report.batch_result
    assert result.failed == 1 and result.saved == (2 if policy == q.ProjectBatchFailure.STOP else 3)
    assert len(result.images) == (3 if policy == q.ProjectBatchFailure.STOP else 4)
    assert result.images[2].state == 'failed'
    assert result.images[0].objects == 1  # Non-area objects are excluded.


@pytest.mark.parametrize('selected', [False, True])
def test_gui_batch_preflight_errors_have_no_output(tmp_path, selected):
    project, _, _ = scene(1)
    o = options(tmp_path/'unused')
    o.batch_selected_objects = selected
    if not selected:
        o.batch_output_directory = ''
    o.inputs = [replay(4, q.GuiInputKind.RUN_BATCH_KEY)]
    report = q.show(project, frame_limit=12, options=o)
    assert report.batch_state == 'idle' and report.batch_result is None
    assert ('select at least' if selected else 'output directory') in report.batch_error
    assert not (tmp_path/'unused').exists()


@pytest.mark.parametrize('explicit_cancel', [False, True])
def test_gui_batch_cancellation_and_finite_exit_release_worker(tmp_path, explicit_cancel):
    project = q.Project()
    doc = q.Document(q.Image(512, 512, [16]*512*512*3))
    doc.set_history_limits(0, 0)
    doc.execute([q.ObjectEdit.create(q.Rect(0, 0, 512, 512)) for _ in range(500)])
    project.add_document(doc)
    o = options(tmp_path/'cancel')
    o.inputs = [replay(4, q.GuiInputKind.RUN_BATCH_KEY)]
    if explicit_cancel:
        o.inputs += [replay(5, q.GuiInputKind.CANCEL_BATCH_KEY)]
        ready = replay(6, q.GuiInputKind.MOVE)
        ready.wait_for = q.GuiInputWait.BATCH_FINISHED
        o.inputs = o.inputs + [ready]
    report = q.show(project, frame_limit=40 if explicit_cancel else 5, options=o)
    assert report.batch_state == 'cancelled'
    assert report.batch_result.saved == 0 and not doc.analysis_records
    if report.batch_result.report_path:
        saved = json.loads(Path(report.batch_result.report_path).read_text(encoding='utf8'))
        assert saved == json.loads(q.project_batch_report_json(report.batch_result))
    else:
        # Cancellation may reach the worker's first checkpoint before it creates
        # the output directory. The in-memory terminal report still exists.
        assert report.batch_result.error == 'export cancelled'
        assert not report.batch_result.images
        assert not (tmp_path/'cancel').exists()
    assert not list((tmp_path/'cancel').glob('.qupathxx-batch-work-*'))


def test_managed_viewer_batch_uses_native_owned_workspace(tmp_path):
    from test_viewer_session import completed, run

    project, _, _ = scene(1)
    source = tmp_path/'managed.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    preset = q.ProjectBatchOptions()
    o = options(tmp_path/'managed-output', preset)
    def producer():
        completed(session, session.catalog())
    report = run(session, producer, frames=40, options=o)
    assert session.stats.state == 'closed' and report.batch_state == 'succeeded'
    assert report.batch_result.saved == 1
    restored = q.Project.open(report.batch_result.images[0].path)
    assert len(restored.document(1).analysis_records) == 1


def test_gui_batch_uses_current_white_parameters_and_nullable_preset(tmp_path):
    project, docs, _ = scene(1)
    o = options(tmp_path/'white')
    preset = q.ProjectBatchOptions()
    o.batch_options = preset
    assert o.batch_options is not None
    o.batch_options = None
    assert o.batch_options is None
    o.analysis_white = [128, 160, 192]
    report = q.show(project, frame_limit=40, options=o)
    assert report.batch_state == 'succeeded'
    image = report.batch_result.images[0]
    result = q.Project.open(image.path).document(1).analysis_records[0].result
    direct = q.analyze(q.AnalysisRequest(docs[0][1], docs[0][2],
                       q.StainTransform([[1, 0, 0], [0, 1, 0]], [128, 160, 192])))
    assert result.white == direct.white and result.means == direct.means


def test_batch_window_remains_visible_after_canvas_focus(tmp_path):
    from PIL import Image

    project, _, _ = scene(1)
    o = options(tmp_path/'focus')
    o.inputs = o.inputs + [replay(8, q.GuiInputKind.LEFT_DOWN, .1, .2),
                           replay(9, q.GuiInputKind.LEFT_UP, .1, .2)]
    capture = tmp_path/'focused.ppm'
    report = q.show(project, frame_limit=30, capture_path=str(capture), options=o)
    assert report.batch_state == 'succeeded'
    with Image.open(capture) as image:
        # Blank window padding, inside the batch panel's left edge. Focusing
        # the full-screen workspace must not cover this independent window.
        x = int((o.width-476)*image.width/o.width)
        y = int(160*image.height/o.height)
        assert image.getpixel((x, y)) == (19, 22, 27)
