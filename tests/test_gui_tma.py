"""Opt-in real ImGui TMA creation, edits, export and managed commands."""
import os

import pytest
import qupathxx as q
from test_tma import scene, rows
from test_gui_tiles import replay
from test_viewer_session import completed, run
from test_tma_case_summary import measurement_table

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in TMA desktop')


def click(frame, target):
    down = replay(frame, q.GuiInputKind.LEFT_DOWN, target=target)
    down.wait_for = q.GuiInputWait.TARGET_READY
    # Tabs select on press and use ImGui's previous-frame overlap hover state.
    return [replay(frame-1, q.GuiInputKind.MOVE, target=target), down,
            replay(frame+1, q.GuiInputKind.LEFT_UP, target=target)]


def options():
    o = q.GuiOptions()
    o.width, o.height = 1280, 1000
    o.tma_open = True
    o.replay_timeout_seconds = 15
    return o


def test_create_grid_and_missing_flag():
    doc = q.Document(q.Image.demo())
    o = options()
    preset = q.TmaGridOptions()
    preset.rows, preset.columns = 2, 3
    o.tma_options = preset
    o.inputs = [*click(3, q.GuiInputTarget.TMA_CREATE), *click(8, q.GuiInputTarget.TMA_MISSING)]
    report = q.show(doc, frame_limit=25, options=o)
    assert not report.tma_error
    assert report.tma_cores == len(report.tma_created_ids) == 6
    assert report.tma_missing_cores == 1 and len(doc.tma_core_ids()) == 5
    assert 0 < report.tma_catalog_max_step <= 512
    assert report.ui_overflow_items == 0


def test_tma_shortcut_preserves_the_active_annotation_tool():
    doc = q.Document(q.Image.demo())
    o = options()
    o.tma_open = False
    o.inputs = [replay(3, q.GuiInputKind.TMA_KEY), replay(8, q.GuiInputKind.TMA_KEY),
                replay(12, q.GuiInputKind.LEFT_DOWN, x=.7, y=.6),
                replay(13, q.GuiInputKind.LEFT_UP, x=.7, y=.6),
                replay(16, q.GuiInputKind.FINISH_KEY)]
    report = q.show(doc, frame_limit=25, options=o)
    assert not report.tma_error and doc.object_count == 0


def test_case_geometry_and_grid_translation():
    doc, ids = scene()
    o = options()
    o.tma_core_bounds = q.Rect(12, 14, 26, 22)
    o.tma_translation = q.Point(4, 7)
    o.inputs = [*click(3, q.GuiInputTarget.TMA_GEOMETRY),
                *click(8, q.GuiInputTarget.TMA_GRID_TAB), *click(13, q.GuiInputTarget.TMA_TRANSLATE)]
    report = q.show(doc, frame_limit=30, options=o)
    assert not report.tma_error
    bounds = doc.annotation(ids[0]).geometry.bounds
    assert (bounds.x, bounds.y, bounds.width, bounds.height) == (16, 21, 26, 22)
    assert report.ui_overflow_items == 0
    o = options()
    o.tma_case_id = 'patient-1'
    o.inputs = click(3, q.GuiInputTarget.TMA_CASE)
    report = q.show(doc, frame_limit=20, options=o)
    assert not report.tma_error and doc.annotation(ids[0]).tma_core.case_id == 'patient-1'


@pytest.mark.parametrize('existing', [False, True])
def test_metadata_export_is_captured_and_new_file_only(tmp_path, existing):
    doc, ids = scene()
    doc.set_tma_missing([ids[1]], True)
    doc.set_tma_case_ids({ids[0]: 'patient'})
    path = tmp_path/'metadata.csv'
    if existing:
        path.write_bytes(b'keep original')
    o = options()
    o.tma_tab = 2
    o.tma_metadata_path = str(path)
    o.inputs = click(4, q.GuiInputTarget.TMA_EXPORT)
    report = q.show(doc, frame_limit=30, options=o)
    if existing:
        assert report.tma_export_state == 'failed' and report.tma_error
        assert path.read_bytes() == b'keep original'
    else:
        assert report.tma_export_state == 'saved' and not report.tma_error
        table = rows(path.read_text(encoding='utf8'))
        assert len(table) == 6 and table[0]['case_id'] == 'patient' and table[1]['slot_state'] == 'missing'
        assert report.tma_metadata_bytes == path.stat().st_size
    assert report.ui_overflow_items == 0


