"""Versioned sampled-shape recipe: independent labels, historical domains and all hosts."""
import gzip
import json
import os
from pathlib import Path
import subprocess
import sqlite3

import numpy as np
import pytest
import qupathxx as q

from test_log_cells import parameters, scene, parse
from test_log_body_cells import body_parameters as legacy_body_parameters
from test_qupath_cells_reference import comparison, CONTRACT, CAPTURE
from test_tissue_levels import pyramid_levels


def sampled_parameters(tile=64):
    p = q.CellParameters(q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
    for name, value in CONTRACT['native'].items():
        setattr(p, name, value)
    p.min_peak_radius = 0
    p.min_seed_distance = 0
    p.tile_size = tile
    return p


@pytest.mark.parametrize('case', CONTRACT['cases'], ids=lambda c: c['name'])
@pytest.mark.parametrize('tile', CONTRACT['tile_sizes'])
def test_production_sampled_mode_pinned_labels_and_measurements(case, tile):
    if 'hole' in case and not q.has_geos:
        pytest.skip('polygon source requires GEOS')
    comparison.verify_capture(CAPTURE)
    doc, obj, transform = scene(case)
    p = sampled_parameters(tile)
    request = q.CellRequest(doc, obj, transform, p)
    result = q.detect_cells(request)
    assert result.algorithm == 'qpx.sampled-log-watershed-bodies/1'
    assert result.nuclei.input_format == 'qpx.sampled-log-rgba-tiles/1'
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
    other = q.detect_cells(q.CellRequest(doc, obj, transform, sampled_parameters(32)))
    assert result.nuclei.labels == other.nuclei.labels and result.cells.labels == other.cells.labels


@pytest.mark.skipif(not q.has_project or not q.has_geos, reason='project/classifier geometry')
def test_sampled_archive_replay_classifier_and_legacy_records(tmp_path):
    doc, obj, transform = scene()
    records, ids, modes = [], [], {}
    for p in [q.CellParameters(), parameters(), legacy_body_parameters(), sampled_parameters()]:
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
    with sqlite3.connect(path) as db:
        assert sorted(run[3] for (run,) in db.execute('SELECT run FROM cell_records')) == [1, 2, 3, 4]
        assert b'QCA\x04' in db.execute('SELECT payload FROM classifier_records').fetchone()[0]
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
    assert rows and all(r['detector_mode'] == 'log-sampled' for r in rows)


@pytest.mark.skipif(not q.has_project, reason='native batch storage')
def test_sampled_batch_mode_has_distinct_resume_and_cli_recipe(tmp_path):
    doc, obj, transform = scene()
    project = q.Project()
    project.add_document(doc)
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.CELLS
    options.cells = parameters()
    path = tmp_path/'checkpoints'
    old = q.run_project_batch(q.ProjectBatchRequest(project, transform, options=options), str(path))
    assert old.saved == 1
    options.cells = sampled_parameters()
    request = q.ProjectBatchRequest(project, transform, options=options)
    changed = q.run_project_batch(request, str(path))
    assert changed.saved == 1 and changed.reused == 0
    assert q.run_project_batch(request, str(path)).reused == 1
    assert all(r['detector_mode'] == 'log-sampled' for r in parse(q.batch_checkpoint_csv(changed.images[0].path)))
    source = tmp_path/'input.qpxproj'
    project.save(str(source))
    from qupathxx import _native
    executable = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    run = subprocess.run([str(executable), str(source), str(tmp_path/'cli'), '--kind', 'cells',
                          '--cell-algorithm', 'log-sampled', '--threshold', '.4'],
                         capture_output=True, text=True, timeout=60)
    assert run.returncode == 0, run.stdout+run.stderr
    report = json.loads(run.stdout.splitlines()[-1])
    rows = parse(q.batch_checkpoint_csv(report['images'][0]['path']))
    assert rows and all(r['detector_mode'] == 'log-sampled' for r in rows)


@pytest.mark.skipif(not q.has_gui or not q.has_geos or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='opt-in desktop')
def test_sampled_gui_captured_recipe_and_published_pixels():
    from test_gui_replay import event
    doc, obj, transform = scene()
    options = q.GuiOptions()
    options.cell_parameters = sampled_parameters()
    expected = q.detect_cells(q.CellRequest(doc, obj, transform, sampled_parameters()))
    options.inputs = [event(2, q.GuiInputKind.SELECT_ALL_KEY), event(4, q.GuiInputKind.DETECT_CELLS_KEY),
                      event(10, q.GuiInputKind.KEEP_CELLS_KEY, wait=q.GuiInputWait.CELLS_READY)]
    report = q.show(doc, frame_limit=50, options=options)
    assert report.cell_state == 'ready' and not report.cell_error
    assert report.cell_result.algorithm == expected.algorithm
    assert report.cell_result.cells.labels == expected.cells.labels
    assert report.cell_annotations_added == 3 and report.cells_published


@pytest.mark.parametrize('change', ['selection', 'rename', 'close', 'authority', 'cancel'])
def test_sampled_worker_rejects_stale_authority_and_cancellation(change):
    doc, obj, transform = scene()
    job = q.CellJob()
    assert job.request(q.CellRequest(doc, obj, transform, sampled_parameters()))
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


def test_sampled_candidate_limit_failure_keeps_document_and_worker_reusable():
    doc, obj, transform = scene()
    p = sampled_parameters()
    p.max_cells = 1
    job = q.CellJob()
    revision = doc.revision
    assert job.request(q.CellRequest(doc, obj, transform, p))
    report = job.wait(doc, obj)
    assert report.state == 'failed' and 'limit' in report.error
    assert job.result is None and doc.revision == revision and doc.object_count == 1
    assert job.request(q.CellRequest(doc, obj, transform, sampled_parameters()))
    assert job.wait(doc, obj).state == 'ready' and job.result.cell_count == 3


@pytest.mark.skipif(not q.has_tiff, reason='native transparent TIFF')
def test_sampled_expansion_respects_transparent_source_domain(tmp_path):
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
    result = q.detect_cells(q.CellRequest(doc, obj, q.StainTransform(CONTRACT['vectors']), sampled_parameters()))
    opaque = rgba[..., 3] == 255
    assert result.nuclei.samples == int(opaque.sum())
    assert not result.nuclei.labels_numpy()[~opaque].any()
    assert not result.cells.labels_numpy()[~opaque].any()
    if q.has_project:
        request = q.CellRequest(doc, obj, q.StainTransform(CONTRACT['vectors']), sampled_parameters())
        record = doc.keep_cells(request, q.detect_cells(request))
        project = q.Project()
        image = project.add_document(doc)
        saved = tmp_path/'alpha-domain.qpxproj'
        project.save(str(saved))
        restored = q.Project.open(str(saved)).document(image)
        assert restored.cell_record(record).result.cells.labels == result.cells.labels
        replay = q.CellRequest.replay(restored, record)
        assert restored.verify_cells(record, replay, q.detect_cells(replay))


@pytest.mark.parametrize('level', [0, 1, 2])
def test_sampled_mode_natural_level_identity_and_work_admission(pyramid_levels, level):
    server, _ = pyramid_levels
    doc = q.Document(server)
    obj = doc.add_rectangle(q.Rect(90, 90, 120, 90))
    p = sampled_parameters()
    p.level = level
    transform = q.StainTransform(CONTRACT['vectors'])
    request = q.CellRequest(doc, obj, transform, p)
    result = q.detect_cells(request)
    assert result.nuclei.level == level
    expected_format = 'qpx.sampled-log-rgba-level-tiles/1' if level else 'qpx.sampled-log-rgba-tiles/1'
    assert result.nuclei.input_format == expected_format
    assert q.input_sha256(request) == result.nuclei.input_sha256
    p.work_bytes = request.estimated_work_bytes-1
    with pytest.raises((ValueError, RuntimeError), match='budget'):
        q.CellRequest(doc, obj, transform, p)


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='managed desktop')
def test_sampled_managed_viewer_copies_recipe_and_reopens_retained_result(tmp_path):
    from test_viewer_session import completed, run
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    source, destination = tmp_path/'input.qpxproj', tmp_path/'saved.qpxproj'
    project.save(str(source))
    session = q.ViewerSession(str(source))

    def producer():
        p = sampled_parameters()
        ticket = session.cells(image, obj, transform, p, retain_result=True)
        p.algorithm = q.CellAlgorithm.DISTANCE_WATERSHED
        result = completed(session, ticket)
        assert result.committed and result.cells.cell_count == 3
        assert result.cells.parameters.algorithm == q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES
        completed(session, session.save(str(destination)))
        completed(session, session.close(True))

    run(session, producer)
    saved = q.Project.open(str(destination)).document(image).latest_cells(obj).result
    assert saved.algorithm == 'qpx.sampled-log-watershed-bodies/1'


