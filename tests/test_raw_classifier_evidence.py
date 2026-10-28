"""Actual T5 raw cohort/capability/typing/source admission controls."""
import ast
import copy
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as E

import pytest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('raw_evidence_test',ROOT/'tools/ci/raw_classifier_evidence.py')
evidence=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),evidence.__dict__)


def native_cases(full=True):
    groups={name:list(lines) for name,lines in evidence.CONTRACT['native_base'].items()}
    for name,lines in evidence.CONTRACT['native_full' if full else 'native_disabled'].items():groups.setdefault(name,[]).extend(lines)
    result=[]
    for name,lines in groups.items():
        case=E.Element('testcase',name=name);E.SubElement(case,'system-out').text='\n'.join(lines);result.append(case)
    return result


def python_cases(gui=True):
    result=[];off=evidence.CONTRACT['training_off_skip']
    for module,names in evidence.CONTRACT['installed'].items():
        for name in names:
            case=E.Element('testcase',classname='tests.'+module,name=name)
            desktop=evidence.CONTRACT['gui_only'].get(module)
            if not gui and desktop and name in desktop['names']:E.SubElement(case,'skipped',**desktop['attributes'])
            elif module==off['module'] and name==off['name']:E.SubElement(case,'skipped',**off['attributes'])
            result.append(case)
    return result


def typing_logs(folder):
    (folder/evidence.CONTRACT['typing_positive_log']).write_text('Success: no issues found in '+str(len(evidence.CONTRACT['typing_positive']))+' source files\n',encoding='utf-8')
    for filename,value in evidence.CONTRACT['typing_negative'].items():
        text=''.join('tests/typing/'+filename+':'+str(line)+': error: intended error\n' for line in value['lines'])
        text+='Found '+str(len(value['lines']))+' errors in 1 file (checked 1 source file)\n'
        (folder/value['log']).write_text(text,encoding='utf-8')


@pytest.mark.parametrize('full',[False,True])
def test_native_profile_requires_its_actual_capability(full):
    assert evidence.native(native_cases(full),full=full)['full'] is full
    with pytest.raises(ValueError):evidence.native(native_cases(not full),full=full)


@pytest.mark.parametrize('suite',sorted(set(evidence.CONTRACT['native_base'])|set(evidence.CONTRACT['native_full'])))
@pytest.mark.parametrize('change',['missing','duplicate','failure','error','skipped','marker_missing','marker_duplicate','disabled_claim'])
def test_native_false_acceptance_controls(suite,change):
    cases=native_cases();case=next(c for c in cases if c.get('name')==suite)
    if change=='missing':cases.remove(case)
    elif change=='duplicate':cases.append(copy.deepcopy(case))
    elif change in ['failure','error','skipped']:E.SubElement(case,change)
    elif change=='marker_missing':case.find('system-out').text='unrelated legacy PASS'
    elif change=='marker_duplicate':case.find('system-out').text+='\n'+case.findtext('system-out')
    else:case.find('system-out').text+='\n'+evidence.CONTRACT['native_disabled']['viewer_session'][0]
    with pytest.raises(ValueError):evidence.native(cases)


@pytest.mark.parametrize('gui',[False,True])
def test_exact_installed_contract_has_explicit_skips(gui):
    report=evidence.python(python_cases(gui),gui=gui)
    assert report['passed']==(307 if gui else 260)
    assert report['declared_skips']==(1 if gui else 48)


@pytest.mark.parametrize('module',list(evidence.CONTRACT['installed']))
@pytest.mark.parametrize('change',['missing','duplicate','rename','wrong_class','failure','error','unexpected_skip'])
def test_installed_module_false_acceptance_controls(module,change):
    cases=python_cases();case=next(c for c in cases if c.get('classname')=='tests.'+module and c.find('skipped') is None)
    if change=='missing':cases.remove(case)
    elif change=='duplicate':cases.append(copy.deepcopy(case))
    elif change=='rename':case.set('name','unrelated_' + case.get('name'))
    elif change=='wrong_class':case.set('classname','other.'+module)
    else:E.SubElement(case,'skipped' if change=='unexpected_skip' else change,message='not evidence')
    with pytest.raises(ValueError):evidence.python(cases,gui=True)


@pytest.mark.parametrize('module',list(evidence.CONTRACT['gui_only']))
@pytest.mark.parametrize('change',['missing','wrong_reason','wrong_type','duplicate','enabled'])
def test_headless_declaration_is_not_an_arbitrary_skip(module,change):
    cases=python_cases(False);case=next(c for c in cases if c.get('classname')=='tests.'+module and c.find('skipped') is not None);skip=case.find('skipped')
    if change=='missing':case.remove(skip)
    elif change=='wrong_reason':skip.set('message','missing model')
    elif change=='wrong_type':skip.set('type','unavailable')
    elif change=='duplicate':case.append(copy.deepcopy(skip))
    else:
        with pytest.raises(ValueError):evidence.python(cases,gui=True)
        return
    with pytest.raises(ValueError):evidence.python(cases)


