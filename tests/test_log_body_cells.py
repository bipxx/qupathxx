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
from test_tissue_levels import pyramid_levels


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
    records, ids, modes = [], [], {}
    for p in [q.CellParameters(), parameters(), body_parameters()]:
        request = q.CellRequest(doc, obj, transform, p)
        result = q.detect_cells(request)
        record = doc.keep_cells(request, result)
        records.append((record, result))
        published = doc.publish_cell_record(record)
        ids.extend(published)
        modes.update({identity: p.algorithm for identity in published})
    body_record, body = records[-1]
    from test_object_classifier_archives import predicted
    options = q.ObjectFeatureOptions()
    options.cell_rgb = True
    _, job = predicted(doc, ids, options)
    job.apply(doc)
    table = doc.classifier_records[0].archive.table
    for row, identity in enumerate(table.object_ids):
        assert table.cell_origin(row).source.parameters.algorithm == modes[identity]
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
    table = restored.classifier_records[0].archive.table
    for row, identity in enumerate(table.object_ids):
        assert table.cell_origin(row).source.parameters.algorithm == modes[identity]
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


@pytest.mark.parametrize('change', ['selection', 'rename', 'close', 'authority', 'cancel'])
def test_body_worker_rejects_stale_authority_and_cancellation(change):
    doc, obj, transform = scene()
    job = q.CellJob()
    assert job.request(q.CellRequest(doc, obj, transform, body_parameters()))
    selected = obj
    if change == 'selection':
        selected = 0
    elif change == 'rename':
        doc.rename_annotation(obj, 'changed')
        doc.undo()
    elif change == 'close':
        doc.server.close()
    elif change == 'authority':
        doc = q.Document(doc.server)
        assert doc.add_rectangle(q.Rect(0, 0, 256, 192)) == obj
    else:
        job.cancel()
    assert job.wait(doc, selected).state == ('cancelled' if change == 'cancel' else 'stale')
    assert job.result is None
    with pytest.raises(RuntimeError, match='no current'):
        job.keep_result(doc, selected)


def test_body_candidate_limit_failure_keeps_document_and_worker_reusable():
    doc, obj, transform = scene()
    p = body_parameters()
    p.max_cells = 1
    job = q.CellJob()
    revision = doc.revision
    assert job.request(q.CellRequest(doc, obj, transform, p))
    report = job.wait(doc, obj)
    assert report.state == 'failed' and 'limit' in report.error
    assert job.result is None and doc.revision == revision and doc.object_count == 1
    assert job.request(q.CellRequest(doc, obj, transform, body_parameters()))
    assert job.wait(doc, obj).state == 'ready' and job.result.cell_count == 3


@pytest.mark.skipif(not q.has_tiff, reason='native transparent TIFF')
def test_body_expansion_respects_transparent_source_domain(tmp_path):
    import tifffile
    from test_log_cells import phantoms
    case = next(c for c in CONTRACT['cases'] if c['name'] == 'isolated_three')
    rgb = phantoms.pixels(CONTRACT, case)
    rgba = np.concatenate([rgb, np.full((*rgb.shape[:2], 1), 255, dtype=np.uint8)], axis=2)
    rgba[55:65, 43:53, 3] = 0
    rgba[95:100, 110:115, 3] = 128
    path = tmp_path/'alpha.tif'
    tifffile.imwrite(path, rgba, photometric='rgb', extrasamples='unassalpha', metadata=None)
    doc = q.Document(q.open_tiff(str(path)))
    obj = doc.add_rectangle(q.Rect(0, 0, 256, 192))
    result = q.detect_cells(q.CellRequest(doc, obj, q.StainTransform(CONTRACT['vectors']), body_parameters()))
    opaque = rgba[..., 3] == 255
    assert result.nuclei.samples == int(opaque.sum())
    assert not result.nuclei.labels_numpy()[~opaque].any()
    assert not result.cells.labels_numpy()[~opaque].any()


@pytest.mark.parametrize('level', [0, 1, 2])
def test_body_mode_natural_level_identity_and_work_admission(pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(90, 90, 120, 90))
    p = body_parameters()
    p.level = level
    transform = q.StainTransform(CONTRACT['vectors'])
    request = q.CellRequest(doc, obj, transform, p)
    result = q.detect_cells(request)
    assert result.nuclei.level == level
    expected_format = 'qpx.log-bodies-rgba-level-tiles/1' if level else 'qpx.log-bodies-rgba-tiles/1'
    assert result.nuclei.input_format == expected_format
    assert q.input_sha256(request) == result.nuclei.input_sha256
    p.work_bytes = request.estimated_work_bytes-1
    with pytest.raises((ValueError, RuntimeError), match='budget'):
        q.CellRequest(doc, obj, transform, p)


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='managed desktop')
def test_body_managed_viewer_copies_recipe_and_reopens_retained_result(tmp_path):
    from test_viewer_session import completed, run
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source, destination = tmp_path/'input.qpxproj', tmp_path/'saved.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))

    def producer():
        p = body_parameters()
        ticket = session.cells(image, obj, transform, p, retain_result=True)
        p.algorithm = q.CellAlgorithm.DISTANCE_WATERSHED
        result = completed(session, ticket)
        assert result.committed and result.cells.cell_count == 3
        assert result.cells.parameters.algorithm == q.CellAlgorithm.LOG_WATERSHED_BODIES
        completed(session, session.save(str(destination)))
        completed(session, session.close(True))

    run(session, producer)
    saved = q.Project.open(str(destination)).document(image).latest_cells(obj).result
    assert saved.algorithm == 'qpx.log-watershed-bodies/1'


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='desktop batch')
def test_body_gui_batch_keeps_distinct_recipe(tmp_path):
    from test_gui_replay import event
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    preset = q.ProjectBatchOptions()
    preset.kind = q.ProjectBatchKind.CELLS
    preset.cells = body_parameters()
    options = q.GuiOptions()
    options.batch_options = preset
    options.batch_output_directory = str(tmp_path/'batch')
    options.inputs = [event(4, q.GuiInputKind.RUN_BATCH_KEY),
                      event(8, q.GuiInputKind.MOVE, wait=q.GuiInputWait.BATCH_FINISHED)]
    report = q.show(project, frame_limit=35, options=options)
    assert report.batch_state == 'succeeded' and not report.batch_error
    saved = q.Project.open(report.batch_result.images[0].path).document(image).latest_cells(obj).result
    assert saved.algorithm == 'qpx.log-watershed-bodies/1'
    assert report.ui_overflow_items == 0
