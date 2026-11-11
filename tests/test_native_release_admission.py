"""Packaging boundary checks with authored bytes, without product builds/data."""
import hashlib
import json
from pathlib import Path
import sys
import tarfile
import pytest

ROOT = Path(__file__).parents[1]
sys.path.insert(0,str(ROOT/'tools/ci'))
import stage_native_release as release


def record(path, ident, parents=()):
    return {'filename':path.name,'source':'independently authored packaging test',
            'bytes':path.stat().st_size,'sha256':release.sha(path),'material':{
                'id':ident,'category':'admitted_open','kind':'derived_asset' if parents else 'code',
                'license_id':'AGPL-3.0-only','license_evidence':'LICENSE',
                'attribution':'qupathxx contributors','source_ids':list(parents)}}


def fixture(tmp_path):
    root,installed=tmp_path/'source',tmp_path/'installed'
    root.mkdir();installed.mkdir()
    for name in ['LICENSE','CMakeLists.txt','pyproject.toml']:
        (root/name).write_text('authored test '+name,encoding='utf8')
    body=release.compiled_manifest(root)
    source_id=hashlib.sha256(body).hexdigest()
    (installed/'app.exe').write_bytes(b'authored executable stand-in')
    header=installed/'include/qupathxx/sdk.hpp';header.parent.mkdir(parents=True)
    header.write_text('source_sha256="'+source_id+'"',encoding='utf8')
    entries=[record(root/n,n) for n in ['LICENSE','CMakeLists.txt','pyproject.toml']]
    entries += [record(installed/'app.exe','app',['CMakeLists.txt']),record(header,'header',['CMakeLists.txt'])]
    profile={'sources':[],'firstparty_license':'LICENSE','compiled_source_sha256':source_id,
             'compiled_material_id':'CMakeLists.txt','stages':{
                 'native':[{'path':'app.exe','input':'app.exe','material_id':'app'}],
                 'sdk':[{'path':'include/qupathxx/sdk.hpp','input':'include/qupathxx/sdk.hpp','material_id':'header'}],
                 'source':[{'path':n,'input':n,'material_id':n} for n in ['LICENSE','CMakeLists.txt','pyproject.toml']]}}
    inventory=root/'inventory.json'
    inventory.write_text(json.dumps({'records':entries,'release_profiles':{'test':profile}}),encoding='utf8')
    return root,installed,inventory


def test_reviewed_release_has_complete_member_and_source_identity_checks(tmp_path):
    root,installed,inventory=fixture(tmp_path)
    output=tmp_path/'release'
    result=release.stage_release(root,installed,output,[inventory],'test')
    assert result['state']=='passed'
    entries=release.admission.inventory([inventory,output/'generated-materials.json'])
    for name,info in result['stages'].items():
        assert release.admission.check_archive(output/info['archive'],entries)==info['stage_files']
    assert release.compiled_manifest(output/'source')==release.compiled_manifest(root)
    # Replacing an output after packaging cannot retain an admitted receipt.
    import zipfile
    with zipfile.ZipFile(output/result['stages']['native']['archive'],'a') as archive:
        archive.writestr('unknown-weights.bin',b'unknown independent asset')
    with pytest.raises(ValueError,match='lacks admitted'):
        release.admission.check_archive(output/result['stages']['native']['archive'],entries)


@pytest.mark.parametrize('change',['compiled','binary','extra','restricted'])
def test_release_refuses_changed_or_unreviewed_inputs_before_staging(tmp_path,change):
    root,installed,inventory=fixture(tmp_path)
    if change=='compiled':
        (root/'cpp').mkdir()
        (root/'cpp/new.cpp').write_text('new compiled input absent from frozen identity')
    if change=='binary':(installed/'app.exe').write_bytes(b'changed executable')
    if change=='extra':(installed/'extra.dat').write_bytes(b'unknown data')
    if change=='restricted':
        value=json.loads(inventory.read_text());value['records'][0]['material']['category']='noncommercial'
        inventory.write_text(json.dumps(value))
    with pytest.raises(ValueError,match='identity|membership|lacks admitted|Restricted/unreviewed'):
        release.stage_release(root,installed,tmp_path/'blocked',[inventory],'test')
    assert not (tmp_path/'blocked').exists()


def test_curated_dependency_archives_match_every_selected_member():
    entries=release.admission.inventory([ROOT/'tests/fixtures/manifest.json'])
    for component in ['opencv','imgui']:
        selection=json.loads((ROOT/f'third_party/sources/{component}-selection.json').read_text())
        archive=ROOT/'third_party/sources'/selection['archive']
        assert release.sha(archive)==selection['sha256']==entries['release-src-'+component]['sha256']
        selected={x['path']:x for x in selection['retained']}
        with tarfile.open(archive) as source:
            actual={x.name:x for x in source}
            assert actual.keys()==selected.keys()
            for name,member in actual.items():
                assert member.isfile() and member.size==selected[name]['bytes']
                assert hashlib.sha256(source.extractfile(member).read()).hexdigest()==selected[name]['sha256']
                assert not name.endswith(('.jpg','.png','.ttf','.otf','.lib','.xml'))
        if component=='opencv':
            assert any(name.endswith('/doc/CMakeLists.txt') for name in actual)
            assert any(name.endswith('/data/CMakeLists.txt') for name in actual)


def test_archive_paths_cannot_substitute_other_admitted_files(tmp_path):
    import zipfile
    a,b=tmp_path/'a',tmp_path/'b'
    a.write_bytes(b'first admitted file');b.write_bytes(b'other admitted file')
    entries={'a':record(a,'a'),'b':record(b,'b')}
    archive=tmp_path/'swapped.zip'
    with zipfile.ZipFile(archive,'w') as target:target.write(b,'a')
    with pytest.raises(ValueError,match='reviewed path identity'):
        release.admission.check_archive(archive,entries,expected={'a':'a'})
    with pytest.raises(ValueError,match='Unexpected archive member'):
        release.admission.check_archive(archive,entries,expected={'b':'b'})
    with pytest.raises(ValueError,match='missing reviewed members'):
        release.admission.check_archive(archive,entries,expected={'a':'b','missing':'a'})
