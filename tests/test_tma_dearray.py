"""Same native detector from Python; independent geometry, ownership and publication."""
import gc
import json
from pathlib import Path

import numpy as np
import pytest
import qupathxx as q

ROOT = Path(__file__).parent / 'fixtures/tma-dearray-v1'
CONTRACT = json.loads((ROOT / 'contract.json').read_text(encoding='utf-8'))


def scene(case):
    raw = (ROOT / case['mask']).read_bytes()
    header = f"P5\n{case['width']} {case['height']}\n255\n".encode()
    assert raw.startswith(header)
    gray = np.frombuffer(raw[len(header):], dtype=np.uint8).reshape(case['height'], case['width'])
    rgb = np.repeat((255-gray)[..., None], 3, axis=2)
    doc = q.Document(q.Image.from_numpy(rgb))
    options = q.TmaDearrayOptions()
    options.rows, options.columns = case['rows'], case['columns']
    options.core_width, options.core_height = case['core_width'], case['core_height']
    return doc, options


@pytest.mark.parametrize('case', CONTRACT['cases'], ids=lambda c: c['name'])
def test_preregistered_geometry_and_refusal(case):
    doc, options = scene(case)
    if case['expected'] == 'review_or_refuse':
        with pytest.raises((ValueError, RuntimeError)):
            q.detect_tma_grid(doc.server, options)
        assert doc.tma_core_count == 0
        return
    result = q.detect_tma_grid(doc.server, options)
    assert result.grid.rows == case['rows'] and result.grid.columns == case['columns']
    for actual, wire, expected in zip(result.cores, result.grid.cores, case['cores'], strict=True):
        assert np.hypot(actual.center.x-expected['center'][0], actual.center.y-expected['center'][1]) <= 2.0
        assert actual.observed == expected['present']
        assert wire.missing != expected['present']
        assert (actual.fit_error is not None) == actual.observed
    assert result.thumbnail_plan.source_pixels == case['width']*case['height']
    assert doc.tma_core_count == 0


def test_preview_and_server_equivalence_and_immutable_retained_result():
    doc, options = scene(CONTRACT['cases'][0])
    server = doc.server
    preview = server.read_region(q.RegionRequest(0, 0, 0, 480, 352))
    result = q.detect_tma_grid(preview, 480, 352, options)
    read = q.detect_tma_grid(server, options)
    assert result.input_sha256 == read.input_sha256
    assert result.recipe_sha256 != read.recipe_sha256
    assert result.thumbnail_plan is None
    grid = result.grid
    grid.image_name = 'changed copy'
    cores = grid.cores
    cores[0].bounds = q.Rect(1, 1, 2, 2)
    grid.cores = cores
    result.cores[0].center.x = 999
    assert result.grid.image_name != 'changed copy'
    assert result.grid.cores[0].bounds.x > 90
    assert result.cores[0].center.x == 112
    with pytest.raises(AttributeError):
        result.candidates = 0
    server.close()
    del doc, server, preview
    gc.collect()
    assert result.cores[-1].center.x == 292


def test_working_budget_admission_and_atomic_publication():
    doc, options = scene(CONTRACT['cases'][1])
    preview = doc.server.read_region(q.RegionRequest(0, 0, 0, 480, 352))
    options.work_bytes = q.tma_dearray_work_bytes(480, 352, options)
    result = q.detect_tma_grid(preview, 480, 352, options)
    assert result.estimated_work_bytes == options.work_bytes
    options.work_bytes -= 1
    with pytest.raises((ValueError, RuntimeError)):
        q.detect_tma_grid(preview, 480, 352, options)
    ids = q.import_qptma(doc, result.grid, 'Detected TMA')
    assert len(ids) == 12 and doc.tma_core_count == 12
    assert doc.annotation(ids[5]).tma_core.missing
    assert doc.undo() and doc.tma_core_count == 0
    assert doc.redo() and doc.tma_core_count == 12
    assert q.parse_qptma(q.qptma_text(result.grid)).cores[5].missing
