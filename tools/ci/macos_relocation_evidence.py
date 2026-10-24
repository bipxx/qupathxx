"""Audit actual uploaded relocated Mac bytes, Apple commands and runtime probes."""
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import posixpath
import sqlite3
import subprocess

spec=importlib.util.spec_from_file_location('qpx_mac_relocated_runtime_evidence',Path(__file__).with_name('check_macos_relocated_runtime.py'))
runtime=importlib.util.module_from_spec(spec);spec.loader.exec_module(runtime)
relocation=runtime.relocation;common=runtime.common;require=runtime.require

SOURCES=['tools/ci/macos_relocation_evidence.py','tools/ci/check_macos_relocated_runtime.py',
         'tools/ci/macos_relocate.py','tools/ci/macos_relocation_plan.py','tools/ci/macos_native_inventory.py',
         'tools/ci/macho_dependencies.py','tools/ci/runtime_manifest.py','tools/ci/pe_dependencies.py',
         'tools/ci/run_macos_native.py','.github/workflows/ci.yml','tests/fixtures/project-v1.qpxproj']


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    path=Path(path);require(path.stat().st_size<=64*1024*1024,'Mac relocated evidence exceeds budget')
    return json.loads(path.read_text(encoding='utf-8'),object_pairs_hook=common.unique_object)


def equal(actual,expected,message):
    require(common.canonical_json(actual)==common.canonical_json(expected),message)


def command_logs(folder,stem,record,expected_arguments):
    require(set(record)=={'arguments','exit_code','stdout_sha256','stderr_sha256'} and
            type(record['exit_code']) is int and record['exit_code']==0,'Apple command fields/status differ')
    equal(record['arguments'],expected_arguments,'Apple command arguments/order differ')
    equal(read(folder/(stem+'.json')),record,'Apple command ledger differs')
    for stream in ['stdout','stderr']:
        path=folder/(stem+'.'+stream+'.log')
        require(path.stat().st_size<=8*1024*1024 and sha(path)==record[stream+'_sha256'],'Apple command raw log differs')


def audit_execution(folder,plan,manifest,stage_root):
    folder=Path(folder);execution=read(folder/'relocation-execution.json')
    require(execution['schema']=='qpx.macos-native-relocation-execution/1' and execution['architecture']==plan['architecture'],
            'Mac relocation execution identity differs')
    require(execution['source_archive_sha256']==plan['source_archive_sha256'] and
            execution['relocated_archive_sha256']==manifest['archive_sha256'] and execution['plan_sha256']==manifest['plan_sha256'],
            'Mac relocation execution archive/plan hashes differ')
    require(execution['ad_hoc_integrity_verified_on_builder'] is True and execution['native_runtime_workflows_verified'] is False
            and execution['signing_verified'] is False and execution['release_ready'] is False,'Mac relocation execution scope differs')
    expected_hashes={item['path']:item['sha256'] for item in manifest['files'] if item['path'] in {edit['target'] for edit in plan['edits']}}
    equal(execution['verified_binary_sha256'],expected_hashes,'Mac verified binary bytes differ from uploaded archive')
    commands=[]
    for edit in sorted(plan['edits'],key=lambda item:(item['target'].startswith('bin/'),item['target'])):
        commands.extend([[str(arg) for arg in command] for command in relocation.apple_commands(edit,PurePosixPath(stage_root)/edit['target'])])
    require(len(execution['apple_tools'])==len(commands),'Mac Apple command cohort differs')
    for i,(record,arguments) in enumerate(zip(execution['apple_tools'],commands)):
        command_logs(folder,f'apple-tool-{i:04}',record,arguments)
    return dict(commands=len(commands),execution_sha256=sha(folder/'relocation-execution.json'),
                ad_hoc_integrity_verified_on_builder=True,publisher_signature_verified=False)


