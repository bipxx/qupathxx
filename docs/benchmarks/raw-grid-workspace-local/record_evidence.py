from pathlib import Path
import datetime,hashlib,importlib.util,json,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];source=r/'build/ci/scientific-grid-159';revision='326c66c15d13631d8f46104c7f41f2c8cb13f951';delivery='5e3c65bef76ed4b655489c2cb2bfc1d980861a8f';fingerprint='6b3c4fb14e290508947e7c8ec71394221a9eb1952ea1b12861f719d70b660261'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==delivery
dirty=subprocess.check_output(['git','status','--porcelain'],cwd=source).splitlines()
allowed={b'?? docs/benchmarks/raw-grid-workspace-local/',b'?? docs/benchmarks/raw-grid-workspace-local-windows-linux.json'}
assert all(line in allowed for line in dirty),dirty
out=source/'docs/benchmarks/raw-grid-workspace-local';out.mkdir(exist_ok=True);manifest={}
def copy(path,name):
 raw=path.read_bytes();target=out/name;target.parent.mkdir(parents=True,exist_ok=True);assert not target.exists() or target.read_bytes()==raw,name;target.write_bytes(raw);manifest[name]=dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest());return raw
def folder(path,name):
 for p in sorted(path.iterdir()):
  if p.is_file():copy(p,name+'/'+p.name)
def cases(path,passed,skipped=0,failed=0):
 values=list(ET.parse(path).getroot().iter('testcase'));assert len(values)==passed+skipped+failed,(path,len(values))
 assert sum(c.find('skipped') is not None for c in values)==skipped
 assert sum(any(c.find(k) is not None for k in ['failure','error']) for c in values)==failed
 return values
