from pathlib import Path
import hashlib,importlib.util,json,re,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((r/'build/raw175-source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source
spec=importlib.util.spec_from_file_location('raw175_audit',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert audit.source_contract(root,source)==proof['files']
folder=root/'docs/benchmarks/raw-scientific-digest-local';folder.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name):shutil.copyfile(p,folder/name)
platforms={}
for side,cache in [('windows','gridfrontends167-package'),('linux','raw174-linux-native')]:
 out=r/('build/raw175-'+side);commands=json.loads((out/'commands.json').read_text(encoding='utf-8'))
 assert commands and all(c['exit']==0 for c in commands)
 cases=list(ET.parse(out/'native-tests.xml').getroot().iter('testcase'));assert len(cases)==1 and cases[0].get('name')=='resampling'
 assert all(cases[0].find(tag) is None for tag in ('failure','error','skipped'))
 lines=cases[0].findtext('system-out').splitlines()
 for marker in audit.BASE['resampling']:assert lines.count(marker)==1
 manifest=r/'build/ci'/cache/'generated/build_manifest.hpp'
 text=manifest.read_text(encoding='utf-8');fingerprint=re.search(r'source_sha256\[\]="([0-9a-f]{64})"',text).group(1)
 assert lines.count('Typed raw sampling engine identity '+fingerprint+' '+source)==1
 binary=r/'build/ci'/cache/('Release/qupathxx_resampling_tests.exe' if side=='windows' else 'qupathxx_resampling_tests')
 platforms[side]={'source':source,'source_tree_sha256':fingerprint,'selected_native_suite':'resampling','executable_sha256':sha(binary),'commands':commands}
 for name in ['commands.json','build.log','tests.log','native-tests.xml']:
  copy(out/name,side+'-'+name)
 if (out/'configure.log').exists():copy(out/'configure.log',side+'-configure.log')
 copy(manifest,side+'-build_manifest.hpp');copy(r/'build/ci'/cache/'CMakeCache.txt',side+'-CMakeCache.txt')
assert platforms['windows']['source_tree_sha256']==platforms['linux']['source_tree_sha256']
san=r/'build/raw175-sanitize-final'
assert all(c['exit']==0 for c in json.loads((san/'commands.json').read_text(encoding='utf-8')))
assert audit.BASE['resampling'][-1] in (san/'tests.log').read_text(encoding='utf-8').splitlines()
for name in ['commands.json','build.log','tests.log']:copy(san/name,'selective-sanitizer-'+name)
cases=list(ET.parse(r/'build/raw175-python.xml').getroot().iter('testcase'));assert len(cases)==85 and all(c.find(t) is None for c in cases for t in ('failure','error','skipped'))
copy(r/'build/raw175-python.xml','python-tests.xml');copy(r/'build/raw175-source-proof.json','source-proof.json')
for name in ['build_raw175_windows.py','build_raw175_linux.py','sanitize_raw175_final.py','record_raw175.py']:copy(r/'build/ci'/name,name)
copy(r/'build/raw174-sanitize-driver.cpp','selective-sanitizer-driver.cpp')
record={'schema':'qpx.scientific-raw-digest-native-local/1','source':source,'source_contract_files':len(proof['files']),'platforms':platforms,
 'reference_cases_per_platform':94,'channel_results_per_platform':3732,'hash_only_hash_pairs_per_platform':94,'python_passed':85,
 'checks':['All ten source dtypes and unchanged independent hashes','Shared malformed-reader validation','Pre-IO budget and cancellation rejection','Hash-only budget excludes output arrays','Metadata epoch captured before decoder callbacks','Legacy exact mean and RGB/area references'],
 'sanitizer_scope':'New raw capture/sampler and fake-reader/exact-mean tests instrumented; inherited linked core remains unsanitized at c4a9a21 and reports that older identity. Not full application sanitizer acceptance.',
 'native_hash_only_implemented':True,'selected_native_resampling_accepted':True,'raw_analysis_workflow_accepted':False,'actual_tiff_sampling_accepted':False,'installed_python_api_accepted':False,'full_native_suite_accepted':False,'gui_accepted':False,'platform_campaign_accepted':False,'release_ready':False,'overall_project_complete':False,
 'cache_state':'Windows core/resampling current; other executables/module older. Linux minimal core/resampling current; selective sanitizer linked older unsanitized core.',
 'remaining':['Explicit grid RawAnalysisRequest/result/jobs','QRA2 retention, project reopen/replay and CSV','Grouped bindings/stubs and CLI/batch/ImGui integration','Actual TIFF-backed sampling tests','Complete installed/native/platform qualification','All remaining original M0-M7 criteria'],
 'evidence':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(folder.iterdir())}}
(root/'docs/benchmarks/raw-scientific-digest-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps({'source':source,'fingerprint':platforms['windows']['source_tree_sha256'],'evidence_files':len(record['evidence'])}))