def test_fill_vacancy_and_dissolve_keep_objects():
    doc, ids = scene()
    doc.remove_annotation(ids[0])
    o = options()
    o.tma_core_bounds = q.Rect(10, 12, 30, 24)
    o.tma_case_id = 'replacement'
    o.inputs = click(3, q.GuiInputTarget.TMA_GEOMETRY)
    report = q.show(doc, frame_limit=20, options=o)
    assert not report.tma_error and report.tma_cores == 6
    core = doc.tma_grid().core(0, 0)
    assert core.id != ids[0] and core.tma_core.case_id == 'replacement'
    o = options()
    o.tma_tab = 1
    o.inputs = click(3, q.GuiInputTarget.TMA_CLEAR)
    report = q.show(doc, frame_limit=20, options=o)
    assert not report.tma_error and report.tma_cores == 0 and doc.object_count == 6
    assert doc.undo() and doc.tma_core_count == 6


@pytest.mark.parametrize('mode', ['inventory', 'weighted', 'input_alias', 'stale'])
def test_case_summary_export(tmp_path, mode):
    doc, ids = scene()
    doc.set_tma_case_ids({ids[0]: 'replicate', ids[1]: 'replicate'})
    snapshot = doc.tma_grid()
    table = measurement_table(snapshot, [(ids[0], 2, 1), (ids[1], 4, 3)])
    source, output = tmp_path/'measurements.csv', tmp_path/'cases.csv'
    source.write_bytes(table.encode())
    o = options()
    o.tma_tab, o.tma_case_summary = 2, True
    o.tma_metadata_path = str(source if mode == 'input_alias' else output)
    if mode != 'inventory':
        o.tma_measurements_path = str(source)
        o.tma_value_column, o.tma_weight_column = 'value', 'weight'
    if mode == 'stale':
        doc.set_tma_missing([ids[0]], True)
    o.inputs = click(4, q.GuiInputTarget.TMA_EXPORT)
    report = q.show(doc, frame_limit=30, options=o)
    assert source.read_bytes() == table.encode()
    assert report.ui_overflow_items == 0
    if mode in ('input_alias', 'stale'):
        assert report.tma_export_state == 'failed' and report.tma_error
        assert not output.exists()
    else:
        assert report.tma_export_state == 'saved' and not report.tma_error
        row = next(r for r in rows(output.read_text(encoding='utf8')) if r['case_id'] == 'replicate')
        assert row['cores'] == '2'
        assert row['weighted_mean'] == ('3.5' if mode == 'weighted' else '')
        assert report.tma_metadata_bytes == output.stat().st_size


@pytest.mark.skipif(not q.has_project, reason='batch project')
def test_per_core_batch_launcher_excludes_missing(tmp_path):
    doc, ids = scene()
    doc.set_tma_missing([ids[1]], True)
    project = q.Project()
    project.add_document(doc, 'TMA')
    o = options()
    o.batch_output_directory = str(tmp_path/'out')
    finished = replay(12, q.GuiInputKind.MOVE)
    finished.wait_for = q.GuiInputWait.BATCH_FINISHED
    o.inputs = [*click(3, q.GuiInputTarget.TMA_BATCH), replay(8, q.GuiInputKind.RUN_BATCH_KEY), finished]
    report = q.show(project, frame_limit=30, options=o)
    assert not report.tma_error and report.batch_state == 'succeeded'
    assert report.batch_result.images[0].records == 5
    assert len(rows(q.batch_checkpoint_csv(report.batch_result.images[0].path))) == 5


@pytest.mark.skipif(not q.has_project, reason='managed project')
def test_managed_grid_and_metadata_commands(tmp_path):
    project = q.Project()
    image = project.add_document(q.Document(q.Image.demo()), 'TMA')
    path, output = tmp_path/'source.qpxproj', tmp_path/'saved.qpxproj'
    project.save(str(path))
    session = q.ViewerSession(str(path))
    preset = q.TmaGridOptions()
    preset.rows, preset.columns = 2, 2
    def producer():
        created = completed(session, session.execute(image, q.tma_grid_edits(preset), expected_revision=0))
        assert len(created.object_ids) == 4
        core = completed(session, session.inspect(image, created.object_ids[0])).object
        metadata = core.tma_core.with_missing(True).with_case_id('queued case')
        event = completed(session, session.execute(image, [q.ObjectEdit.tma_metadata(core.id, metadata)], expected_revision=created.document_revision))
        assert event.committed
        completed(session, session.save(str(output)))
        completed(session, session.close(True))
    run(session, producer)
    saved = q.Project.open(str(output)).document(image)
    assert saved.tma_core_count == 4 and len(saved.tma_core_ids()) == 3
    assert saved.tma_grid().core(0, 0).tma_core.case_id == 'queued case'
