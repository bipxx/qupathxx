"""Selected stable-pyramid workflow, persisted mode and old snapshot boundary."""
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import sqlite3
import subprocess

import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native


@pytest.fixture
def scene(tmp_path):
    import tifffile
    path = tmp_path / 'pyramid.tif'
    with tifffile.TiffWriter(path) as writer:
        for level, (width, height) in enumerate([(512, 384), (205, 154)]):
            yy, xx = np.indices((height, width))
            dark = (xx % 17 < 5) | (yy % 13 < 3)
            rgb = np.repeat(np.where(dark, 30, 245).astype(np.uint8)[..., None], 3, axis=2)
            writer.write(rgb, tile=(32, 32), photometric='rgb', metadata=None, subfiletype=int(level > 0))
    project = q.Project()
    image = project.add_image(str(path))
    doc = project.document(image)
    obj = doc.add_rectangle(q.Rect(0, 0, 512, 384))
    transform = q.StainTransform([[.65, .70, .29], [.07, .99, .11]])
    return project, image, doc, obj, transform


def parameters(tile=31):
    p = q.TissueParameters()
    p.level, p.tile_size, p.canonical_pyramid = 1, tile, True
    return p


def test_snapshot_pixels_partitions_identity_and_qtm6(scene, tmp_path):
    project, image, doc, obj, transform = scene
    metadata = doc.metadata
    level = metadata.levels[1]
    pixels = doc.server.read_region(q.RegionRequest(0, 0, 1, level.width, level.height))
    rgba = pixels.numpy()
    od = np.maximum(0, np.log10(255 / np.maximum(rgba[..., :3].astype(float), 1)))
    expected = (rgba[..., 3] == 255) & (od.astype(np.float32).astype(float).sum(axis=2) >= .3)
    request = q.TissueRequest(doc, obj, transform, parameters(16))
    first = q.detect_tissue(request)
    second = q.detect_tissue(q.TissueRequest(doc, obj, transform, parameters(31)))
    np.testing.assert_array_equal(np.frombuffer(first.labels, dtype=np.uint32).reshape(expected.shape) != 0, expected)
    assert first.labels == second.labels
    assert first.input_sha256 == second.input_sha256 == q.input_sha256(request)
    assert first.algorithm == 'qpx.pyramid-threshold-components/1'
    record = doc.keep_tissue(request, first)
    path = tmp_path / 'saved.qpxproj'
    project.save(str(path))
    with sqlite3.connect(path) as db:
        assert db.execute('SELECT mask FROM tissue_records').fetchone()[0][:4] == b'QTM\x06'
    restored = q.Project.open(str(path)).document(image)
    replay = q.TissueRequest.replay(restored, record)
    assert replay.parameters.canonical_pyramid
    assert restored.verify_tissue(record, replay, q.detect_tissue(replay))
    rows = list(csv.DictReader(io.StringIO(q.measurements_csv(q.MeasurementTableRequest(
        restored, q.MeasurementTableKind.TISSUE, [record])))))
    assert sum(int(row['component_samples'] or 0) for row in rows) == first.foreground_pixels


def test_snapshot_budget_and_batch_consumers(scene, tmp_path):
    project, image, doc, obj, transform = scene
    p = parameters()
    request = q.TissueRequest(doc, obj, transform, p)
    p.work_bytes = request.estimated_work_bytes - 1
    with pytest.raises((ValueError, RuntimeError, IndexError), match='budget'):
        q.TissueRequest(doc, obj, transform, p)
    p = parameters()
    p.work_bytes = 128 * 1024 * 1024
    options = q.ProjectBatchOptions()
    options.kind, options.tissue = q.ProjectBatchKind.TISSUE, p
    request = q.ProjectBatchRequest(project, transform, options=options)
    assert json.loads(request.parameters_json)['tissue_canonical_pyramid'] is True
    source = tmp_path / 'input.qpxproj'
    project.save(str(source))
    sdk = q.run_project_batch(request, str(tmp_path / 'sdk'))
    assert sdk.state == 'succeeded' and sdk.saved == 1
    expected = q.Project.open(sdk.images[0].path).document(image).latest_tissue(obj).result
    cli = Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name == 'nt' else 'qupathxx_batch')
    command = [str(cli), str(source), str(tmp_path / 'cli'), '--kind', 'tissue', '--level', '1',
               '--tile-size', '31', '--tissue-canonical-pyramid', '--tissue-work-bytes', str(p.work_bytes)]
    done = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert done.returncode == 0, done.stdout + done.stderr
    report = json.loads(done.stdout.splitlines()[-1])
    actual = q.Project.open(report['images'][0]['path']).document(image).latest_tissue(obj).result
    assert actual.labels == expected.labels and actual.input_sha256 == expected.input_sha256
    again = subprocess.run(command, capture_output=True, text=True, timeout=30)
    assert again.returncode == 0 and json.loads(again.stdout.splitlines()[-1])['reused'] == 1
    options.kind = q.ProjectBatchKind.ROI
    with pytest.raises(ValueError, match='tissue batch'):
        q.ProjectBatchRequest(project, transform, options=options)


def test_legacy_zero_extension_keeps_recorded_exact_boundary(tmp_path):
    # This immutable small fixture and boundary were produced by source6219a08f.
    base = Path(__file__).resolve().parents[1] / 'docs/benchmarks/large-slide-workflow'
    fixture = base / 'legacy.qpxproj'
    baseline = json.loads((base / 'legacy-boundary.json').read_text(encoding='utf-8'))
    path = tmp_path / 'legacy.qpxproj'
    path.write_bytes(fixture.read_bytes())
    project = q.Project.open(str(path))
    record = project.document(baseline['image_id']).tissue_record(baseline['record_id'])
    assert not record.result.parameters.canonical_pyramid
    # Project snapshots charge the UTF-8 project path. Relocating this fixture
    # changes that charge, even on the original engine; retain the exact bound.
    budget = baseline['snapshot_bytes'] + len(str(path.resolve()).encode('utf-8')) - baseline['project_path_bytes']
    save = q.ProjectSaveJob(budget)
    assert save.request(project, str(path)) and save.wait().applied
    assert save.poll().snapshot_bytes == budget
    with pytest.raises((ValueError, RuntimeError, IndexError), match='snapshot exceeds memory budget'):
        q.ProjectSaveJob(budget - 1).request(project, str(path))
    with sqlite3.connect(path) as db:
        mask = db.execute('SELECT mask FROM tissue_records').fetchone()[0]
    assert hashlib.sha256(mask).hexdigest() == baseline['mask_sha256']
