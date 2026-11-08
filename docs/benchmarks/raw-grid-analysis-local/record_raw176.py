from pathlib import Path
from datetime import datetime,timezone
import hashlib,importlib.util,json,re,shutil,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];root=r/'build/ci/scientific-grid-159'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True,encoding='utf-8').strip()
proof=json.loads((r/'build/raw176-source-proof.json').read_text(encoding='utf-8'));assert proof['source']==source
spec=importlib.util.spec_from_file_location('raw176_audit',root/'tools/ci/scientific_grid_evidence.py');audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
assert audit.source_contract(root,source)==proof['files']
folder=root/'docs/benchmarks/raw-grid-analysis-local';folder.mkdir(exist_ok=False)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def copy(p,name):shutil.copyfile(p,folder/name)
platforms={}
for side,cache in [('prefixed-windows','gridfrontends167-package'),('prefixed-linux','raw174-linux-native'),('prefixed-tiff-linux','raw176-tiff-linux-native')]:
 out=r/('build/raw176-'+side);commands=json.loads((out/'commands.json').read_text(encoding='utf-8'))
 assert commands and all(c['exit']==0 for c in commands)
 cases=list(ET.parse(out/'native-tests.xml').getroot().iter('testcase'))
 if side=='prefixed-tiff-linux':cases+=list(ET.parse(out/'raw-image-tests.xml').getroot().iter('testcase'));copy(out/'raw-image-tests.xml',side+'-raw-image-tests.xml')
 assert len(cases)==(2 if side=='prefixed-tiff-linux' else 1)
 cases=sorted(cases,key=lambda c:c.get('name')!='resampling');assert cases[0].get('name')=='resampling'
 if side=='prefixed-tiff-linux':
  assert cases[1].get('name')=='raw_image' and 'Actual UInt16 TIFF scientific grid analysis, ROI upsampling and hash-only digest passed' in cases[1].findtext('system-out').splitlines()
  assert cases[1].findtext('system-out').splitlines().count('Independent deflate TIFF typed grid analysis through prefixed zlib passed')==1
  assert all(cases[1].find(tag) is None for tag in ('failure','error','skipped'))
 assert all(cases[0].find(tag) is None for tag in ('failure','error','skipped'))
 lines=cases[0].findtext('system-out').splitlines()
 for marker in audit.BASE['resampling']:assert lines.count(marker)==1
 manifest=r/'build/ci'/cache/'generated/build_manifest.hpp'
 text=manifest.read_text(encoding='utf-8');fingerprint=re.search(r'source_sha256\[\]="([0-9a-f]{64})"',text).group(1)
 assert lines.count('Typed raw sampling engine identity '+fingerprint+' '+source)==1
 binary=r/'build/ci'/cache/('Release/qupathxx_resampling_tests.exe' if side=='prefixed-windows' else 'qupathxx_resampling_tests')
 platforms[side]={'source':source,'source_tree_sha256':fingerprint,'selected_native_suites':['resampling','raw_image'] if side=='prefixed-tiff-linux' else ['resampling'],'executable_sha256':sha(binary),'commands':commands}
 for name in ['commands.json','build.log','native-tests.xml']+(['tests-resampling.log','tests-raw-image.log'] if side=='prefixed-tiff-linux' else ['tests.log']):
  copy(out/name,side+'-'+name)
 if (out/'configure.log').exists():copy(out/'configure.log',side+'-configure.log')
 if (out/'configure-repeat.log').exists():copy(out/'configure-repeat.log',side+'-configure-repeat.log')
 copy(manifest,side+'-build_manifest.hpp');copy(r/'build/ci'/cache/'CMakeCache.txt',side+'-CMakeCache.txt')
 if side=='prefixed-tiff-linux':platforms[side]['raw_image_executable_sha256']=sha(r/'build/ci'/cache/'qupathxx_raw_image_tests')