def audit_runtime(folder,plan,manifest,report,logical_work,python_executable,repository_root,fixture):
    folder=Path(folder);logical=PurePosixPath(logical_work)
    package=logical/'fresh relocated package with spaces'
    require(report['schema']=='qpx.macos-relocated-runtime/1' and report['architecture']==plan['architecture'],
            'Mac runtime report identity differs')
    require(report['relocated_archive_sha256']==manifest['archive_sha256'] and report['plan_sha256']==manifest['plan_sha256'],
            'Mac runtime input archive/plan differs')
    for key in ['fixture_preserved','path_with_spaces','loader_diagnostics_used','all_package_bytes_preserved']:
        require(report[key] is True,'Mac runtime proof missing: '+key)
    require(report['loader_search_overrides_used'] is False and report['release_ready'] is False,'Mac runtime search/release scope differs')
    require(report['fresh_package']==str(package),'Mac runtime fresh package path differs')
    require(report['fixture_sha256']==fixture['sha256'] and runtime.sha(folder/'input.qpxproj')==fixture['sha256'],'Mac runtime fixture input differs')
    capis=[item['target'] for item in plan['edits'] if PurePosixPath(item['target']).name.startswith('libgeos_c.')]
    require(len(capis)==1,'Mac runtime C API cohort differs')
    checkpoints=list((folder/'batch').rglob('*.qpxproj'));require(len(checkpoints)==1,'Mac runtime checkpoint cohort differs')
    relative=checkpoints[0].relative_to(folder).as_posix()
    definitions=[
        ('region',[str(package/'bin/qupathxx_region'),'--thumbnail',str(logical/'input.ppm'),'1','1',str(logical/'output.pam'),'--source-pixels','2'],True),
        ('batch',[str(package/'bin/qupathxx_batch'),str(logical/'input.qpxproj'),str(logical/'batch'),'--kind','od','--image','1','--object','1'],True),
        ('export',[str(package/'bin/qupathxx_batch'),'--export-csv',str(logical/relative),str(logical/'measurements.csv')],True),
        ('geometry',[python_executable,'-I',str(PurePosixPath(repository_root)/'tools/ci/check_macos_relocated_runtime.py'),
                     '--geometry-child',str(package/capis[0]),'--geometry-output',str(logical/'geometry.json')],False)]
    require(len(report['commands'])==len(definitions),'Mac runtime command cohort differs')
    for record,(label,arguments,diagnostics) in zip(report['commands'],definitions):
        require(set(record)=={'label','arguments','exit_code','stdout_sha256','stderr_sha256','loader_diagnostics'} and
                record['label']==label and type(record['exit_code']) is int and record['exit_code']==0
                and record['loader_diagnostics'] is diagnostics,'Mac runtime command identity/status differs')
        equal(record['arguments'],arguments,'Mac runtime command arguments differ')
        equal(read(folder/(label+'.json')),record,'Mac runtime command ledger differs')
        for stream in ['stdout','stderr']:
            path=folder/(label+'.'+stream+'.log')
            require(path.stat().st_size<=8*1024*1024 and sha(path)==record[stream+'_sha256'],'Mac runtime raw command bytes differ')
        if diagnostics:
            expected=sorted(str(package/path) for path in runtime.closure(plan,'bin/'+PurePosixPath(arguments[0]).name))
            equal(runtime.loaded_from_diagnostics((folder/(label+'.stderr.log')).read_text(encoding='utf-8'),runtime.alias_names(plan)),
                  expected,'Mac native CLI loaded captured SDK images outside fresh package')
    batch=runtime.batch_contract((folder/'batch.stdout.log').read_text(encoding='utf-8'))
    require(batch['source_identity']==fixture['identity'],'Mac batch fixture identity differs')
    equal(report['batch'],batch,'Mac runtime batch summary differs')
    equal(report['csv'],runtime.csv_contract((folder/'measurements.csv').read_text(encoding='utf-8')),'Mac runtime CSV values differ')
    require((folder/'output.pam').read_bytes().split(b'ENDHDR\n',1)[1]==bytes([60,100,180,255]) and
            report['region_rgba']==[60,100,180,255],'Mac native region pixels differ')
    with sqlite3.connect(checkpoints[0].resolve().as_uri()+'?mode=ro',uri=True) as database:
        require(database.execute('SELECT count(*) FROM analysis_records').fetchone()[0]==1,'Mac checkpoint retained analysis differs')
    geometry=read(folder/'geometry.json')
    equal(report['geometry'],geometry,'Mac geometry child summary differs')
    equal(json.loads((folder/'geometry.stdout.log').read_text(encoding='utf-8'),object_pairs_hook=common.unique_object),
          geometry,'Mac geometry stdout differs from child record')
    require(geometry['schema']=='qpx.macos-geos-relocated-child/1' and geometry['release_ready'] is False and
            type(geometry['union_area']) in (int,float) and geometry['union_area']==150 and geometry['library']==str(package/capis[0]),
            'Mac GEOS C API/union result differs')
    expected=sorted(str(package/path) for path in runtime.closure(plan,capis[0]) if PurePosixPath(path).name.startswith('libgeos'))
    require([item['path'] for item in geometry['mapped']]==expected,'Mac GEOS image mappings differ')
    file_hashes={item['path']:item['sha256'] for item in manifest['files']}
    for item in geometry['mapped']:
        relative=PurePosixPath(item['path']).relative_to(package).as_posix()
        require(item['sha256']==file_hashes[relative],'Mac mapped GEOS bytes differ from uploaded archive')
    for key,path in [('checkpoint_sha256',checkpoints[0]),('csv_sha256',folder/'measurements.csv'),
                     ('pam_sha256',folder/'output.pam'),('geometry_report_sha256',folder/'geometry.json')]:
        require(report[key]==sha(path),'Mac runtime retained artifact hash differs: '+key)
    return dict(native_region_exact=True,native_batch_checkpoint=True,native_batch_csv=True,geos_union_area=150,
                package_image_mappings_verified=True,loader_search_overrides_used=False,fixture_preserved=True)


