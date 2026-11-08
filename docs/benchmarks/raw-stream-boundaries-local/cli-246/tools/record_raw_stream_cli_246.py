from pathlib import Path
import hashlib,json,shutil,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';o=r/'build/raw-stream-cli-246';f=o/'final';dst=s/'docs/benchmarks/raw-stream-boundaries-local/cli-246'
commands=json.loads((f/'install-commands.json').read_text());assert commands[-1]['name']=='verification' and commands[-1]['exit']==0
cases=list(ET.parse(f/'python-tests.xml').getroot().iter('testcase'));ids=[(t.get('classname'),t.get('name')) for t in cases];assert len(ids)==len(set(ids))
assert not any(t.find('failure') is not None or t.find('error') is not None for t in cases)
new=[t for t in cases if t.get('classname','').endswith('test_raw_stream_contour_cli')];assert len(new)==18 and all(t.find('skipped') is None for t in new)
native=list(ET.parse(f/'native-tests.xml').getroot().iter('testcase'));assert len(native)==9 and all(all(t.find(k) is None for k in ['skipped','failure','error']) for t in native)
valid=json.loads((f/'typing-valid.log').read_text());invalid=json.loads((f/'typing-invalid.log').read_text());assert valid['summary']['filesAnalyzed']==5 and valid['summary']['errorCount']==0;assert invalid['summary']['filesAnalyzed']==4 and invalid['summary']['errorCount']==17
proof=json.loads((f/'installed-proof.json').read_text());dst.mkdir(parents=True,exist_ok=False)
for folder,label in [(o,'initial'),(f,'final')]:
 for p in folder.iterdir():
  if p.is_file() and p.suffix in ['.json','.xml','.log']:
   target=dst/label/p.name;target.parent.mkdir(exist_ok=True);shutil.copyfile(p,target)
for p in (f/'active-shutdown').glob('*.json'):
 target=dst/'final/active-shutdown'/p.name;target.parent.mkdir(exist_ok=True);shutil.copyfile(p,target)
for name in ['raw_stream_cli_246_final_backend.py','raw_stream_cli_246_final_proof.py','verify_raw_stream_cli_246.py','record_raw_stream_cli_246.py']:
 target=dst/'tools'/name;target.parent.mkdir(exist_ok=True);shutil.copyfile(r/'build/ci'/name,target)
manifest={p.relative_to(dst).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in dst.rglob('*') if p.is_file()}
receipt={'schema':'qpx.local-installed-workflow-observation/1','runtime_source':proof['source'],'implementation_source':'97c38880614a5ef45d668b0746d72714a9b546df','source_fingerprint':proof['fingerprint'],'scope':'Windows cp312 native streamed contour CLI, installed with matching Python/app/region runtime. Local engineering evidence only.','wheel':{k:proof[k] for k in ['wheel_bytes','wheel_sha256']},'native_passed':len(native),'python':{'unique_cases':len(cases),'passed':sum(t.find('skipped') is None for t in cases),'declared_skips':[{'classname':t.get('classname'),'name':t.get('name'),'reason':t.find('skipped').get('message')} for t in cases if t.find('skipped') is not None],'new_cli_cases':len(new)},'typing':{'positive_files':5,'negative_files':4,'intended_errors':17},'initial_cli_observation':'32 passed, one declared skip; CLI97c3888 compared with separate installed Python7c77367. Final run uses matching installed source.','behavior':['Native and installed Python geometry/count/commitment parity on full and cropped targets','Unicode input/model paths, class metadata and parented annotation save/reopen','Input/model/reader/existing-output/sidecar collision refusal; no overwrite','Inapplicable flags and four independent quota refusals leave no destination','Real running-child interruption returns130, joins worker and releases reader without publishing','No-clip preserves center masking while omitting ROI geometry intersection'],'identity':'All installed members, source/fingerprint, dependency origins and notices match. All thirteen native products retained; prior products authenticated before cache reuse. Both complete SDK trees match245. Separate consumer reuses test dependencies; not clean-machine support.','remaining':['Streamed project batches','Historical QWS1 geometry/reverification','Demonstrated Mac arm64 repair and full integrated qualification','Scientific/model and process-memory/hardware acceptance','Clean-machine supported distribution and release decisions'],'files':manifest}
(dst/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'receipt':str(dst/'receipt.json'),'python':receipt['python'],'raw_files':len(manifest)},indent=2))
