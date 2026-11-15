"""Desktop-only packaging must preserve finite membership and source identity."""
import hashlib
import json
from pathlib import Path
import sys
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/ci'))
import check_material_staging as admission
from stage_native_release import compiled_manifest
from stage_desktop_release import stage_desktop


def fixture(tmp_path):
    root, runtime = tmp_path/'source', tmp_path/'runtime'
    root.mkdir(); runtime.mkdir()
    records = []
    def file(base, name, data):
        path = base/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path
    def record(path, ident, kind='code', parents=('license',)):
        value = {'filename':path.name, 'source':'authored desktop staging test',
                 'sha256':hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes':path.stat().st_size,
                 'material':{'id':ident, 'category':'admitted_open', 'kind':kind,
                             'license_id':'AGPL-3.0-only', 'license_evidence':'LICENSE',
                             'attribution':'authored test', 'source_ids':list(parents)}}
        if kind == 'source_archive':value['review'] = 'Authored archive stand-in; no external contents'
        records.append(value)
        return ident
    record(file(root, 'LICENSE', b'Authored test license notice'), 'license', 'notice', ())
    members = []
    for index, (name, data) in enumerate([
        ('CMakeLists.txt', b'project(Authored)\n'), ('pyproject.toml', b'# authored control metadata\n'),
        ('cpp/demo.cpp', b'int main(){return 0;}\n'), ('assets/ui/workspace.rml', b'<rml>\r\n</rml>\r\n'),
    ]):
        record(file(root, name, data), 'source-'+str(index))
        members.append({'path':name, 'input':name, 'material_id':'source-'+str(index)})
    members.append({'path':'LICENSE', 'input':'LICENSE', 'material_id':'license'})
    body = compiled_manifest(root)
    manifest_file = file(root, 'inputs.sha256', body)
    record(manifest_file, 'inputs', 'derived_asset')
    record(file(root, 'dependency.zip', b'Authored source archive stand-in'), 'dependency', 'source_archive')
    app = file(runtime, 'qupathxx_app.exe', b'Authored PE stand-in')
    record(app, 'app', 'derived_asset')
    ui = file(runtime, 'ui/workspace.rml', b'<rml>\n</rml>\n')
    record(ui, 'ui', 'derived_asset')
    digest = lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
    profile = {'kind':'desktop-only', 'firstparty_license':'license',
               'source_inputs_sha256':digest(manifest_file), 'source_material_id':'inputs',
               'scope':'authored desktop admission test', 'sources':['dependency'],
               'composition':{'app_sha256':digest(app), 'native_base_sha256':'1'*64,
                              'native_candidate_sha256':'2'*64, 'frontend_commit':'3'*40,
                              'native_replacement_sources':{'cpp/demo.cpp':digest(root/'cpp/demo.cpp')}},
               'ui_source_correspondence':{'workspace.rml':{
                   'source_sha256':digest(root/'assets/ui/workspace.rml'), 'runtime_sha256':digest(ui)}},
               'stages':{'source':members, 'desktop':[
                   {'path':'qupathxx_app.exe', 'input':'qupathxx_app.exe', 'material_id':'app'},
                   {'path':'ui/workspace.rml', 'input':'ui/workspace.rml', 'material_id':'ui'}]}}
    data = {'records':records, 'release_profiles':{'windows-molecular-desktop':profile}}
    manifest = root/'inventory.json'
    def save():manifest.write_text(json.dumps(data), encoding='utf8')
    save()
    return root, runtime, manifest, data, save


def test_desktop_package_verifies_both_archives_and_uses_no_sdk(tmp_path):
    root, runtime, manifest, _, _ = fixture(tmp_path)
    output = tmp_path/'release'
    result = stage_desktop(root, runtime, output, [manifest])
    assert result['state'] == 'passed' and set(result['stages']) == {'desktop', 'source'}
    entries = admission.inventory([manifest, output/'generated-materials.json'])
    for info in result['stages'].values():
        assert admission.check_archive(output/info['archive'], entries) == info['stage_files']
    with zipfile.ZipFile(output/result['stages']['desktop']['archive']) as archive:
        assert set(archive.namelist()) == {'qupathxx_app.exe', 'ui/workspace.rml', 'RELEASE.json'}
        assert archive.read('ui/workspace.rml') == b'<rml>\n</rml>\n'
    assert (output/'source/assets/ui/workspace.rml').read_bytes() == b'<rml>\r\n</rml>\r\n'


@pytest.mark.parametrize('failure', ['changed_app', 'missing_app', 'admitted_extra', 'restricted_app',
                                      'changed_source', 'missing_source_member', 'wrong_app_binding',
                                      'substantive_ui_change'])
def test_desktop_admission_refuses_before_creating_output(tmp_path, failure):
    root, runtime, manifest, data, save = fixture(tmp_path)
    profile = data['release_profiles']['windows-molecular-desktop']
    expected = ''
    if failure == 'changed_app':
        (runtime/'qupathxx_app.exe').write_bytes(b'Changed app')
        expected = 'Release input identity differs'
    elif failure == 'missing_app':
        (runtime/'qupathxx_app.exe').unlink()
        expected = 'Release input identity differs'
    elif failure == 'admitted_extra':
        (runtime/'unreviewed-name.txt').write_bytes((root/'LICENSE').read_bytes())
        expected = 'Runtime membership differs'
    elif failure == 'restricted_app':
        next(row for row in data['records'] if row['material']['id'] == 'app')['material']['category'] = 'noncommercial'
        expected = 'Restricted/unreviewed material'
    elif failure == 'changed_source':
        (root/'cpp/demo.cpp').write_bytes(b'int main(){return 1;}\n')
        expected = 'corresponding-source inputs'
    elif failure == 'missing_source_member':
        profile['stages']['source'] = [row for row in profile['stages']['source'] if row['path'] != 'cpp/demo.cpp']
        expected = 'omits recorded build inputs'
    elif failure == 'wrong_app_binding':
        profile['composition']['app_sha256'] = '4'*64
        expected = 'executable differs'
    elif failure == 'substantive_ui_change':
        path = runtime/'ui/workspace.rml';path.write_bytes(b'<rml>different</rml>\n')
        row = next(row for row in data['records'] if row['material']['id'] == 'ui')
        row.update(sha256=hashlib.sha256(path.read_bytes()).hexdigest(), bytes=path.stat().st_size)
        profile['ui_source_correspondence']['workspace.rml']['runtime_sha256'] = row['sha256']
        expected = 'differs substantively'
    save()
    output = tmp_path/'rejected'
    with pytest.raises(ValueError, match=expected):
        stage_desktop(root, runtime, output, [manifest])
    assert not output.exists()
