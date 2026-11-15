"""Stage a finite reviewed desktop/source profile without rebuilding native code.

This is development packaging. It reuses material admission and exact ZIP/member
checks, but deliberately does not relabel or package a frozen native SDK.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

import check_material_staging as admission
from stage_native_release import compiled_manifest, exact_file, safe_relative, sha

ROOT = Path(__file__).resolve().parents[2]


def stage_desktop(root, runtime, output, inventory_paths, profile_name='windows-molecular-desktop'):
    root, runtime, output = (Path(path).absolute() for path in (root, runtime, output))
    for path in (output, *output.parents):
        if path.exists() and admission.reparse(path):
            raise ValueError('Output ancestry contains a link/reparse point')
    entries = admission.inventory(inventory_paths)
    profiles = [json.loads(Path(path).read_text(encoding='utf8')).get('release_profiles', {}).get(profile_name)
                for path in inventory_paths]
    profiles = [profile for profile in profiles if profile]
    if len(profiles) != 1:
        raise ValueError('Need exactly one finite reviewed desktop profile')
    profile = profiles[0]
    if profile.get('kind') != 'desktop-only' or set(profile.get('stages', {})) != {'desktop', 'source'}:
        raise ValueError('Desktop profile requires desktop and source stages only')
    body = compiled_manifest(root)
    source_id = hashlib.sha256(body).hexdigest()
    if source_id != profile.get('source_inputs_sha256'):
        raise ValueError('Checkout differs from reviewed corresponding-source inputs')
    admission.admitted(profile['source_material_id'], entries)
    if entries[profile['source_material_id']]['sha256'] != source_id:
        raise ValueError('Source manifest material differs from the reviewed profile')
    composition = profile.get('composition', {})
    for key in ('app_sha256', 'native_base_sha256', 'native_candidate_sha256', 'frontend_commit'):
        value = composition.get(key, '')
        size = 40 if key == 'frontend_commit' else 64
        if len(value) != size or any(c not in '0123456789abcdef' for c in value):
            raise ValueError('Invalid desktop composition identity: ' + key)
    if not composition.get('native_replacement_sources'):
        raise ValueError('Desktop composition lacks its native correction inputs')
    for name, digest in composition['native_replacement_sources'].items():
        if sha(root/safe_relative(name)) != digest:
            raise ValueError('Native correction input changed: ' + name)
    install_members = set()
    source_members = set()
    for name, members in profile['stages'].items():
        if not members:
            raise ValueError('Empty reviewed desktop stage: ' + name)
        paths = set()
        for member in members:
            target = safe_relative(member['path']).as_posix()
            if target.casefold() in paths:
                raise ValueError('Duplicate/colliding desktop member: ' + target)
            paths.add(target.casefold())
            admission.admitted(member['material_id'], entries)
            source = safe_relative(member['input'])
            path = (root if name == 'source' else runtime)/source
            if not path.is_file():
                raise ValueError('Release input identity differs: ' + str(path))
            exact_file(path, entries[member['material_id']])
            (source_members if name == 'source' else install_members).add(source.as_posix())
    compiled_paths = {line.rsplit('=', 1)[0] for line in body.decode('utf8').splitlines()}
    if not compiled_paths.issubset(source_members):
        raise ValueError('Source stage omits recorded build inputs')
    app = [member for member in profile['stages']['desktop'] if member['path'] == 'qupathxx_app.exe']
    if len(app) != 1 or entries[app[0]['material_id']]['sha256'] != composition['app_sha256']:
        raise ValueError('Desktop executable differs from the reviewed composition')
    for name, hashes in profile.get('ui_source_correspondence', {}).items():
        source = root/'assets/ui'/safe_relative(name)
        asset = runtime/'ui'/safe_relative(name)
        if sha(source) != hashes['source_sha256'] or sha(asset) != hashes['runtime_sha256']:
            raise ValueError('UI source/runtime identity changed: ' + name)
        if source.read_bytes().replace(b'\r\n', b'\n') != asset.read_bytes().replace(b'\r\n', b'\n'):
            raise ValueError('UI runtime differs substantively from corresponding source: ' + name)
    admission.check_stage(runtime, entries)
    actual = {path.relative_to(runtime).as_posix() for path in runtime.rglob('*') if path.is_file()}
    if actual != install_members:
        raise ValueError('Runtime membership differs: extra=' + str(sorted(actual-install_members))
                         + ', missing=' + str(sorted(install_members-actual)))
    for ident in profile['sources']:
        admission.admitted(ident, entries)
        if entries[ident]['material']['kind'] != 'source_archive' or not entries[ident].get('review'):
            raise ValueError('Dependency source needs its explicit reviewed archive: ' + ident)

    output.mkdir(parents=True, exist_ok=False)
    generated, result = [], {}
    for name, members in profile['stages'].items():
        stage = output/name
        stage.mkdir()
        expected = {}
        for member in members:
            source = (root if name == 'source' else runtime)/member['input']
            target = stage/safe_relative(member['path'])
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            exact_file(target, entries[member['material_id']])
            expected[member['path']] = member['material_id']

        def metadata(path, parents):
            digest = sha(path)
            record = {'filename':path.name, 'source':'tools/ci/stage_desktop_release.py; reviewed desktop metadata',
                      'bytes':path.stat().st_size, 'sha256':digest, 'material':{
                          'id':'desktop-metadata-'+digest, 'category':'admitted_open', 'kind':'derived_asset',
                          'license_id':'AGPL-3.0-only', 'license_evidence':'LICENSE',
                          'attribution':'qupathxx contributors', 'source_ids':parents}}
            entries[record['material']['id']] = record
            generated.append(record)
            expected[path.relative_to(stage).as_posix()] = record['material']['id']

        if name == 'source':
            for path in inventory_paths:
                relative = Path(path).absolute().relative_to(root)
                target = stage/relative
                if target.exists():
                    raise ValueError('Inventory snapshot collides with a source member')
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
                metadata(target, [profile['firstparty_license']])
            (stage/'source-inputs.sha256').write_bytes(body)
            metadata(stage/'source-inputs.sha256', [profile['source_material_id']])
            if compiled_manifest(stage) != body:
                raise ValueError('Source archive changes recorded build inputs')
        info = {'schema':'qpx.desktop-release/1', 'profile':profile_name, 'stage':name,
                'source_inputs_sha256':source_id, 'composition':composition,
                'scope':profile['scope'], 'dependency_sources':profile['sources'],
                'ui_source_correspondence':profile.get('ui_source_correspondence', {})}
        (stage/'RELEASE.json').write_text(json.dumps(info, indent=2)+'\n', encoding='utf8')
        metadata(stage/'RELEASE.json', [profile['source_material_id']])
        count = admission.check_stage(stage, entries)
        if {path.relative_to(stage).as_posix() for path in stage.rglob('*') if path.is_file()} != set(expected):
            raise ValueError('Output stage differs from its exact reviewed membership')
        for path, ident in expected.items():
            exact_file(stage/path, entries[ident])
        archive = output/('qupathxx-windows-x64-molecular-'+name+'-'+composition['frontend_commit'][:8]+'.zip')
        with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as target:
            for path in sorted(stage.rglob('*')):
                if path.is_file():
                    target.write(path, path.relative_to(stage).as_posix())
        if admission.check_archive(archive, entries, expected=expected) != count:
            raise ValueError('Desktop ZIP membership differs from its stage')
        result[name] = {'archive':archive.name, 'sha256':sha(archive), 'bytes':archive.stat().st_size,
                        'stage_files':count, 'archive_members':count}
    (output/'generated-materials.json').write_text(json.dumps({'records':generated}, indent=2)+'\n', encoding='utf8')
    result = {'state':'passed', 'profile':profile_name, 'source_inputs_sha256':source_id,
              'composition':composition, 'stages':result}
    (output/'admission.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, action='append')
    parser.add_argument('--profile', default='windows-molecular-desktop')
    args = parser.parse_args()
    result = stage_desktop(args.root, args.runtime, args.output, args.inventory or [
        args.root/'tests/fixtures/manifest.json', args.root/'assets/fonts/font-source.json'], args.profile)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
