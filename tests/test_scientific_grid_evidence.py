"""Reject incomplete scientific-grid artifacts without weakening capability gates."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import xml.etree.ElementTree as ET
import pytest

spec=importlib.util.spec_from_file_location('qpx_grid_test_audit',Path(__file__).resolve().parents[1]/'tools/ci/scientific_grid_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_cases(project=True,geos=True):
    flags=dict(project=project,no_project=not project,geos=geos,no_geos=not geos,both=project and geos)
    messages={k:list(v) for k,v in evidence.BASE.items()}
    for suite,flag,text in evidence.CONDITIONAL:
        messages.setdefault(suite,[])
        if flags[flag]: messages[suite].append(text)
    cases=[]
    for name,lines in messages.items():
        c=ET.Element('testcase',name=name);ET.SubElement(c,'system-out').text='\n'.join(lines)+'\n';cases.append(c)
    return cases


@pytest.mark.parametrize('project,geos',[(False,False),(False,True),(True,False),(True,True)])
def test_native_requires_explicit_capability_branches(project,geos):
    report=evidence.native(native_cases(project,geos),project,geos)
    assert report['project']==project and report['geos']==geos and not report['scientific_acceptance']
    with pytest.raises(ValueError):evidence.native(native_cases(not project,geos),project,geos)
    with pytest.raises(ValueError):evidence.native(native_cases(project,not geos),project,geos)


@pytest.mark.parametrize('suite',['cells','project_batch'])
@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','marker','duplicate_marker'])
def test_native_incomplete_or_nonpassing_rejects(suite,failure):
    cases=native_cases();case=next(c for c in cases if c.get('name')==suite)
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    elif failure in ('failure','error','skipped'):ET.SubElement(case,failure)
    elif failure=='marker':case.find('system-out').text='unrelated success'
    else:case.find('system-out').text*=2
    with pytest.raises(ValueError):evidence.native(cases)


def installed(gui):
    cases=[]
    for module,names in evidence.CASES.items():
        for name in names:
            c=ET.Element('testcase',classname='tests.'+module,name=name)
            if module=='test_grid_cell_frontends' and name in evidence.GUI_CASES and not gui:
                ET.SubElement(c,'skipped',type='pytest.skip',message=evidence.GUI_CASES[name][0])
            cases.append(c)
    return cases


@pytest.mark.parametrize('gui',[True,False])
def test_installed_exact_cohort_and_scope(gui):
    result=evidence.python(installed(gui),gui)
    assert result['passed']==(106 if gui else 104)
    assert result['declared_headless_gui_skips']==(0 if gui else 2)
    assert not result['scientific_acceptance']


@pytest.mark.parametrize('gui',[True,False])
@pytest.mark.parametrize('failure',['missing','duplicate','failure','error','skipped','name','class'])
def test_installed_missing_changed_or_nonpassing_rejects(gui,failure):
    cases=installed(gui);case=cases[0]
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    elif failure in ('failure','error','skipped'):ET.SubElement(case,failure)
    elif failure=='name':case.set('name','other')
    else:case.set('classname','other')
    with pytest.raises(ValueError):evidence.python(cases,gui)


@pytest.mark.parametrize('failure',['unexpected_pass','wrong_reason','xfail','missing'])
def test_headless_desktop_declaration_cannot_hide_coverage(failure):
    cases=installed(False);case=next(c for c in cases if c.get('name') in evidence.GUI_CASES)
    skip=case.find('skipped')
    if failure=='unexpected_pass':case.remove(skip)
    elif failure=='wrong_reason':skip.set('message','GEOS unavailable')
    elif failure=='xfail':skip.set('type','pytest.xfail')
    else:cases.remove(case)
    with pytest.raises(ValueError):evidence.python(cases,False)


@pytest.mark.parametrize('text,accepted',[
 ('Success: no issues found in 5 source files\n',True),
 ('Success: no issues found in 4 source files\n',False),
 ('Success: no issues found in 3 source files\n',False),
 ('Success: no issues found in 6 source files\n',False),
 ('Success: no issues found in 5 source files\n'*2,False),
 ('Success: no issues found in 4 source files\nSuccess: no issues found in 5 source files\n',False),
 ('other.py:1: error: broken\nSuccess: no issues found in 5 source files\n',False)])
def test_positive_typing_is_current_and_unambiguous(tmp_path,text,accepted):
    path=tmp_path/'typing.log';path.write_text(text,encoding='utf-8')
    if accepted:assert evidence.positive_typing(path)['files']==5
    else:
        with pytest.raises(ValueError):evidence.positive_typing(path)


@pytest.mark.parametrize('spec',list(evidence.NEGATIVE.values()))
@pytest.mark.parametrize('change',['none','missing','duplicate','foreign'])
def test_negative_typing_exact_errors_include_repeated_line(tmp_path,spec,change):
    name,lines=spec;lines=list(lines)
    if change=='missing':lines.pop()
    elif change=='duplicate':lines.append(lines[-1])
    text=''.join(f'{name}:{line}: error: fixture\n' for line in lines)
    if change=='foreign':text+='other.py:1: error: unrelated\n'
    path=tmp_path/'typing.log';path.write_text(text,encoding='utf-8')
    if change=='none':assert evidence.negative_typing(path,name,lines)['expected_errors']==len(lines)
    else:
        with pytest.raises(ValueError):evidence.negative_typing(path,*spec)


@pytest.mark.parametrize('fixture,actual,accepted',[(False,b'a\r\nb\r\n',True),(False,b'changed\n',False),(True,b'a\r\nb\r\n',False),(True,b'a\nb\n',True)])
def test_batched_source_binding_preserves_raw_fixtures(tmp_path,monkeypatch,fixture,actual,accepted):
    name='tests/fixtures/scientific-grid-v1/reference.json' if fixture else 'cpp/a.cpp'
    p=tmp_path/name;p.parent.mkdir(parents=True);p.write_bytes(actual)
    monkeypatch.setattr(evidence,'EXTRA',(name,))
    monkeypatch.setattr(evidence.subprocess,'check_output',lambda *a,**kw:b'')
    blob=b'a\nb\n';response=b'a'*40+b' blob '+str(len(blob)).encode()+b'\n'+blob+b'\n'
    calls=[]
    def read(*args,**kwargs):
        calls.append((args,kwargs));return SimpleNamespace(stdout=response)
    monkeypatch.setattr(evidence.subprocess,'run',read)
    if accepted:assert len(evidence.source_contract(tmp_path,'b'*40))==1
    else:
        with pytest.raises(ValueError,match='source differs'):evidence.source_contract(tmp_path,'b'*40)
    assert len(calls)==1 and calls[0][1]['input']==('b'*40+':'+name+'\n').encode()
