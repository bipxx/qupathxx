"""Complete offline runtime evidence controls; all process artifacts are synthetic."""
import copy
import importlib.util
import json
from pathlib import Path, PurePosixPath
import shutil
import sqlite3

import pytest
from test_macos_relocation_plan import snapshot,ROOTS
from test_macos_relocate import stage_fixture
from test_macos_relocated_runtime import csv_text,batch_text

spec=importlib.util.spec_from_file_location('qpx_mac_full_runtime_audit_tests',Path(__file__).resolve().parents[1]/'tools/ci/macos_relocation_evidence.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
rt=audit.runtime
LOGICAL='/Users/runner/repo/build/ci/macos-native/relocated-runtime'
REPOSITORY='/Users/runner/repo'
PYTHON='/opt/toolcache/python3'
PAM=b'P7\nWIDTH 1\nHEIGHT 1\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n'+bytes([60,100,180,255])


def fixture(tmp_path):
    source=snapshot(tmp_path,native_dependencies=((0xc,'@rpath/libgeos_c.1.dylib'),),
        capi_id='@rpath/libgeos_c.1.dylib',cpp_id='@rpath/libgeos.3.15.0.dylib',
        capi_dependencies=((0xc,'@rpath/libgeos.3.15.0.dylib'),))
    plan=rt.relocation.planner.create_plan(source,'arm64',ROOTS)
    stage=stage_fixture(tmp_path,source,plan)
    manifest=rt.relocation.pack(stage,plan,tmp_path/'relocated.zip')
    folder=tmp_path/'runtime';folder.mkdir()
    logical=PurePosixPath(LOGICAL);package=logical/'fresh relocated package with spaces'
    project=Path(__file__).resolve().parent/'fixtures/project-v1.qpxproj'
    expected_fixture=rt.fixture_contract(project);shutil.copy2(project,folder/'input.qpxproj')
    (folder/'input.ppm').write_bytes(rt.REGION_INPUT);(folder/'output.pam').write_bytes(PAM)
    (folder/'measurements.csv').write_text(csv_text(),encoding='utf-8')
    checkpoint=folder/'batch/selected/checkpoint.qpxproj';checkpoint.parent.mkdir(parents=True)
    with sqlite3.connect(checkpoint) as db:
        db.execute('CREATE TABLE analysis_records(id INTEGER)');db.execute('INSERT INTO analysis_records VALUES(1)')
    definitions=[
        ('region',[str(package/'bin/qupathxx_region'),'--thumbnail',str(logical/'input.ppm'),'1','1',str(logical/'output.pam'),'--source-pixels','2'],True),
        ('batch',[str(package/'bin/qupathxx_batch'),str(logical/'input.qpxproj'),str(logical/'batch'),'--kind','od','--image','1','--object','1'],True),
        ('export',[str(package/'bin/qupathxx_batch'),'--export-csv',str(logical/'batch/selected/checkpoint.qpxproj'),str(logical/'measurements.csv')],True),
        ('geometry',[PYTHON,'-I',REPOSITORY+'/tools/ci/check_macos_relocated_runtime.py','--geometry-child',str(package/'lib/libgeos_c.1.dylib'),
                     '--geometry-output',str(logical/'geometry-child.json')],False)]
    hashes={item['path']:item['sha256'] for item in manifest['files']}
    geometry=dict(schema='qpx.macos-geos-relocated-child/1',library=str(package/'lib/libgeos_c.1.dylib'),
        geos_version='3.15.0-CAPI-1.21.0',union_area=150.0,
        mapped=[dict(path=str(package/key),sha256=hashes[key]) for key in rt.closure(plan,'lib/libgeos_c.1.dylib')],release_ready=False)
    (folder/'geometry-child.json').write_bytes(rt.common.canonical_json(geometry))
    batch=batch_text(source_identity=expected_fixture['identity'])
    commands=[]
    for label,arguments,diagnostics in definitions:
        stdout=(batch if label=='batch' else json.dumps(geometry) if label=='geometry' else '').encode('utf-8')
        stderr=(''.join('dyld[123]: <UUID> '+str(package/key)+'\n' for key in rt.closure(plan,'bin/'+PurePosixPath(arguments[0]).name)) if diagnostics else '').encode('utf-8')
        (folder/(label+'.stdout.log')).write_bytes(stdout);(folder/(label+'.stderr.log')).write_bytes(stderr)
        record=dict(label=label,arguments=arguments,exit_code=0,loader_diagnostics=diagnostics,
                    stdout_sha256=audit.sha(folder/(label+'.stdout.log')),stderr_sha256=audit.sha(folder/(label+'.stderr.log')))
        commands.append(record);(folder/(label+'.json')).write_bytes(rt.common.canonical_json(record))
    report=dict(schema='qpx.macos-relocated-runtime/1',architecture='arm64',relocated_archive_sha256=manifest['archive_sha256'],
        plan_sha256=manifest['plan_sha256'],fixture_sha256=expected_fixture['sha256'],fixture_preserved=True,
        fresh_package=str(package),path_with_spaces=True,loader_search_overrides_used=False,loader_diagnostics_used=True,
        commands=commands,batch=rt.batch_contract(batch),csv=rt.csv_contract(csv_text()),region_rgba=[60,100,180,255],geometry=geometry,
        checkpoint_sha256=audit.sha(checkpoint),csv_sha256=audit.sha(folder/'measurements.csv'),pam_sha256=audit.sha(folder/'output.pam'),
        geometry_report_sha256=audit.sha(folder/'geometry-child.json'),all_package_bytes_preserved=True,release_ready=False)
    return folder,plan,manifest,report,expected_fixture


def check(values):
    folder,plan,manifest,report,expected_fixture=values
    return audit.audit_runtime(folder,plan,manifest,report,LOGICAL,PYTHON,REPOSITORY,expected_fixture)


def test_complete_off_host_runtime_auditor_accepts_synthetic_protocol(tmp_path):
    result=check(fixture(tmp_path))
    assert result['native_region_exact'] and result['native_batch_checkpoint'] and result['native_batch_csv']
    assert result['geos_union_area']==150 and result['package_image_mappings_verified']
    assert result['loader_search_overrides_used'] is False


@pytest.mark.parametrize('failure',['release','search_override','fixture','archive','plan','fresh_path','missing_command','arguments',
    'boolean_exit','raw_log','input_project','input_pixels','pam_header','pam_pixels','csv','batch','geometry_area','geometry_path',
    'geometry_hash','checkpoint','geometry_stdout','missing_mapping','outside_mapping'])
def test_changed_artifacts_and_coordinated_runtime_claims_reject(tmp_path,failure):
    values=fixture(tmp_path);folder,plan,manifest,report,expected_fixture=values
    if failure=='release':report['release_ready']=True
    elif failure=='search_override':report['loader_search_overrides_used']=True
    elif failure=='fixture':report['fixture_sha256']='0'*64
    elif failure=='archive':report['relocated_archive_sha256']='0'*64
    elif failure=='plan':report['plan_sha256']='0'*64
    elif failure=='fresh_path':report['fresh_package']='/other/package'
    elif failure=='missing_command':report['commands'].pop()
    elif failure=='arguments':report['commands'][0]['arguments'][1]='--other'
    elif failure=='boolean_exit':report['commands'][0]['exit_code']=False
    elif failure=='raw_log':(folder/'region.stdout.log').write_bytes(b'changed')
    elif failure=='input_project':(folder/'input.qpxproj').write_bytes(b'changed')
    elif failure=='input_pixels':(folder/'input.ppm').write_bytes(b'changed')
    elif failure=='pam_header':(folder/'output.pam').write_bytes(PAM.replace(b'WIDTH 1',b'WIDTH 2'));report['pam_sha256']=audit.sha(folder/'output.pam')
    elif failure=='pam_pixels':(folder/'output.pam').write_bytes(PAM[:-1]+b'\0');report['pam_sha256']=audit.sha(folder/'output.pam')
    elif failure=='csv':(folder/'measurements.csv').write_text(csv_text(mean_channel_1='NaN'),encoding='utf-8');report['csv_sha256']=audit.sha(folder/'measurements.csv')
    elif failure=='batch':report['batch']['records']=2
    elif failure.startswith('geometry_') and failure!='geometry_stdout':
        geometry=copy.deepcopy(report['geometry'])
        if failure=='geometry_area':geometry['union_area']=True
        elif failure=='geometry_path':geometry['mapped'][0]['path']='/build/geos/lib/old.dylib'
        elif failure=='geometry_hash':geometry['mapped'][0]['sha256']='0'*64
        report['geometry']=geometry;(folder/'geometry-child.json').write_bytes(rt.common.canonical_json(geometry))
        (folder/'geometry.stdout.log').write_bytes(rt.common.canonical_json(geometry))
        report['geometry_report_sha256']=audit.sha(folder/'geometry-child.json')
        report['commands'][-1]['stdout_sha256']=audit.sha(folder/'geometry.stdout.log')
        (folder/'geometry.json').write_bytes(rt.common.canonical_json(report['commands'][-1]))
    elif failure=='checkpoint':
        with sqlite3.connect(folder/'batch/selected/checkpoint.qpxproj') as db:db.execute('DELETE FROM analysis_records')
        report['checkpoint_sha256']=audit.sha(folder/'batch/selected/checkpoint.qpxproj')
    elif failure=='geometry_stdout':(folder/'geometry.stdout.log').write_bytes(b'{}')
    elif failure in ['missing_mapping','outside_mapping']:
        text='' if failure=='missing_mapping' else 'dyld[123]: <UUID> /build/geos/lib/libgeos_c.1.dylib\n'
        (folder/'region.stderr.log').write_text(text,encoding='utf-8')
        report['commands'][0]['stderr_sha256']=audit.sha(folder/'region.stderr.log')
        (folder/'region.json').write_bytes(rt.common.canonical_json(report['commands'][0]))
    with pytest.raises(ValueError):check(values)
