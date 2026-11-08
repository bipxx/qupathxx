from pathlib import Path
import base64,csv,hashlib,io,json,re,shutil,subprocess,xml.etree.ElementTree as ET,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/qupath-project-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=json.loads((o/'compiler-inputs-final.json').read_bytes());env=q.build_environment()
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fingerprint
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==1
assert json.loads((o/'build-final-exit.json').read_bytes())['returncode']==0
initial=json.loads((o/'compiler-inputs.json').read_bytes())
assert [p for p,h in inputs['files'].items() if initial['files'].get(p)!=h]==['cpp/gui.cpp','cpp/qupath_project.cpp']
cohorts={name:ET.parse(o/(name+'.xml')).findall('.//testcase') for name in ('before','model','native','desktop','desktop-final','optional-metadata')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
assert len(cohorts['before'])==2 and sum(c.find('failure') is not None for c in cohorts['before'])==1
assert all(c.find('failure') is None and c.find('error') is None for c in cohorts['model'])
assert sum(passed(c) for c in cohorts['model'])==242
assert all(passed(c) for c in cohorts['desktop-final']+cohorts['optional-metadata'])
assert len(cohorts['desktop-final'])==1 and len(cohorts['optional-metadata'])==2
assert sum(c.find('failure') is not None for c in cohorts['desktop'])==1
new={};desktop={}
for name in ('model','desktop','desktop-final','optional-metadata'):
    for c in cohorts[name]:
        key=(c.get('classname'),c.get('name'))
        if key[0]=='tests.test_qupath_project':new[key]=c
        if name.startswith('desktop'):desktop[key]=c
assert len(new)==23 and all(passed(c) for c in new.values())
assert len(desktop)==3 and all(passed(c) for c in desktop.values())
native=cohorts['native'];assert {c.get('name') for c in native}=={'qupath_project','project','interchange'} and all(passed(c) for c in native)
assert 'QuPath project native preview, source guards, new publication and replay refusal passed' in ''.join(c.findtext('system-out','') for c in native)
for name in ('stubs','typing-valid'):assert json.loads((o/(name+'-command.json')).read_bytes())['returncode']==0
assert json.loads((o/'typing-invalid-command.json').read_bytes())['returncode']==1
assert re.findall(r'qupath_project_invalid.py:(\d+): error:',(o/'typing-invalid.log').read_text())==['4','5']
wheel,=list((o/'wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;assert 'qupath-project-r5-env' in str(site)
members=[]
with zipfile.ZipFile(wheel) as archive:
    for name in archive.namelist():
        if name.startswith('qupathxx/') and not name.endswith('/'):
            assert archive.read(name)==(site/name).read_bytes();members.append(name)
    assert len(members)==62
    for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):assert archive.read(name)==(w/'src'/name).read_bytes()
    record,=list(n for n in archive.namelist() if n.endswith('.dist-info/RECORD'))
    for name,hash_value,size in csv.reader(io.StringIO(archive.read(record).decode())):
        if name==record:assert hash_value==size=='';continue
        data=archive.read(name);assert len(data)==int(size)
        assert hash_value=='sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('=')
    before_wheel,=list((o/'wheel-before-stubs').glob('*.whl'))
    with zipfile.ZipFile(before_wheel) as before:
        native_member,=list(n for n in members if n.endswith('.pyd'))
        assert before.read(native_member)==archive.read(native_member)
package=json.loads((o/'package.json').read_bytes());assert package['final_product_installs']==1 and not package['native_rebuild_for_stubs']
assert package['native_sha256']==digest(Path(_native.__file__)) and package['final_wheel_sha256']==digest(wheel)
fixture=w/'tests/fixtures/qupath-project-0.7.0';manifest=json.loads((fixture/'manifest.json').read_bytes())
for name,h in manifest['files'].items():assert digest(fixture/name)==h
for name,h in manifest['probes'].items():assert digest(w/name)==h
controls=json.loads((o/'export-controls.json').read_bytes())
for c in controls:
    label=c['case'];directory=o/('export-'+label)
    assert c['source_files_unchanged'] and c['no_server_cache_created']
    assert json.loads((o/('export-'+label+'-exit.json')).read_bytes())['returncode']==c['returncode']
    assert not list(directory.glob('data/*/server.json'))
    if c['returncode']==0:
        for p in (fixture/'project').rglob('*'):
            if not p.is_file() or 'qupathxx-exchange' in p.parts:continue
            relative=p.relative_to(fixture/'project')
            if label=='optional-metadata' and relative.as_posix() in ('classifiers/classes.json','data/metadata.json'):
                assert not (directory/relative).exists()
            else:assert digest(directory/relative)==digest(p)
    else:assert not (directory/'qupathxx-exchange').exists()
for p in (fixture/'project/qupathxx-exchange').iterdir():assert digest(o/'export-readonly/qupathxx-exchange'/p.name)==digest(p)
optional=json.loads((o/'export-optional-metadata/qupathxx-exchange/index.json').read_bytes())
assert optional['files']=={'classifiers/classes.json':None,'data/metadata.json':None}
codec=json.loads((r/'build/tiff-jpeg-r2/validation.json').read_bytes())
for product in codec['products']:
    if product['cache_path'].endswith('.lib'):assert digest(cache/product['cache_path'])==product['sha256']
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/_native.cp312-win_amd64.pyd','Release/qupathxx_qupath_project_tests.exe','Release/qupathxx_project_tests.exe','Release/qupathxx_interchange_tests.exe','qupathxx_core.dir/Release/qupath_project.obj','qupathxx_core.dir/Release/provenance.obj'):
    source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
summary=dict(native_source=inputs['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),source_fingerprint=fingerprint,compiler_inputs=len(inputs['files']),installed_members=len(members),native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),model_passes=242,model_declared_skips=sum(c.find('skipped') is not None for c in cohorts['model']),additional_targeted_passes=2,distinct_new_workflow_passes=len(new),distinct_desktop_passes=len(desktop),native_suites=3,baseline_missing_api_failures=1,initial_desktop_test_failures=1,native_build_attempts=2,successful_native_builds=1,final_product_installs=1,intended_typing_errors=2,installed_stub_check=True,record_verified=True,stub_repackage_native_unchanged=True,prior_codec_archives_unchanged=True,full_platform_campaign=False,export_controls=controls,ui_capture_reviewed='import-final.png: two images and nested parent/child outlines visible',products=retained)
(o/'validation.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
