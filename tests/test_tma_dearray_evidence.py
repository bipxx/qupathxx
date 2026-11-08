"""Reject incomplete native/installed/typing evidence and stale cohort manifests."""
import copy
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('dearray_test_evidence',ROOT/'tools/ci/tma_dearray_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_case(project=True):
    contract=evidence.CONTRACT['native'];case=ET.Element('testcase',name=contract['suite'])
    lines=['TMA_DEARRAY_FIXTURE '+name+' max_center_error_pixels=0 missing_flags=match' for name in contract['fixtures']]
    lines+=contract['markers']+(contract['project_markers'] if project else [contract['project_disabled_marker']])
    ET.SubElement(case,'system-out').text='\n'.join(lines)+'\n';return case


def installed_cases():
    return [ET.Element('testcase',classname=module,name=name) for module,names in evidence.CONTRACT['installed'].items() for name in names]


def desktop_cases(gui):
    cases=[ET.Element('testcase',classname=module,name=name) for module,names in evidence.CONTRACT['desktop'].items() for name in names]
    if not gui:
        for case in cases:ET.SubElement(case,'skipped',**evidence.CONTRACT['headless_skip'])
    return cases


@pytest.mark.parametrize('project',[False,True])
def test_complete_native_capability_is_explicit(project):
    report=evidence.native([native_case(project)],project=project)
    assert report['fixture_cases']==5 and report['center_tolerance_pixels']==2 and report['project_workflows_covered']==project


@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','geometry_missing','geometry_duplicate','geometry_over_tolerance','geometry_negative','geometry_infinite','geometry_malformed','missing_flags','physical_missing','project_disabled','marker_duplicate'])
def test_false_native_evidence_rejects(change):
    case=native_case();cases=[case];out=case.find('system-out');lines=out.text.splitlines()
    if change=='missing':cases=[]
    elif change=='duplicate':cases.append(copy.deepcopy(case))
    elif change in ['failure','error','skipped']:ET.SubElement(case,change)
    elif change=='geometry_missing':lines.pop(0)
    elif change=='geometry_duplicate':lines.append(lines[0])
    elif change.startswith('geometry_'):
        value={'geometry_over_tolerance':'2.000001','geometry_negative':'-0.01','geometry_infinite':'1e999','geometry_malformed':'nan'}[change]
        lines[0]=lines[0].replace('=0 ', '='+value+' ')
    elif change=='missing_flags':lines[0]=lines[0].replace('missing_flags=match','missing_flags=differ')
    elif change=='physical_missing':lines.remove(next(m for m in lines if m.startswith('TMA_DEARRAY_PHYSICAL ')))
    elif change=='project_disabled':cases=[native_case(False)]
    else:lines.append(evidence.CONTRACT['native']['markers'][0])
    out.text='\n'.join(lines)+'\n'
    with pytest.raises(ValueError):evidence.native(cases)


def test_project_enabled_markers_do_not_prove_disabled_configuration():
    with pytest.raises(ValueError):evidence.native([native_case()],project=False)


def test_complete_installed_and_desktop_cohorts():
    assert evidence.python(installed_cases())['passed']==sum(map(len,evidence.CONTRACT['installed'].values()))
    assert evidence.desktop(desktop_cases(True),gui=True)==dict(passed=13,declared_desktop_skips=0)
    assert evidence.desktop(desktop_cases(False))==dict(passed=0,declared_desktop_skips=13)


@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','renamed'])
def test_false_installed_evidence_rejects(change):
    cases=installed_cases()
    if change=='missing':cases.pop()
    elif change=='duplicate':cases.append(copy.deepcopy(cases[-1]))
    elif change=='renamed':cases[-1].set('name','different')
    else:ET.SubElement(cases[-1],change)
    with pytest.raises(ValueError):evidence.python(cases)


@pytest.mark.parametrize('change',['missing','duplicate','failure','gui_skip','headless_pass','wrong_skip_type','wrong_skip_reason','duplicate_skip'])
def test_false_desktop_evidence_rejects(change):
    gui=change=='gui_skip';cases=desktop_cases(gui)
    if change=='missing':cases.pop()
    elif change=='duplicate':cases.append(copy.deepcopy(cases[-1]))
    elif change=='failure':ET.SubElement(cases[0],'failure')
    elif gui:ET.SubElement(cases[0],'skipped',**evidence.CONTRACT['headless_skip'])
    elif change=='headless_pass':cases[0].remove(cases[0].find('skipped'))
    elif change=='wrong_skip_type':cases[0].find('skipped').set('type','xfail')
    elif change=='wrong_skip_reason':cases[0].find('skipped').set('message','not available')
    else:ET.SubElement(cases[0],'skipped',**evidence.CONTRACT['headless_skip'])
    with pytest.raises(ValueError):evidence.desktop(cases,gui=gui)


def typing_logs(folder):
    (folder/evidence.CONTRACT['typing_positive_log']).write_text('Success: no issues found in 1 source file\n',encoding='utf-8')
    for filename,contract in evidence.CONTRACT['typing_negative'].items():
        (folder/contract['log']).write_text(''.join('tests/typing/'+filename+':'+str(line)+': error: intended error\n' for line in contract['lines']),encoding='utf-8')


def test_complete_positive_and_negative_typing(tmp_path):
    typing_logs(tmp_path);report=evidence.typing(tmp_path)
    assert report['positive_files']==1 and report['intended_negative_errors']==6 and len(report['log_sha256'])==3


@pytest.mark.parametrize('change',['positive_missing','positive_old_count','positive_error','negative_missing','negative_wrong_line','negative_duplicate','negative_unrelated'])
def test_false_typing_evidence_rejects(tmp_path,change):
    typing_logs(tmp_path);positive=tmp_path/evidence.CONTRACT['typing_positive_log'];negative=tmp_path/next(iter(evidence.CONTRACT['typing_negative'].values()))['log']
    if change=='positive_missing':positive.unlink()
    elif change=='positive_old_count':positive.write_text('Success: no issues found in 6 source files\n',encoding='utf-8')
    elif change=='positive_error':positive.write_text(positive.read_text(encoding='utf-8')+'other.py:1: error: unexpected\n',encoding='utf-8')
    elif change=='negative_missing':negative.unlink()
    elif change=='negative_wrong_line':negative.write_text(negative.read_text(encoding='utf-8').replace(':4:',':3:'),encoding='utf-8')
    elif change=='negative_duplicate':negative.write_text(negative.read_text(encoding='utf-8')*2,encoding='utf-8')
    else:negative.write_text(negative.read_text(encoding='utf-8')+'other.py:1: error: unexpected\n',encoding='utf-8')
    with pytest.raises((ValueError,OSError)):evidence.typing(tmp_path)


def test_declared_dearray_cohorts_match_independent_source_collection():
    import os
    import subprocess
    import sys
    contract={**evidence.CONTRACT['installed'],**evidence.CONTRACT['desktop']};env=os.environ.copy();env.pop('PYTEST_ADDOPTS',None)
    env.update(PYTEST_DISABLE_PLUGIN_AUTOLOAD='1',PYTHONUTF8='1',PYTHONIOENCODING='utf-8')
    command=[sys.executable,'-I','-m','pytest',*[str(ROOT/'tests'/(module+'.py')) for module in contract],'--collect-only','-q','--noconftest','--color=no','-o','addopts=']
    result=subprocess.run(command,cwd=ROOT,env=env,capture_output=True,text=True,encoding='utf-8',timeout=60)
    assert result.returncode==0,result.stdout+result.stderr;actual={module:[] for module in contract}
    for line in result.stdout.splitlines():
        if '::' not in line:continue
        path,name=line.split('::',1);module=Path(path).stem;assert module in actual;actual[module].append(name)
    assert all(names and len(names)==len(set(names)) and sorted(names)==contract[module] for module,names in actual.items())


def test_producer_and_all_auditors_wire_required_dearray_protocol():
    # Helper tests alone cannot detect an auditor that never calls its gates.
    import ast
    for filename,required in {
        'accept_unified_engine.py':{'source_contract','native','python','desktop','typing'},
        'accept_manylinux.py':{'source_contract','native','python','desktop','typing'},
        'accept_macos_native.py':{'source_contract','native'},
    }.items():
        tree=ast.parse((ROOT/'tools/ci'/filename).read_text(encoding='utf-8'))
        calls={node.func.attr for node in ast.walk(tree) if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and isinstance(node.func.value,ast.Name) and node.func.value.id=='dearray'}
        assert required<=calls, filename
    producer=(ROOT/'tools/ci/run.py').read_text(encoding='utf-8');tree=ast.parse(producer)
    assert any(isinstance(node,ast.Import) and any(alias.name=='tma_dearray_evidence' for alias in node.names) for node in ast.walk(tree))
    assert 'dearray.CONTRACT["typing_positive_log"]' in producer and 'dearray.CONTRACT["typing_negative"]' in producer
    assert any(isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and isinstance(node.func.value,ast.Name) and node.func.value.id=='dearray' and node.func.attr=='typing' for node in ast.walk(tree))
