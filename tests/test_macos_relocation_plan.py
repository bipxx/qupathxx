"""Independent load-name graph fixtures; planning never runs package binaries."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

import pytest
from test_macho_dependencies import command

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('qpx_mac_plan_tests',ROOT/'tools/ci/macos_relocation_plan.py')
planner=importlib.util.module_from_spec(spec);spec.loader.exec_module(planner)
ROOTS=dict(native='/build/native',geos='/build/geos',opencv='/build/opencv')


def name(kind,text,current=0x010203,compatibility=0x010000):
    prefix=struct.pack('<IIII',24,123,current,compatibility) if kind in [0xc,0xd,0x80000018,0x8000001f,0x20,0x80000023] else struct.pack('<I',12)
    return command(kind,prefix+text.encode('utf-8')+b'\0')


def binary(kind,identity=None,dependencies=(),current=0x010203,flags=0x85,extra=(),rpaths=('/build/private/SDK',)):
    cmds=([name(0xd,identity,current)] if identity else [])
    cmds += [name(0xc,'/usr/lib/libSystem.B.dylib')]
    cmds += [name(mechanism,path) for mechanism,path in dependencies]
    cmds += [name(0x8000001c,path) for path in rpaths]
    cmds += [command(0x32,struct.pack('<IIII',1,0x0b0000,0x0f0000,0))]
    if kind==2:cmds += [name(0xe,'/usr/lib/dyld')]
    cmds += list(extra)
    return struct.pack('<IIIIIIII',0xfeedfacf,0x0100000c,0,kind,len(cmds),sum(map(len,cmds)),flags,0)+b''.join(cmds)


def snapshot(tmp_path, native_dependencies=((0xc,'@rpath/libcapi.1.dylib'),),capi_dependencies=((0xc,'@rpath/libcpp.1.dylib'),),
             capi_id='@rpath/libcapi.1.dylib',cpp_id='@rpath/libcpp.1.dylib',capi_current=0x010203,cpp_current=0x010203,
             native_flags=0x85,native_extra=(),cpp_extra=(),other=None):
    native=tmp_path/'native';native.mkdir()
    sdks={key:tmp_path/key for key in ['geos','opencv']}
    for sdk in sdks.values():
        (sdk/'lib').mkdir(parents=True)
        (sdk/'share/licenses/sdk').mkdir(parents=True)
        (sdk/'share/licenses/sdk/LICENSE').write_bytes(b'fixture SDK notice\n')
    for filename in ['qupathxx_batch','qupathxx_region']:
        (native/filename).write_bytes(binary(2,dependencies=native_dependencies,flags=native_flags,extra=native_extra))
    capi=binary(6,capi_id,capi_dependencies,current=capi_current)
    cpp=binary(6,cpp_id,current=cpp_current,extra=cpp_extra)
    (sdks['geos']/'lib/libcapi.1.dylib').write_bytes(capi)
    (sdks['geos']/'lib/libcapi.dylib').write_bytes(capi)
    (sdks['geos']/'lib/libcpp.1.dylib').write_bytes(cpp)
    (sdks['opencv']/'lib/libcpp-copy.dylib').write_bytes(cpp)
    for key,data in (other or {}).items():
        path=tmp_path/key;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    archive=tmp_path/'snapshot.zip'
    planner.inventory.capture(native,sdks,'arm64',archive,tmp_path/'inventory.json')
    return archive


def test_direct_loader_paths_alias_coalescing_notices_and_closures(tmp_path):
    archive=snapshot(tmp_path)
    original=archive.read_bytes()
    plan=planner.create_plan(archive,'arm64',ROOTS)
    assert archive.read_bytes()==original
    assert len(plan['files'])==6 and len(plan['edits'])==4
    capi=next(item for item in plan['edits'] if item['target']=='lib/libcapi.1.dylib')
    assert len(capi['source_aliases'])==2 and capi['install_id']=='@rpath/libcapi.1.dylib'
    assert capi['changes']==[dict(old='@rpath/libcpp.1.dylib',new='@loader_path/libcpp.1.dylib')]
    batch=next(item for item in plan['edits'] if item['target']=='bin/qupathxx_batch')
    assert batch['changes']==[dict(old='@rpath/libcapi.1.dylib',new='@executable_path/../lib/libcapi.1.dylib')]
    assert batch['remove_rpaths']==['/build/private/SDK'] and plan['planned_rpaths']==[]
    assert plan['entry_points'][0]['bundled_images']==['bin/qupathxx_batch','lib/libcapi.1.dylib','lib/libcpp.1.dylib']
    assert plan['entry_points'][0]['system_requirements']==['/usr/lib/dyld','/usr/lib/libSystem.B.dylib']
    assert plan['declared_minimum_macos']==0x0b0000 and plan['minimum_macos_metadata_complete']
    assert not any(plan[key] for key in ['package_bytes_written','apple_tools_executed','dynamic_loading_verified','symbol_abi_verified','signing_verified','release_ready'])


@pytest.mark.parametrize('reference',['@rpath/libcapi.dylib','/build/geos/lib/libcapi.1.dylib',
    '@loader_path/../geos/lib/libcapi.1.dylib','@executable_path/../geos/lib/libcapi.1.dylib'])
def test_alias_and_explicit_source_locations(reference,tmp_path):
    plan=planner.create_plan(snapshot(tmp_path,native_dependencies=((0xc,reference),)),'arm64',ROOTS)
    edges=[edge for edge in plan['dependency_edges'] if edge['dependency']==reference]
    assert len(edges)==2 and all(edge['target']=='lib/libcapi.1.dylib' for edge in edges)


@pytest.mark.parametrize('mechanism',[0xc,0x80000018,0x8000001f,0x20,0x80000023])
def test_dependency_kinds_and_versions_are_preserved(mechanism,tmp_path):
    plan=planner.create_plan(snapshot(tmp_path,native_dependencies=((mechanism,'@rpath/libcapi.1.dylib'),)),'arm64',ROOTS)
    batch=next(item for item in plan['edits'] if item['target']=='bin/qupathxx_batch')
    expected=batch['expected_dependencies'][-1]
    assert expected['mechanism']==planner.inventory.macho.DYLIBS[mechanism]
    assert expected['compatibility_version']==0x010000 and expected['current_version']==0x010203 and expected['timestamp']==123


@pytest.mark.parametrize('failure',['missing','missing_weak','relative','host_private','noncanonical','version_floor',
    'system_identity','flat','no_twolevel','environment','sub_library','target_collision','ambiguous_alias'])
def test_unresolved_ambiguous_and_unsupported_graphs_reject(failure,tmp_path):
    kwargs={}
    if failure in ['missing','missing_weak','relative','host_private','noncanonical']:
        dependency={'missing':'@rpath/missing.dylib','missing_weak':'@rpath/missing.dylib','relative':'libcapi.1.dylib',
                    'host_private':'/usr/local/lib/libcapi.1.dylib','noncanonical':'/build/geos/lib/../lib/libcapi.1.dylib'}[failure]
        kwargs['native_dependencies']=((0x80000018 if failure=='missing_weak' else 0xc,dependency),)
    elif failure=='version_floor':kwargs['cpp_current']=1
    elif failure=='system_identity':kwargs['cpp_id']='/usr/lib/libSystem.B.dylib'
    elif failure=='flat':kwargs['native_flags']=0x185
    elif failure=='no_twolevel':kwargs['native_flags']=5
    elif failure=='environment':kwargs['native_extra']=(name(0x27,'DYLD_LIBRARY_PATH=/private'),)
    elif failure=='sub_library':kwargs['native_extra']=(name(0x15,'alternate'),)
    elif failure=='target_collision':kwargs['cpp_id']='@rpath/libCAPI.1.dylib'
    elif failure=='ambiguous_alias':kwargs['other']={'opencv/lib/libcapi.dylib':binary(6,'@rpath/libother.1.dylib')}
    with pytest.raises(ValueError):planner.create_plan(snapshot(tmp_path,**kwargs),'arm64',ROOTS)


@pytest.mark.parametrize('roots',[{},dict(ROOTS,native='relative'),dict(ROOTS,native='/'),dict(ROOTS,native='/build/../native'),
    dict(ROOTS,native='/build'),dict(ROOTS,native=ROOTS['geos']),dict(ROOTS,native='/build\\native'),
    dict(ROOTS,native='/build\x01native'),dict(ROOTS,native='/'+('x'*4097))])
def test_bad_source_roots_reject(roots,tmp_path):
    with pytest.raises(ValueError):planner.create_plan(snapshot(tmp_path),'arm64',roots)


def test_cycle_uses_finite_unique_closures(tmp_path):
    archive=snapshot(tmp_path,cpp_extra=(name(0xc,'@rpath/libcapi.1.dylib'),))
    plan=planner.create_plan(archive,'arm64',ROOTS)
    assert len(plan['entry_points'][0]['bundled_images'])==3


def test_cli_new_plan_and_no_input_mutation(tmp_path):
    archive=snapshot(tmp_path);original=archive.read_bytes()
    roots=tmp_path/'roots.json';roots.write_text(json.dumps(ROOTS),encoding='utf-8')
    output=tmp_path/'plan.json'
    command_line=[sys.executable,'-I',str(ROOT/'tools/ci/macos_relocation_plan.py'),
        '--archive',str(archive),'--architecture','arm64','--source-roots',str(roots),'--output',str(output)]
    assert subprocess.run(command_line,capture_output=True).returncode==0
    before=output.read_bytes()
    assert subprocess.run(command_line,capture_output=True).returncode!=0
    assert output.read_bytes()==before and archive.read_bytes()==original
    assert not list(tmp_path.glob('.*.qpx-tmp-*'))
