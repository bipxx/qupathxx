import os
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_object_classifier import workspace, parameters, trained

pytestmark = pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI') != '1',
                                reason='requires GUI build and QUPATHXX_TEST_GUI=1')


def event(frame, kind, target=q.GuiInputTarget.CANVAS, wait=q.GuiInputWait.IMMEDIATE):
    result = q.GuiInput()
    result.frame, result.kind, result.target, result.wait_for = frame, kind, target, wait
    return result


def click(frame, target):
    return [event(frame, q.GuiInputKind.LEFT_DOWN, target, q.GuiInputWait.TARGET_READY),
            event(frame+1, q.GuiInputKind.LEFT_UP, target)]


def ready(frame, condition):
    return event(frame, q.GuiInputKind.MOVE, wait=condition)


def test_real_window_trains_previews_applies_undo_redo_and_saves(tmp_path):
    if not q.has_classifier_training:
        pytest.skip('optional classifier training backend')
    doc, ids = workspace()
    doc.clear_history()
    options = q.GuiOptions()
    options.autosave_interval_seconds = 0
    options.classifier_parameters = parameters()
    path = tmp_path/'trained.qpxforest'
    options.classifier_model_path = str(path)
    options.inputs = [event(2, q.GuiInputKind.OBJECT_CLASSIFIERS_KEY),
                      *click(5, q.GuiInputTarget.CLASSIFIER_TRAIN), ready(8, q.GuiInputWait.CLASSIFIER_TRAINED),
                      *click(12, q.GuiInputTarget.CLASSIFIER_PREDICT), ready(16, q.GuiInputWait.CLASSIFIER_PREDICTED),
                      *click(20, q.GuiInputTarget.CLASSIFIER_APPLY), ready(24, q.GuiInputWait.CLASSIFIER_APPLIED),
                      event(28, q.GuiInputKind.UNDO_KEY), event(32, q.GuiInputKind.REDO_KEY),
                      *click(36, q.GuiInputTarget.CLASSIFIER_SAVE_MODEL), ready(42, q.GuiInputWait.CLASSIFIER_MODEL_SAVED)]
    report = q.show(doc, frame_limit=100, options=options)
    assert not report.classifier_error and not report.classifier_io_error
    assert report.classifier_state == 'applied' and report.classified_objects == 2
    assert report.classifier_model.training_rows == 6 and report.classifier_model.tree_count == 24
    assert report.classifier_result.table.object_ids == ids[-2:]
    assert report.classifier_io_state == 'saved' and path.is_file()
    assert q.load_forest_model(str(path)).sha256 == report.classifier_model.sha256
    assert [doc.annotation(i).class_name for i in ids[-2:]] == ['small', 'large']
    assert doc.undo() and all(not doc.annotation(i).class_name for i in ids[-2:])
    assert report.ui_overflow_items == 0


@pytest.mark.parametrize('size,scale', [((1440, 900), 1.), ((960, 720), 1.25)])
def test_loaded_model_predicts_and_applies_in_responsive_window(tmp_path, size, scale):
    doc, ids = workspace()
    model = trained(doc, ids)
    path = tmp_path/'model.qpxforest'
    q.save_forest_model(model, str(path))
    options = q.GuiOptions()
    options.width, options.height, options.ui_scale = *size, scale
    options.classifier_open = True
    features = q.ObjectFeatureOptions()
    features.snapshot_bytes = 1024*1024
    options.classifier_features = features
    options.classifier_model_path = str(path)
    options.autosave_interval_seconds = 0
    options.inputs = [*click(4, q.GuiInputTarget.CLASSIFIER_LOAD_MODEL), ready(8, q.GuiInputWait.CLASSIFIER_MODEL_LOADED),
                      *click(12, q.GuiInputTarget.CLASSIFIER_PREDICT), ready(16, q.GuiInputWait.CLASSIFIER_PREDICTED),
                      *click(20, q.GuiInputTarget.CLASSIFIER_APPLY), ready(24, q.GuiInputWait.CLASSIFIER_APPLIED)]
    report = q.show(doc, frame_limit=70, options=options)
    assert not report.classifier_error and not report.classifier_io_error
    assert report.classifier_io_state == 'loaded' and report.classified_objects == 2
    assert [doc.annotation(i).class_name for i in ids[-2:]] == ['small', 'large']
    assert report.classifier_model.sha256 == model.sha256
    assert report.classifier_result.table.options.snapshot_bytes == features.snapshot_bytes
    assert report.ui_overflow_items == 0


