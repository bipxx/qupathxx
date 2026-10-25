"""Real ImGui detection, shared immutable preview and owner-queued application."""
import os
import pytest
import qupathxx as q
from test_gui_tma import click, options
from test_gui_tiles import replay
from test_tma_dearray import CONTRACT, scene
from test_viewer_session import completed, run

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in TMA dearray desktop')


def settings(detector):
    o = options()
    o.height = 1100
    o.tma_dearray_options = detector
    return o


def test_detect_preview_is_immutable_and_explicit(tmp_path):
    doc, detector = scene(CONTRACT['cases'][1])
    before = doc.revision
    o = settings(detector)
    o.inputs = click(4, q.GuiInputTarget.TMA_DETECT_GRID)
    report = q.show(doc, frame_limit=40, capture_path=str(tmp_path/'detected-preview.ppm'), options=o)
    assert report.qptma_state == 'preview ready' and not report.tma_error
    assert report.qptma_preview_cores == 12 and report.qptma_preview_missing == 1
    assert report.tma_dearray_result.candidates == 11
    assert report.tma_dearray_result.grid.cores[5].missing
    assert report.tma_dearray_result.cores[0].center.x == 112
    assert doc.revision == before and doc.tma_core_count == 0
    assert 0 < report.qptma_preview_max_step <= 512 and report.ui_overflow_items == 0


def test_detect_apply_undo_redo_and_project_reopen(tmp_path):
    doc, detector = scene(CONTRACT['cases'][3])
    project = q.Project()
    image = project.add_document(doc, 'Skewed synthetic TMA')
    path = tmp_path/'detected.qpxproj'
    project.save(str(path))
    o = settings(detector)
    o.inputs = [*click(4, q.GuiInputTarget.TMA_DETECT_GRID), *click(12, q.GuiInputTarget.QPTMA_APPLY)]
    report = q.show(project, frame_limit=45, options=o)
    assert report.qptma_state == 'imported' and not report.tma_error
    assert doc.tma_core_count == 12 and doc.tma_grid().core(1, 1).tma_core.missing
    assert doc.undo() and doc.tma_core_count == 0
    assert doc.redo() and doc.tma_core_count == 12
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    assert restored.tma_grid().core(1, 1).tma_core.missing
    center = restored.tma_grid().core(0, 0).geometry.bounds
    assert abs(center.x+center.width/2-112) <= 2
    assert report.ui_overflow_items == 0


@pytest.mark.parametrize('case', [CONTRACT['cases'][5], CONTRACT['cases'][6]], ids=lambda c: c['name'])
def test_unobservable_preview_refuses_without_edit(case):
    doc, detector = scene(case)
    before = doc.revision
    o = settings(detector)
    o.inputs = click(4, q.GuiInputTarget.TMA_DETECT_GRID)
    report = q.show(doc, frame_limit=35, options=o)
    assert report.qptma_state == 'failed' and report.tma_error
    assert report.qptma_preview_cores == 0 and report.tma_dearray_result is None
    assert doc.revision == before and doc.tma_core_count == 0 and report.ui_overflow_items == 0


@pytest.mark.parametrize('action,state', [('discard', 'idle'), ('close', 'cancelled'), ('edit', 'stale')])
def test_preview_lifecycle_never_implicitly_applies(action, state):
    doc, detector = scene(CONTRACT['cases'][0])
    if action == 'edit':
        doc.add_rectangle(q.Rect(1, 1, 2, 2), 'Earlier edit')
    o = settings(detector)
    second = click(14, q.GuiInputTarget.QPTMA_DISCARD_PREVIEW) if action == 'discard' else [replay(14, q.GuiInputKind.TMA_KEY if action == 'close' else q.GuiInputKind.UNDO_KEY)]
    o.inputs = [*click(4, q.GuiInputTarget.TMA_DETECT_GRID), *second]
    report = q.show(doc, frame_limit=40, options=o)
    assert report.qptma_state == state and report.qptma_preview_cores == 0
    assert doc.tma_core_count == 0 and report.tma_dearray_result is None
    assert report.ui_overflow_items == 0


def test_working_budget_failure_does_not_publish():
    doc, detector = scene(CONTRACT['cases'][0])
    detector.work_bytes = 1
    o = settings(detector)
    o.inputs = click(4, q.GuiInputTarget.TMA_DETECT_GRID)
    report = q.show(doc, frame_limit=35, options=o)
    assert report.qptma_state == 'failed' and 'budget' in report.tma_error
    assert not doc.tma_core_count and not report.qptma_preview_cores


def test_managed_detection_copies_options_and_uses_atomic_owner_apply(tmp_path):
    doc, detector = scene(CONTRACT['cases'][1])
    project = q.Project()
    image = project.add_document(doc, 'Managed synthetic TMA')
    source, output = tmp_path/'source.qpxproj', tmp_path/'saved.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))
    retained = []
    def producer():
        initial = completed(session, session.inspect(image))
        ticket = session.detect_tma_grid(image, detector, expected_revision=initial.document_revision)
        detector.columns = 1
        event = completed(session, ticket)
        assert not event.committed and event.tma_dearray.grid.columns == 4
        retained.append(event.tma_dearray)
        applied = completed(session, session.execute(image, q.qptma_grid_edits(event.tma_dearray.grid), expected_revision=event.document_revision))
        assert applied.committed and len(applied.object_ids) == 12
        stale = session.execute(image, q.qptma_grid_edits(event.tma_dearray.grid), expected_revision=event.document_revision)
        assert session.wait(stale, 15).state == 'stale'
        session.take(stale)
        assert completed(session, session.save(str(output))).committed
        completed(session, session.close(True))
    run(session, producer)
    restored = q.Project.open(str(output)).document(image)
    assert restored.tma_core_count == 12 and restored.tma_grid().core(1, 1).tma_core.missing
    assert retained[0].cores[0].center.x == 112 and session.stats.retained_bytes == 0
