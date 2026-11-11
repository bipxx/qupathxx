"""Authored development-package boundaries; no engine or external module builds."""
import hashlib
import json
from pathlib import Path
import sys

import pytest

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / 'tools/ci'))
import stage_native_release as release


def record(path, ident, parents=()):
    return {'filename': path.name, 'source': 'independently authored plugin deployment test',
            'bytes': path.stat().st_size, 'sha256': release.sha(path), 'material': {
                'id': ident, 'category': 'admitted_open', 'kind': 'derived_asset' if parents else 'code',
                'license_id': 'AGPL-3.0-only', 'license_evidence': 'LICENSE',
                'attribution': 'qupathxx test authors', 'source_ids': list(parents)}}


def fixture(tmp_path):
    root, installed = tmp_path / 'source', tmp_path / 'installed'
    root.mkdir(); installed.mkdir()
    recipe = ['cpp/include/qupathxx/reader_plugin.h', 'cpp/ppm_decoder.hpp', 'cpp/ppm_decoder.cpp',
              'examples/reader_plugin/CMakeLists.txt', 'examples/reader_plugin/p6_plugin.cpp',
              'examples/reader_plugin/reader_plugin_profile.hpp.in', 'examples/reader_plugin/write_manifest.cmake']
    authored = ['LICENSE', 'CMakeLists.txt', 'pyproject.toml', 'cpp/header_oracle.c', *recipe]
    for name in authored:
        path = root / name; path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('authored source ' + name, encoding='utf8')
    source_id = hashlib.sha256(release.compiled_manifest(root)).hexdigest()
    identity = 'share/qupathxx/reader-plugin/reader-plugin-host-source.json'
    descriptor_path = 'plugins/p6/reader-plugins.json'
    inputs = {'qupathxx/qupathxx_region.exe': b'authored host stand-in',
              'include/qupathxx/reader_plugin.h': (root / recipe[0]).read_bytes(),
              'plugins/p6/qpx_p6_plugin.dll': b'authored independent module stand-in',
              identity: json.dumps({'schema': 'qpx.reader-plugin-host-source/1', 'source_sha256': source_id, 'profile': 'windows-x64-msvc1944-release-md'}).encode(),
              descriptor_path: json.dumps({'schema': 'qpx.reader-plugins/1', 'plugins': [{
                  'provider_id': 'org.qupathxx.p6', 'abi': 1, 'version': '1.0.0',
                  'contract': 'p6-max255-display/1', 'display_name': 'P6 display example',
                  'profile': 'windows-x64-msvc1944-release-md',
                  'capabilities': {'display_rgba': True, 'raw_samples': False}, 'dependencies': [],
                  'module': {'path': 'qpx_p6_plugin.dll', 'sha256': hashlib.sha256(b'authored independent module stand-in').hexdigest()}}]}).encode()}
    for name, data in inputs.items():
        path = installed / name; path.parent.mkdir(parents=True, exist_ok=True); path.write_bytes(data)
    records = [record(root / name, 'source-' + name) for name in authored]
    records += [record(installed / name, 'install-' + name, ['source-LICENSE']) for name in inputs
                if name != 'include/qupathxx/reader_plugin.h']
    for entry in records:
        if entry['material']['id'] == 'install-plugins/p6/qpx_p6_plugin.dll':
            entry['material']['source_ids'] += ['source-' + name for name in recipe]
    stages = {name: [] for name in ['host', 'kit', 'module', 'source']}
    for name in inputs:
        group = 'module' if name.startswith('plugins/') else 'kit' if name.startswith('include/') else 'host'
        ident = 'source-' + recipe[0] if name == 'include/qupathxx/reader_plugin.h' else 'install-' + name
        stages[group].append({'path': name, 'input': name, 'material_id': ident})
    stages['source'] = [{'path': name, 'input': name, 'material_id': 'source-' + name} for name in authored]
    profile = {'kind': 'reader-plugin-dev', 'compiled_source_sha256': source_id,
               'compiled_material_id': 'source-CMakeLists.txt', 'firstparty_license': 'source-LICENSE',
               'installed_source_identity': identity, 'plugin_descriptor_input': descriptor_path,
               'plugin_recipe_inputs': recipe, 'sources': [], 'stages': stages}
    profile['module_recipe_material_ids'] = ['source-' + name for name in recipe]
    inventory = root / 'inventory.json'
    value = {'records': records, 'release_profiles': {'authored-plugin-test': profile}}
    inventory.write_text(json.dumps(value), encoding='utf8')
    return root, installed, inventory, value


def test_plugin_stage_checks_actual_modules_source_and_every_archive_member(tmp_path):
    root, installed, inventory, _ = fixture(tmp_path)
    output = tmp_path / 'admitted'
    result = release.stage_release(root, installed, output, [inventory], 'authored-plugin-test')
    assert result['state'] == 'passed' and set(result['stages']) == {'host', 'kit', 'module', 'source'}
    entries = release.admission.inventory([inventory, output / 'generated-materials.json'])
    for stage in result['stages'].values():
        assert release.admission.check_archive(output / stage['archive'], entries) == stage['archive_members']
    assert release.compiled_manifest(output / 'source') == release.compiled_manifest(root)
    assert b'cpp/header_oracle.c=' in release.compiled_manifest(root)


@pytest.mark.parametrize('mode,reason', [
    ('extra', 'lacks admitted'), ('recipe', 'explicit reviewed build inputs'),
    ('host-id', 'different compiled source identity'), ('module-binding', 'actual admitted module'),
    ('changed-c', 'compiled source identity'), ('module-lineage', 'corresponding-source recipe lineage')])
def test_plugin_stage_refuses_specific_unreviewed_or_mismatched_inputs(tmp_path, mode, reason):
    root, installed, inventory, value = fixture(tmp_path)
    if mode == 'extra': (installed / 'unknown.bin').write_bytes(b'not admitted')
    if mode == 'recipe': value['release_profiles']['authored-plugin-test']['plugin_recipe_inputs'].remove('examples/reader_plugin/CMakeLists.txt')
    if mode == 'module-lineage':
        for entry in value['records']:
            if entry['material']['id'] == 'install-plugins/p6/qpx_p6_plugin.dll':
                entry['material']['source_ids'] = ['source-LICENSE']
    if mode == 'changed-c': (root / 'cpp/header_oracle.c').write_bytes(b'changed C source')
    if mode in {'host-id', 'module-binding'}:
        name = 'share/qupathxx/reader-plugin/reader-plugin-host-source.json' if mode == 'host-id' else 'plugins/p6/reader-plugins.json'
        path = installed / name; body = json.loads(path.read_text())
        if mode == 'host-id': body['source_sha256'] = '0' * 64
        else: body['plugins'][0]['module']['sha256'] = '0' * 64
        path.write_text(json.dumps(body), encoding='utf8')
        # Keep byte admission valid, isolating the semantic binding boundary.
        for entry in value['records']:
            if entry['material']['id'] == 'install-' + name:
                entry.update(bytes=path.stat().st_size, sha256=release.sha(path))
    inventory.write_text(json.dumps(value), encoding='utf8')
    with pytest.raises(ValueError, match=reason):
        release.stage_release(root, installed, tmp_path / 'refused', [inventory], 'authored-plugin-test')
    assert not (tmp_path / 'refused').exists()