def test_typing_positive_and_negative_nonempty_counts(tmp_path):
    typing_logs(tmp_path);report=evidence.typing(tmp_path)
    assert (report['positive_files'],report['negative_files'],report['expected_errors'])==(7,5,28)


@pytest.mark.parametrize('kind',['zero_files','wrong_files','missing_error','extra_error','wrong_line','wrong_summary','missing_log'])
def test_typing_false_acceptance_controls(kind,tmp_path):
    typing_logs(tmp_path);path=tmp_path/evidence.CONTRACT['typing_positive_log']
    if kind in ['zero_files','wrong_files']:path.write_text('Success: no issues found in '+('0' if kind=='zero_files' else '1')+' source files\n',encoding='utf-8')
    else:
        filename,value=next(iter(evidence.CONTRACT['typing_negative'].items()));path=tmp_path/value['log'];text=path.read_text(encoding='utf-8')
        if kind=='missing_error':text='\n'.join(text.splitlines()[1:])
        elif kind=='extra_error':text+='unrelated.py:1: error: extra\n'
        elif kind=='wrong_line':text=text.replace(':9: error:',':999: error:')
        elif kind=='wrong_summary':text=text.replace('checked 1 source file','checked 0 source files')
        elif kind=='missing_log':path.unlink()
        if kind!='missing_log':path.write_text(text,encoding='utf-8')
    with pytest.raises((ValueError,OSError)):evidence.typing(tmp_path)


@pytest.mark.parametrize('argument',[0,1,'false',None])
def test_capabilities_must_be_explicit_bools(argument):
    with pytest.raises(ValueError):evidence.native([],full=argument)
    with pytest.raises(ValueError):evidence.python([],gui=argument)


@pytest.mark.parametrize('name,required',[
 ('run.py',{'native','python','typing'}),('run_macos_native.py',{'native'}),
 ('accept_unified_engine.py',{'source_contract','native','python','typing'}),
 ('accept_manylinux.py',{'source_contract','native','python','typing'}),
 ('accept_macos_native.py',{'source_contract','native'}),
])
def test_production_owners_execute_shared_raw_admission(name,required):
    tree=ast.parse((ROOT/'tools/ci'/name).read_text(encoding='utf-8'))
    calls={n.func.attr for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and isinstance(n.func.value,ast.Name) and n.func.value.id=='raw_classifier'}
    assert required<=calls
    if name.startswith('accept_'):
        calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='verify_source_snapshot']
        assert any("raw_classifier_sources" in ast.unparse(n) for n in calls)


def test_source_closure_covers_shared_kernels_jobs_frontends_and_consumers():
    assert {'cpp/spatial_bindings.cpp','tests/test_spatial_python.py','tests/typing/spatial_valid.py','tests/typing/spatial_invalid.py'}<=set(evidence.SOURCES)
    assert {'cpp/spatial_analysis.cpp','cpp/include/qupathxx/spatial_analysis.hpp','cpp/tests/spatial_analysis_tests.cpp',
            'tests/fixtures/qupath-spatial-0.7.0/contract.json','tests/fixtures/qupath-spatial-0.7.0/reference.json',
            'tests/fixtures/qupath-spatial-0.7.0/manifest.json','tools/upstream/qupath_spatial_reference.groovy',
            'tools/capture_qupath_spatial.py'}<=set(evidence.SOURCES)
    assert {'tools/profile_raw_stream_contours.py','tools/check_tma_large_pyramid.py','tests/test_raw_stream_resource_profile.py','tests/fixtures/raw-stream-resources-v1/contract.json','tests/raw_stream_contour_shutdown_child.py','tests/raw_file_contour_shutdown_child.py','tools/reference_raw_contour_seams.py','tests/fixtures/raw-contour-seams-v1/reference.json','cpp/raw_pixel_frontend.cpp','cpp/raw_pixel_window_workflows.cpp','cpp/tissue_contours.cpp','cpp/raw_pixel_file_jobs.cpp','cpp/project_batch.cpp','cpp/include/qupathxx/raw_pixel_frontend.hpp','tests/test_raw_pixel_python.py','tests/test_gui_pixel_classifier.py','tests/test_viewer_session.py','tools/ci/raw-classifier-cases.json','tests/test_ci_installed_diagnostics.py','tests/fixtures/ci-installed-diagnostics-v1/legacy-command-contract.json'}<=set(evidence.SOURCES)
    assert len(evidence.SOURCES)==len(set(evidence.SOURCES))