assert platforms['prefixed-windows']['source_tree_sha256']==platforms['prefixed-linux']['source_tree_sha256']
san=r/'build/raw176-sanitize'
assert all(c['exit']==0 for c in json.loads((san/'commands.json').read_text(encoding='utf-8')))
san_lines=(san/'tests.log').read_text(encoding='utf-8').splitlines()
assert san_lines.count(audit.BASE['resampling'][-1])==1
san_source=json.loads((r/'build/raw176-analysis-source-proof.json').read_text(encoding='utf-8'))
san_compiler=json.loads((r/'build/raw176-analysis-compiler-input-proof.json').read_text(encoding='utf-8'))
assert san_lines.count('Typed raw sampling engine identity '+san_compiler['source_tree_sha256']+' '+san_source['source'])==1
assert all(v['checked_out_sha256']==proof['files'][n]['checked_out_sha256'] for n,v in san_source['files'].items() if n not in ('cmake/TiffReader.cmake','cpp/tests/raw_image_tests.cpp'))
copy(r/'build/raw176-analysis-source-proof.json','selective-sanitizer-source-proof.json');copy(r/'build/raw176-analysis-compiler-input-proof.json','selective-sanitizer-compiler-input-proof.json')
for name in ['commands.json','build.log','tests.log']:copy(san/name,'selective-sanitizer-'+name)
cases=list(ET.parse(r/'build/raw176-python-final.xml').getroot().iter('testcase'));assert len(cases)==90 and all(c.find(t) is None for c in cases for t in ('failure','error','skipped'))
copy(r/'build/raw176-python-final.xml','python-tests.xml');copy(r/'build/raw176-source-proof.json','source-proof.json')
compiler=json.loads((r/'build/raw176-compiler-input-proof.json').read_text(encoding='utf-8'));assert compiler['source']==source
assert all(sha(root/n)==digest for n,digest in compiler['files'].items())
assert all(p['source_tree_sha256']==compiler['source_tree_sha256'] for p in platforms.values())
copy(r/'build/raw176-compiler-input-proof.json','compiler-input-proof.json')
for name in ['build_raw176_prefixed_windows.py','build_raw176_prefixed_linux.py','build_raw176_prefixed_tiff_linux.py','sanitize_raw176.py','record_raw176.py']:copy(r/'build/ci'/name,name)
copy(r/'build/raw176-sanitize-driver.cpp','selective-sanitizer-driver.cpp')
pyd=list((r/'build/ci/gridfrontends167-package/Release').glob('*.pyd'));assert len(pyd)==1 and sha(pyd[0])=='bc286d4d160c59fbccf853173820ed88280ddc4ef28e0ce31cc3553e9c9755d0'
for name in ['commands.json','build.log','configure.log']:copy(r/'build/raw176-tiff-link-failure'/name,'rejected-tiff-link-'+name)
for name in ['tiff-compile-flags.make','tiff-zip-header-dependencies.txt','tiff-zip-undefined-symbols.txt','configured-zconf.h']:copy(r/'build/raw176-prefixed-tiff-linux'/name,name)
for name in ['generated-zconf.h','copied-zconf.h','CMakeCache.txt','tiff-flags.make','tiff-imports.txt']:copy(r/'build/raw176-prefix-reconfigure-observation'/name,'rejected-prefix-'+name)
for name in ['commands.json','tests.log']:copy(r/'build/raw176-tiff-fix-enabled-linux'/name,'rejected-unprefixed-test-transport-'+name)
record={'observed_at_utc':datetime.now(timezone.utc).isoformat(),'schema':'qpx.raw-grid-analysis-native-local/1','source':source,'analysis_implementation_source':'e7aee351e92d114cd3e77adbd3222058421c9d36','tiff_header_fix_source':'25cd377','tiff_repeat_configure_fix_source':source,'source_contract_files':len(proof['files']),'compiler_inputs':compiler['compiler_inputs'],'platforms':platforms,
 'sampling_reference_cases_per_platform':94,'statistics_reference_cases_per_platform':188,'channel_results_per_platform':3732,'hash_only_hash_pairs_per_platform':94,'python_passed':90,
 'checks':['All ten source dtypes and unchanged independent hashes','Shared malformed-reader validation','Pre-IO budget and cancellation rejection','Hash-only budget excludes output arrays','Metadata epoch captured before decoder callbacks','188 Fraction-derived statistics references with core-center ROI support','Job ready, cancelled, stale and calibration identity','Actual UInt16 TIFF analysis and ROI upsampling','Independent fixed deflate TIFF decode through prefixed zlib','Legacy raw_image suite and exact mean/RGB/area references'],
 'sanitizer_source':san_source['source'],'sanitizer_scope':'Raw capture/sampler, raw analysis and fake-reader/exact-mean/statistics tests instrumented at e7aee35; remaining linked core unsanitized at e7aee35. All bound sources unchanged in final candidate except cmake/TiffReader.cmake and cpp/tests/raw_image_tests.cpp, neither included in this selective probe. Not a full sanitizer campaign or TIFF codec instrumentation. Existing misleading-indentation warnings suppressed for inherited raw-analysis translation unit.',
 'baseline_native_module_sha256':sha(pyd[0]),'baseline_native_module_unchanged':True,'native_hash_only_implemented':True,'native_grid_analysis_requests_jobs_implemented':True,'selected_native_resampling_accepted':True,'raw_analysis_workflow_accepted':False,'actual_uint16_tiff_grid_analysis_accepted':True,'all_dtype_tiff_sampling_accepted':False,'installed_python_api_accepted':False,'full_native_suite_accepted':False,'gui_accepted':False,'platform_campaign_accepted':False,'release_ready':False,'overall_project_complete':False,
 'cache_state':'Windows core/resampling current; other executables/module older. Linux minimal core/resampling current; fresh Linux TIFF/project configuration exercises raw_image and resampling.',
 'remaining':['QRA2 retention, project reopen/replay and CSV','Grouped bindings/stubs and CLI/batch/ImGui integration','Actual TIFF-backed sampling across all dtypes and broader reader formats','Complete installed/native/platform qualification','All remaining original M0-M7 criteria'],
 'evidence':{p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(folder.iterdir())}}
(root/'docs/benchmarks/raw-grid-analysis-local-windows-linux.json').write_bytes((json.dumps(record,indent=2)+'\n').encode('utf-8'))
print(json.dumps({'source':source,'fingerprint':platforms['prefixed-windows']['source_tree_sha256'],'evidence_files':len(record['evidence'])}))
