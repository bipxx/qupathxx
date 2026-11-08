"""Relocation verifier and simulated Apple-tool orchestration; no native execution."""
import hashlib
import importlib.util
from pathlib import Path
import subprocess
import struct
import zipfile

import pytest
from test_macos_relocation_plan import snapshot,binary,ROOTS

spec=importlib.util.spec_from_file_location('qpx_mac_relocation_tests',Path(__file__).resolve().parents[1]/'tools/ci/macos_relocate.py')
runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)


def relocated_bytes(edit,**changes):
    deps=edit['expected_dependencies']
    mechanisms={value:key for key,value in runtime.inventory.macho.DYLIBS.items()}
    private=[(mechanisms[item['mechanism']],item['name']) for item in deps if item['name']!='/usr/lib/libSystem.B.dylib']
    options=dict(kind=2 if edit['target'].startswith('bin/') else 6,identity=edit['install_id'],
                 dependencies=private,rpaths=())
    options.update(changes)
    return binary(**options)


def planned(tmp_path):
    archive=snapshot(tmp_path)
    plan=runtime.planner.create_plan(archive,'arm64',ROOTS)
    return archive,plan


def stage_fixture(tmp_path,archive,plan):
    stage=tmp_path/'stage';stage.mkdir()
    edits={item['target']:item for item in plan['edits']}
    with zipfile.ZipFile(archive) as source:
        for item in plan['files']:
            path=stage/item['target'];path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(relocated_bytes(edits[item['target']]) if item['target'] in edits else source.read(item['source']))
    return stage


def test_verifies_direct_paths_and_rejects_protected_metadata_changes(tmp_path):
    archive,plan=planned(tmp_path)
    edit=next(item for item in plan['edits'] if item['target']=='bin/qupathxx_batch')
    assert runtime.verify_binary(edit,relocated_bytes(edit))['slices'][0]['rpaths']==[]
    for change in [dict(rpaths=('/build/private/SDK',)),dict(flags=0x185),
                   dict(dependencies=()),dict(dependencies=((0x80000018,'@executable_path/../lib/libcapi.1.dylib'),))]:
        with pytest.raises(ValueError):runtime.verify_binary(edit,relocated_bytes(edit,**change))
    data=bytearray(relocated_bytes(edit));struct.pack_into('<I',data,4,0x01000007)
    with pytest.raises(ValueError):runtime.verify_binary(edit,bytes(data))


def test_complete_package_archive_reinspection_and_source_immutability(tmp_path):
    archive,plan=planned(tmp_path);original=archive.read_bytes();stage=stage_fixture(tmp_path,archive,plan)
    output=tmp_path/'package.zip';report=runtime.pack(stage,plan,output)
    assert runtime.common.canonical_json(runtime.inspect_archive(output,plan))==runtime.common.canonical_json(report)
    assert len(report['files'])==6 and len(report['binaries'])==4 and report['static_loader_paths_match_plan']
    assert not any(report[key] for key in ['runtime_workflows_verified','external_runtime_availability_verified','symbol_abi_verified','signing_verified','release_ready'])
    assert archive.read_bytes()==original
    with pytest.raises(ValueError):runtime.pack(stage,plan,output)


@pytest.mark.parametrize('failure',['notice','unrelocated','extra','missing','verified_hash'])
def test_failed_package_preflight_leaves_no_published_or_temporary_zip(tmp_path,failure):
    archive,plan=planned(tmp_path);stage=stage_fixture(tmp_path,archive,plan)
    hashes=None
    if failure=='notice':next((stage/'licenses').rglob('LICENSE')).write_bytes(b'changed')
    elif failure=='unrelocated':
        edit=plan['edits'][0]
        with zipfile.ZipFile(archive) as source:(stage/edit['target']).write_bytes(source.read(edit['source']))
    elif failure=='extra':(stage/'extra').write_bytes(b'unknown')
    elif failure=='missing':(stage/plan['files'][0]['target']).unlink()
    elif failure=='verified_hash':hashes={plan['edits'][0]['target']:'0'*64}
    with pytest.raises(ValueError):runtime.pack(stage,plan,tmp_path/'rejected.zip',hashes)
    assert not (tmp_path/'rejected.zip').exists() and not list(tmp_path.glob('.*.qpx-tmp-*'))


def test_non_mac_runner_rejects_before_creating_any_work(tmp_path,monkeypatch):
    archive,plan=planned(tmp_path)
    monkeypatch.setattr(runtime.platform,'system',lambda:'Windows')
    with pytest.raises(ValueError):runtime.relocate(archive,'arm64',ROOTS,tmp_path/'work')
    assert not (tmp_path/'work').exists()


def fake_tools(monkeypatch,plan,fail=False):
    edits={item['target']:item for item in plan['edits']};calls=[]
    monkeypatch.setattr(runtime.platform,'system',lambda:'Darwin')
    monkeypatch.setattr(runtime.platform,'machine',lambda:'arm64')
    def run(arguments,**options):
        calls.append(arguments)
        assert options['env'].get('DYLD_LIBRARY_PATH') is None
        if fail:return subprocess.CompletedProcess(arguments,1,b'',b'simulated tool refusal')
        target=Path(arguments[-1]);relative='/'.join(target.parts[-2:])
        if arguments[0]=='/usr/bin/install_name_tool':target.write_bytes(relocated_bytes(edits[relative]))
        return subprocess.CompletedProcess(arguments,0,b'',b'')
    monkeypatch.setattr(runtime.subprocess,'run',run)
    return calls


def test_simulated_tool_orchestration_publishes_only_reinspected_stage(tmp_path,monkeypatch):
    archive,plan=planned(tmp_path);original=archive.read_bytes()
    monkeypatch.setenv('DYLD_LIBRARY_PATH','/must-not-affect-relocation')
    monkeypatch.setenv('LD_PRELOAD','/must-not-be-loaded')
    calls=fake_tools(monkeypatch,plan)
    work=tmp_path/'work';report=runtime.relocate(archive,'arm64',ROOTS,work)
    assert report['ad_hoc_integrity_verified_on_builder'] and not report['signing_verified']
    assert len(report['verified_binary_sha256'])==4 and len(calls)==12
    assert (work/'macos-relocated-native.zip').exists() and (work/'relocation-execution.json').exists()
    assert archive.read_bytes()==original
    assert not report['native_runtime_workflows_verified'] and not report['release_ready']


def test_tool_failure_retains_stage_logs_but_never_claims_complete_package(tmp_path,monkeypatch):
    archive,plan=planned(tmp_path);calls=fake_tools(monkeypatch,plan,fail=True)
    work=tmp_path/'work'
    with pytest.raises(ValueError):runtime.relocate(archive,'arm64',ROOTS,work)
    assert len(calls)==1 and (work/'apple-tool-0000.stderr.log').read_bytes()==b'simulated tool refusal'
    assert not (work/'macos-relocated-native.zip').exists() and not (work/'relocation-execution.json').exists()


def test_signature_verification_cannot_modify_staged_bytes(tmp_path,monkeypatch):
    archive,plan=planned(tmp_path);fake_tools(monkeypatch,plan)
    previous=runtime.subprocess.run
    def run(arguments,**options):
        result=previous(arguments,**options)
        if '--verify' in arguments:
            path=Path(arguments[-1]);path.write_bytes(path.read_bytes()+b'changed')
        return result
    monkeypatch.setattr(runtime.subprocess,'run',run)
    with pytest.raises(ValueError,match='changed during signature verification'):
        runtime.relocate(archive,'arm64',ROOTS,tmp_path/'work')
