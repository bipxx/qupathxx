"""Actual post-install producer boundaries retain independent diagnostics."""
import ast
from collections import Counter
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from types import SimpleNamespace

import pytest

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'tools/ci/run.py'
spec=importlib.util.spec_from_file_location('qpx_diagnostic_producer',SOURCE)
producer=importlib.util.module_from_spec(spec)
exec(compile(SOURCE.read_bytes(),str(SOURCE),'exec',dont_inherit=True),producer.__dict__)


def test_real_child_failure_and_timeout_keep_later_logs_and_overall_failure(tmp_path,monkeypatch):
    monkeypatch.setattr(producer,'ROOT',tmp_path)
    diagnostics=producer.InstalledDiagnostics(tmp_path/'stages.json')
    diagnostics.run([sys.executable,'-I','-c','raise SystemExit(7)'],tmp_path/'failed.log')
    diagnostics.run([sys.executable,'-I','-c','import time;time.sleep(5)'],tmp_path/'timeout.log',timeout=.05)
    diagnostics.run(['must-not-start'],tmp_path/'blocked.log',requires=('failed',))
    diagnostics.run([sys.executable,'-I','-c','print("independent result")'],tmp_path/'independent.log')
    with pytest.raises(RuntimeError,match='installed diagnostics failed'): diagnostics.finish()
    saved=json.loads((tmp_path/'stages.json').read_text())
    assert saved['state']=='failed'
    assert [s['state'] for s in saved['stages']]==['failed','timed_out','not_run','passed']
    assert saved['stages'][2]['blocked_by']==['failed'] and not (tmp_path/'blocked.log').exists()
    assert 'independent result' in (tmp_path/'independent.log').read_text()
    assert (tmp_path/'failed.log').is_file() and (tmp_path/'timeout.log').is_file()


def test_expected_negative_child_is_passed_without_retry(tmp_path,monkeypatch):
    monkeypatch.setattr(producer,'ROOT',tmp_path);marker=tmp_path/'once.txt'
    command=[sys.executable,'-I','-c',"from pathlib import Path;import sys;p=Path(sys.argv[1]);p.open('a').write('once\\n');raise SystemExit(1)",str(marker)]
    d=producer.InstalledDiagnostics(tmp_path/'stages.json');d.run(command,tmp_path/'negative.log',expected=1);d.finish()
    assert marker.read_text()=='once\n'
    assert json.loads((tmp_path/'stages.json').read_text())['state']=='passed'
    with pytest.raises(RuntimeError,match='already finished'):d.execute('late',lambda:None)


@pytest.mark.parametrize('timeout',[0,-1,float('inf'),float('nan'),2401])
def test_invalid_deadline_rejects_before_child_launch(tmp_path,timeout):
    d=producer.InstalledDiagnostics(tmp_path/'stages.json')
    with pytest.raises(ValueError,match='finite bounded timeout'):d.run(['must-not-run'],tmp_path/'invalid.log',timeout=timeout)
    assert not (tmp_path/'invalid.log').exists()


def test_unknown_duplicate_and_empty_stage_plans_do_not_pass(tmp_path):
    d=producer.InstalledDiagnostics(tmp_path/'stages.json')
    with pytest.raises(ValueError,match='unknown'):d.execute('early',lambda:None,requires=('not-created',))
    with pytest.raises(RuntimeError):d.finish()
    d=producer.InstalledDiagnostics(tmp_path/'other.json');d.execute('one',lambda:None)
    with pytest.raises(ValueError,match='duplicate'):d.execute('one',lambda:None)


def test_operator_interrupt_is_not_swallowed(tmp_path):
    d=producer.InstalledDiagnostics(tmp_path/'stages.json')
    def interrupt():raise KeyboardInterrupt()
    with pytest.raises(KeyboardInterrupt):d.execute('interrupted',interrupt)
    assert json.loads((tmp_path/'stages.json').read_text())['state']=='running'


