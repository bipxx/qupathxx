"""TMA axis qualification rejects green-but-incomplete native/installed evidence."""
import copy
import importlib.util
from pathlib import Path
import xml.etree.ElementTree as ET
import pytest

spec=importlib.util.spec_from_file_location('qpx_axis_test_evidence',Path(__file__).resolve().parents[1]/'tools/ci/tma_axis_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_cases():
    cases=[]
    for suite,markers in evidence.CONTRACT['native'].items():
        case=ET.Element('testcase',name=suite)
        ET.SubElement(case,'system-out').text='\n'.join(markers)+'\n'
        cases.append(case)
    return cases


def python_cases(group):
    return [ET.Element('testcase',classname='tests.'+module,name=name)
        for module,names in evidence.CONTRACT[group].items() for name in names]


def headless_cases():
    cases=python_cases('desktop')
    for case in cases:ET.SubElement(case,'skipped',type='pytest.skip',message='opt-in TMA desktop')
    return cases


def test_complete_named_native_installed_and_desktop_contracts():
    assert evidence.native(native_cases())['suites']==len(evidence.CONTRACT['native'])
    assert evidence.python(python_cases('installed'))['passed']==sum(map(len,evidence.CONTRACT['installed'].values()))
    assert evidence.desktop(python_cases('desktop'),gui=True)['declared_desktop_skips']==0
    assert evidence.desktop(headless_cases())['passed']==0


@pytest.mark.parametrize('suite',list(evidence.CONTRACT['native']))
@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','marker','duplicate_marker'])
def test_native_suite_and_each_axis_semantic_marker_required(suite,change):
    cases=native_cases();case=next(c for c in cases if c.get('name')==suite)
    if change=='missing':cases.remove(case)
    elif change=='duplicate':cases.append(copy.deepcopy(case))
    elif change in ['failure','error','skipped']:ET.SubElement(case,change)
    elif change=='marker':case.find('system-out').text='generic green suite'
    else:case.find('system-out').text*=2
    with pytest.raises(ValueError):evidence.native(cases)


@pytest.mark.parametrize('group',['installed','desktop'])
@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','renamed','wrong_class'])
def test_every_named_installed_and_gui_case_is_required(group,change):
    cases=python_cases(group);case=cases[-1]
    if change=='missing':cases.pop()
    elif change=='duplicate':cases.append(copy.deepcopy(case))
    elif change in ['failure','error','skipped']:ET.SubElement(case,change)
    elif change=='renamed':case.set('name','unrelated')
    else:case.set('classname','tests.unrelated')
    with pytest.raises(ValueError):
        if group=='installed':evidence.python(cases)
        else:evidence.desktop(cases,gui=True)


@pytest.mark.parametrize('change',['missing_case','executed','wrong_reason','xfail','duplicate_skip','failed'])
def test_headless_desktop_evidence_requires_exact_declared_skip(change):
    cases=headless_cases();case=cases[-1];skip=case.find('skipped')
    if change=='missing_case':cases.pop()
    elif change=='executed':case.remove(skip)
    elif change=='wrong_reason':skip.set('message','missing display')
    elif change=='xfail':skip.set('type','pytest.xfail')
    elif change=='duplicate_skip':ET.SubElement(case,'skipped',type='pytest.skip',message='opt-in TMA desktop')
    else:ET.SubElement(case,'failure')
    with pytest.raises(ValueError):evidence.desktop(cases)


@pytest.mark.parametrize('change',['none','missing','duplicate','wrong_line','foreign'])
def test_negative_typing_requires_all_and_only_the_three_errors(tmp_path,change):
    lines=[4,5,6]
    if change=='missing':lines.pop()
    elif change=='duplicate':lines.append(6)
    elif change=='wrong_line':lines[-1]=7
    text=''.join(f'tests/typing/tma_axes_invalid.py:{line}: error: fixture\n' for line in lines)
    if change=='foreign':text+='other.py:9: error: unrelated\n'
    path=tmp_path/'typing.log';path.write_text(text,encoding='utf-8')
    if change=='none':assert evidence.typing(path)['expected_errors']==3
    else:
        with pytest.raises(ValueError):evidence.typing(path)


@pytest.mark.parametrize('mutated',[False,True])
def test_axis_source_binding_rejects_changed_workflow_bytes(tmp_path,monkeypatch,mutated):
    from types import SimpleNamespace
    names=['cpp/tma.cpp','tests/test_tma_axes.py'];blob=b'original\n'
    for name in names:
        path=tmp_path/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'original\r\n')
    if mutated:(tmp_path/names[-1]).write_bytes(b'changed\n')
    monkeypatch.setattr(evidence,'SOURCES',names)
    response=(b'a'*40+b' blob 9\n'+blob+b'\n')*2
    monkeypatch.setattr(evidence.common.subprocess,'run',lambda *a,**kw:SimpleNamespace(returncode=0,stdout=response))
    if mutated:
        with pytest.raises(ValueError,match='TMA axis source differs'):evidence.source_contract(tmp_path,'b'*40)
    else:assert set(evidence.source_contract(tmp_path,'b'*40))==set(names)