def test_reviewed_predictions_become_stale_after_real_undo():
    doc, ids = workspace()
    model = trained(doc, ids)
    doc.rename_annotation(ids[0], 'Undo during classifier review')
    options = q.GuiOptions()
    options.classifier_open, options.classifier_model = True, model
    options.inputs = [*click(4, q.GuiInputTarget.CLASSIFIER_PREDICT), ready(8, q.GuiInputWait.CLASSIFIER_PREDICTED),
                      event(12, q.GuiInputKind.UNDO_KEY)]
    report = q.show(doc, frame_limit=45, options=options)
    assert report.classifier_state == 'stale' and report.classifier_result is None
    assert doc.annotation(ids[0]).name == 'Cell 0'
    assert all(not doc.annotation(i).class_name for i in ids[-2:])
    assert report.classifier_model.sha256 == model.sha256


def test_unknown_model_file_never_replaces_current_model(tmp_path):
    doc, ids = workspace()
    model = trained(doc, ids)
    path = tmp_path/'bad.qpxforest'
    path.write_bytes(b'not a classifier')
    options = q.GuiOptions()
    options.classifier_open, options.classifier_model = True, model
    options.classifier_model_path = str(path)
    options.inputs = click(4, q.GuiInputTarget.CLASSIFIER_LOAD_MODEL)
    report = q.show(doc, frame_limit=40, options=options)
    assert report.classifier_io_state == 'failed' and report.classifier_io_error
    assert report.classifier_model.sha256 == model.sha256
    assert path.read_bytes() == b'not a classifier'
    assert all(not doc.annotation(i).class_name for i in ids[-2:])


def test_real_window_retains_training_and_reuses_model_from_reopened_project(tmp_path):
    if not q.has_classifier_training or not q.has_project:
        pytest.skip('requires training and project storage')
    doc, ids = workspace()
    project = q.Project(); image = project.add_document(doc, 'Classifier study')
    options = q.GuiOptions(); options.classifier_open = True
    options.autosave_interval_seconds = 0; options.classifier_parameters = parameters()
    options.inputs = [*click(4, q.GuiInputTarget.CLASSIFIER_TRAIN), ready(8, q.GuiInputWait.CLASSIFIER_TRAINED),
                      *click(12, q.GuiInputTarget.CLASSIFIER_KEEP_TRAINING), ready(16, q.GuiInputWait.CLASSIFIER_TRAINING_RETAINED),
                      *click(20, q.GuiInputTarget.CLASSIFIER_PREDICT), ready(24, q.GuiInputWait.CLASSIFIER_PREDICTED),
                      *click(28, q.GuiInputTarget.CLASSIFIER_APPLY), ready(32, q.GuiInputWait.CLASSIFIER_APPLIED)]
    report = q.show(project, frame_limit=65, options=options)
    assert not report.classifier_error and doc.classifier_record_count == 2
    training, applied = doc.classifier_records
    assert training.archive.purpose == 'training' and applied.applied
    assert report.classifier_record_id == applied.id and applied.changed_objects == ids[-2:]
    assert doc.undo() and all(doc.annotation(i).class_name == '' for i in ids[-2:])
    path = tmp_path/'classifier-study.qpxproj'; project.save(str(path))
    reopened = q.Project.open(str(path)); current = reopened.document(image)
    reused = q.GuiOptions(); reused.classifier_open = True
    reused.width, reused.height, reused.ui_scale = 960, 720, 1.25
    reused.autosave_interval_seconds = 0; reused.classifier_record_id = training.id
    reused.inputs = [*click(4, q.GuiInputTarget.CLASSIFIER_USE_RECORD), ready(8, q.GuiInputWait.CLASSIFIER_RECORD_LOADED),
                     *click(12, q.GuiInputTarget.CLASSIFIER_PREDICT), ready(16, q.GuiInputWait.CLASSIFIER_PREDICTED),
                     *click(20, q.GuiInputTarget.CLASSIFIER_APPLY), ready(24, q.GuiInputWait.CLASSIFIER_APPLIED)]
    second = q.show(reopened, frame_limit=65, options=reused)
    assert not second.classifier_error and second.classifier_model.sha256 == training.archive.model.sha256
    assert second.classified_objects == 2 and current.classifier_record_count == 3
    assert report.ui_overflow_items == second.ui_overflow_items == 0
