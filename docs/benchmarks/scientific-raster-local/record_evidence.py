from pathlib import Path
import datetime, hashlib, json, re, shutil, subprocess, xml.etree.ElementTree as ET

root=Path('C:/Users/HEQ/Projects/qupathxx')
source=root/'build/ci/scientific-grid-159'
build=root/'build/ci/raster163-native'
out=source/'docs/benchmarks/scientific-raster-local'
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
for name in ('raster163-qualified-build.log','raster163-qualified-tests.log','raster163-qualified-tests.xml','raster163-contract-tests.xml'):
    shutil.copyfile(root/'build'/name,out/name)
shutil.copyfile(build/'generated/build_manifest.hpp',out/'build_manifest.hpp')
shutil.copyfile(build/'CMakeCache.txt',out/'CMakeCache.txt')
native=list(ET.parse(out/'raster163-qualified-tests.xml').getroot().iter('testcase'))
pure=list(ET.parse(out/'raster163-contract-tests.xml').getroot().iter('testcase'))
for cases,count in ((native,49),(pure,23)):
    assert len(cases)==count
    assert all(c.find('failure') is None and c.find('error') is None and c.find('skipped') is None for c in cases)
text=(out/'raster163-qualified-tests.xml').read_text(encoding='utf-8')
markers=['Native explicit-grid raster eligibility, per-axis metadata, precision and legacy isolation passed','GEOS explicit-grid edges, ROI holes and anisotropic areas passed']
assert all(marker in text for marker in markers)
cache=(out/'CMakeCache.txt').read_text(encoding='utf-8')
configuration={}
for key in ('PYTHON','GUI','PROJECT','INTERCHANGE','GEOS','OPENSLIDE','TIFF','TRAINING','TESTS'):
    found=re.search(r'^QUPATHXX_BUILD_'+key+r':BOOL=(ON|OFF)$',cache,re.M)
    if found:configuration[key.lower()]=found[1]=='ON'
assert configuration['geos'] and not configuration['python'] and not configuration['gui']
receipt=dict(schema='qpx.scientific-raster-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source=revision,
    compiled_revision=re.search(r'git_revision\[\]="([^"]+)"',configured)[1],source_tree_sha256=fingerprint,compiler_inputs=len(inputs),
    compiler='MSVC 19.44.35225.0',platform='Windows AMD64',configuration=configuration,native_suites_passed=len(native),independent_contract_tests_passed=len(pure),required_semantic_markers=markers,
    explicit_grid_raster_native_verified=True,decoded_snapshot_hash_single_read_verified=True,geos_explicit_grid_mapping_verified=True,
    installed_python_verified=False,cell_request_recipe_implemented=False,new_cell_archive_implemented=False,scientific_detector_accepted=False,platform_campaign_accepted=False,release_ready=False,
    scope='Native shared raster construction, full per-axis metadata, eligible core labels, one-read canonical input identity and GEOS contour mapping. Legacy natural-grid paths and scalar archive guards retain their contract.',
    remaining=['explicit-grid cell request and typed physical parameters','physical LoG/shape/body detector integration','QCL5/QCA5 record, reopen and replay','Python/CLI/ImGui/batch integration','scientific fixtures/tolerances and all remaining M0-M7 gates'])
receipt['source_proof_note']='Compiled before source commit; configured dirty revision retained verbatim. All compiler inputs match this source commit modulo recorded Git line endings and exactly match the configured raw-byte source fingerprint. No binary rebuilt solely to change its Git label.'
receipt['native_artifacts']={p.name:dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted((build/'Release').iterdir()) if p.suffix=='.dll' or p.name in ('qupathxx_tissue_contour_tests.exe','qupathxx_tissue_record_tests.exe','qupathxx_tissue_tests.exe')}
receipt['evidence']={p.name:dict(bytes=p.stat().st_size,sha256=sha(p.read_bytes())) for p in sorted(out.iterdir()) if p.is_file()}
(source/'docs/benchmarks/scientific-raster-local-windows.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode())
print(json.dumps({k:receipt[k] for k in ('source','source_tree_sha256','compiler_inputs','native_suites_passed','independent_contract_tests_passed')},indent=2))
