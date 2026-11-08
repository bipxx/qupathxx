"""Thin consumer coverage; native tests own the exhaustive count oracle."""
import csv
import gc
import io
import json
import os
from pathlib import Path
import subprocess

import pytest
import qupathxx as q
from qupathxx import _native


def scene():
    doc = q.Document(q.Image.demo())
    ids = [doc.add_cell(q.Roi.rectangle(q.Rect(x - 1, y - 1, 2, 2)))
           for x, y in [(20, 20), (24, 20), (20, 28), (80, 80)]]
    doc.set_calibration(.5, .25)
    project = q.Project()
    image = project.add_document(doc, 'Calibrated cell neighborhoods')
    options = q.SpatialNeighborhoodOptions()
    options.units, options.radius = q.SpatialDistanceUnits.MICROMETRES, 2
    return project, image, doc, ids, options


def test_direct_job_retained_replay_and_csv(tmp_path):
    project, image, doc, ids, options = scene()
    request = q.SpatialNeighborhoodRequest(doc, list(reversed(ids)), ids, options)
    result = q.measure_spatial_neighborhoods(request)
    assert result.algorithm == 'qpx.centroid-radius-count/1'
    assert [row.source_id for row in result.rows()] == ids
    assert [row.count for row in result.rows()] == [2, 1, 1, 0]
    assert result.row_count == result.source_count == result.target_count == 4
    assert result.x_scale == .5 and result.y_scale == .25
    with pytest.raises((ValueError, IndexError)):
        result.rows(limit=4097)
    job = q.SpatialNeighborhoodJob()
    assert job.request(request)
    assert job.wait(doc).state == 'ready'
    assert job.result.sha256 == result.sha256
    record = doc.keep_neighborhood(request, result)
    assert doc.neighborhood_record_count == 1 and doc.neighborhood_status(record) == 'current'
    path = tmp_path / 'cells.qpxproj'
    project.save(str(path))
    restored = q.Project.open(str(path)).document(image)
    retained = restored.neighborhood_record(record)
    assert retained.result.sha256 == result.sha256
    assert retained.result.input_sha256 == result.input_sha256
    assert restored.neighborhood_status(record) == 'unverified'
    replay = q.SpatialNeighborhoodRequest.replay(restored, record)
    assert restored.verify_neighborhood(record, replay, q.measure_spatial_neighborhoods(replay))
    rows = list(csv.DictReader(io.StringIO(q.spatial_neighborhood_csv(retained.result))))
    assert [int(row['count']) for row in rows] == [2, 1, 1, 0]
    assert {row['units'] for row in rows} == {'um'} and {row['radius'] for row in rows} == {'2'}
    csv_path = tmp_path / 'counts.csv'
    q.save_spatial_neighborhood_csv(retained.result, str(csv_path))
    assert csv_path.read_text(encoding='utf-8') == q.spatial_neighborhood_csv(result)
    restored.add_cell(q.Roi.rectangle(q.Rect(19, 19, 2, 2)))
    replay = q.SpatialNeighborhoodRequest.replay(restored, record)
    assert replay.target_count == 4
    assert restored.verify_neighborhood(record, replay, q.measure_spatial_neighborhoods(replay))
    restored.set_calibration(1, 1)
    assert restored.neighborhood_status(record) == 'objects_or_calibration_changed'
    with pytest.raises(ValueError, match='changed'):
        q.SpatialNeighborhoodRequest.replay(restored, record)
    del request, job, project, doc, restored
    gc.collect()
    assert retained.result.row(0).count == 2


def test_cancel_retry_and_invalid_radius():
    _, _, doc, ids, options = scene()
    for value in [0, -1, float('inf'), float('nan')]:
        options.radius = value
        with pytest.raises(ValueError, match='radius'):
            q.SpatialNeighborhoodRequest(doc, ids, ids, options)
    options.radius = 2
    request = q.SpatialNeighborhoodRequest(doc, ids, ids, options)
    job = q.SpatialNeighborhoodJob()
    assert job.request(request)
    job.cancel()
    assert job.wait(doc).state == 'cancelled' and job.result is None
    assert job.request(request) and job.wait(doc).state == 'ready'
    assert [row.count for row in job.result.rows()] == [2, 1, 1, 0]


def test_packaged_cli_operation_and_csv(tmp_path):
    project, image, doc, ids, options = scene()
    original = tmp_path / 'input.qpxproj'
    project.save(str(original))
    expected = q.measure_spatial_neighborhoods(q.SpatialNeighborhoodRequest(doc, ids, ids, options))
    output = tmp_path / 'measured.qpxproj'
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), '--measure-spatial', str(original), str(output), '--image', str(image),
               '--operation', 'radius-count', '--radius', '2', '--units', 'um']
    for obj in ids:
        command += ['--source', str(obj), '--target', str(obj)]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    report = json.loads(completed.stdout)
    assert report['schema'] == 'qpx.spatial-neighborhood-cli/1' and report['radius'] == 2
    actual = q.Project.open(str(output)).document(image).neighborhood_record(report['record_id']).result
    assert actual.sha256 == expected.sha256
    export = tmp_path / 'cli.csv'
    common = ['--image', str(image), '--record', str(report['record_id']), '--operation', 'radius-count']
    for args in [
        [str(cli), '--verify-spatial', str(output), *common],
        [str(cli), '--export-spatial', str(output), str(export), *common],
    ]:
        done = subprocess.run(args, capture_output=True, text=True, timeout=30)
        assert done.returncode == 0, done.stdout + done.stderr
    assert export.read_text(encoding='utf-8') == q.spatial_neighborhood_csv(expected)
