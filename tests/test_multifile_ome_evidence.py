"""Distinguishing controls for actual MFO producer/auditor admission seams."""
import ast
import copy
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace
import xml.etree.ElementTree as ET

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('multifile_evidence_test',ROOT/'tools/ci/multifile_ome_evidence.py')
evidence=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),evidence.__dict__)


def native_cases(tiff=True):
    result=[]
    for suite,markers in evidence.CONTRACT['native'].items():
        case=ET.Element('testcase',name=suite);ET.SubElement(case,'system-out').text='\n'.join(markers);result.append(case)
    case=ET.Element('testcase',name=evidence.CONTRACT['tiff_suite'])
    ET.SubElement(case,'system-out').text='\n'.join([evidence.CONTRACT['tiff_marker'],*evidence.CONTRACT['tiff_additional_markers']] if tiff else [evidence.CONTRACT['tiff_disabled_marker']])
    return result+[case]


def python_cases(gui=True):
    cases=[ET.Element('testcase',classname='tests.'+module,name=name)
           for category in ('installed','desktop') for module,names in evidence.CONTRACT[category].items() for name in names]
    if not gui:
        for case in cases:
            if evidence.desktop_member(case): ET.SubElement(case,'skipped',**evidence.CONTRACT['headless_skip'])
    return cases


def typing_logs(folder):
    (folder/evidence.CONTRACT['typing_positive_log']).write_text('Success: no issues found in 1 source file\n',encoding='utf-8')
    for filename,contract in evidence.CONTRACT['typing_negative'].items():
        (folder/contract['log']).write_text(''.join('tests/typing/'+filename+':'+str(line)+': error: intended error\n' for line in contract['lines']),encoding='utf-8')


@pytest.mark.parametrize('enabled',[False,True])
def test_native_requires_explicit_enabled_or_disabled_capability(enabled):
    assert evidence.native(native_cases(enabled),tiff=enabled)==dict(suites=3,tiff_enabled=enabled,compound_provenance_codecs=True)
    with pytest.raises(ValueError): evidence.native(native_cases(not enabled),tiff=enabled)


@pytest.mark.parametrize('suite',range(3))
@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','marker_missing','marker_duplicate','contradictory_capability'])
def test_native_false_acceptance_controls(suite,change):
    cases=native_cases();case=cases[suite]
    if change=='missing': cases.remove(case)
    elif change=='duplicate': cases.append(copy.deepcopy(case))
    elif change in ('failure','error','skipped'): ET.SubElement(case,change)
    elif change=='marker_missing': case.find('system-out').text='unrelated semantics'
    elif change=='marker_duplicate': case.find('system-out').text+='\n'+case.findtext('system-out')
    else: cases[-1].find('system-out').text+='\n'+evidence.CONTRACT['tiff_disabled_marker']
    with pytest.raises(ValueError): evidence.native(cases)


@pytest.mark.parametrize('gui',[False,True])
def test_complete_installed_desktop_and_typing_evidence(gui,tmp_path):
    cases=python_cases(gui);assert evidence.python(cases)['passed']==54
    assert evidence.desktop(cases,gui=gui)==dict(passed=2 if gui else 0,declared_desktop_skips=0 if gui else 2)
    typing_logs(tmp_path);assert evidence.typing(tmp_path)['intended_negative_errors']==3


@pytest.mark.parametrize('module',list(evidence.CONTRACT['installed']))
@pytest.mark.parametrize('change',['missing','duplicate','same_count_substitution','wrong_class','failure','error','skipped'])
def test_installed_false_acceptance_controls(module,change):
    cases=python_cases();case=next(c for c in cases if c.get('classname')=='tests.'+module)
    if change=='missing': cases.remove(case)
    elif change=='duplicate': cases.append(copy.deepcopy(case))
    elif change=='same_count_substitution': case.set('name','unrelated_case')
    elif change=='wrong_class': case.set('classname','unrelated_module')
    else: ET.SubElement(case,change)
    with pytest.raises(ValueError): evidence.python(cases)


@pytest.mark.parametrize('change',['missing','duplicate','same_count_substitution','wrong_class','failure','error','gui_skip','headless_pass','wrong_skip_reason','wrong_skip_type','duplicate_skip'])
def test_desktop_false_acceptance_controls(change):
    gui=change=='gui_skip';cases=python_cases(gui);case=next(c for c in cases if evidence.desktop_member(c))
    if change=='missing': cases.remove(case)
    elif change=='duplicate': cases.append(copy.deepcopy(case))
    elif change=='same_count_substitution': case.set('name','unrelated_desktop_case')
    elif change=='wrong_class': case.set('classname','unrelated_module')
    elif change in ('failure','error'): ET.SubElement(case,change)
    elif change=='gui_skip': ET.SubElement(case,'skipped',**evidence.CONTRACT['headless_skip'])
    elif change=='headless_pass': case.remove(case.find('skipped'))
    elif change=='wrong_skip_reason': case.find('skipped').set('message','different reason')
    elif change=='wrong_skip_type': case.find('skipped').set('type','pytest.xfail')
    else: ET.SubElement(case,'skipped',**evidence.CONTRACT['headless_skip'])
    with pytest.raises(ValueError): evidence.desktop(cases,gui=gui)


