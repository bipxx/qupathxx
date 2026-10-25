from pathlib import Path
import collections,datetime,hashlib,json,re,shutil,subprocess,xml.etree.ElementTree as ET
root=Path(r'C:/Users/HEQ/Projects/qupathxx');source=root/'build/ci/scientific-grid-159';build=root/'build/ci/gridfrontends167-package'
out=source/'docs/benchmarks/grid-cell-frontends-local';out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
revision='d31ceb786faaa818d0b8ae1a3467dd96fbb55555'
subprocess.run(['git','merge-base','--is-ancestor',revision,'HEAD'],cwd=source,check=True)
inputs=sorted(set([p for pattern in ('cpp/**/*.cpp','cpp/**/*.hpp','cmake/**/*.cmake','cmake/**/*.in','src/**/*.py') for p in source.glob(pattern)]+[source/'CMakeLists.txt',source/'pyproject.toml']),key=lambda p:p.as_posix())
requests=''.join(f'{revision}:{p.relative_to(source).as_posix()}\n' for p in inputs).encode()
raw=subprocess.run(['git','cat-file','--batch'],input=requests,cwd=source,stdout=subprocess.PIPE,check=True).stdout
offset=0;manifest='';proof=[]
for p in inputs:
    end=raw.index(b'\n',offset);header=raw[offset:end].decode().split();assert header[1]=='blob',header
    size=int(header[2]);blob=raw[end+1:end+1+size];assert raw[end+1+size:end+2+size]==b'\n';offset=end+2+size
    actual=p.read_bytes();assert actual==blob or actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),p
    name=p.relative_to(source).as_posix();digest=sha(actual);manifest+=f'{name}={digest}\n'
    proof.append(dict(path=name,checkout_sha256=digest,git_blob_sha256=sha(blob),line_endings_only=actual!=blob))
assert offset==len(raw);fingerprint=sha(manifest.encode());configured=(build/'generated/build_manifest.hpp').read_text(encoding='utf-8')
assert f'source_sha256[]="{fingerprint}"' in configured
(out/'compiler-input-proof.json').write_bytes((json.dumps(dict(source=revision,compiler_inputs=len(inputs),source_tree_sha256=fingerprint,files=proof),indent=2)+'\n').encode())
names=['gridfrontends167-focused-build.log','gridfrontends167-second-build.log','gridfrontends167-first-tests.xml','gridfrontends167-focused-tests.xml','gridfrontends167-first-package.log','gridfrontends167-final-package.log','gridfrontends167-package-native-tests.log','gridfrontends167-package-native-tests.xml','gridfrontends167-contract-tests.xml','gridfrontends167-first-integrated-tests.xml','gridfrontends167-first-regression-tests.xml','gridfrontends167-final-desktop-tests.xml','gridfrontends167-final-desktop-extra-tests.xml','gridfrontends167-valid-typing.log','gridfrontends167-all-invalid-typing.log','gridfrontends167-first-installed.json','gridfrontends167-final-installed.json','gridfrontends167-grid-desktop.png']
for name in names: shutil.copyfile(root/'build'/name,out/name)
for name in ('build_manifest.hpp',): shutil.copyfile(build/'generated'/name,out/name)
shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt');shutil.copyfile(__file__,out/'record_evidence.py')
shutil.copyfile(root/'build/gridfrontends167-cli-final/commands.json',out/'cli-commands.json')
shutil.copyfile(root/'build/ci/verify_gridfrontends167_cli.py',out/'verify_cli.py')
shutil.copyfile(root/'build/ci/build_gridfrontends167.py',out/'build_first.py');shutil.copyfile(root/'build/ci/build_gridfrontends167_final.py',out/'build_final.py')
def cases(name,passed,skipped=0):
    values=list(ET.parse(out/name).getroot().iter('testcase'))
    assert len(values)==passed+skipped,(name,len(values));assert all(c.find('failure') is None and c.find('error') is None for c in values),name
    assert sum(c.find('skipped') is not None for c in values)==skipped,name
    return values
native=cases('gridfrontends167-package-native-tests.xml',50);pure=cases('gridfrontends167-contract-tests.xml',33)
regression=cases('gridfrontends167-first-regression-tests.xml',224,20);desktop=cases('gridfrontends167-final-desktop-tests.xml',50);extra=cases('gridfrontends167-final-desktop-extra-tests.xml',3)
assert sum('test_grid_cell_frontends' in c.attrib.get('classname','') for c in desktop)==6
final_ids={(c.attrib['classname'],c.attrib['name']) for c in desktop+extra}
initial_skips=[c for c in regression if c.find('skipped') is not None]
resolved_skips=[c for c in initial_skips if (c.attrib['classname'],c.attrib['name']) in final_ids]
unresolved_skips=[c for c in initial_skips if (c.attrib['classname'],c.attrib['name']) not in final_ids]
assert len(resolved_skips)==13 and len(unresolved_skips)==7

