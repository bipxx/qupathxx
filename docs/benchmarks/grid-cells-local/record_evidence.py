from pathlib import Path
import datetime, hashlib, json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path('C:/Users/HEQ/Projects/qupathxx')
source=root/'build/ci/scientific-grid-159'
build=root/'build/ci/gridcells164-native'
out=source/'docs/benchmarks/grid-cells-local'
out.mkdir(exist_ok=True)
sha=lambda b:hashlib.sha256(b).hexdigest()
revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
inputs=sorted(set([p for pattern in ('cpp/**/*.cpp','cpp/**/*.hpp','cmake/**/*.cmake','cmake/**/*.in','src/**/*.py') for p in source.glob(pattern)]+[source/'CMakeLists.txt',source/'pyproject.toml']),key=lambda p:p.as_posix())
requests=''.join(f'{revision}:{p.relative_to(source).as_posix()}\n' for p in inputs).encode()
proc=subprocess.run(['git','cat-file','--batch'],input=requests,cwd=source,stdout=subprocess.PIPE,check=True)
raw=proc.stdout;offset=0;manifest='';proof=[]
for path in inputs:
    stop=raw.index(b'\n',offset);header=raw[offset:stop].decode().split();assert header[1]=='blob',header
    size=int(header[2]);blob=raw[stop+1:stop+1+size];assert raw[stop+1+size:stop+2+size]==b'\n'
    offset=stop+2+size
    actual=path.read_bytes();assert actual==blob or actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),path
    name=path.relative_to(source).as_posix();digest=sha(actual);manifest+=f'{name}={digest}\n'
    proof.append(dict(path=name,checkout_sha256=digest,git_blob_sha256=sha(blob),line_endings_only=actual!=blob))
assert offset==len(raw)
fingerprint=sha(manifest.encode())
configured=(build/'generated/build_manifest.hpp').read_text(encoding='utf-8')
assert f'source_sha256[]="{fingerprint}"' in configured,fingerprint
(out/'compiler-input-proof.json').write_bytes((json.dumps(dict(source=revision,compiler_inputs=len(inputs),source_tree_sha256=fingerprint,files=proof),indent=2)+'\n').encode())
for name in ('gridcells164-qualified-build.log','gridcells164-qualified-tests.log','gridcells164-qualified-tests.xml','gridcells164-contract-tests.xml',
             'gridcells164-first-cell-tests.log','gridcells164-first-recipe.hpp','gridcells164-corrected-cell-tests.log'):
    shutil.copyfile(root/'build'/name,out/name)
shutil.copyfile(build/'generated/build_manifest.hpp',out/'build_manifest.hpp')
shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt')
shutil.copyfile(__file__,out/'record_evidence.py')
native=list(ET.parse(out/'gridcells164-qualified-tests.xml').getroot().iter('testcase'))
pure=list(ET.parse(out/'gridcells164-contract-tests.xml').getroot().iter('testcase'))
for cases,count in ((native,49),(pure,27)):
    assert len(cases)==count
    assert all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases)
markers=['Native grid cell request, float64 LoG, metric shape/body, calibrated features and one-read detection passed',
         '32 exact metric LoG signs and 128 rational-distance body expansion references passed',
         'GEOS explicit-grid edges, ROI holes and anisotropic areas passed']
text=(out/'gridcells164-qualified-tests.xml').read_text(encoding='utf-8');assert all(m in text for m in markers)
cache=(out/'CMakeCache.txt').read_text(encoding='utf-8')
configuration={}
for key in ('PYTHON','GUI','PROJECT','INTERCHANGE','GEOS','OPENSLIDE','TIFF','TRAINING','TESTS'):
    found=re.search(r'^QUPATHXX_BUILD_'+key+r':BOOL=(ON|OFF)$',cache,re.M)
    if found:configuration[key.lower()]=found[1]=='ON'
assert configuration['geos'] and not configuration['python'] and not configuration['gui']
receipt=dict(schema='qpx.grid-cells-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=revision,
    compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],source_tree_sha256=fingerprint,compiler_inputs=len(inputs),
    compiler='MSVC 19.44.35225.0',platform='Windows AMD64',configuration=configuration,native_suites_passed=len(native),independent_contract_tests_passed=len(pure),required_semantic_markers=markers,
    exact_curvature_cases=32,exact_expansion_cases=128,native_grid_cell_detection_verified=True,one_read_detection_verified=True,
    complete_pre_io_request_admission_verified=True,installed_python_verified=False,new_cell_archive_implemented=False,
    scientific_detector_accepted=False,platform_campaign_accepted=False,release_ready=False,
    scope='Native explicit-grid capture and typed units, float64 channel/LoG path, physical shape/seed metrics, strict-radius watershed bodies, per-axis masks/features/GEOS contours and existing CellJob lifecycle. Legacy recipes and QCL1-4 retain their tests.',
    remaining=['QCL5/QCA5 full-grid typed-parameter record, reopen/replay/verification','Python/CLI/ImGui/batch exposure and installed rebuild','source-bound platform cohorts','biological fixtures/rights/tolerances and all remaining M0-M7 requirements'])
receipt['source_proof_note']='Compiled before source commit; configured dirty revision retained verbatim. All compiler inputs match the committed source modulo recorded Git line endings and exactly match its configured raw-byte compiler-input fingerprint. No binary rebuilt solely to change its Git label.'
receipt['retained_development_failures']=[
    dict(log='gridcells164-first-cell-tests.log',reason='New physical EDT call used the legacy finite infinity sentinel. Fixed both new fields to positive IEEE infinity; retained initial recipe snapshot. The strict physical EDT contract was not weakened.'),
    dict(log='gridcells164-corrected-cell-tests.log',reason='Test contour budget equalled 65536 bytes, below the base ROI/component overhead. Corrected test budget and added pre-IO rejection for an impossible contour base budget.')]
receipt['native_artifacts']={p.name:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix=='.dll' or (p.name.startswith('qupathxx_') and p.name.endswith('_tests.exe'))}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/grid-cells-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps({k:receipt[k] for k in ('source','source_tree_sha256','compiler_inputs','native_suites_passed','independent_contract_tests_passed')},indent=2))