@pytest.mark.parametrize('change',['missing','positive_error','positive_duplicate','negative_missing','wrong_line','foreign_error'])
def test_typing_false_acceptance_controls(tmp_path,change):
    typing_logs(tmp_path);positive=tmp_path/evidence.CONTRACT['typing_positive_log'];negative=tmp_path/next(iter(evidence.CONTRACT['typing_negative'].values()))['log']
    if change=='missing': positive.unlink()
    elif change=='positive_error': positive.write_text('source.py:1: error: bad\n',encoding='utf-8')
    elif change=='positive_duplicate': positive.write_text(positive.read_text()*2,encoding='utf-8')
    elif change=='negative_missing': negative.unlink()
    elif change=='wrong_line': negative.write_text(negative.read_text().replace(':4:',':40:'),encoding='utf-8')
    else: negative.write_text(negative.read_text()+'other.py:1: error: unexpected\n',encoding='utf-8')
    with pytest.raises((ValueError,OSError)): evidence.typing(tmp_path)


@pytest.mark.parametrize('change',['none','text','raw_newline','missing','truncated','trailing'])
def test_source_contract_binds_text_and_raw_fixtures(tmp_path,monkeypatch,change):
    names=['source.cpp','docs/benchmarks/multifile-ome-preparation/fixture/data.tif'];blobs=[b'line\n',b'raw\r\n']
    for name,blob in zip(names,blobs):
        path=tmp_path/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(blob)
    (tmp_path/names[0]).write_bytes(b'line\r\n')
    monkeypatch.setattr(evidence,'SOURCES',names)
    stream=b''.join(b'a'*40+b' blob '+str(len(blob)).encode()+b'\n'+blob+b'\n' for blob in blobs)
    if change=='text': (tmp_path/names[0]).write_bytes(b'changed\n')
    elif change=='raw_newline': (tmp_path/names[1]).write_bytes(b'raw\n')
    elif change=='missing': stream=b'x missing\n'
    elif change=='truncated': stream=stream[:-1]
    elif change=='trailing': stream+=b'unknown'
    calls=[]
    def run(command,**kwargs):
        calls.append((command,kwargs));return SimpleNamespace(returncode=0,stdout=stream)
    monkeypatch.setattr(evidence.common.subprocess,'run',run)
    if change=='none': assert set(evidence.source_contract(tmp_path,'b'*40))==set(names)
    else:
        with pytest.raises(ValueError): evidence.source_contract(tmp_path,'b'*40)
    assert len(calls)==1 and calls[0][0][-2:]==['cat-file','--batch']


def test_named_contract_matches_independent_collection():
    modules=sorted(set(evidence.CONTRACT['installed'])|set(evidence.CONTRACT['desktop']))
    env=os.environ.copy()
    for key in ('PYTEST_ADDOPTS','PYTEST_PLUGINS'): env.pop(key,None)
    env.update(PYTEST_DISABLE_PLUGIN_AUTOLOAD='1',PYTHONUTF8='1',PYTHONIOENCODING='utf-8')
    cmd=[sys.executable,'-I','-m','pytest',*[str(ROOT/'tests'/(name+'.py')) for name in modules],'--collect-only','-q','--noconftest','--color=no','-o','addopts=']
    result=subprocess.run(cmd,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    expected={name:sorted(evidence.CONTRACT['installed'].get(name,[])+evidence.CONTRACT['desktop'].get(name,[])) for name in modules}
    actual={name:[] for name in modules}
    for line in result.stdout.splitlines():
        if '::' not in line:continue
        path,name=line.split('::',1);actual[Path(path).stem].append(name)
    assert {name:sorted(cases) for name,cases in actual.items()}==expected


def test_producer_and_all_auditors_require_protocol():
    for filename,required in {
        'run.py':{'native','python','desktop','typing'},
        'accept_unified_engine.py':{'source_contract','native','python','desktop','typing'},
        'accept_manylinux.py':{'source_contract','native','python','desktop','typing'},
        'accept_macos_native.py':{'source_contract','native'},
    }.items():
        tree=ast.parse((ROOT/'tools/ci'/filename).read_text(encoding='utf-8'))
        calls={n.func.attr for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and isinstance(n.func.value,ast.Name) and n.func.value.id=='multifile'}
        assert required<=calls,filename


@pytest.mark.parametrize('filename',['accept_unified_engine.py','accept_macos_native.py'])
@pytest.mark.parametrize('valid',[False,True])
def test_actual_native_auditor_assignment_rejects_missing_semantics(filename,valid):
    tree=ast.parse((ROOT/'tools/ci'/filename).read_text(encoding='utf-8'))
    node=next(n for n in ast.walk(tree) if isinstance(n,ast.Assign) and any('multifile_ome_native' in ast.unparse(t) for t in n.targets))
    cases=native_cases()
    if not valid: cases[-1].find('system-out').text='old raw suite without multi-file checks'
    scope=dict(multifile=evidence,entry={},entries=[{}],native_cases=cases,cases=cases,job={'name':'Windows GUI'})
    code=compile(ast.Module(body=[node],type_ignores=[]),filename,'exec')
    if valid: exec(code,scope)
    else:
        with pytest.raises(ValueError): exec(code,scope)


@pytest.mark.parametrize('change',['missing','duplicate','disabled_leak'])
def test_real_compound_cancellation_marker_cannot_be_omitted_or_claimed_when_disabled(change):
    cases=native_cases(change!='disabled_leak');out=cases[-1].find('system-out');marker=evidence.CONTRACT['tiff_additional_markers'][0]
    if change=='missing':out.text='\n'.join(line for line in out.text.splitlines() if line!=marker)
    else:out.text+='\n'+marker
    with pytest.raises(ValueError):evidence.native(cases,tiff=change!='disabled_leak')
