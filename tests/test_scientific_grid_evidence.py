"""Reject incomplete scientific-grid artifacts without weakening capability gates."""
import importlib.util
from pathlib import Path
from types import SimpleNamespace
import xml.etree.ElementTree as ET
import pytest

spec=importlib.util.spec_from_file_location('qpx_grid_test_audit',Path(__file__).resolve().parents[1]/'tools/ci/scientific_grid_evidence.py')
evidence=importlib.util.module_from_spec(spec);spec.loader.exec_module(evidence)


def native_cases(project=True,geos=True,tiff=True):
    flags=dict(project=project,no_project=not project,geos=geos,no_geos=not geos,both=project and geos,tiff=tiff,raw_project=project and tiff)
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


@pytest.mark.parametrize('suite',['cells','project_batch','raw_image'])
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
            if name in evidence.GUI_CASES and not gui:
                ET.SubElement(c,'skipped',type='pytest.skip',message=evidence.GUI_CASES[name][0])
            cases.append(c)
    return cases


@pytest.mark.parametrize('gui',[True,False])
def test_installed_exact_cohort_and_scope(gui):
    result=evidence.python(installed(gui),gui)
    assert result['passed']==sum(map(len,evidence.CASES.values()))-(0 if gui else len(evidence.GUI_CASES))
    assert result['declared_headless_gui_skips']==(0 if gui else len(evidence.GUI_CASES))
    assert not result['scientific_acceptance']


@pytest.mark.parametrize('module',['test_public_stubs','test_qupath_rgb_resampling','test_rgb_nominal_reference','test_rgb_nominal_api','test_rgb_nominal_preview','test_nominal_cell_python','test_nominal_cell_batches'])
def test_installed_manifest_matches_independent_pytest_collection(module):
    # Manifest-derived fixtures cannot detect a new real test omitted from CASES.
    import os
    import subprocess
    import sys
    root=Path(__file__).resolve().parents[1]
    env=os.environ.copy()
    env.pop('PYTEST_ADDOPTS',None)
    env['PYTEST_DISABLE_PLUGIN_AUTOLOAD']='1'
    command=[sys.executable,'-I','-m','pytest',str(root/'tests'/(module+'.py')),
             '--collect-only','-q','--noconftest','--color=no','-o','addopts=']
    result=subprocess.run(command,cwd=root,env=env,capture_output=True,
                          text=True,encoding='utf-8',timeout=60)
    assert result.returncode==0,result.stdout+result.stderr
    names=[line.split('::',1)[1] for line in result.stdout.splitlines()
           if module+'.py::' in line]
    assert names and len(names)==len(set(names))
    assert sorted(names)==evidence.CASES[module]


@pytest.mark.parametrize('failure',['missing','duplicate','failure','skipped'])
def test_raw_partial_result_stub_case_is_required(failure):
    cases=installed(False)
    case=next(c for c in cases if c.get('name')==
              'test_raw_pixel_partial_results_are_optional_but_predictions_are_not')
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    else:ET.SubElement(case,failure)
    with pytest.raises(ValueError,match='scientific installed'):
        evidence.python(cases,False)


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


@pytest.mark.parametrize('failure',['missing','duplicate','failure','skipped'])
@pytest.mark.parametrize('module,count',[('test_qupath_grid_cells_reference',32),('test_qupath_roi_cell_debug',14),('test_qupath_resampled_cell_debug',26),('test_scientific_raw_tiff_layouts',36),('test_raw_streaming',11),('test_raw_materialized_streaming',18),('test_rgb_nominal_api',224)])
def test_required_scientific_cohort_is_not_an_optional_science_skip(failure,module,count):
    cases=installed(False)
    selected=[c for c in cases if c.get('classname')=='tests.'+module]
    assert len(selected)==count
    case=selected[0]
    if failure=='missing':cases.remove(case)
    elif failure=='duplicate':cases.append(case)
    else:ET.SubElement(case,failure)
    with pytest.raises(ValueError):evidence.python(cases,False)


