"""Explicit polygon sampling preserves capture, archives and restart identity."""
import json
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import sqlite3

import numpy as np
import pytest
import qupathxx as q

from test_viewer_session import GUI, completed, run
from test_tissue_levels import pyramid_levels

COVERAGE = q.RoiSamplingPolicy.GEOMETRY_COVERAGE
WINDING = q.RoiSamplingPolicy.POLYGON_WINDING
pytestmark = pytest.mark.skipif(not q.has_geos, reason='polygon topology')


def scene():
    rgb = np.random.default_rng(109).integers(1, 255, (32, 32, 3), dtype=np.uint8)
    doc = q.Document(q.Image.from_numpy(rgb))
    # Sloped edges cross sample centers inside the half-open bounding grid.
    roi = q.Roi.polygon([q.Point(2.5, 2.5), q.Point(27.5, 2.5), q.Point(2.5, 27.5)])
    obj = doc.add_roi(roi)
    return doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), rgb


@pytest.mark.parametrize('reverse', [False, True])
def test_half_open_edges_holes_and_vertices(reverse):
    shell = [(0, 0), (10, 0), (10, 10), (0, 10)]
    hole = [(2, 2), (8, 2), (8, 8), (2, 8)]
    if reverse:
        shell.reverse(); hole.reverse()
    roi = q.Roi.polygon([q.Point(*p) for p in shell], [[q.Point(*p) for p in hole]])
    for xy, winding in [((0, 0), True), ((10, 0), False), ((10, 5), False),
                        ((0, 5), True), ((5, 10), False), ((2, 5), False),
                        ((8, 5), True), ((5, 2), False), ((5, 8), True)]:
        p = q.Point(*xy)
        assert roi.contains(p) and roi.contains_sample(p, COVERAGE)
        assert roi.contains_sample(p, WINDING) == winding
    with pytest.raises(ValueError):
        roi.contains_sample(q.Point(float('nan'), 0), WINDING)


@pytest.mark.parametrize('tile', [16, 31, 256])
@pytest.mark.parametrize('kind', [q.AnalysisKind.OPTICAL_DENSITY_MEANS, q.AnalysisKind.STAIN_MEANS])
def test_native_means_and_masks_match_independent_half_open_triangle(tile, kind):
    doc, obj, transform, rgb = scene()
    y, x = np.mgrid[:32, :32]
    mask = (x >= 2) & (y >= 2) & (x+y < 29)
    request = q.AnalysisRequest(doc, obj, transform, kind, tile_size=tile, sampling_policy=WINDING)
    result = q.analyze(request)
    od = np.log10(255 / rgb.astype(float))
    np.testing.assert_allclose(result.means, od[mask].mean(axis=0), atol=2e-7, rtol=1e-6)
    assert request.sampling_policy == result.sampling_policy == WINDING
    assert result.samples == int(mask.sum())
    assert result.input_sha256 == q.input_sha256(request)
    ordinary = q.AnalysisRequest(doc, obj, transform, kind, tile_size=tile)
    assert ordinary.sampling_policy == COVERAGE
    assert q.analyze(ordinary).samples > result.samples
    assert q.input_sha256(ordinary) != result.input_sha256
    p = q.TissueParameters(); p.threshold = 0; p.level = 0; p.tile_size = tile; p.sampling_policy = WINDING
    tissue_request = q.TissueRequest(doc, obj, transform, p)
    p.sampling_policy = COVERAGE  # Captured native value stays immutable.
    tissue = q.detect_tissue(tissue_request)
    assert tissue.parameters.sampling_policy == WINDING
    assert tissue.input_sha256 == result.input_sha256
    actual = np.zeros(mask.shape, dtype=bool)
    b = tissue.level_bounds
    bx, by, bw, bh = map(int, (b.x, b.y, b.width, b.height))
    actual[by:by+bh, bx:bx+bw] = np.frombuffer(tissue.mask, dtype=np.uint8).reshape(bh, bw) > 0
    np.testing.assert_array_equal(actual, mask)


