"""ImGui project batches use the same pinned model and verified reuse path."""
import os
from pathlib import Path

import pytest
import qupathxx as q
from test_project_pixel_batch import scene, options, request
from test_gui_batch import options as gui_options
from test_gui_tiles import replay

pytestmark = pytest.mark.skipif(
    not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1',
    reason='opt-in desktop pixel batch workflow',
)


@pytest.mark.parametrize('selected', [False, True])
def test_gui_pixel_batch_fresh_resume_and_pinned_model(tmp_path, selected):
    project, docs, model = scene()
    preset = options(model)
    expected_parameters = request(project, model, preset).parameters_sha256
    ui = gui_options(tmp_path/'out', preset)
    ui.width, ui.height = 1280, 900
    ui.batch_selected_objects = selected
    if selected:
        ui.inputs = [replay(2, q.GuiInputKind.SELECT_ALL_KEY), *ui.inputs]
    revisions = [doc.revision for _, doc, _ in docs]
    for attempt in range(2):
        report = q.show(project, frame_limit=40, options=ui)
        assert report.batch_state == 'succeeded' and not report.batch_error
        result = report.batch_result
        count = 1 if selected else 2
        assert result.parameters_sha256 == expected_parameters
        assert (result.saved, result.reused, result.failed) == ((count, 0, 0) if attempt == 0 else (0, count, 0))
        for image in result.images:
            saved = q.Project.open(image.path).document(image.image_id)
            assert len(saved.pixel_stream_records) == image.objects == image.records == 2
            assert image.verified_blocks > 0
            assert image.inferred_blocks > 0 if attempt == 0 else image.inferred_blocks == 0 and image.hashed_blocks > 0
            for record in saved.pixel_stream_records:
                file = q.PixelStreamFile(record.link.path)
                file.verify()
                assert file.link().same_content(record.link)
        assert report.ui_overflow_items == 0
    assert revisions == [doc.revision for _, doc, _ in docs]
    assert not any(doc.pixel_stream_records for _, doc, _ in docs)


@pytest.mark.parametrize('policy,expected,failed', [
    (q.ProjectBatchFailure.STOP, 'failed', 1),
    (q.ProjectBatchFailure.CONTINUE, 'partial', 2),
])
def test_gui_pixel_batch_file_limit_failure_policy(tmp_path, policy, expected, failed):
    project, docs, model = scene()
    preset = options(model)
    preset.pixel_file_limit = 4096
    preset.failure = policy
    ui = gui_options(tmp_path/'out', preset)
    report = q.show(project, frame_limit=40, options=ui)
    assert report.batch_state == expected and report.batch_result.failed == failed
    assert not report.batch_result.saved
    assert not list((tmp_path/'out').rglob('*.qps'))
    assert not any(doc.pixel_stream_records for _, doc, _ in docs)


@pytest.mark.parametrize('explicit_cancel', [False, True])
def test_gui_pixel_batch_cancellation_leaves_no_incomplete_output(tmp_path, explicit_cancel):
    project, docs, model = scene(images=1, targets=1, size=2048)
    ui = gui_options(tmp_path/'out', options(model))
    ui.inputs = [replay(4, q.GuiInputKind.RUN_BATCH_KEY)]
    if explicit_cancel:
        done = replay(6, q.GuiInputKind.MOVE)
        done.wait_for = q.GuiInputWait.BATCH_FINISHED
        ui.inputs += [replay(5, q.GuiInputKind.CANCEL_BATCH_KEY), done]
    report = q.show(project, frame_limit=40 if explicit_cancel else 5, options=ui)
    assert report.batch_state == 'cancelled' and not report.batch_result.saved
    assert Path(report.batch_result.report_path).is_file()
    assert not list((tmp_path/'out').rglob('*.qps'))
    assert not list((tmp_path/'out').rglob('result.qpxproj'))
    assert not docs[0][1].pixel_stream_records