@pytest.mark.parametrize('failure',['unexpected_pass','wrong_reason','xfail','missing'])
def test_headless_desktop_declaration_cannot_hide_coverage(failure):
    cases=installed(False);case=next(c for c in cases if c.get('name') in evidence.GUI_CASES)
    skip=case.find('skipped')
    if failure=='unexpected_pass':case.remove(skip)
    elif failure=='wrong_reason':skip.set('message','GEOS unavailable')
    elif failure=='xfail':skip.set('type','pytest.xfail')
    else:cases.remove(case)
    with pytest.raises(ValueError):evidence.python(cases,False)


POSITIVE_COUNT=len(evidence.POSITIVE)
POSITIVE_SUCCESS=f'Success: no issues found in {POSITIVE_COUNT} source files\n'
@pytest.mark.parametrize('text,accepted',[
 (POSITIVE_SUCCESS,True),
 (f'Success: no issues found in {POSITIVE_COUNT-1} source files\n',False),
 (f'Success: no issues found in {POSITIVE_COUNT-2} source files\n',False),
 (f'Success: no issues found in {POSITIVE_COUNT+1} source files\n',False),
 (POSITIVE_SUCCESS*2,False),
 (f'Success: no issues found in {POSITIVE_COUNT-1} source files\n'+POSITIVE_SUCCESS,False),
 ('other.py:1: error: broken\n'+POSITIVE_SUCCESS,False)])

def test_positive_typing_is_current_and_unambiguous(tmp_path,text,accepted):
    path=tmp_path/'typing.log';path.write_text(text,encoding='utf-8')
    if accepted:assert evidence.positive_typing(path)['files']==POSITIVE_COUNT
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
        calls.append((args,kwargs));return SimpleNamespace(returncode=0,stdout=response)
    monkeypatch.setattr(evidence.subprocess,'run',read)
    if accepted:assert len(evidence.source_contract(tmp_path,'b'*40))==1
    else:
        with pytest.raises(ValueError,match='source differs'):evidence.source_contract(tmp_path,'b'*40)
    assert len(calls)==1 and calls[0][1]['input']==('b'*40+':'+name+'\n').encode()


@pytest.mark.parametrize('failure',['missing','duplicate'])
@pytest.mark.parametrize('marker',[
    '99 pinned RGB nominal reads, fixed-point bytes, global crop context, admission and cancellation passed',
    '99 public RGB nominal reader hashes, exact halo IO, closed-reader ownership and decoder/cancellation rejection passed',
    'Streamed raw materialization exact means, pinned chunk bits/support, cancellation, admission and dense fallback passed',
    'Streamed raw digest canonical HWC/validity hashes, bounded chunks, metadata epoch, late faults and admission passed',
    'Native exact typed weighted means, subnormal/cancellation, atomic rejection and nearest-even rounding passed',
    'Native typed scientific raw sampling, per-channel finite/support, source-bit provenance and pre-IO admission passed','Typed raw hash-only identity, lower memory admission and pre-decoder metadata epoch passed','188 independent derived raw grid statistics references, ROI/core support, hash-only digest and native jobs passed','Independent Python struct/hashlib QRA1 and QRA2 bytes, mixed typed CSV and job replay passed','QRA2 raw grid retention, canonical re-encoding, semantic corruption and fresh replay passed','Shared ROI grid raw count/context planning and pre-IO admission passed'])
def test_exact_raw_native_markers_are_required_once(failure,marker):
    cases=native_cases()
    case=next(c for c in cases if c.get('name')=='resampling')
    lines=case.findtext('system-out').splitlines()
    if failure=='missing':lines.remove(marker)
    else:lines.append(marker)
    case.find('system-out').text='\n'.join(lines)+'\n'
    with pytest.raises(ValueError,match='marker differs'):evidence.native(cases)


def test_tiff_scientific_markers_follow_declared_reader_capability():
    evidence.native(native_cases(tiff=False),tiff=False)
    with pytest.raises(ValueError):evidence.native(native_cases(tiff=False),tiff=True)
    with pytest.raises(ValueError):evidence.native(native_cases(tiff=True),tiff=False)
