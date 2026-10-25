"""Required hierarchy evidence cannot be replaced by counts or disabled tests."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import xml.etree.ElementTree as ET

import pytest

spec=importlib.util.spec_from_file_location('qpx_hierarchy_test_audit',Path(__file__).resolve().parents[1]/'tools/ci/hierarchy_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_cases(enabled=True):
    result=[]
    for name,text in [('interchange',evidence.MARKER if enabled else ''),
                      ('recovery',evidence.PATH_MARKER+'\n'+(evidence.RECOVERY_MARKER if enabled else ''))]:
        case=ET.Element('testcase',name=name);ET.SubElement(case,'system-out').text=text+'\n';result.append(case)
    return result


def installed():
    return [ET.Element('testcase',classname='tests.'+name,name=case)
            for name,values in evidence.CASES.items() for case in values]


@pytest.mark.parametrize('enabled',[True,False])
def test_native_capability_is_explicit(enabled):
    result=evidence.native(native_cases(enabled),enabled)
    assert result['hierarchy_enabled'] is enabled
    assert result['hierarchy_qualified']==int(enabled) and result['physical_path_policy']


@pytest.mark.parametrize('suite',[0,1])
@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','marker','duplicate_marker'])
def test_nonpassing_or_incomplete_native_rejects(suite,failure):
    cases=native_cases();case=cases[suite]
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    elif failure in ['failure','error','skipped']:ET.SubElement(case,failure)
    elif failure=='marker':case.find('system-out').text='unrelated success'
    else:case.find('system-out').text*=2
    with pytest.raises(ValueError):evidence.native(cases)


@pytest.mark.parametrize('enabled',[True,False])
def test_wrong_capability_cannot_qualify(enabled):
    with pytest.raises(ValueError):evidence.native(native_cases(not enabled),enabled)


@pytest.mark.parametrize('gui',[True,False])
def test_complete_installed_contract_passes(gui):
    expected=sum(len(values) for name,values in evidence.CASES.items() if gui or name!=evidence.GUI)
    assert evidence.python(installed(),gui)['passed']==expected


@pytest.mark.parametrize('gui',[True,False])
@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','name','class'])
def test_installed_omissions_or_nonpassing_cases_reject(gui,failure):
    cases=installed();case=next(c for c in cases if c.get('classname').endswith('test_recovery_long_paths'))
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    elif failure in ['failure','error','skipped']:ET.SubElement(case,failure)
    elif failure=='name':case.set('name','other')
    else:case.set('classname','other')
    with pytest.raises(ValueError):evidence.python(cases,gui)


def test_gui_cannot_be_replaced_by_headless_success():
    cases=[c for c in installed() if not c.get('classname').endswith(evidence.GUI)]
    evidence.python(cases,False)
    with pytest.raises(ValueError):evidence.python(cases,True)


@pytest.mark.parametrize('fixture,data,accepted',[(False,b'a\r\nb\r\n',True),(False,b'changed\n',False),
                                               (True,b'a\r\nb\r\n',False),(True,b'a\nb\n',True)])
def test_source_binding_normalizes_code_only(tmp_path,monkeypatch,fixture,data,accepted):
    relative='docs/benchmarks/annotation-migration-recovery-local/capture.log' if fixture else 'cpp/source.cpp'
    path=tmp_path/relative;path.parent.mkdir(parents=True);path.write_bytes(data)
    monkeypatch.setattr(evidence,'SOURCES',[relative])
    monkeypatch.setattr(evidence.common.subprocess,'run',lambda *a,**kw:SimpleNamespace(returncode=0,stdout=b'a\nb\n'))
    if accepted:assert len(evidence.source_contract(tmp_path,'a'*40))==1
    else:
        with pytest.raises(ValueError,match='source differs'):evidence.source_contract(tmp_path,'a'*40)


@pytest.mark.parametrize('lines',[[4,5,7],[4,5],[4,5,7,7],[4,5,6]])
def test_typing_errors_require_exact_lines(tmp_path,lines):
    path=tmp_path/'typing.log';path.write_text(''.join(f'hierarchy_invalid.py:{line}: error: invalid\n' for line in lines),encoding='utf-8')
    if lines==[4,5,7]:assert evidence.typing_errors(path)['expected_errors']==3
    else:
        with pytest.raises(ValueError):evidence.typing_errors(path)