def test_preparation_and_all_legacy_command_arguments_remain_intact():
    expected=json.loads((ROOT/'tests/fixtures/ci-installed-diagnostics-v1/legacy-command-contract.json').read_text())
    main=next(n for n in ast.parse(SOURCE.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='main')
    start=next(i for i,n in enumerate(main.body) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='diagnostics' for t in n.targets))
    extension=ast.parse("raw_classifier.native(list(ET.parse(work/'native-tests.xml').getroot().iter('testcase')),full=full)").body[0]
    key_node=lambda node:ast.dump(node,include_attributes=False)
    assert sum(key_node(n)==key_node(extension) for n in main.body[:start])==1
    prefix=ast.dump(ast.Module(body=[n for n in main.body[:start] if key_node(n)!=key_node(extension)],type_ignores=[]),include_attributes=False)
    assert prefix==ast.dump(ast.parse(expected['preparation_source']),include_attributes=False)
    commands=[]
    for n in ast.walk(ast.Module(body=main.body[start:],type_ignores=[])):
        if isinstance(n,ast.Call) and (ast.unparse(n.func)=='run' or ast.unparse(n.func)=='diagnostics.run'):
            commands.append(dict(args=[ast.unparse(a) for a in n.args],keywords=[(k.arg,ast.unparse(k.value)) for k in n.keywords]))
    key=lambda value:json.dumps(value,sort_keys=True)
    # Keep every frozen legacy command; admit exactly the two new raw typing
    # command sites (the negative site runs once per named fixture).
    additions=ast.parse("""diagnostics.run([test_python,'-I','-m','mypy','--strict','--no-incremental',*raw_classifier.CONTRACT['typing_positive']],work/raw_classifier.CONTRACT['typing_positive_log'],env)
diagnostics.run([test_python,'-I','-m','mypy','--strict','--no-incremental','tests/typing/'+filename],work/contract['log'],env,expected=1)""")
    extra=[dict(args=[ast.unparse(a) for a in n.args],keywords=[(k.arg,ast.unparse(k.value)) for k in n.keywords]) for n in ast.walk(additions) if isinstance(n,ast.Call)]
    assert Counter(map(key,commands))==Counter(map(key,expected['commands']))+Counter(map(key,extra))
    finish=next(i for i,n in enumerate(main.body) if isinstance(n,ast.Expr) and isinstance(n.value,ast.Call) and ast.unparse(n.value.func)=='diagnostics.finish')
    publication=next(i for i,n in enumerate(main.body) if "work / 'result.json'" in ast.unparse(n))
    assert finish<publication


def post_install_code():
    main=next(n for n in ast.parse(SOURCE.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='main')
    start=next(i for i,n in enumerate(main.body) if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='diagnostics' for t in n.targets))
    finish=next(i for i,n in enumerate(main.body) if isinstance(n,ast.Expr) and isinstance(n.value,ast.Call) and ast.unparse(n.value.func)=='diagnostics.finish')
    return compile(ast.Module(body=main.body[start:finish+1],type_ignores=[]),str(SOURCE),'exec')


