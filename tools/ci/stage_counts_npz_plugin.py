"""Stage exact native matrix NPZ sources, notices and a validated Windows module."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
import zipfile

from check_material_staging import check_archive, check_stage, inventory

ROOT = Path(__file__).resolve().parents[2]
PRODUCT = tuple('plugins/counts_npz/' + name for name in ('CMakeLists.txt', 'counts_npz.cpp',
    'npy_array.hpp', 'npz_members.hpp', 'npz_matrix.hpp', 'npz_zlib.cmake')) + tuple('plugins/format-common/' + name for name in (
    'plugin_build.cmake', 'format_plugin_profile.hpp.in', 'write_manifest.cmake', 'binary_input.hpp', 'reader_input.hpp')) + (
    'plugins/image-common/gzip_member.hpp',)
GUIDES = ('plugins/counts_npz/tests/CMakeLists.txt', 'plugins/counts_npz/tests/project_probe.cpp',
          'tools/ci/check_counts_npz_plugin.py')
ZLIB = 'bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('module', 'validation', 'kit', 'zlib-archive', 'output', 'archive'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--profile', choices=('counts', 'continuous'), default='counts')
    args = parser.parse_args()
    group_name = args.profile + '_npz_plugin'
    provider = f'org.qupathxx.{args.profile}-npz'
    abi = 1
    product, guides = PRODUCT, GUIDES
    if args.profile == 'continuous':
        abi = 2
        product = tuple(path for path in PRODUCT if path not in (
            'plugins/counts_npz/CMakeLists.txt', 'plugins/counts_npz/counts_npz.cpp')) + (
            'plugins/continuous_npz/CMakeLists.txt', 'plugins/continuous_npz/continuous_npz.cpp',
            'plugins/format-common/json_tree.hpp')
        guides += ('tools/ci/check_continuous_npz_plugin.py',)
    module = args.module.read_bytes()
    manifest = args.module.with_name('format-plugins.json').read_bytes()
    item, = json.loads(manifest)['plugins']
    result = json.loads(args.validation.read_bytes())
    assert result['passed'] and result['module_sha256'] == sha(module) and result['manifest_sha256'] == sha(manifest)
    assert item['provider_id'] == provider and item['abi'] == abi and not item['dependencies']
    assert item['module']['sha256'] == sha(module) and item['module']['path'] == args.module.name
    zlib = args.zlib_archive.read_bytes()
    assert sha(zlib) == ZLIB
    inventory_path = ROOT / 'tests/fixtures/manifest.json'
    original = inventory_path.read_text(encoding='utf-8')
    group = json.loads(original).get(group_name, {'members': [], 'deliveries': {}})
    assert sha(module) not in group['deliveries'], 'This exact module already has a staged delivery'
    paths, membership = {}, {}
    records = {record['material']['id']: record for record in group['members']}
    material_prefix = 'npz-module' if args.profile == 'counts' else 'continuous-npz-module'

    def add(name, data, origins=('qupathxx-root-license',), kind='code', license_id='AGPL-3.0-only'):
        ident = f'{material_prefix}-{license_id}-{sha(data)[:24]}'
        paths[name], membership[name] = data, ident
        records.setdefault(ident, {'filename': Path(name).name, 'source': name, 'bytes': len(data), 'sha256': sha(data),
            'material': {'id': ident, 'category': 'admitted_open', 'kind': kind, 'license_id': license_id,
                'license_evidence': 'licenses/zlib/LICENSE' if license_id == 'Zlib' else 'LICENSE',
                'attribution': 'Jean-loup Gailly and Mark Adler' if license_id == 'Zlib' else 'qupathxx contributors',
                'source_ids': list(dict.fromkeys(origins))}})
        return ident

    sources = [add('source/' + path, (ROOT / path).read_bytes()) for path in product]
    for path in guides:
        add('source/' + path, (ROOT / path).read_bytes())
    for name in ('include/qupathxx/format_plugin.h', 'include/qupathxx/reader_plugin.h',
                 'share/qupathxx/format-plugin/profile.cmake', 'share/qupathxx/format-plugin/LICENSE'):
        sources.append(add('source/format-plugin-kit/' + name, (args.kit / name).read_bytes(),
                           kind='notice' if name.endswith('/LICENSE') else 'code'))
    name = 'source/third_party/zlib-1.3.2.tar.gz'
    paths[name], membership[name] = zlib, 'release-src-zlib'
    with tarfile.open(fileobj=io.BytesIO(zlib), mode='r:gz') as dep:
        notice = dep.extractfile('zlib-1.3.2/LICENSE').read()
    name = 'licenses/zlib/LICENSE'
    assert (args.module.parent / name).read_bytes() == notice
    sources.extend(('release-src-zlib', add(name, notice, ('release-src-zlib',), 'notice', 'Zlib')))
    add('licenses/qupathxx/LICENSE', (ROOT / 'LICENSE').read_bytes(), kind='notice')
    add('README.md', (ROOT / f'plugins/{args.profile}_npz/README.md').read_bytes(), kind='asset')
    binary = add(args.module.name, module, sources)
    add('format-plugins.json', manifest, (binary,), 'asset')
    info = {'module_sha256': sha(module), 'validation_sha256': sha(args.validation.read_bytes()),
            'source_files': {name: sha(data) for name, data in paths.items() if name.startswith('source/')}}
    add('SOURCE.json', (json.dumps(info, indent=2) + '\n').encode(), (binary,), 'asset')
    group['members'] = list(records.values())
    group['deliveries'][sha(module)] = {'members': membership}
    tail = json.dumps(group, indent=2).replace('\n', '\n  ')
    match = re.search(r'^  "' + group_name + r'": ', original, re.M)
    if match:
        start = match.end()
        _, length = json.JSONDecoder().raw_decode(original[start:])
        updated = original[:start] + tail + original[start + length:]
    else:
        updated = original[:original.rfind('}')].rstrip() + f',\n  "{group_name}": {tail}\n}}\n'
    assert json.loads(updated)[group_name] == group
    args.output.mkdir()
    for name, data in paths.items():
        target = args.output / name
        target.parent.mkdir(exist_ok=True, parents=True)
        target.write_bytes(data)
    inventory_path.write_text(updated, encoding='utf-8', newline='')
    try:
        admitted = inventory([inventory_path])
        check_stage(args.output, admitted)
        with zipfile.ZipFile(args.archive, 'x', zipfile.ZIP_DEFLATED) as package:
            for name in sorted(paths):
                package.write(args.output / name, name)
        check_archive(args.archive, admitted, expected=membership)
    except Exception:
        inventory_path.write_text(original, encoding='utf-8', newline='')
        raise
    print(f'Admitted {len(paths)} NPZ matrix package and archive members')


if __name__ == '__main__':
    main()
