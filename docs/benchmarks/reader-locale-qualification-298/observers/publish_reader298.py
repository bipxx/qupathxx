from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/reader-locale-qualification-298'
obs=root/'build/q298-all/20261029T001131Z'
campaign=json.loads((out/'campaign.json').read_text());assert campaign['source']=='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
assert all(v['state']=='run_identified' for v in campaign['roles'].values())
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: freeze repaired reader source298 once

- Freeze the public repair closeout9fbc9315 after its local native28c33573
  delivery, not the failed original295 source. Authenticate898 source paths and
  425 canonical compiler blobs against the local binary. Fresh-checkout173
  newline-only differences are explicit; binaries retain their original build
  identity.1087 fresh-checkout auditor controls pass with shared test dependencies.
- Dispatch each complete original-source role exactly once and persist the
  handles before observing: engine40792323979, Mac40792333489 and portable
  40792343129. Initial observation confirms engine/portable in progress and Mac
  queued. No artifacts or role acceptance are claimed yet. Preserve the frozen
  checkout, exact payloads and complete auditor commands; collect by the same
  handles without redispatch after timeouts or queued observations.
- New observer/docs work does not rebuild the native engine or inherit hosted
  acceptance. Original295 remains terminal/unaccepted overall, with accepted
  Mac-native/portable roles and retained Windows failures. Main/release remain
  unchanged; broader scientific, biological, hardware and support gates stay open.
'''.encode()
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n')
target=work/'docs/benchmarks/reader-locale-qualification-298';target.mkdir(exist_ok=False);entries=[]
for path in sorted(out.iterdir()):
    if path.is_file():entries.append((path,path.name))
for path in sorted(obs.iterdir()):
    if path.is_file():entries.append((path,'initial-observation/'+path.name))
for name in ('prepare_reader298.py','prepare_dispatch_reader298.py','dispatch_reader298.py','prepare_followup_reader298.py','collect_candidate298.py','publish_reader298.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
files=[]
for source,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
    files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'receipt.json').write_text(json.dumps(dict(schema='qpx.reader-locale-qualification/1',state='dispatched-observing-not-accepted',source=campaign['source'],ref=campaign['ref'],local_native_source=campaign['local_native_source'],source_paths=898,canonical_compiler_blobs_equal=425,preflight_controls=1087,shared_test_dependencies=True,clean_machine=False,run_ids={k:v['run']['databaseId'] for k,v in campaign['roles'].items()},initial_status={'engine':'in_progress','macos':'queued','portable':'in_progress'},authenticated_archives=0,complete_original_auditors_prepared=True,combined_accepted=False,main_promoted=False,release_ready=False,files=files),indent=2)+'\n')
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text();headings=re.findall(r'^#{1,3} .+$',text,re.M)
    old='''rejection. No old-source restart or acceptance transfer follows. Qualify the
repaired successor; retain qualified281. Nominal comparison296 still has8/9'''
    new='''rejection. No old-source restart or acceptance transfer follows. Frozen
[candidate298](benchmarks/reader-locale-qualification-298/receipt.json) at9fbc9315
passes1087 fresh-checkout controls and has one engine/Mac/portable run dispatched.
Observe its original handles/auditors; no acceptance yet. Retain qualified281.
Nominal comparison296 still has8/9'''
    assert text.count(old)==1;text=text.replace(old,new)
    if name=='project-plan.md':
        anchor=text.index('## Execution order from the present state');prefix=text[:anchor];tail=text[anchor:]
        old='''rejects Unicode auto-selection. Repair297 is locally verified at native28c33573;
freeze and qualify its successor using complete own-source auditors. Preserve'''
        new='''rejects Unicode auto-selection. Repair297 is locally verified at native28c33573.
Candidate298 is frozen at9fbc9315; observe its original engine40792323979,
Mac40792333489 and portable40792343129 runs and collect all11 archives before
executing their complete original-source auditors. Preserve'''
        assert tail.count(old)==1;text=prefix+tail.replace(old,new)
    if name=='api-delivery-plan.md':
        old='''ASCII-only folding and passes local Windows/GCC checks. Freeze and qualify its
successor; retain qualified281 and all original failed evidence. Broader'''
        new='''ASCII-only folding and passes local Windows/GCC checks. Candidate298 at9fbc9315
passes fresh-checkout controls and has original engine/Mac/portable runs live or
queued; observe their exact handles with complete frozen-source auditors. Retain
qualified281 and all original failed evidence. Broader'''
        assert text.count(old)==1;text=text.replace(old,new)
    assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
attrs=work/'.gitattributes'
with attrs.open('a',encoding='utf-8',newline='\n') as stream:stream.write('\ndocs/benchmarks/reader-locale-qualification-298/** -text -whitespace\n')
print('Published frozen298 source proof, payloads, exact run handles and original complete audit plan')
