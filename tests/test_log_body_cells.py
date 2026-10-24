"""Versioned production expansion: pinned labels, saved replay and all hosts."""
import gzip
import json
import os
from pathlib import Path
import subprocess

import numpy as np
import pytest
import qupathxx as q

from test_log_cells import parameters, scene, parse
from test_qupath_cells_reference import comparison, CONTRACT, CAPTURE


def body_parameters(tile=64):
    p = parameters(tile)
    p.algorithm = q.CellAlgorithm.LOG_WATERSHED_BODIES
    return p


@pytest.mark.parametrize('case', CONTRACT['cases'], ids=lambda c: c['name'])
@pytest.mark.parametrize('tile', CONTRACT['tile_sizes'])
def test_production_body_mode_pinned_labels_and_measurements(case, tile):
    if 'hole' in case and not q.has_geos:
        pytest.skip('polygon source requires GEOS')
    comparison.verify_capture(CAPTURE)
    doc, obj, transform = scene(case)
    p = body_parameters(tile)
    request = q.CellRequest(doc, obj, transform, p)
    result = q.detect_cells(request)
    assert result.algorithm == 'qpx.log-watershed-bodies/1'
    assert result.nuclei.input_format == 'qpx.log-bodies-rgba-tiles/1'
    previous = q.detect_cells(q.CellRequest(doc, obj, transform, parameters(tile)))
    assert result.nuclei.labels == previous.nuclei.labels
    width, height = CONTRACT['width'], CONTRACT['height']
    masks = {name: comparison.canvas(getattr(result, attr), width, height)
             for name, attr in [('nucleus', 'nuclei'), ('body', 'cells')]}
    if 'roi' not in case and case.get('grid_equivalent') is not False:
        for name, actual in masks.items():
            wanted = np.frombuffer(gzip.decompress((CAPTURE/case['name']/(name+'.u32le.gz')).read_bytes()),
                                   dtype='<u4').reshape(height, width)
            np.testing.assert_array_equal(actual, wanted)
    # Independent source-pixel arithmetic covers context and non-equivalent
    # grid cases without pretending they match upstream segmentation.
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 0, width, height)).numpy()[:, :, :3]
    for feature in result.features:
        for mask, count, means in [(masks['nucleus'], feature.nucleus_pixels, feature.nucleus_rgb),
                                   (masks['body'], feature.cell_pixels, feature.cell_rgb)]:
            selected = pixels[mask == feature.label]
            assert len(selected) == count
            np.testing.assert_allclose(selected.mean(axis=0), means, rtol=0, atol=1e-10)
    other = q.detect_cells(q.CellRequest(doc, obj, transform, body_parameters(32)))
    assert result.nuclei.labels == other.nuclei.labels and result.cells.labels == other.cells.labels


@pytest.mark.skipif(not q.has_project or not q.has_geos, reason='project/classifier geometry')
def test_body_archive_replay_classifier_and_legacy_records(tmp_path):
    doc, obj, transform = scene()
    records = []
    for p in [q.CellParameters(), parameters(), body_parameters()]:
        result = q.detect_cells(q.CellRequest(doc, obj, transform, p))
        request = q.CellRequest(doc, obj, transform, p)
        record = doc.keep_cells(request, result)
        records.append((record, result))
    body_record, body = records[-1]
    ids = doc.publish_cell_record(body_record)
    from test_object_classifier_archives import predicted
    options = q.ObjectFeatureOptions()
    options.cell_rgb = True
    _, job = predicted(doc, ids, options)
    job.apply(doc)
    source = doc.classifier_records[0].archive.table.cell_origin(0).source
    assert source.parameters.algorithm == q.CellAlgorithm.LOG_WATERSHED_BODIES
    project = q.Project()
    image = project.add_document(doc)
    path = tmp_path/'bodies.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    for record, original in records:
        saved = restored.cell_record(record).result
        assert saved.algorithm == original.algorithm
        assert saved.cells.labels == original.cells.labels
        replay = q.CellRequest.replay(restored, record)
        assert restored.verify_cells(record, replay, q.detect_cells(replay))
    source = restored.classifier_records[0].archive.table.cell_origin(0).source
    assert source.algorithm == body.algorithm and source.parameters.algorithm == body.parameters.algorithm
    rows = parse(q.cell_measurements_csv(q.CellTableRequest(restored, [body_record])))
    assert rows and all(r['detector_mode'] == 'log-bodies' for r in rows)


@pytest.mark.skipif(not q.has_project, reason='native batch storage')
def test_body_batch_mode_has_distinct_resume_and_cli_recipe(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    project.add_document(doc)
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.CELLS
    options.cells = parameters()
    path = tmp_path/'checkpoints'
    old = q.run_project_batch(q.ProjectBatchRequest(project, transform, options=options), str(path))
    assert old.saved == 1
    options.cells = body_parameters()
    request = q.ProjectBatchRequest(project, transform, options=options)
    changed = q.run_project_batch(request, str(path))
    assert changed.saved == 1 and changed.reused == 0
    assert q.run_project_batch(request, str(path)).reused == 1
    assert all(r['detector_mode'] == 'log-bodies' for r in parse(q.batch_checkpoint_csv(changed.images[0].path)))
    source = tmp_path/'input.qpxproj'
    project.save(str(source))
    from qupathxx import _native
    executable = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    run = subprocess.run([str(executable), str(source), str(tmp_path/'cli'), '--kind', 'cells',
                          '--cell-algorithm', 'log-bodies', '--threshold', '.4'],
                         capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stdout+run.stderr
    report = json.loads(run.stdout.splitlines()[-1])
    rows = parse(q.batch_checkpoint_csv(report['images'][0]['path']))
    assert rows and all(r['detector_mode'] == 'log-bodies' for r in rows)


@pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in desktop')
def test_body_gui_captured_recipe_and_published_pixels():
    from test_gui_replay import event
    doc, obj, transform = scene()
    options = q.GuiOptions()
    options.cell_parameters = body_parameters()
    expected = q.detect_cells(q.CellRequest(doc, obj, transform, body_parameters()))
    options.inputs = [event(2, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.KEEP_CELLS_KEY, wait=q.GuiInputWait.CELLS_READY)]
    report = q.show(doc, frame_limit=50, options=options)
    assert report.cell_state == 'ready' and not report.cell_error
    assert report.cell_result.algorithm == expected.algorithm
    assert report.cell_result.cells.labels == expected.cells.labels
    assert report.cell_annotations_added == 3 and report.cells_published