@pytest.mark.parametrize('gui,failure',[(gui,failure) for gui in [False,True] for failure in [None,'python-tests','python-tests-with-junit','bad-junit','pixel-stream-file-benchmark','typing','raw-classifier-cohorts','raw-positive','raw-negative','raw-classifier-typing-validation','qupath-cells-body-comparison','bad-negative-lines','standalone-gui','standalone-timeout','standalone-archive','windows-native-runtime-manifest'] if gui or failure not in ('standalone-gui','standalone-timeout','standalone-archive','windows-native-runtime-manifest')])
def test_actual_producer_tail_collects_independent_stages(tmp_path,monkeypatch,gui,failure):
    trace=[];work=tmp_path;mesa=work/'mesa';mesa.mkdir()
    for name in ['opengl32.dll','libgallium_wgl.dll']:(mesa/name).write_bytes(b'synthetic')
    def archive(wheel,output):
        if failure=='standalone-archive':raise OSError('injected archive failure')
        stage=output/'standalone';stage.mkdir()
        for name in ['qupathxx_app.exe','qupathxx_batch.exe','qupathxx_region.exe']:(stage/name).write_bytes(b'synthetic')
        return stage
    def run(arguments,log,env=None,timeout=2400,expected=0):
        name=Path(log).stem;trace.append(name);Path(log).write_text('observed child\n',encoding='utf-8')
        if name==failure:raise RuntimeError('injected '+name)
        if failure=='standalone-timeout' and name=='standalone-gui':raise subprocess.TimeoutExpired(arguments,1)
        if name=='python-tests':
            (work/'python-tests.xml').write_text('<wrong/>' if failure=='bad-junit' else '<testsuite/>')
            if failure=='python-tests-with-junit':raise RuntimeError('unrelated pytest failure with valid JUnit')
        if expected==1:
            file=next((Path(str(a)).name for a in arguments if str(a).endswith('_invalid.py') or str(a).endswith('invalid_api.py')),None)
            lines={'invalid_api.py':[6,7,8,9,11,13,14],'qptma_invalid.py':[4,5,6],'hierarchy_invalid.py':[4,5,7]}.get(file,[])
            if failure=='bad-negative-lines' and name=='typing-negative':lines=[99]
            Path(log).write_text(''.join(str(file)+':'+str(line)+': error: intended\n' for line in lines))
        if '--layout' in arguments:
            path=Path(arguments[arguments.index('--layout')+1]);path.write_bytes(b'; qupathxx workspace layout v1\n[Docking][Data]')
        if name=='standalone-batch':
            path=work/'standalone-batch';path.mkdir();(path/'one.qpxproj').write_bytes(b'synthetic')
        if name=='standalone-region':Path(arguments[-1]).write_bytes(b'ENDHDR\n'+bytes([16,32,64,255])*4)
        if name=='standalone-thumbnail':Path(arguments[-1]).write_bytes(b'ENDHDR\n'+bytes([16,32,64,255]))
        if name=='standalone-tiff-thumbnail':Path(arguments[-1]).write_bytes(b'WIDTH 31\nHEIGHT 20\nENDHDR\n'+bytes([40,80,160,255])*620)
        if name in ('standalone-slide-region','installed-slide-region'):Path(arguments[-1]).write_bytes(b'same synthetic pixels')
        if name=='standalone-slide-thumbnail':Path(log).write_text('source pixel limit')
    monkeypatch.setattr(producer,'run',run)
    contract=lambda prefix:dict(typing_positive_log=prefix+'-positive.log',typing_negative={'invalid.py':dict(log=prefix+'-negative.log')})
    scientific=SimpleNamespace(POSITIVE=['positive.py'],NEGATIVE={'scientific-negative.log':('scientific_invalid.py',[1])},positive_typing=lambda p:None,negative_typing=lambda *args:None)
    dearray=SimpleNamespace(CONTRACT=contract('dearray'),typing=lambda p:None)
    multifile=SimpleNamespace(CONTRACT=contract('multifile'),typing=lambda p:None,python=lambda cases:None,desktop=lambda cases,gui:None)
    axis=SimpleNamespace(TYPING_LOG='axis-negative.log',typing=lambda p:None)
    def raw_check(stage):
        if failure==stage:raise ValueError('injected '+stage)
    raw_classifier=SimpleNamespace(CONTRACT=dict(typing_positive=['raw-positive.py'],typing_positive_log='raw-positive.log',typing_negative={'raw_invalid.py':dict(log='raw-negative.log')}),python=lambda cases,gui:raw_check('raw-classifier-cohorts'),typing=lambda p:raw_check('raw-classifier-typing-validation'))
    scope=dict(producer.__dict__);scope.update(InstalledDiagnostics=producer.InstalledDiagnostics,run=run,work=work,test_python='installed-python',sys=sys,args=SimpleNamespace(mode='gui' if gui else 'headless',extended_readers=True),env={'QUPATHXX_HAMAMATSU_FIXTURE':'ndpi','QUPATHXX_APERIO_MULTILEVEL_FIXTURE':'aperio','QUPATHXX_APERIO_FIXTURE':'small-slide'},scientific=scientific,dearray=dearray,multifile=multifile,raw_classifier=raw_classifier,axis=axis,os=SimpleNamespace(name='nt',pathsep=os.pathsep,environ={'SystemRoot':'C:/Windows'}),archive_windows=archive,wheel=work/'wheel.whl',mesa=mesa,install=work/'install')
    if failure:
        with pytest.raises(RuntimeError,match='installed diagnostics failed'):exec(post_install_code(),scope)
    else:exec(post_install_code(),scope)
    ledger=json.loads((work/'diagnostic-stages.json').read_text());stages={s['name']:s for s in ledger['stages']}
    assert ledger['state']==('failed' if failure else 'passed')
    assert all(trace.count(name)==1 for name in trace),'independent preludes must not replay'
    assert 'stubs' in trace and 'multifile-positive' in trace and 'hierarchy-typing-negative' in trace
    if failure in ('python-tests','bad-junit'):assert stages['multifile-cohorts']['blocked_by']==['python-junit']
    if failure=='python-tests-with-junit':
        assert stages['python-tests']['state']=='failed' and stages['python-junit']['state']=='passed'
        assert stages['multifile-cohorts']['state']=='passed'
    if failure=='pixel-stream-file-benchmark':
        for name in ['pixel-stream-read-benchmark','pixel-stream-record-benchmark']:
            assert name not in trace and stages[name]['blocked_by']==[failure]
    if failure=='typing':assert stages['typing-positive-validation']['blocked_by']==['typing']
    if failure in ('raw-positive','raw-negative'):assert stages['raw-classifier-typing-validation']['blocked_by']==[failure]
    if failure in ('raw-classifier-cohorts','raw-classifier-typing-validation'):assert stages[failure]['state']=='failed'
    if failure=='bad-negative-lines':assert stages['typing-negative-validation']['state']=='failed'
    if gui and failure in ('standalone-gui','standalone-timeout'):
        assert stages['standalone-layout']['state']==('timed_out' if failure=='standalone-timeout' else 'failed')
        assert stages['standalone-region']['state']==stages['standalone-slide-region']['state']=='passed'
    if failure in ('windows-native-runtime-manifest','standalone-archive'):
        assert stages['standalone-preparation']['state']=='not_run' and stages['standalone-layout']['state']=='not_run'
