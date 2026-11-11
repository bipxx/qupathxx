"""Stage one reviewed native release; refuse changed/extra/unadmitted members.

Development packaging only. The canonical inventory holds a finite reviewed
membership, not a rule that automatically licenses new checkout/build files.
Every output directory AND ZIP is checked before a passing receipt is written.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import zipfile
import check_material_staging as admission

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def safe_relative(name):
    path = PurePosixPath(name)
    if (not name or path.is_absolute() or '..' in path.parts or '\\' in name
            or ':' in name or path.as_posix() != name or name == '.'):
        raise ValueError('Unsafe release path: ' + name)
    return Path(name)


def exact_file(path, record):
    for part in (path, *path.parents):
        if admission.reparse(part):
            raise ValueError('Release input contains a link/reparse point: ' + str(part))
    if not path.is_file() or path.stat().st_size != record['bytes'] or sha(path) != record['sha256']:
        raise ValueError('Release input identity differs: ' + str(path))


def compiled_manifest(root):
    # Matches CMakeLists.txt QPX_BUILD_INPUTS, including optional Python source
    # bytes for identity; this never imports, discovers or runs Python products.
    groups = {'cpp': {'.c','.cpp','.hpp','.h','.inc'}, 'assets/fonts': {'.ttf'},
              'assets/ui': {'.rml','.rcss'}, 'cmake': {'.cmake','.in'},
              'src': {'.py'}, 'examples': {'.cpp'}}
    paths = [p for folder, suffixes in groups.items() for p in (root/folder).rglob('*')
             if p.is_file() and p.suffix in suffixes]
    # Match the selected external-module recipe inputs in the native CMake
    # identity too; a changed installed build recipe cannot retain the old ID.
    plugin_example = root/'examples/reader_plugin'
    if (plugin_example/'CMakeLists.txt').is_file():
        paths.append(plugin_example/'CMakeLists.txt')
    paths += [p for p in plugin_example.rglob('*') if p.is_file() and p.suffix in {'.cmake','.in'}]
    plugin_project = root/'examples/reader_plugin_project/CMakeLists.txt'
    if plugin_project.is_file():
        paths.append(plugin_project)
    paths += [root/'CMakeLists.txt', root/'pyproject.toml']
    paths.sort(key=lambda p: p.as_posix())
    return ''.join(p.relative_to(root).as_posix()+'='+sha(p)+'\n' for p in paths).encode('utf8')


def stage_release(root, installed, output, inventory_paths, profile_name='windows-native'):
    root, installed, output = Path(root).absolute(), Path(installed).absolute(), Path(output).absolute()
    entries = admission.inventory(inventory_paths)
    profiles = [json.loads(Path(p).read_text(encoding='utf8')).get('release_profiles', {}).get(profile_name)
                for p in inventory_paths]
    profiles = [p for p in profiles if p]
    if len(profiles) != 1:
        raise ValueError('Need exactly one reviewed release profile')
    profile = profiles[0]
    source_id = profile['compiled_source_sha256']
    if not re.fullmatch('[0-9a-f]{64}', source_id):
        raise ValueError('Invalid native source identity')
    body = compiled_manifest(root)
    if hashlib.sha256(body).hexdigest() != source_id:
        raise ValueError('Checkout differs from compiled source identity')
    groups = profile['stages']
    plugin_development = profile.get('kind') == 'reader-plugin-dev'
    required_stages = {'host','kit','module','source'} if plugin_development else {'native','sdk','source'}
    if set(groups) != required_stages or any(not group for group in groups.values()):
        raise ValueError('Reader plugin development needs four finite reviewed stages' if plugin_development
                         else 'Native release needs three finite reviewed stages')
    expected_install = set()
    for name, members in groups.items():
        seen = set()
        for member in members:
            target = str(safe_relative(member['path'])).casefold()
            if target in seen:
                raise ValueError('Duplicate/colliding release path: ' + member['path'])
            seen.add(target)
            ident = member['material_id']
            admission.admitted(ident, entries)
            if name == 'source':
                path = root/safe_relative(member['input'])
            else:
                relative = safe_relative(member['input'])
                expected_install.add(relative.as_posix())
                path = installed/relative
            exact_file(path, entries[ident])
    # Refuse surplus install files; silently dropping an injected data/model or
    # unexpected generated notice would conceal an incomplete producer review.
    admission.check_stage(installed, entries)
    actual_install = {p.relative_to(installed).as_posix() for p in installed.rglob('*') if p.is_file()}
    if actual_install != expected_install:
        raise ValueError('Installed membership differs: extra=' + str(sorted(actual_install-expected_install))
                         + ', missing=' + str(sorted(expected_install-actual_install)))
    if plugin_development:
        # The standalone C kit deliberately has no core SDK/library. Bind the
        # installed native host through its explicitly reviewed CMake receipt.
        identity = safe_relative(profile['installed_source_identity'])
        if identity.as_posix() not in expected_install:
            raise ValueError('Host source identity is outside reviewed install membership')
        host_identity = json.loads((installed/identity).read_text(encoding='utf8'))
        if host_identity.get('schema') != 'qpx.reader-plugin-host-source/1':
            raise ValueError('Installed plugin host source receipt has an unsupported schema')
        if host_identity.get('source_sha256') != source_id:
            raise ValueError('Installed plugin host has a different compiled source identity')
        required_recipe = {'cpp/include/qupathxx/reader_plugin.h', 'cpp/ppm_decoder.hpp', 'cpp/ppm_decoder.cpp',
                           'examples/reader_plugin/CMakeLists.txt', 'examples/reader_plugin/p6_plugin.cpp',
                           'examples/reader_plugin/reader_plugin_profile.hpp.in', 'examples/reader_plugin/write_manifest.cmake'}
        recipe = profile['plugin_recipe_inputs']
        if len(recipe) != len(set(recipe)) or not required_recipe.issubset(recipe):
            raise ValueError('Plugin source recipe lacks its explicit reviewed build inputs')
        source_inputs = {m['input'] for m in groups['source']}
        if not set(recipe).issubset(source_inputs):
            raise ValueError('Plugin build recipe is absent from the corresponding source stage')
        descriptor_input = safe_relative(profile['plugin_descriptor_input'])
        if descriptor_input.as_posix() not in expected_install:
            raise ValueError('Plugin descriptor is outside reviewed install membership')
        descriptor = json.loads((installed/descriptor_input).read_text(encoding='utf8'))
        plugins = descriptor.get('plugins', [])
        if descriptor.get('schema') != 'qpx.reader-plugins/1' or len(plugins) != 1:
            raise ValueError('Development descriptor must bind exactly one selected P6 module')
        plugin = plugins[0]
        if (plugin.get('provider_id') != 'org.qupathxx.p6' or plugin.get('abi') != 1
                or plugin.get('version') != '1.0.0' or plugin.get('contract') != 'p6-max255-display/1'
                or plugin.get('display_name') != 'P6 display example'
                or plugin.get('profile') != host_identity.get('profile') or not host_identity.get('profile')
                or plugin.get('capabilities') != {'display_rgba':True,'raw_samples':False}
                or plugin.get('dependencies') != []):
            raise ValueError('Development descriptor exceeds the admitted P6 profile')
        module_input = descriptor_input.parent/safe_relative(plugin['module']['path'])
        if module_input.as_posix() not in expected_install or sha(installed/module_input) != plugin['module']['sha256']:
            raise ValueError('Development descriptor differs from the actual admitted module')
        module_members = [m for m in groups['module'] if m['input'] == module_input.as_posix()]
        if len(module_members) != 1:
            raise ValueError('Actual P6 module is outside the reviewed module stage')
        ancestors = set()
        def lineage(ident):
            if ident in ancestors: return
            ancestors.add(ident)
            for parent in entries[ident]['material']['source_ids']: lineage(parent)
        lineage(module_members[0]['material_id'])
        module_recipe = profile['module_recipe_material_ids']
        source_materials = {m['material_id'] for m in groups['source']}
        if (not module_recipe or len(module_recipe) != len(set(module_recipe))
                or not set(module_recipe).issubset(ancestors & source_materials)):
            raise ValueError('Actual P6 binary lacks its exact corresponding-source recipe lineage')
    else:
        sdk_header = installed/'include/qupathxx/sdk.hpp'
        if ('source_sha256="'+source_id+'"') not in sdk_header.read_text(encoding='utf8'):
            raise ValueError('Installed SDK has a different compiled source identity')
    output.mkdir(parents=True, exist_ok=False)
    generated, results = [], {}

    def metadata(path, parents):
        record = {'filename':path.name, 'source':'reviewed native release metadata; tools/ci/stage_native_release.py',
                  'sha256':sha(path), 'bytes':path.stat().st_size, 'material':{
                      'id':'release-metadata-'+sha(path), 'category':'admitted_open', 'kind':'derived_asset',
                      'license_id':'AGPL-3.0-only', 'license_evidence':'LICENSE',
                      'attribution':'qupathxx contributors', 'source_ids':parents}}
        entries[record['material']['id']] = record
        generated.append(record)
        expected[path.relative_to(stage).as_posix()] = record['material']['id']

    for name, members in groups.items():
        stage = output/name
        stage.mkdir()
        expected = {member['path']:member['material_id'] for member in members}
        for member in members:
            source = (root if name == 'source' else installed)/member['input']
            target = stage/member['path']
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            exact_file(target, entries[member['material_id']])
        if name == 'source':
            # Inventory cannot contain its own digest. The exact frozen snapshot
            # is a generated metadata member with explicit first-party lineage.
            for inventory_path in inventory_paths:
                relative = Path(inventory_path).absolute().relative_to(root)
                target = stage/relative
                target.parent.mkdir(parents=True, exist_ok=True)
                if target.exists():
                    raise ValueError('Inventory snapshot collides with a source member')
                shutil.copyfile(inventory_path, target)
                metadata(target, [profile['firstparty_license']])
            (stage/'source-inputs.sha256').write_bytes(body)
            metadata(stage/'source-inputs.sha256', [profile['compiled_material_id']])
            if compiled_manifest(stage) != body:
                raise ValueError('Source kit changed compiled inputs')
        info = {'profile':profile_name, 'stage':name, 'source_sha256':source_id,
                'membership_inventory':'tests/fixtures/manifest.json',
                'scope':profile.get('scope','L01 local exact membership; P02 install/use/removal and publication pending'),
                'dependency_sources':profile['sources']}
        (stage/'RELEASE.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf8')
        metadata(stage/'RELEASE.json',[profile['compiled_material_id']])
        count = admission.check_stage(stage, entries)
        if {p.relative_to(stage).as_posix() for p in stage.rglob('*') if p.is_file()} != set(expected):
            raise ValueError('Stage differs from reviewed member paths')
        for relative, ident in expected.items():
            exact_file(stage/relative, entries[ident])
        archive = output/('qupathxx-windows-x64-'+name+('-reader-plugin-dev.zip' if plugin_development else '-l01.zip'))
        with zipfile.ZipFile(archive,'x',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as target:
            for path in sorted(stage.rglob('*')):
                if path.is_file():
                    target.write(path,path.relative_to(stage).as_posix())
        if admission.check_archive(archive,entries,expected=expected) != count:
            raise ValueError('Archive membership differs from stage')
        results[name] = {'archive':archive.name,'sha256':sha(archive),'bytes':archive.stat().st_size,
                         'stage_files':count,'archive_members':count}
    (output/'generated-materials.json').write_text(json.dumps({'records':generated},indent=2)+'\n',encoding='utf8')
    result = {'state':'passed','profile':profile_name,'source_sha256':source_id,'stages':results}
    (output/'admission.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--installed',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--inventory',type=Path,action='append')
    parser.add_argument('--profile',default='windows-native')
    args = parser.parse_args()
    print(json.dumps(stage_release(args.root,args.installed,args.output,args.inventory or [
        args.root/'tests/fixtures/manifest.json',args.root/'assets/fonts/font-source.json'],args.profile),indent=2))


if __name__ == '__main__':
    main()