@pytest.mark.skipif(not q.has_project, reason='project archives')
@pytest.mark.parametrize('policy', [COVERAGE, WINDING])
def test_project_replay_retains_policy_and_rejects_cross_policy(tmp_path, policy):
    doc, obj, transform, _ = scene()
    request = q.AnalysisRequest(doc, obj, transform, sampling_policy=policy)
    result = q.analyze(request); record = doc.keep_analysis(request, result)
    p = q.TissueParameters(); p.threshold = 0; p.level = 0; p.sampling_policy = policy
    tr = q.TissueRequest(doc, obj, transform, p); tissue = q.detect_tissue(tr)
    tissue_id = doc.keep_tissue(tr, tissue)
    project = q.Project(); image = project.add_document(doc)
    path = tmp_path/'sampling.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        blob = db.execute("SELECT mask FROM tissue_records").fetchone()[0]
        assert blob[:4] == b"QTM"+bytes([1 if policy == COVERAGE else 2])
    restored = q.Project.open(str(path)).document(image)
    replay = q.AnalysisRequest.replay(restored, record)
    assert replay.sampling_policy == policy
    assert restored.verify_analysis(record, replay, q.analyze(replay))
    treplay = q.TissueRequest.replay(restored, tissue_id)
    assert treplay.parameters.sampling_policy == policy
    assert restored.verify_tissue(tissue_id, treplay, q.detect_tissue(treplay))
    wrong = q.AnalysisRequest(restored, obj, transform, sampling_policy=WINDING if policy == COVERAGE else COVERAGE)
    with pytest.raises(ValueError, match="saved analysis parameters"):
        restored.verify_analysis(record, wrong, q.analyze(wrong))
    assert restored.tissue_record(tissue_id).result.labels == tissue.labels


@pytest.mark.skipif(not q.has_project, reason='batch archives')
@pytest.mark.parametrize('kind', [q.ProjectBatchKind.ROI, q.ProjectBatchKind.TISSUE])
def test_batch_manifest_and_resume_follow_sampling_policy(tmp_path, kind):
    doc, obj, transform, _ = scene(); project = q.Project(); project.add_document(doc)
    o = q.ProjectBatchOptions(); o.kind = kind; o.tissue.threshold = 0; o.tissue.level = 0
    first = q.ProjectBatchRequest(project, transform, options=o)
    assert q.run_project_batch(first, str(tmp_path)).saved == 1
    if kind == q.ProjectBatchKind.ROI:
        o.roi_sampling_policy = WINDING
        key = 'roi_sampling_policy'
    else:
        o.tissue.sampling_policy = WINDING
        key = 'tissue_sampling_policy'
    second = q.ProjectBatchRequest(project, transform, options=o)
    assert second.parameters_sha256 != first.parameters_sha256
    assert json.loads(second.parameters_json)[key] == 1
    assert key not in json.loads(first.parameters_json)
    report = q.run_project_batch(second, str(tmp_path))
    assert report.saved == 1 and report.reused == 0
    assert q.run_project_batch(second, str(tmp_path)).reused == 1
    o.kind = q.ProjectBatchKind.CELLS
    with pytest.raises(ValueError, match='sampling policy'):
        o.validate()


def test_live_command_queues_explicit_policy_without_renderer(tmp_path):
    session = q.ViewerSession(str(tmp_path/'not-opened.qpxproj'))
    ticket = session.analyze_with_sampling_policy(1, 1, q.StainTransform([[1, 0, 0], [0, 1, 0]]), WINDING)
    assert session.poll(ticket).action == 'analyze'
    assert session.cancel(ticket) and session.take(ticket).state == 'cancelled'
    assert session.stats.tickets == 0


@GUI
@pytest.mark.skipif(not q.has_project, reason='project archives')
def test_live_producer_archives_winding_analysis(tmp_path):
    doc, obj, transform, _ = scene(); project = q.Project(); image = project.add_document(doc)
    original = tmp_path/'original.qpxproj'; project.save(str(original))
    output = tmp_path/'output.qpxproj'; session = q.ViewerSession(str(original))
    expected = q.analyze(q.AnalysisRequest(doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS, sampling_policy=WINDING))
    def producer():
        event = completed(session, session.analyze_with_sampling_policy(image, obj, transform, WINDING))
        assert event.committed and event.analysis.sampling_policy == WINDING
        assert event.analysis.samples == expected.samples
        completed(session, session.save(str(output))); completed(session, session.close(True))
    run(session, producer)
    saved = q.Project.open(str(output)).document(image).latest_analysis(obj).result
    assert saved.sampling_policy == WINDING and saved.input_sha256 == expected.input_sha256