@pytest.mark.parametrize('changed_path',['cpp/raw_pixel_frontend.cpp','tests/raw_stream_contour_shutdown_child.py','tests/raw_file_contour_shutdown_child.py','tools/reference_raw_contour_seams.py','tests/fixtures/raw-contour-seams-v1/reference.json',
    'tests/fixtures/qupath-spatial-0.7.0/contract.json','tests/fixtures/qupath-spatial-0.7.0/reference.json','tests/fixtures/qupath-spatial-0.7.0/manifest.json','tools/upstream/qupath_spatial_reference.groovy','tools/capture_qupath_spatial.py'])
def test_actual_git_source_mismatch_and_publication_snapshot(tmp_path,monkeypatch,changed_path):
    if shutil.which('git') is None:
        pytest.skip('development source-proof fixture requires git; package consumers do not')
    paths=[changed_path,'tools/ci/raw-classifier-cases.json']
    for name in paths:
        path=tmp_path/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((ROOT/name).read_bytes())
    if changed_path.startswith('tests/fixtures/qupath-spatial-0.7.0/'):
        # Mirror the production raw-byte policy before Git admits oracle files.
        (tmp_path/'.gitattributes').write_text('tests/fixtures/qupath-spatial-0.7.0/** -text\n',encoding='utf-8')
    def git(*args):return subprocess.check_output(['git','-C',str(tmp_path),*args],stderr=subprocess.STDOUT,text=True).strip()
    git('init','-q');git('add','.')
    git('-c','user.name=Preflight','-c','user.email=preflight@example.invalid','-c','commit.gpgsign=false','commit','-qm','source witness')
    commit=git('rev-parse','HEAD');monkeypatch.setattr(evidence,'SOURCES',paths);proof=evidence.source_contract(tmp_path,commit)
    assert evidence.common.verify_source_snapshot(tmp_path,proof)==2
    changed=tmp_path/paths[0];changed.write_bytes(changed.read_bytes()+b'\n// changed\n')
    with pytest.raises(ValueError):evidence.source_contract(tmp_path,commit)
    with pytest.raises(ValueError):evidence.common.verify_source_snapshot(tmp_path,proof)


def test_git_source_control_declares_missing_development_tool(tmp_path,monkeypatch):
    monkeypatch.setattr(shutil,'which',lambda name:None)
    def unexpected(*args,**kwargs):
        raise AssertionError('missing-tool declaration must precede subprocess work')
    monkeypatch.setattr(subprocess,'check_output',unexpected)
    with pytest.raises(pytest.skip.Exception,match='development source-proof fixture requires git'):
        test_actual_git_source_mismatch_and_publication_snapshot(tmp_path,monkeypatch,'cpp/raw_pixel_frontend.cpp')


@pytest.mark.parametrize('flag',['-O','-OO'])
def test_missing_raw_evidence_rejects_under_optimized_python(flag):
    code="""import importlib.util
from pathlib import Path
p=Path({path!r});spec=importlib.util.spec_from_file_location('optimized_raw',p);m=importlib.util.module_from_spec(spec);exec(compile(p.read_bytes(),str(p),'exec',dont_inherit=True),m.__dict__)
try:m.native([])
except ValueError:print('rejected')
else:raise SystemExit('false acceptance')
""".format(path=str(ROOT/'tools/ci/raw_classifier_evidence.py'))
    done=subprocess.run([sys.executable,flag,'-I','-c',code],capture_output=True,text=True,timeout=30)
    assert done.returncode==0 and done.stdout.strip()=='rejected',done.stderr


@pytest.mark.parametrize('full,suite,marker',[(full,suite,marker) for full in [False,True] for group in ['native_base','native_full' if full else 'native_disabled'] for suite,markers in evidence.CONTRACT[group].items() for marker in markers])
def test_each_native_semantic_marker_is_required(full,suite,marker):
    cases=native_cases(full)
    case=next(c for c in cases if c.get('name')==suite)
    lines=case.findtext('system-out').splitlines();lines.remove(marker)
    case.find('system-out').text='\n'.join(lines)
    with pytest.raises(ValueError):evidence.native(cases,full=full)


def test_gui_declarations_name_only_real_cases_and_do_not_hide_headless_cases():
    for module,desktop in evidence.CONTRACT['gui_only'].items():
        assert desktop['names']==sorted(set(desktop['names']))
        assert set(desktop['names'])<=set(evidence.CONTRACT['installed'][module])
    for module in ['test_raw_stream_contour_batch','test_raw_file_contours']:
        assert set(evidence.CONTRACT['gui_only'][module]['names'])<set(evidence.CONTRACT['installed'][module])
