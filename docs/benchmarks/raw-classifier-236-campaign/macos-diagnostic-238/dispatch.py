"""One Mac-only diagnostic dispatch; never promotes or accepts a campaign."""
from pathlib import Path
import datetime
import hashlib
import importlib.util
import json
import subprocess
import time

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/'build/ci/raw-replay-macos-238'
OUT=ROOT/'build/raw-replay-macos-238'
OUT.mkdir(exist_ok=False)
STATE=OUT/'dispatch.json'
COMMIT='c3c19a91b6bcf5470863d416379d3eb08755c3d2'
REF='feature/raw-replay-macos-diagnostic-238'

def query(args):
    return json.loads(subprocess.check_output(['gh',*args],encoding='utf-8',timeout=45))

def load(name):
    path=SOURCE/'tools/ci'/(name+'.py')
    spec=importlib.util.spec_from_file_location('diag_'+name,path)
    module=importlib.util.module_from_spec(spec)
    exec(compile(path.read_bytes(),str(path),'exec',dont_inherit=True),module.__dict__)
    return module

assert subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()==COMMIT
assert not subprocess.check_output(['git','-C',str(SOURCE),'status','--porcelain']).strip()
assert subprocess.check_output(['git','-C',str(SOURCE),'ls-remote','origin','refs/heads/'+REF],text=True).split()[0]==COMMIT
parent=json.loads((ROOT/'build/rawt5-236/campaign.json').read_text(encoding='utf-8'))
for attempt in parent['attempts']:
    run=query(['run','view',str(attempt['run']['databaseId']),'--repo','bipxx/qupathxx','--json','headSha,status,conclusion'])
    assert run['headSha']==parent['source'] and run['status']=='completed'
engine=json.loads((ROOT/'build/rawt5-236/hosted/engine-audit.json').read_text(encoding='utf-8'))
portable=json.loads((ROOT/'build/rawt5-236/hosted/portable-audit.json').read_text(encoding='utf-8'))
assert engine['windows_linux_engineering_scope_accepted'] and portable['state']=='build-and-fresh-consumer-accepted'
proofs={name:load(name+'_evidence').source_contract(SOURCE,COMMIT) for name in
        ['multifile_ome','scientific_grid','tma_axis','tma_dearray','qptma','resampling','hierarchy','raw_classifier']}
common=load('qptma_evidence')
unique=common.verify_source_snapshot(SOURCE,*proofs.values())
proof={'source':COMMIT,'domains':{name:len(p) for name,p in proofs.items()},'unique_source_files':unique,'proofs':proofs}
proof_path=OUT/'frozen-source-proofs.json'
proof_path.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
profiles=load('campaign_inputs')
semantic=profiles.full_inputs('macos');wire=profiles.gh_inputs('macos',semantic)
def runs():
    return query(['run','list','--repo','bipxx/qupathxx','--workflow','ci.yml','--branch',REF,'--event','workflow_dispatch','--limit','20','--json','databaseId,headSha,status,conclusion,url'])
previous={run['databaseId'] for run in runs()}
assert not previous,'Inspect any existing diagnostic; do not dispatch twice'
state={'schema':'qpx.raw-replay-macos-diagnostic/1','source':COMMIT,'ref':REF,
       'purpose':'Locate the Mac arm64 replay failure and preserve compiled programs before CTest; not full T5 qualification.',
       'semantic_inputs':semantic,'wire_inputs':wire,'preflight_controls':164,
       'source_proof_sha256':hashlib.sha256(proof_path.read_bytes()).hexdigest(),
       'state':'dispatch_started','attempted_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
       'known_runs_before':sorted(previous),'complete_campaign_accepted':False,'release_ready':False}
def save():STATE.write_text(json.dumps(state,indent=2)+'\n',encoding='utf-8')
save()
try:
    result=subprocess.run(['gh','workflow','run','ci.yml','--repo','bipxx/qupathxx','--ref',REF,'--json'],
                          input=json.dumps(wire),capture_output=True,text=True,encoding='utf-8',timeout=45)
    state.update(returncode=result.returncode,stdout=result.stdout,stderr=result.stderr,
                 state='dispatch_acknowledged' if result.returncode==0 else 'dispatch_uncertain');save()
    if result.returncode:raise RuntimeError('Uncertain dispatch; inspect without repeating')
except BaseException:
    if state['state']=='dispatch_started':state['state']='dispatch_uncertain';save()
    raise
for _ in range(15):
    found=[run for run in runs() if run['databaseId'] not in previous and run['headSha']==COMMIT]
    if len(found)>1:raise RuntimeError('Ambiguous diagnostic identity; do not repeat')
    if found:
        state.update(state='run_identified',run=found[0]);save()
        print('Mac diagnostic',found[0]['databaseId'],found[0]['url'],unique,'bound inputs',flush=True)
        break
    time.sleep(1)
else:raise RuntimeError('Acknowledged diagnostic not yet identified; poll without repeating')