@pytest.mark.skipif(not q.has_gui or not q.has_project or os.environ.get('QUPATHXX_TEST_GUI') != '1', reason='desktop batch')
def test_sampled_gui_batch_keeps_distinct_recipe(tmp_path):
    from test_gui_replay import event
    doc, obj, transform = scene()
    project = q.Project()
    image = project.add_document(doc)
    preset = q.ProjectBatchOptions()
    preset.kind = q.ProjectBatchKind.CELLS
    preset.cells = sampled_parameters()
    options = q.GuiOptions()
    options.batch_options = preset
    options.batch_output_directory = str(tmp_path/'batch')
    options.inputs = [event(4, q.GuiInputKind.RUN_BATCH_KEY),
                      event(8, q.GuiInputKind.MOVE, wait=q.GuiInputWait.BATCH_FINISHED)]
    report = q.show(project, frame_limit=35, options=options)
    assert report.batch_state == 'succeeded' and not report.batch_error
    saved = q.Project.open(report.batch_result.images[0].path).document(image).latest_cells(obj).result
    assert saved.algorithm == 'qpx.sampled-log-watershed-bodies/1'
    assert report.ui_overflow_items == 0


def test_sampled_constructor_preset_keeps_existing_defaults():
    old = q.CellParameters()
    assert old.algorithm == q.CellAlgorithm.DISTANCE_WATERSHED
    assert old.min_peak_radius == 1 and old.min_seed_distance == 4
    sampled = q.CellParameters(algorithm=q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
    assert sampled.min_peak_radius == sampled.min_seed_distance == 0
    assert sampled.connectivity == 8
    sampled.validate()


@pytest.mark.parametrize('field,value', [('min_peak_radius', 1.), ('min_seed_distance', 4.), ('connectivity', 4)])
def test_sampled_rejects_inactive_controls(field, value):
    p = sampled_parameters()
    setattr(p, field, value)
    with pytest.raises(ValueError, match='sampled LoG requires'):
        p.validate()


def test_sampled_inclusive_physical_area_matches_frozen_pixel_cutoff():
    doc, obj, transform = scene()
    result = q.detect_cells(q.CellRequest(doc, obj, transform, sampled_parameters()))
    exact = result.calibrated_feature(result.features[0].label).nucleus_sample_area_um2
    request = q.CellRequest.with_nucleus_area_limits(doc, obj, transform, exact, exact, sampled_parameters())
    kept = q.detect_cells(request)
    assert kept.cell_count > 0
    assert all(kept.calibrated_feature(f.label).nucleus_sample_area_um2 == exact for f in kept.features)


def test_sampled_physical_request_rejects_changed_calibration():
    doc, obj, transform = scene()
    job = q.CellJob()
    request = q.CellRequest.with_nucleus_area_limits(doc, obj, transform, 0., None, sampled_parameters())
    assert job.request(request)
    doc.set_calibration(.5, .6)
    assert job.wait(doc, obj).state == 'stale' and job.result is None


@pytest.mark.skipif(not q.has_project, reason='native project storage')
@pytest.mark.parametrize('change', ['domain', 'length', 'padding'])
def test_sampled_signed_invalid_historical_domain_is_rejected(tmp_path, change):
    from test_cell_records import rewrite_checksum
    doc = q.Document(q.Image.from_numpy(np.full((5, 5, 3), 255, dtype=np.uint8)))
    obj = doc.add_rectangle(q.Rect(0, 0, 5, 5))
    p = sampled_parameters();p.context_padding = 0
    request = q.CellRequest(doc, obj, q.StainTransform(CONTRACT['vectors']), p)
    result = q.detect_cells(request)
    assert result.cell_count == 0
    doc.keep_cells(request, result)
    project = q.Project();project.add_document(doc)
    path = tmp_path/'domain.qpxproj';project.save(str(path))
    with sqlite3.connect(path) as db:
        data = bytearray(db.execute('SELECT run FROM cell_records').fetchone()[0])
        assert data[:4] == b'QCL\x04'
        first = len(data)-64-4
        if change == 'domain':data[first] ^= 1
        if change == 'length':data[first-8] ^= 1
        if change == 'padding':data[-65] |= 128
        rewrite_checksum(data, result.nuclei.roi)
        db.execute('UPDATE cell_records SET run=?', (data,))
    with pytest.raises(ValueError, match='domain'):
        q.Project.open(str(path))


@pytest.mark.skipif(not os.environ.get('QUPATHXX_HAMAMATSU_FIXTURE'), reason='opt-in pinned Hamamatsu NDPI fixture')
@pytest.mark.parametrize('padding', [0, 32])
def test_sampled_real_slide_keeps_characterization_and_exact_saved_replay(tmp_path, padding):
    import importlib.util
    root = Path(__file__).resolve().parents[1]
    spec = importlib.util.spec_from_file_location('sampled_ndpi_capture', root/'tools/compare_qupath_ndpi_cells.py')
    capture = importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
    folder = root/'tests/fixtures/qupath-ndpi-cells-0.7.0/capture'
    contract, reference = capture.verify_capture(folder)
    image = Path(os.environ['QUPATHXX_HAMAMATSU_FIXTURE'])
    capture.identify_image(image, contract)
    server = q.open_slide(str(image))
    try:
        doc = q.Document(server);obj = doc.add_rectangle(q.Rect(*contract['roi']))
        transform = q.StainTransform(contract['vectors'], contract['white'])
        p = q.CellParameters(q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
        for name, value in contract['native'].items():setattr(p, name, value)
        p.min_peak_radius = p.min_seed_distance = 0;p.context_padding = padding;p.tile_size = 64
        request = q.CellRequest(doc, obj, transform, p);result = q.detect_cells(request)
        # Freeze the declared /1 recipe, without calling this biological parity.
        baseline = json.loads((root/'docs/benchmarks/sampled-shape-ndpi-windows.json').read_text())
        report = next(r for r in baseline['reports'] if r['context_padding'] == padding)
        wanted = next(r for r in report['results'] if r['mode'] == 'sampled_shape')
        import hashlib
        labels = capture.local_labels(result.nuclei, contract['roi'])
        assert result.cell_count == wanted['native_count']
        assert hashlib.sha256(labels.astype('<u4', copy=False).tobytes()).hexdigest() == wanted['labels_sha256']
        pixels = server.read_region(q.RegionRequest(*contract['roi'][:2], 0, *contract['roi'][2:])).numpy()
        bodies = capture.local_labels(result.cells, contract['roi'])
        for f in result.features:
            for mask, prefix in [(labels, 'nucleus'), (bodies, 'cell')]:
                selected = pixels[..., :3][mask == f.label]
                assert len(selected) == getattr(f, prefix+'_pixels')
                np.testing.assert_allclose(selected.mean(axis=0), getattr(f, prefix+'_rgb'), rtol=0, atol=1e-10)
        record = doc.keep_cells(request, result)
        project = q.Project();image_id = project.add_document(doc)
        path = tmp_path/'ndpi-sampled.qpxproj';project.save(str(path))
        restored = q.Project.open(str(path)).document(image_id)
        saved = restored.cell_record(record).result
        assert saved.nuclei.labels == result.nuclei.labels and saved.cells.labels == result.cells.labels
        replay = q.CellRequest.replay(restored, record)
        assert restored.verify_cells(record, replay, q.detect_cells(replay))
        assert reference['count'] == 506
    finally:
        server.close()