markers=['Grid ROI planning, typed batch recipe, per-image calibration, interrupted resume and QCL5/CSV checkpoints passed','Grid viewer producer copies, owner calibration capture, retention, cancellation and stale delivery passed','Independent Python QCR1/QCA5 typed grid source bytes decode and canonical re-encode passed','QCL5 SQLite save, reopen, provenance attachment, replay and second save passed']
text=(out/'gridfrontends167-package-native-tests.xml').read_text(encoding='utf-8');assert all(m in text for m in markers)
positive=(out/'gridfrontends167-valid-typing.log').read_text(encoding='utf-8-sig');assert 'Success: no issues found in 5 source files' in positive
negative=(out/'gridfrontends167-all-invalid-typing.log').read_text(encoding='utf-8-sig')
errors=collections.Counter((m[0],int(m[1])) for m in re.findall(r'(\w+\.py):(\d+): error:',negative))
expected=collections.Counter({(name,line):1 for name,lines in {'invalid_api.py':[6,7,8,9,11,13,14],'qptma_invalid.py':[4,5,6],'hierarchy_invalid.py':[4,5,7],'scientific_grid_invalid.py':[4,6,7,8,9,10],'scientific_channels_invalid.py':[4,6,7,8,9],'grid_cells_invalid.py':[3,4,5,6,7,8]}.items() for line in lines});expected[('grid_cells_invalid.py',3)]=2
assert errors==expected,(errors,expected)
installed=json.loads((out/'gridfrontends167-final-installed.json').read_text(encoding='utf-8'));assert installed['source_tree_sha256']==fingerprint and installed['native_bytes_unchanged_after_stub_packaging'] and installed['packaged_sources_and_stubs_match']
commands=json.loads((out/'cli-commands.json').read_text(encoding='utf-8'));assert len(commands)==12 and sum(c['exit']==2 for c in commands)==8
cache=(out/'CMakeCache.txt').read_text(encoding='utf-8');configuration={}
for key in ('PYTHON','GUI','PROJECT','INTERCHANGE','GEOS','OPENSLIDE','TIFF','CLASSIFIER_TRAINING','TESTS'):
    found=re.search(r'^QUPATHXX_BUILD_'+key+r':BOOL=(ON|OFF)$',cache,re.M)
    if found:configuration[key.lower()]=found[1]=='ON'
assert all(configuration[k] for k in ('python','gui','project','geos','tests'))
receipt=dict(schema='qpx.grid-cell-frontends-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=revision,compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],source_tree_sha256=fingerprint,compiler_inputs=len(inputs),configuration=configuration,platform='Windows AMD64',compiler='MSVC 19.44.35225.0',native_suites_passed=50,independent_contract_tests_passed=33,installed_regression_passed=224,installed_regression_initial_skips=20,final_desktop_cohort_passed=50,final_additional_desktop_passed=3,new_grid_python_workflow_cases=6,positive_typing_files=5,expected_negative_typing_errors=31,typing_mode='mypy --strict --no-incremental',required_semantic_markers=markers,installed=installed,scientific_detector_accepted=False,platform_campaign_accepted=False,release_ready=False,
    scope='Count-based ROI capture across CLI, batch, managed viewer and ImGui; typed Python metadata ownership, QCL5 retention/replay, classifier-source lifetime, per-axis preview/publication, checkpoint resume and CSV export. Synthetic 6x9 RGB scene; engineering evidence only.',
    reuse='Regression and first integrated cases used the first wheel. Final stub-packaged wheel has exactly the same native module hash and source fingerprint; final sources/stubs match and regenerate from that installed extension. Final desktop/typing cases ran after reinstall.',
    source_proof_note='Compiled before source commit. Dirty compiled revision is retained verbatim; committed compiler inputs agree modulo recorded Git line endings and exactly match the configured raw-byte fingerprint. All package native targets were built after the final compiler-input edit. No rebuild solely for a Git label.',
    initial_regression_skips=[dict(name=c.attrib['name'],reason=c.find('skipped').attrib.get('message','')) for c in regression if c.find('skipped') is not None],
    remaining=['Complete frozen exact-source platform cohorts','Disabled OpenSlide/pyramid, interchange and training configurations','Scientific labels/tolerances, hardware/performance, rights/licensing and all remaining M0-M7 requirements'])
receipt['initial_skips_executed_in_final_desktop']=13
receipt['remaining_capability_skips']=7
receipt['native_artifacts']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix in ('.dll','.pyd') or (p.name.startswith('qupathxx_') and p.suffix=='.exe')}
receipt['sqlite_dependency']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in (root/'build/deps/sqlite-amalgamation-3540100').glob('sqlite3.[ch]')}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/grid-cell-frontends-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps({k:receipt[k] for k in ('source','source_tree_sha256','compiler_inputs','native_suites_passed','installed_regression_passed','final_desktop_cohort_passed')},indent=2))