def audit(folder,root,commit,architecture,ci_result):
    folder,root=Path(folder),Path(root);sources={}
    for relative in SOURCES:
        committed=subprocess.run(['git','-C',str(root),'show',commit+':'+relative],capture_output=True)
        actual=(root/relative).read_bytes()
        require(committed.returncode==0 and committed.stdout==actual,'Mac relocation tooling/fixture differs from dispatched source: '+relative)
        sources[relative]=hashlib.sha256(actual).hexdigest()
    work=PurePosixPath(ci_result['flags']['GEOS_ROOT']).parents[1]
    roots=dict(native=str(work/'native-build'),geos=ci_result['flags']['GEOS_ROOT'],opencv=str(work/'opencv-sdk'))
    equal(read(folder/'macos-relocation-source-roots.json'),roots,'Mac relocation root declarations differ from build')
    plan=relocation.planner.create_plan(folder/'macos-native-files.zip',architecture,roots)
    relocated=folder/'relocated-native'
    equal(read(relocated/'relocation-plan.json'),plan,'Mac recorded relocation plan differs from original snapshot')
    manifest=relocation.inspect_archive(relocated/'macos-relocated-native.zip',plan)
    equal(read(relocated/'macos-relocated-file-inventory.json'),manifest,'Mac relocated file inventory differs from archive')
    execution=audit_execution(relocated,plan,manifest,str(work/'relocated-native/candidate'))
    runtime_report=read(folder/'macos-relocated-runtime.json')
    fixture=runtime.fixture_contract(root/'tests/fixtures/project-v1.qpxproj')
    probe=audit_runtime(folder/'relocated-runtime',plan,manifest,runtime_report,str(work/'relocated-runtime'),
                        ci_result['python_executable'],str(work.parents[2]),fixture)
    return dict(source=commit,architecture=architecture,tooling_sha256=sources,
                source_archive_sha256=plan['source_archive_sha256'],relocated_archive_sha256=manifest['archive_sha256'],
                plan_sha256=manifest['plan_sha256'],files=len(manifest['files']),binaries=len(manifest['binaries']),
                execution=execution,runtime=probe,relocated_native_host_engineering_accepted=True,
                publisher_signature_verified=False,older_os_compatibility_verified=False,python_gui_openslide_validated=False,
                arbitrary_abi_verified=False,release_ready=False,
                scope='Exact-source original/relocated archive, Apple command/log and native region/batch/CSV/GEOS runtime proofs on builder. '
                      'Fresh path with spaces and actual package image mappings; no search overrides. '
                      'No older-OS, arbitrary ABI, macOS Python/GUI/OpenSlide, publisher signing or supported-release acceptance.')