@pytest.mark.skipif(not q.has_project, reason='project archives')
@pytest.mark.parametrize('bad_policy', [0, 2, 2**64-1])
def test_archive_rejects_checksum_preserving_unknown_winding_policy(tmp_path, bad_policy):
    doc, obj, transform, _ = scene()
    p = q.TissueParameters(); p.level = 0; p.threshold = 0; p.sampling_policy = WINDING
    r = q.TissueRequest(doc, obj, transform, p); doc.keep_tissue(r, q.detect_tissue(r))
    project = q.Project(); project.add_document(doc); path = tmp_path/'bad.qpxproj'; project.save(str(path))
    with sqlite3.connect(path) as db:
        blob = bytearray(db.execute("SELECT mask FROM tissue_records").fetchone()[0])
        assert blob[:4] == b'QTM\x02'
        struct.pack_into('<Q', blob, 4, bad_policy)
        geometry = doc.annotation(obj).geometry.wkt.encode('utf-8')
        blob[-64:] = hashlib.sha256(struct.pack('<Q', len(geometry))+geometry+blob[:-64]).hexdigest().encode('ascii')
        db.execute("UPDATE tissue_records SET mask=?", (blob,))
    with pytest.raises(ValueError, match='sampling policy'):
        q.Project.open(str(path))


