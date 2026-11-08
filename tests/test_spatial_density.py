"""Thin density consumers; native tests own topology and numeric edge cases."""
import concurrent.futures
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
    doc.set_calibration(2, .5)
    window = doc.add_rectangle(q.Rect(0, 0, 20, 20))
    empty = doc.add_rectangle(q.Rect(30, 30, 2, 2))
    tissue = [doc.add_rectangle(q.Rect(x, 0, 10, 10)) for x in [0, 5]]
    excluded = [doc.add_rectangle(q.Rect(4, 0, 2, 10))]
    cells = [doc.add_cell(q.Roi.rectangle(q.Rect(x - 1, y - 1, 2, 2)))
             for x, y in [(2, 2), (5, 5), (8, 8)]]
    project = q.Project()
    image = project.add_document(doc, 'Tissue density')
    return project, image, doc, [window, empty], tissue, excluded, cells


def test_direct_job_retention_reopen_csv_and_lifetimes(tmp_path):
    project, image, doc, windows, tissue, excluded, cells = scene()
    request = q.SpatialDensityRequest(doc, windows[::-1], tissue[::-1], excluded, cells[::-1])
    result = q.measure_spatial_density(request)
    assert result.algorithm == 'qpx.roi-tissue-density/1'
    assert result.geometry_policy == 'qpx.polygon512-closed-exclusion/1'
    assert result.window_count == result.row_count == 2
    assert result.cell_count == 3 and result.tissue_count == 2 and result.exclusion_count == 1
    assert [r.window_id for r in result.rows()] == windows
    row, empty = result.rows()
    assert (row.count, row.area_pixels) == (2, 130)
    assert row.area_mm2 == pytest.approx(.00013)
    assert row.density_per_mm2 == pytest.approx(2 / .00013)
    assert row.footprint is not None and len(row.geometry_sha256) == 64
    assert empty.count == empty.area_pixels == empty.area_mm2 == 0
    assert empty.density_per_mm2 is empty.footprint is None
    assert result.belongs_to(request) and request.input_sha256 == result.input_sha256
    assert request.window_object(0).geometry.area == 400
    assert result.cell_object(0).kind == q.ObjectKind.CELL
    with pytest.raises(AttributeError):
        row.count = 999
    with pytest.raises((ValueError, IndexError)):
        result.rows(limit=4097)
    options = request.options
    options.prefer_nucleus = False
    assert request.options.prefer_nucleus
    job = q.SpatialDensityJob()
    assert job.request(request)
    stats = job.wait(doc)
    assert stats.state == 'ready' and stats.completed_rows == stats.total_rows == 2
    assert stats.membership_tests == result.membership_tests
    assert stats.geometry_operations == result.geometry_operations
    assert job.result.sha256 == result.sha256
    record_id = doc.keep_density(request, result)
    assert doc.density_record_count == len(doc.density_records) == 1
    assert doc.density_status(record_id) == 'current'
    path = tmp_path / 'density.qpxproj'
    project.save(str(path))
    reopened = q.Project.open(str(path)).document(image)
    retained = reopened.density_record(record_id)
    assert retained.result.sha256 == result.sha256
    assert retained.result.input_sha256 == result.input_sha256
    assert reopened.density_status(record_id) == 'unverified'
    replay = q.SpatialDensityRequest.replay(reopened, record_id)
    assert reopened.verify_density(record_id, replay, q.measure_spatial_density(replay))
    rows = list(csv.DictReader(io.StringIO(q.spatial_density_csv(retained.result))))
    assert [int(r['count']) for r in rows] == [2, 0]
    assert rows[1]['status'] == 'empty_area' and rows[1]['density_per_mm2'] == ''
    assert {r['units'] for r in rows} == {'cells/mm2'}
    target = tmp_path / 'density.csv'
    q.save_spatial_density_csv(retained.result, str(target))
    assert target.read_text(encoding='utf-8') == q.spatial_density_csv(result)
    reopened.add_cell(q.Roi.rectangle(q.Rect(1, 1, 1, 1)))
    replay = q.SpatialDensityRequest.replay(reopened, record_id)
    assert replay.cell_count == 3
    assert reopened.verify_density(record_id, replay, q.measure_spatial_density(replay))
    reopened.set_calibration(1, 1)
    with pytest.raises(ValueError, match='changed'):
        q.SpatialDensityRequest.replay(reopened, record_id)
    del request, job, result, project, doc, reopened
    gc.collect()
    assert retained.result.row(0).count == 2 and retained.result.window_object(0).geometry.area == 400


def test_owner_cancel_retry_quotas_and_stale():
    _, _, doc, windows, tissue, excluded, cells = scene()
    request = q.SpatialDensityRequest(doc, windows, tissue, excluded, cells)
    with concurrent.futures.ThreadPoolExecutor(1) as pool:
        assert pool.submit(q.measure_spatial_density, request).result().row(0).count == 2
        with pytest.raises(RuntimeError, match='owner'):
            pool.submit(request.matches, doc).result()
    job = q.SpatialDensityJob()
    assert job.request(request)
    job.cancel()
    assert job.wait(doc).state == 'cancelled' and job.result is None
    assert job.request(request) and job.wait(doc).state == 'ready'
    limit = q.SpatialDensityOptions()
    limit.max_membership_tests = 1
    bounded = q.SpatialDensityRequest(doc, windows, tissue, excluded, cells, limit)
    assert job.request(bounded) and job.wait(doc).state == 'failed' and job.result is None
    assert job.request(request)
    doc.rename_annotation(tissue[0], 'changed tissue')
    assert job.wait(doc).state == 'stale' and job.result is None
    with pytest.raises(ValueError, match='Cell'):
        q.SpatialDensityRequest(doc, windows, tissue, excluded, windows)
    with pytest.raises(ValueError):
        q.SpatialDensityRequest(doc, windows, [], excluded, cells)


def test_packaged_cli_measure_export_verify(tmp_path):
    project, image, doc, windows, tissue, excluded, cells = scene()
    original = tmp_path / 'input.qpxproj'
    project.save(str(original))
    expected = q.measure_spatial_density(q.SpatialDensityRequest(doc, windows, tissue, excluded, cells))
    output = tmp_path / 'measured.qpxproj'
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), '--measure-spatial', str(original), str(output), '--image', str(image),
               '--operation', 'tissue-density']
    for cohort, flag in [(windows, '--source'), (tissue, '--tissue'),
                         (excluded, '--exclude'), (cells, '--target')]:
        for obj in cohort:
            command += [flag, str(obj)]
    completed = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert completed.returncode == 0, completed.stdout + completed.stderr
    report = json.loads(completed.stdout)
    assert report['schema'] == 'qpx.spatial-density-cli/1' and report['units'] == 'cells/mm2'
    actual = q.Project.open(str(output)).document(image).density_record(report['record_id']).result
    assert actual.sha256 == expected.sha256
    export = tmp_path / 'cli.csv'
    common = ['--image', str(image), '--record', str(report['record_id']), '--operation', 'tissue-density']
    for args in [[str(cli), '--verify-spatial', str(output), *common],
                 [str(cli), '--export-spatial', str(output), str(export), *common]]:
        done = subprocess.run(args, capture_output=True, text=True, timeout=30)
        assert done.returncode == 0, done.stdout + done.stderr
    assert export.read_text(encoding='utf-8') == q.spatial_density_csv(expected)
