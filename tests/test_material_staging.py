import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import pytest

spec = importlib.util.spec_from_file_location('material_staging', Path(__file__).parents[1] / 'tools/ci/check_material_staging.py')
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


def fixture(tmp_path):
    stage = tmp_path / 'stage'
    stage.mkdir()
    payload = b'independently specified test artifact'
    (stage / 'example.bin').write_bytes(payload)
    record = {'sha256': hashlib.sha256(payload).hexdigest(), 'bytes': len(payload),
              'url': 'https://example.invalid/exact-fixture', 'material': {
                  'id': 'example', 'category': 'admitted_open', 'kind': 'evaluation_data',
                  'license_id': 'CC0-1.0', 'license_evidence': 'independent test declaration',
                  'attribution': 'qupathxx test authors', 'source_ids': []}}
    return stage, record


def test_admission_and_refusals(tmp_path):
    stage, record = fixture(tmp_path)
    assert guard.check_stage(stage, {'example': record}) == 1
    for category in ('proprietary', 'noncommercial', 'unreviewed'):
        changed = copy.deepcopy(record)
        changed['material']['category'] = category
        with pytest.raises(ValueError, match='Restricted/unreviewed'):
            guard.check_stage(stage, {'example': changed})
    for license_id in ('CC-BY-NC-4.0', 'research only', 'unknown'):
        changed = copy.deepcopy(record)
        changed['material']['license_id'] = license_id
        with pytest.raises(ValueError, match='Unreviewed license'):
            guard.check_stage(stage, {'example': changed})
    changed = copy.deepcopy(record)
    changed['material']['kind'] = 'derived_asset'
    changed['material']['source_ids'] = ['restricted-source']
    parent = copy.deepcopy(record)
    parent['material'].update(id='restricted-source', category='noncommercial')
    with pytest.raises(ValueError, match='Restricted/unreviewed'):
        guard.check_stage(stage, {'example': changed, 'restricted-source': parent})
    changed['material']['source_ids'] = ['missing']
    with pytest.raises(ValueError, match='missing material source'):
        guard.check_stage(stage, {'example': changed})
    changed['material']['source_ids'] = ['example']
    with pytest.raises(ValueError, match='Cyclic'):
        guard.check_stage(stage, {'example': changed})
    with pytest.raises(ValueError, match='outside open pool'):
        guard.check_stage(stage, {'example': record}, [stage])
    changed = copy.deepcopy(record)
    changed['source'] = '../qupathxx-restricted/proprietary/example.bin'
    with pytest.raises(ValueError, match='Restricted source reference'):
        guard.check_stage(stage, {'example': changed})
    (stage / 'unregistered.bin').write_bytes(b'unknown')
    with pytest.raises(ValueError, match='lacks admitted'):
        guard.check_stage(stage, {'example': record})


def test_inventory_and_actual_font_assets(tmp_path):
    _, record = fixture(tmp_path)
    path = tmp_path / 'inventory.json'
    path.write_text(json.dumps({'downloads': [record]}), encoding='utf-8')
    assert guard.inventory([path])['example'] == record
    with pytest.raises(ValueError, match='duplicate'):
        guard.inventory([path, path])
    root = Path(__file__).parents[1]
    entries = guard.inventory([root / 'assets/fonts/font-source.json'])
    stage = tmp_path / 'fonts'
    stage.mkdir()
    for font in (root / 'assets/fonts').glob('IBMPlexSans-*.ttf'):
        (stage / font.name).write_bytes(font.read_bytes())
    assert guard.check_stage(stage, entries) == 2


def test_restricted_paths_and_links(tmp_path):
    stage, record = fixture(tmp_path)
    restricted = stage / 'noncommercial'
    restricted.mkdir()
    with pytest.raises(ValueError, match='Restricted pool path'):
        guard.check_stage(stage, {'example': record})
    restricted.rmdir()
    link = stage / 'alias'
    try:
        link.symlink_to(stage / 'example.bin')
    except OSError:
        pytest.skip('Host cannot create a symlink')
    with pytest.raises(ValueError, match='link/reparse'):
        guard.check_stage(stage, {'example': record})
