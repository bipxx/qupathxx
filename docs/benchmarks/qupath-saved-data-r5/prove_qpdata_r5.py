from pathlib import Path
import base64,csv,hashlib,io,json,shutil,subprocess,xml.etree.ElementTree as ET,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/qupath-saved-data-r5';cache=r/'build/ci/gridfrontends167-package'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs=json.loads((o/'compiler-inputs.json').read_bytes());env=q.build_environment()
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in sorted(inputs['files'].items())).encode()).hexdigest()
assert env.git_revision==inputs['source'] and env.source_tree_sha256==fingerprint
assert all(digest(w/p)==h for p,h in inputs['files'].items())
assert json.loads((o/'build-exit.json').read_bytes())['returncode']==0
cohorts={name:ET.parse(o/(name+'.xml')).findall('.//testcase') for name in ('before','model','native','model-final','desktop','observer')}
passed=lambda c:all(c.find(k) is None for k in ('failure','error','skipped'))
assert len(cohorts['before'])==2 and all(c.find('failure') is not None for c in cohorts['before'])
assert sum(passed(c) for c in cohorts['model'])==251 and sum(c.find('failure') is not None for c in cohorts['model'])==2
assert len(cohorts['model-final'])==2 and all(passed(c) for c in cohorts['model-final'])
assert len(cohorts['desktop'])==2 and all(passed(c) for c in cohorts['desktop'])
assert len(cohorts['observer'])==6 and all(passed(c) for c in cohorts['observer'])
merged={}
for name in ('model','model-final','desktop','observer'):
    for c in cohorts[name]:merged[(c.get('classname'),c.get('name'))]=c
assert all(c.find('failure') is None and c.find('error') is None for c in merged.values())
direct={k:c for k,c in merged.items() if k[0]=='tests.test_qupath_saved_data' or (k[0]=='tests.test_qupath_project' and k[1].startswith('test_native_saved_data'))}
assert len(direct)==12 and all(passed(c) for c in direct.values())
native=cohorts['native'];assert {c.get('name') for c in native}=={'qupath_project','project','interchange'} and all(passed(c) for c in native)
assert 'Native QuPath saved-data geometry, calibration, truncation, trailing bytes and cancellation passed' in ''.join(c.findtext('system-out','') for c in native)
assert json.loads((o/'stubs-command.json').read_bytes())['returncode']==0
wheel,=list((o/'wheel').glob('*.whl'));site=Path(q.__file__).parent.parent;assert 'qupath-saved-data-r5-env' in str(site)
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
package=json.loads((o/'package.json').read_bytes());assert package['final_product_installs']==1 and not package['stub_repackaging']
assert package['wheel_sha256']==digest(wheel)
prior=json.loads((r/'build/qupath-project-r5/compiler-inputs-final.json').read_bytes())
unchanged_public=[p for p in prior['files'] if p.startswith('cpp/include/') or p.endswith('bindings.cpp') or p.startswith('src/')]
assert all(inputs['files'][p]==prior['files'][p] for p in unchanged_public)
fixture=w/'tests/fixtures/qupath-saved-data-0.7.0';manifest=json.loads((fixture/'manifest.json').read_bytes())
for name,h in manifest['files'].items():assert digest(fixture/name)==h
for name,h in manifest['probes'].items():assert digest(w/name)==h
assert json.loads((o/'capture-exit.json').read_bytes())['returncode']==0
codec=json.loads((r/'build/tiff-jpeg-r2/validation.json').read_bytes())
for product in codec['products']:
    if product['cache_path'].endswith('.lib'):assert digest(cache/product['cache_path'])==product['sha256']
products=o/'products';products.mkdir(exist_ok=False);retained=[]
for relative in ('Release/_native.cp312-win_amd64.pyd','Release/qupathxx_qupath_project_tests.exe','Release/qupathxx_project_tests.exe','Release/qupathxx_interchange_tests.exe','qupathxx_core.dir/Release/qupath_saved_data.obj','qupathxx_core.dir/Release/provenance.obj'):
    source=cache/relative;target=products/source.name;shutil.copyfile(source,target);retained.append(dict(path=target.relative_to(o).as_posix(),cache_path=relative,sha256=digest(target)))
summary=dict(native_source=inputs['source'],observer_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=w,text=True).strip(),source_fingerprint=fingerprint,compiler_inputs=len(inputs['files']),installed_members=len(members),native_sha256=digest(Path(_native.__file__)),wheel_sha256=digest(wheel),initial_model_passes=251,initial_model_failures=2,initial_model_declared_skips=5,corrected_model_passes=2,distinct_direct_workflow_passes=len(direct),distinct_selected_passes=sum(passed(c) for c in merged.values()),remaining_declared_skips=[k for k,c in merged.items() if c.find('skipped') is not None],desktop_passes=2,observer_control_passes=6,native_suites=3,baseline_missing_decoder_failures=2,native_build_attempts=1,successful_native_builds=1,final_product_installs=1,installed_stub_check=True,record_verified=True,public_headers_bindings_and_python_unchanged=True,typing_reused='qupath-project-r5: unchanged public signatures; current generated-stub check passes',prior_codec_archives_unchanged=True,full_platform_campaign=False,ui_capture_reviewed='direct-import.png: nested ellipse, line, points, polygon/holes, detection and cell/nucleus outlines visible; supplementary glyph appearance is not qualified',products=retained)
(o/'validation.json').write_bytes((json.dumps(summary,indent=2)+'\n').encode());print(json.dumps({k:v for k,v in summary.items() if k!='products'},indent=2))
