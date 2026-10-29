"""Opt-in numerical policy must survive every native workflow boundary."""
import csv
import io
import json

import numpy as np
import pytest
import qupathxx as q

from test_nominal_cell_python import scene, request, FIXTURE
from test_nominal_cell_batches import cohort, execute, invoke, flags


def compatible():
    doc, obj, transform, p = scene()
    p.processing = q.CellProcessingPolicy.IMAGEJ_154P
    return doc, obj, transform, p


def test_processing_defaults_and_request_copy_preserve_distinct_recipes():
    doc, obj, transform, p = scene()
    assert p.processing == q.CellProcessingPolicy.QUANTITATIVE
    quantitative = request(doc, obj, transform, p)
    p.processing = q.CellProcessingPolicy.IMAGEJ_154P
    imagej = request(doc, obj, transform, p)
    p.processing = q.CellProcessingPolicy.QUANTITATIVE
    imagej.grid_parameters.processing = q.CellProcessingPolicy.QUANTITATIVE
    assert quantitative.grid_parameters.processing == p.processing
    assert imagej.grid_parameters.processing == q.CellProcessingPolicy.IMAGEJ_154P
    a, b = q.detect_cells(quantitative), q.detect_cells(imagej)
    assert a.algorithm == 'qpx.nominal-rgb-log-watershed-bodies/1'
    assert b.algorithm == 'qpx.nominal-rgb-imagej154p-log-watershed-bodies/1'
    assert a.nuclei.input_sha256 == b.nuclei.input_sha256
    for mask, name in [(b.nuclei, 'nucleus'), (b.cells, 'body')]:
        expected = np.frombuffer((FIXTURE / (name + '.u32le')).read_bytes(), dtype='<u4').reshape(128, 171)
        np.testing.assert_array_equal(mask.labels_numpy(), expected)


def test_imagej_processing_refuses_area_grids():
    doc, obj, transform, p = compatible()
    with pytest.raises(ValueError, match='nominal'):
        q.CellRequest.for_grid_roi(doc, obj, transform, q.GridCellSampling(), p)


@pytest.mark.skipif(not q.has_project, reason='requires project storage')
def test_processing_mixed_history_csv_and_saved_replay(tmp_path):
    doc, obj, transform, p = compatible()
    imagej = request(doc, obj, transform, p)
    run = doc.keep_cells(imagej, q.detect_cells(imagej))
    p.processing = q.CellProcessingPolicy.QUANTITATIVE
    quantitative = request(doc, obj, transform, p)
    old = doc.keep_cells(quantitative, q.detect_cells(quantitative))
    for calibration, schema in [(False, 9), (True, 10)]:
        options = q.CellTableOptions(); options.include_capture_calibration = calibration
        rows = list(csv.DictReader(io.StringIO(q.cell_measurements_csv(q.CellTableRequest(doc, [run, old], options)))))
        assert len(rows) == 6 and all(None not in row for row in rows)
        assert {row['table_schema'] for row in rows} == {'qpx.cell-table/' + str(schema)}
        assert {row['processing_policy'] for row in rows} == {'quantitative/1', 'imagej154p/1'}
    old_csv = q.cell_measurements_csv(q.CellTableRequest(doc, [old]))
    assert 'processing_policy' not in old_csv and 'qpx.cell-table/7' in old_csv
    project = q.Project(); image = project.add_document(doc)
    path = tmp_path / 'policies.qpxproj'; project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    for identity, policy in [(run, q.CellProcessingPolicy.IMAGEJ_154P), (old, q.CellProcessingPolicy.QUANTITATIVE)]:
        replay = q.CellRequest.replay(restored, identity)
        assert replay.grid_parameters.processing == policy
        assert restored.verify_cells(identity, replay, q.detect_cells(replay))


@pytest.mark.skipif(not q.has_project or not q.has_geos, reason='requires classifier publication')
def test_classifier_origin_retains_processing_policy(tmp_path):
    from test_object_classifier_archives import model_for
    doc, obj, transform, p = compatible(); captured = request(doc, obj, transform, p)
    run = doc.keep_cells(captured, q.detect_cells(captured)); ids = doc.publish_cell_record(run)
    options = q.ObjectFeatureOptions(); options.cell_rgb = True
    features = q.ObjectFeatureRequest(doc, ids, options); table = q.extract_object_features(features)
    job = q.ObjectClassifierJob(); assert job.predict(features, model_for(table.features))
    assert job.wait(doc).state == 'ready'; job.apply(doc)
    record = doc.classifier_records[0]
    project = q.Project(); image = project.add_document(doc)
    path = tmp_path / 'classifier.qpxproj'; project.save(str(path))
    origin = q.Project.open(str(path)).document(image).classifier_record(record.id).archive.table.cell_origin(0).source
    assert origin.grid_parameters.processing == q.CellProcessingPolicy.IMAGEJ_154P
    assert origin.algorithm == 'qpx.nominal-rgb-imagej154p-log-watershed-bodies/1'


@pytest.mark.skipif(not q.has_project, reason='requires project batches')
def test_batch_resume_does_not_alias_processing_policies(tmp_path):
    project, _, transform, options, _ = cohort()
    old = q.ProjectBatchRequest(project, transform, options=options)
    p = options.grid_cells; p.processing = q.CellProcessingPolicy.IMAGEJ_154P; options.grid_cells = p
    new = q.ProjectBatchRequest(project, transform, options=options)
    assert old.parameters_sha256 != new.parameters_sha256
    assert 'processing' not in json.loads(old.parameters_json)['nominal_cells']
    recipe = json.loads(new.parameters_json)['nominal_cells']
    assert recipe['processing'] == 'imagej154p/1'
    assert recipe['detector_algorithm'] == 'qpx.nominal-rgb-imagej154p-log-watershed-bodies/1'
    a = execute(old, tmp_path); b = execute(new, tmp_path); c = execute(new, tmp_path)
    assert a.saved == b.saved == c.reused == 2 and b.reused == c.saved == 0
    assert {entry.path for entry in a.images}.isdisjoint(entry.path for entry in b.images)
    for entry in b.images:
        record = q.Project.open(entry.path).document(entry.image_id).cell_records[0]
        assert record.result.grid_parameters.processing == q.CellProcessingPolicy.IMAGEJ_154P


@pytest.mark.skipif(not q.has_project, reason='requires native project CLI')
@pytest.mark.parametrize('before', [False, True])
def test_cli_processing_flag_order_and_retained_recipe(tmp_path, before):
    project, _, _, _, _ = cohort(); source = tmp_path / 'source.qpxproj'; project.save(str(source))
    policy = ['--cell-processing', 'imagej154p']
    result = invoke(source, tmp_path / 'out', policy + flags() if before else flags() + policy)
    assert result.returncode == 0, result.stdout + result.stderr
    saved = json.loads(result.stdout.splitlines()[-1])['images']
    assert len(saved) == 2
    for entry in saved:
        checkpoint = q.Project.open(entry['path']).document(entry['image_id'])
        assert checkpoint.cell_record_count == 1
        assert checkpoint.cell_records[0].result.grid_parameters.processing == q.CellProcessingPolicy.IMAGEJ_154P


def test_gui_options_copy_processing_policy():
    _, _, _, p = compatible(); options = q.GuiOptions(); options.grid_cell_parameters = p
    p.processing = q.CellProcessingPolicy.QUANTITATIVE
    options.grid_cell_parameters.processing = q.CellProcessingPolicy.QUANTITATIVE
    assert options.grid_cell_parameters.processing == q.CellProcessingPolicy.IMAGEJ_154P
