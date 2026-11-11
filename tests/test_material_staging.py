import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import pytest
import zipfile
import stat

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


def test_release_archive_requires_member_admission(tmp_path):
    stage, record = fixture(tmp_path)
    archive = tmp_path/'release.zip'
    with zipfile.ZipFile(archive, 'w') as out:
        out.write(stage/'example.bin', 'data/example.bin')
    assert guard.check_archive(archive, {'example': record}) == 1
    with pytest.raises(ValueError, match='outside open pool'):
        guard.check_archive(archive, {'example': record}, [tmp_path])
    wrapper = copy.deepcopy(record)
    wrapper.update(sha256=hashlib.sha256(archive.read_bytes()).hexdigest(), bytes=archive.stat().st_size)
    wrapper['material']['id'] = 'open-wrapper'
    with pytest.raises(ValueError, match='member lacks admitted'):
        guard.check_archive(archive, {'open-wrapper': wrapper})
    restricted = copy.deepcopy(record)
    restricted['material']['category'] = 'noncommercial'
    with pytest.raises(ValueError, match='Restricted/unreviewed'):
        guard.check_archive(archive, {'example': restricted, 'open-wrapper': wrapper})
    changed = copy.deepcopy(record)
    changed['material']['source_ids'] = ['restricted-parent']
    restricted['material']['id'] = 'restricted-parent'
    with pytest.raises(ValueError, match='Restricted/unreviewed'):
        guard.check_archive(archive, {'example': changed, 'restricted-parent': restricted})
    with pytest.raises(ValueError, match='member limit'):
        guard.check_archive(archive, {'example': record}, max_members=0)
    with pytest.raises(ValueError, match='byte limit'):
        guard.check_archive(archive, {'example': record}, max_bytes=1)


@pytest.mark.parametrize('name', ['../escape', '/absolute', 'C:/drive', 'a\\b', 'a/./b', 'noncommercial/payload'])
def test_archive_rejects_unsafe_paths(tmp_path, name):
    _, record = fixture(tmp_path)
    archive = tmp_path/'unsafe.zip'
    with zipfile.ZipFile(archive, 'w') as out:
        entry = zipfile.ZipInfo('placeholder')
        entry.filename = name  # Write the unsafe raw spelling, not a sanitized fixture.
        out.writestr(entry, b'independently specified test artifact')
    with pytest.raises(ValueError, match='[Uu]nsafe|Restricted'):
        guard.check_archive(archive, {'example': record})


def test_archive_rejects_duplicate_and_link(tmp_path):
    _, record = fixture(tmp_path)
    archive = tmp_path/'collision.zip'
    with zipfile.ZipFile(archive, 'w') as out:
        out.writestr('A.bin', b'one')
        out.writestr('a.bin', b'two')
    with pytest.raises(ValueError, match='colliding'):
        # First member must be admitted before reaching the collision.
        first = copy.deepcopy(record)
        first.update(sha256=hashlib.sha256(b'one').hexdigest(), bytes=3)
        guard.check_archive(archive, {'example': first})
    archive = tmp_path/'link.zip'
    entry = zipfile.ZipInfo('link')
    entry.create_system = 3
    entry.external_attr = (stat.S_IFLNK | 0o777) << 16
    with zipfile.ZipFile(archive, 'w') as out:
        out.writestr(entry, 'elsewhere')
    with pytest.raises(ValueError, match='Linked/special'):
        guard.check_archive(archive, {'example': record})


def test_complete_active_inventory_annotations_load():
    root = Path(__file__).parents[1]
    entries = guard.inventory([root/'tests/fixtures/manifest.json', root/'assets/fonts/font-source.json'])
    for ident in entries:
        guard.admitted(ident, entries)