@pytest.mark.skipif(not q.has_project, reason='project batch CLI')
@pytest.mark.parametrize('kind', ['od', 'tissue'])
def test_cli_uses_the_same_sampling_policy_as_native_batch(tmp_path, kind):
    from qupathxx import _native
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    doc, obj, transform, _ = scene(); project = q.Project(); image = project.add_document(doc)
    source = tmp_path/'input.qpxproj'; project.save(str(source))
    flag = '--roi-sampling' if kind == 'od' else '--tissue-sampling'
    result = subprocess.run([str(cli), str(source), str(tmp_path/'output'), '--kind', kind,
                             flag, 'winding', '--threshold', '0', '--level', '0',
                             '--stains', '1,0,0,0,1,0'], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    checkpoints = list((tmp_path/'output').rglob('*.qpxproj'))
    assert len(checkpoints) == 1
    restored = q.Project.open(str(checkpoints[0])).document(image)
    if kind == 'od':
        actual = restored.latest_analysis(obj).result
        expected = q.analyze(q.AnalysisRequest(doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS, sampling_policy=WINDING))
        assert actual.sampling_policy == WINDING
        assert actual.samples == expected.samples and actual.input_sha256 == expected.input_sha256
    else:
        actual = restored.latest_tissue(obj).result
        p = q.TissueParameters(); p.threshold = 0; p.level = 0; p.sampling_policy = WINDING
        expected = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
        assert actual.parameters.sampling_policy == WINDING and actual.labels == expected.labels
    result = subprocess.run([str(cli), str(source), str(tmp_path/'bad-output'), '--kind', 'cells',
                             flag, 'winding'], capture_output=True, text=True)
    assert result.returncode == 2 and 'sampling policy' in result.stderr


@GUI
@pytest.mark.parametrize('workflow', ['roi', 'tissue'])
def test_imgui_requests_match_explicit_native_sampling(workflow):
    from test_gui_analysis import event
    doc, obj, _, _ = scene(); options = q.GuiOptions()
    options.roi_sampling_policy = options.tissue_sampling_policy = WINDING
    options.tissue_threshold = 0; options.tissue_level = 0
    transform = q.StainTransform([options.analysis_vectors[:3], options.analysis_vectors[3:]], options.analysis_white)
    key = q.GuiInputKind.ANALYZE_KEY if workflow == 'roi' else q.GuiInputKind.DETECT_TISSUE_KEY
    options.inputs = [event(4, q.GuiInputKind.SELECT_ALL_KEY), event(8, key)]
    report = q.show(doc, frame_limit=60, options=options)
    if workflow == 'roi':
        expected = q.analyze(q.AnalysisRequest(doc, obj, transform, q.AnalysisKind.OPTICAL_DENSITY_MEANS, sampling_policy=WINDING))
        assert report.analysis_state == 'ready', report.analysis_error
        assert report.analysis_samples == expected.samples
        np.testing.assert_array_equal(report.analysis_means, expected.means)
    else:
        p = q.TissueParameters(); p.threshold = 0; p.level = 0; p.sampling_policy = WINDING
        expected = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
        assert report.tissue_state == 'ready', report.tissue_error
        assert report.tissue_samples == expected.samples and report.tissue_input_sha256 == expected.input_sha256



def test_multipart_and_analytic_predicates_keep_their_domains():
    parts = [q.PolygonPart([q.Point(x, y) for x, y in [(0,0),(4,0),(4,4),(0,4)]]),
             q.PolygonPart([q.Point(x, y) for x, y in [(8,0),(12,0),(12,4),(8,4)]])]
    roi = q.Roi.multipolygon(parts)
    for xy, expected in [((0,2),True),((4,2),False),((8,2),True),((12,2),False),
                         ((6,2),False),((2,2),True),((10,2),True)]:
        assert roi.contains_sample(q.Point(*xy), WINDING) == expected
    for shape in [q.Roi.rectangle(q.Rect(0,0,4,4)), q.Roi.ellipse(q.Rect(0,0,4,4)),
                  q.Roi.polyline([q.Point(0,0),q.Point(4,4)]), q.Roi.points([q.Point(2,2)])]:
        for xy in [(0,0),(2,2),(4,2),(4,4),(6,6)]:
            point=q.Point(*xy)
            assert shape.contains_sample(point, WINDING) == shape.contains(point)


@pytest.mark.parametrize('span', [223, 997])
@pytest.mark.parametrize('origin', [16.5, 1048576.5])
@pytest.mark.parametrize('reverse', [False, True])
def test_exact_diagonal_boundary_uses_orientation_without_interpolation(span, origin, reverse):
    vertices = [q.Point(origin, origin), q.Point(origin+span, origin), q.Point(origin, origin+span)]
    if reverse:
        vertices.reverse()
    roi = q.Roi.polygon(vertices)
    for offset in range(1, span):
        point = q.Point(origin+offset, origin+span-offset)
        assert roi.contains(point)
        assert not roi.contains_sample(point, WINDING), offset


def test_long_diagonal_native_roi_and_tissue_counts():
    doc = q.Document(q.Image(256, 256, [100]*256*256*3))
    obj = doc.add_roi(q.Roi.polygon([q.Point(16.5,16.5), q.Point(239.5,16.5), q.Point(16.5,239.5)]))
    transform = q.StainTransform([[1,0,0],[0,1,0]])
    request = q.AnalysisRequest(doc, obj, transform, sampling_policy=WINDING)
    assert q.analyze(request).samples == 223*224//2
    coverage = q.AnalysisRequest(doc, obj, transform)
    assert q.analyze(coverage).samples == 223*224//2+222
    p = q.TissueParameters(); p.level = 0; p.threshold = 0; p.sampling_policy = WINDING
    tissue = q.detect_tissue(q.TissueRequest(doc, obj, transform, p))
    assert tissue.samples == tissue.foreground_pixels == 223*224//2


@pytest.mark.skipif(not q.has_project, reason='project archives')
@pytest.mark.parametrize('level', [1, 2])
@pytest.mark.parametrize('cleanup', [False, True])
def test_winding_pyramid_and_cleanup_archives_replay(tmp_path, pyramid_levels, level, cleanup):
    server, _ = pyramid_levels
    scale = server.metadata.levels[level].downsample
    doc = q.Document(server)
    roi = q.Roi.polygon([q.Point(.5*scale,.5*scale), q.Point(41.5*scale,.5*scale), q.Point(.5*scale,41.5*scale)])
    obj = doc.add_roi(roi); transform = q.StainTransform([[1,0,0],[0,1,0]])
    p = q.TissueParameters(); p.level = level; p.threshold = 0; p.sampling_policy = WINDING
    if cleanup:
        p.open_radius = 1
    request = q.TissueRequest(doc, obj, transform, p); result = q.detect_tissue(request)
    assert result.samples == 41*42//2
    assert result.algorithm == 'qpx.threshold-components/winding-'+('3' if cleanup else '2')
    assert result.input_format == 'qpx.rgba-level-tiles/winding-1'
    assert result.input_sha256 == q.input_sha256(request)
    if not cleanup:
        y,x = np.mgrid[:41,:41]
        np.testing.assert_array_equal(np.frombuffer(result.mask,dtype=np.uint8).reshape(41,41)>0, x+y<41)
    record = doc.keep_tissue(request, result); project = q.Project()
    image = project.add_document(doc, source_path=str(tmp_path/'tissue-levels.tif'))
    path = tmp_path/'pyramid.qpxproj'; project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    replay = q.TissueRequest.replay(restored, record); actual = q.detect_tissue(replay)
    assert actual.algorithm == result.algorithm and actual.labels == result.labels
    assert actual.parameters.sampling_policy == WINDING and actual.parameters.open_radius == int(cleanup)
    assert restored.verify_tissue(record, replay, actual)
