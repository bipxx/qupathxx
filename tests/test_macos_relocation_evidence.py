"""Offline Apple-command audit controls using labelled simulated native fixtures."""
import copy
import importlib.util
from pathlib import Path

import pytest
from test_macos_relocation_plan import snapshot,ROOTS
from test_macos_relocate import runtime as executor, fake_tools

spec=importlib.util.spec_from_file_location('qpx_mac_relocation_evidence_tests',Path(__file__).resolve().parents[1]/'tools/ci/macos_relocation_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def fixture(tmp_path,monkeypatch):
    archive=snapshot(tmp_path);plan=executor.planner.create_plan(archive,'arm64',ROOTS)
    fake_tools(monkeypatch,plan)
    folder=tmp_path/'relocated-native';executor.relocate(archive,'arm64',ROOTS,folder)
    manifest=executor.inspect_archive(folder/'macos-relocated-native.zip',plan)
    report=evidence.read(folder/'relocation-execution.json')
    logical='/Users/runner/repo/build/ci/macos-native/relocated-native/candidate'
    for i,record in enumerate(report['apple_tools']):
        relative='/'.join(Path(record['arguments'][-1]).parts[-2:])
        record['arguments'][-1]=logical+'/'+relative
        (folder/f'apple-tool-{i:04}.json').write_bytes(executor.common.canonical_json(record))
    (folder/'relocation-execution.json').write_bytes(executor.common.canonical_json(report))
    return folder,plan,manifest,logical,report


def test_offline_command_audit_accepts_exact_simulated_cohort(tmp_path,monkeypatch):
    folder,plan,manifest,logical,report=fixture(tmp_path,monkeypatch)
    result=evidence.audit_execution(folder,plan,manifest,logical)
    assert result['commands']==12 and result['ad_hoc_integrity_verified_on_builder']
    assert result['publisher_signature_verified'] is False


@pytest.mark.parametrize('failure',['source','archive','plan','hashes','release','integrity','missing_command','extra_command',
    'arguments','order','boolean_status','raw_log','ledger'])
def test_changed_command_identity_scope_status_or_raw_evidence_rejects(tmp_path,monkeypatch,failure):
    folder,plan,manifest,logical,report=fixture(tmp_path,monkeypatch)
    if failure=='source':report['source_archive_sha256']='0'*64
    elif failure=='archive':report['relocated_archive_sha256']='0'*64
    elif failure=='plan':report['plan_sha256']='0'*64
    elif failure=='hashes':report['verified_binary_sha256'].pop(next(iter(report['verified_binary_sha256'])))
    elif failure=='release':report['release_ready']=True
    elif failure=='integrity':report['ad_hoc_integrity_verified_on_builder']=False
    elif failure=='missing_command':report['apple_tools'].pop()
    elif failure=='extra_command':report['apple_tools'].append(copy.deepcopy(report['apple_tools'][0]))
    elif failure=='arguments':report['apple_tools'][0]['arguments'][0]='/usr/bin/other-tool'
    elif failure=='order':report['apple_tools'][0],report['apple_tools'][1]=report['apple_tools'][1],report['apple_tools'][0]
    elif failure=='boolean_status':report['apple_tools'][0]['exit_code']=False
    elif failure=='raw_log':(folder/'apple-tool-0000.stderr.log').write_bytes(b'altered log')
    elif failure=='ledger':(folder/'apple-tool-0000.json').write_bytes(b'{}')
    (folder/'relocation-execution.json').write_bytes(executor.common.canonical_json(report))
    with pytest.raises(ValueError):evidence.audit_execution(folder,plan,manifest,logical)