spec=importlib.util.spec_from_file_location('evidence',source/'tools/ci/scientific_grid_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
for mode,dirname,names,geos in [
 ('windows','raw184-native',{n:n+'.xml' for n in ['resampling','cells','raw_image','viewer_session','project_batch']},True),
 ('tiff-linux','raw184-tiff-linux',dict(resampling='native-tests.xml',cells='cells-tests.xml',raw_image='raw-image-tests.xml',viewer_session='viewer-session-tests.xml',project_batch='project-batch-tests.xml'),False)]:
 path=r/'build'/dirname;commands=json.loads((path/'commands.json').read_text(encoding='utf-8'));assert len(commands)==7 and all(c['exit']==0 for c in commands)
 flags=dict(project=True,no_project=False,geos=geos,no_geos=not geos,both=geos,tiff=True,raw_project=True)
 for suite,name in names.items():
  value=cases(path/name,1)[0];assert value.get('name')==suite;lines=value.findtext('system-out','').splitlines()
  for marker in m.BASE.get(suite,[]):assert lines.count(marker)==1,(mode,suite,marker)
  for required_suite,flag,marker in m.CONDITIONAL:
   if suite==required_suite:assert lines.count(marker)==int(flags[flag]),(mode,suite,marker)
  if suite=='resampling':assert 'Typed raw sampling engine identity '+fingerprint+' '+revision in lines
 folder(path,mode)
 cache=r/('build/ci/gridfrontends167-package' if mode=='windows' else 'build/ci/raw176-tiff-linux-native')
 for name in ['CMakeCache.txt','generated/build_manifest.hpp']:copy(cache/name,mode+'/'+Path(name).name)
 assert fingerprint in (cache/'generated/build_manifest.hpp').read_text(encoding='utf-8')
 for p in (r/('build/raw183-native' if mode=='windows' else 'build/raw183-tiff-linux')).iterdir():
  if p.is_file():copy(p,'rejected-lazy-identity/'+mode+'/'+p.name)
 rejected=cases(r/('build/raw183-native/raw_image.xml' if mode=='windows' else 'build/raw183-tiff-linux/raw-image-tests.xml'),0,0,1)[0]
 assert 'raw analysis source has no observed file identity' in rejected.findtext('system-out','')
folder(r/'build/raw184-windows-focus','focused-native')
proof=json.loads(copy(r/'build/raw184-compiler-input-proof.json','compiler-input-proof.json'));assert proof['source']==revision and proof['compiler_inputs']==311
for name,digest in proof['files'].items():assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
copy(r/'build/raw184-source-proof.json','compiled-source-proof.json');delivery_proof=json.loads(copy(r/'build/raw185-delivery-source-proof.json','delivery-source-proof.json'));assert delivery_proof['source']==delivery and len(delivery_proof['files'])==493
assert m.source_contract(source,delivery)==delivery_proof['files']
scientific=cases(r/'build/raw185-scientific-final/scientific-tests.xml',218);scientific_scope=m.python(scientific,True)
folder(r/'build/raw185-scientific-final','scientific');folder(r/'build/raw185-scientific','scientific-before-context-case')
cases(r/'build/raw184-scientific/scientific-tests.xml',211,0,6);folder(r/'build/raw184-scientific','rejected-live-contract')
cases(r/'build/raw185-runtime/python-tests.xml',143,3);folder(r/'build/raw185-runtime','runtime')
cases(r/'build/raw184-desktop/desktop-tests.xml',33);folder(r/'build/raw184-desktop','desktop')
cases(r/'build/raw185-pure-tests.xml',120);copy(r/'build/raw185-pure-tests.xml','pure-tests.xml')
folder(r/'build/raw184-typing','typing');typing=m.typing(r/'build/raw184-typing')
folder(r/'build/raw184-python-isolated','stubs-and-identity')
san=r/'build/raw185-sanitize';san_commands=json.loads((san/'commands.json').read_text(encoding='utf-8'));assert len(san_commands)==2 and all(c['exit']==0 for c in san_commands)
san_text=(san/'tests.log').read_text(encoding='utf-8')
# This driver runs geometry/raw tests, not the RGB sampler test function.
rgb_marker='Native scientific RGB float64 exact means, support, reader and cancellation checks passed'
assert san_text.splitlines().count(rgb_marker)==0
for marker in m.BASE['resampling']:
 if marker!=rgb_marker:assert san_text.splitlines().count(marker)==1,marker
assert 'Typed raw sampling engine identity '+fingerprint+' '+revision in san_text
folder(san,'selective-sanitizer');copy(r/'build/raw185-sanitize-driver.cpp','selective-sanitizer/driver.cpp')
# Preserve every timing sample; source identities and the shared fixture are explicit.
before=json.loads(copy(r/'build/raw183-owner-before/results.json','owner-before.json'));after=json.loads(copy(r/'build/raw183-owner-after/results.json','owner-after.json'))
capture_before=json.loads(copy(r/'build/raw183-capture-before.json','capture-before.json'));capture_after=json.loads(copy(r/'build/raw183-capture-after.json','capture-after.json'))
assert before['compiled_source']==capture_before['compiled_source']=='8ece80e52f1b93bd62e19f0c4cc5215d6357fd68'
assert after['compiled_source']==capture_after['compiled_source']==revision
assert [v['samples'] for v in before['rows']]==[v['samples'] for v in after['rows']]
assert [v['samples'] for v in capture_before['rows']]==[v['samples'] for v in capture_after['rows']]
copy(r/'build/raw183-owner-before/uniform.tif','owner-fixture.tif')
summary=[dict(samples=b['samples'],median_keep_before_ms=b['median_keep_ns']/1e6,median_keep_after_ms=a['median_keep_ns']/1e6) for b,a in zip(before['rows'],after['rows'])]
capture_summary=[dict(samples=b['samples'],median_capture_before_ms=b['median_capture_ns']/1e6,median_capture_after_ms=a['median_capture_ns']/1e6) for b,a in zip(capture_before['rows'],capture_after['rows'])]
copy(r/'build/ci/source182-before-raw183-native/inventory.json','prior-artifact-inventory.json');copy(r/'build/ci/rejected-raw183-before-raw184/inventory.json','rejected-artifact-inventory.json')
for name in ['build_raw184_windows.py','build_raw184_tiff_linux.py','build_raw184_focus.py','stage_raw184_python.py','test_raw185_runtime.py','test_raw185_scientific_final.py','test_raw184_desktop.py','check_raw184_typing.py','sanitize_raw185.py','measure_raw183_before.py','measure_raw183_after.py','measure_raw183_capture_before.py','measure_raw183_capture_after.py']:copy(r/'build/ci'/name,'runners/'+name)
# Frozen oracle bytes remain the exact qualified source169 contract, not a new expected hash.
frozen=source/'tests/fixtures/scientific-grid-v1/contract.md';assert hashlib.sha256(frozen.read_bytes()).hexdigest()=='d74f202495ca3bd6df88fe543dce5c597778c65ece006326d3097e2f49e77bbd'
old=json.loads(subprocess.check_output(['git','show','8ece80e:tests/fixtures/raw-grid-archive-v1/reference.json'],cwd=source));new=json.loads((source/'tests/fixtures/raw-grid-archive-v1/reference.json').read_text(encoding='utf-8'))
for b,a in zip(old['cases'],new['cases']):assert all(b[key]==a[key] for key in ['bytes_hex','sha256','input_sha256'])
package=r/'build/ci/raw183-python-env/Lib/site-packages/qupathxx';inventory={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(package.iterdir()) if p.is_file()}
copy(r/'build/ci/record_raw185_initial.py','rejected-receipt-auditor.py')
copy(Path(__file__),'record_evidence.py')
receipt=dict(schema='qpx.raw-grid-workspace-local/1',observed_at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),compiled_source=revision,delivery_source=delivery,source_tree_sha256=fingerprint,compiler_inputs=311,compiled_source_contract_files=492,delivery_source_contract_files=493,
 compiler_input_reuse='All311compiler-input bytes equal tested326c66c; delivery changes fixture-contract references, documentation and one Python context-budget regression only. Module revision remains326c66c.',candidate_inventory=inventory,
 native_suites_per_configuration=5,native_configurations=['Windows GUI/TIFF/project/GEOS enabled','Linux TIFF/project enabled,GUI/GEOS disabled'],
 runtime_passed=143,runtime_declared_raw_desktop_skips=3,desktop_enabled_regression_cases_passed=33,scientific=scientific_scope,pure_checks_passed=120,typing=typing,
 selective_sanitizer='ASan/UBSan instrument sampler,analysis,records,archive,CSV,raster and selected raw/geometry/numeric tests; link matching unsanitized326c66c core/TIFF/project dependencies. Not a full sanitizer campaign.',
 roi_context_workspace_pre_pixel_io_checked=True,lazy_provenance_single_metadata_epoch_checked=True,private_eligible_count_authority_checked=True,qra1_qra2_golden_bytes_unchanged=True,historical_low_work_budget_readable=True,
 coordinate_precision_pointwise_oracle_grids=1092,frozen_contract_source='3fc3bcb460ce590d9f1a0c48d97ca8a6dc400062',frozen_contract_sha256=hashlib.sha256(frozen.read_bytes()).hexdigest(),
 owner_keep_observations=summary,owner_capture_observations=capture_summary,performance_scope='Seven observations per configuration on a shared unreserved workstation; all raw samples retained. No controlled speedup ratio or application/hardware latency acceptance. Near precision boundaries retain the original scan.',
 rejected_observations=['Sourcec770baa passed synthetic resampling but both actual TIFF suites rejected lazy provenance queried before metadata initialization; corrected within the single-metadata capture.','Broader scientific run211pass/6fail: six old fixture manifests hashed the live expanding contract. Preserve exactly the original qualified169contract bytes and point manifests at that frozen snapshot; no numeric fixture/native-header or original hash was replaced.'],
 limits=['Conservative captured/work payload ledger; reader decode/cache,allocator/runtime overhead,process RSS and separately held caller results have separate budgets.','Selected local native and Windows installed/GUI evidence only; no full platform/wheel/installer campaign or Mac/Python/OpenSlide qualification.','No biological/scientific equivalence, hardware/UI budget or supported-release acceptance.'],release_ready=False,overall_project_complete=False,milestones={f'M{i}':'Partial' for i in range(8)},artifacts=manifest)
(source/'docs/benchmarks/raw-grid-workspace-local-windows-linux.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'))
print('Workspace receipt:',len(manifest),'payloads; scientific',scientific_scope,'runtime143+3skips,desktop33,pure120,typing6/4')
