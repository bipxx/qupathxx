"""Historical opacity domains remain part of exact sampled-recipe replay."""
import sqlite3

import numpy as np
import pytest
import qupathxx as q

from test_cell_records import rewrite_checksum

pytestmark = pytest.mark.skipif(not q.has_project, reason='native project storage')


@pytest.mark.skipif(not q.has_tiff, reason='native RGBA TIFF')
@pytest.mark.parametrize('changed', [False, True], ids=['original', 'changed'])
def test_sampled_replay_compares_historical_domain(tmp_path, changed):
    import tifffile
    rgba = np.full((5, 5, 4), 255, dtype=np.uint8)
    rgba[0, 0, 3] = 0
    image = tmp_path/'white-alpha.tif'
    tifffile.imwrite(image, rgba, photometric='rgb', extrasamples='unassalpha', metadata=None)
    doc = q.Document(q.open_tiff(str(image)))
    obj = doc.add_rectangle(q.Rect(0, 0, 5, 5))
    parameters = q.CellParameters(q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
    parameters.context_padding = 0
    request = q.CellRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), parameters)
    result = q.detect_cells(request)
    assert result.cell_count == 0 and result.nuclei.samples == 24
    record = doc.keep_cells(request, result)
    project = q.Project()
    identity = project.add_document(doc)
    path = tmp_path/'domain.qpxproj'
    project.save(str(path))
    if changed:
        with sqlite3.connect(path) as db:
            data = bytearray(db.execute('SELECT run FROM cell_records').fetchone()[0])
            assert data[:4] == b'QCL\x04'
            # Swap an opaque and transparent sample inside the ROI, preserving
            # counts, empty output labels and a valid archive checksum.
            data[len(data)-64-4] ^= 3
            rewrite_checksum(data, result.nuclei.roi)
            db.execute('UPDATE cell_records SET run=?', (data,))
    restored = q.Project.open(str(path)).document(identity)
    assert restored.cell_status(record) == 'unverified'
    replay = q.CellRequest.replay(restored, record)
    fresh = q.detect_cells(replay)
    assert fresh.nuclei.labels == result.nuclei.labels and fresh.nuclei.samples == 24
    assert restored.verify_cells(record, replay, fresh) is (not changed)
    assert restored.cell_status(record) == ('verification_failed' if changed else 'current')


def test_sampled_domain_rejects_geometry_boundary_outside_sample_grid(tmp_path):
    doc = q.Document(q.Image.from_numpy(np.full((3, 3, 3), 255, dtype=np.uint8)))
    roi = q.Roi.ellipse(q.Rect(0, 0, 1.5, 3))
    assert roi.contains(q.Point(1.5, 1.5))
    obj = doc.add_roi(roi)
    parameters = q.CellParameters(q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)
    request = q.CellRequest(doc, obj, q.StainTransform([[1, 0, 0], [0, 1, 0]]), parameters)
    result = q.detect_cells(request)
    assert result.cell_count == 0 and result.nuclei.samples == 3
    doc.keep_cells(request, result)
    project = q.Project()
    project.add_document(doc)
    path = tmp_path/'boundary.qpxproj'
    project.save(str(path))
    with sqlite3.connect(path) as db:
        data = bytearray(db.execute('SELECT run FROM cell_records').fetchone()[0])
        # Move the middle-row opaque sample onto the ellipse's right tip. The
        # geometric point is covered, but its center is outside the sampling grid.
        data[len(data)-64-2] ^= (1 << 3) | (1 << 4)
        rewrite_checksum(data, result.nuclei.roi)
        db.execute('UPDATE cell_records SET run=?', (data,))
    with pytest.raises(ValueError, match='domain'):
        q.Project.open(str(path))
