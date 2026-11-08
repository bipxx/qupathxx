from pathlib import Path
import hashlib
import json
import re
import shutil

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-cell-qualification-306'
campaign=json.loads((out/'campaign.json').read_text(encoding='utf-8'))
assert campaign['source']=='701fa672db3745aebc7fc5d71108b2c8780cbb6e'
assert all(v['state']=='run_identified' for v in campaign['roles'].values())
observations=sorted((root/'build/q306-all').glob('*/verified.json'));assert observations
obs=observations[0].parent
assert not json.loads((obs/'verified.json').read_text(encoding='utf-8'))['files']
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## Qualification306: close audit inputs before dispatch

- Pre-dispatch inspection found that the complete engine and Mac auditors still expected the pre-Gaussian/pre-response native suite totals. Share the explicit pair of added suite names for the count delta, while the existing scientific auditor separately requires their exact success markers. Expect57 GUI and56 headless/Mac native suites; do not replace named suite admission with a loose minimum.35 native refusal controls pass, including missing/duplicate/failed/skipped/wrong-marker response cases.
- Freeze repaired source701fa672 in its own clean checkout/ref. Authenticate1428 source paths and434 canonical compiler blobs against local native26cae2af, retaining172 explicit newline-only checkout differences.1106 complete fresh-checkout preflight controls pass. Native binaries keep their original build identity; hosted roles must compile the frozen source themselves.
- Complete engine40817699577, Mac40817708194 and portable40817717210 runs were dispatched exactly once. Each original handle, semantic/wire input and the original complete auditor commands are persisted before collection. The initial observation shows all three queued and no archives; it grants no role or combined acceptance.
- Keep terminal298 and other old runs terminal. Observe306 by its existing handles; neither a queue nor an observation timeout justifies redispatch. Collect/authenticate all11 required archives and execute the corresponding complete original-source auditors at terminal states. Mac Python/GUI/OpenSlide, broader science/context/feature-domain, whole-slide resource/seam, representative speed/hardware, licensing/support/release and every M0-M7 exit remain open.
'''.encode('utf-8')
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
append=dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),append_sha256=hashlib.sha256(addition).hexdigest(),prefix_preserved=True)
(out/'compilexx-append-proof.json').write_bytes((json.dumps(append,indent=2)+'\n').encode('utf-8'))
target=work/'docs/benchmarks/original-source-cell-qualification-306';target.mkdir(exist_ok=False)
entries=[(path,path.name) for path in sorted(out.iterdir()) if path.is_file()]
entries.extend((path,'initial-observation/'+path.name) for path in sorted(obs.iterdir()) if path.is_file())
for name in ('prepare_original306.py','prepare_dispatch_original306.py','dispatch_original306.py','prepare_followup_original306.py','collect_candidate306.py','publish_original306.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
entries.append((root/'build/original-source-305/qualification306-native-cohort-refusals.xml','native-cohort-refusals.xml'))
files=[]
for path,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,dest)
    raw=dest.read_bytes();assert raw==path.read_bytes();files.append(dict(path=name,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
receipt=dict(schema='qpx.original-source-cell-qualification/1',state='dispatched-observing-not-accepted',source=campaign['source'],ref=campaign['ref'],
    local_native_source=campaign['local_native_source'],source_paths=1428,canonical_compiler_blobs_equal=434,preflight_controls=1106,
    shared_test_dependencies=True,clean_machine=False,run_ids={k:v['run']['databaseId'] for k,v in campaign['roles'].items()},
    initial_status={role:json.loads((obs/(role+'-run.json')).read_text(encoding='utf-8'))['status'] for role in campaign['roles']},
    authenticated_archives=0,complete_original_auditors_prepared=True,combined_accepted=False,main_promoted=False,release_ready=False,files=files)
(target/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'))
status='''[Frozen original-source306](benchmarks/original-source-cell-qualification-306/receipt.json)
at701fa672 passes1106 fresh-checkout auditor controls and authenticates1428 source
paths/434 canonical compiler inputs against local native26cae2af without relabeling
its binaries. The complete engine40817699577, Mac40817708194 and portable40817717210
runs were dispatched once with their original auditor commands prepared. Observe
these handles and collect all11 required archives; no hosted or combined acceptance
is claimed yet. The two added native suites are explicitly required by both suite
count and marker checks. All broader science/context/feature-domain, whole-slide
resources/seams, representative speed/hardware, support/license/release and M0-M7
requirements remain open. Preserve qualified281 and terminal298 outcomes.
'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text(encoding='utf-8');headings=re.findall(r'^#{1,3} .+$',text,re.M)
    first,rest=text.split('\n\n',1);changed=first+'\n\n'+status+'\n'+rest
    assert re.findall(r'^#{1,3} .+$',changed,re.M)==headings
    if name=='project-plan.md':
        begin='## Current execution map';end='## Execution order from the present state'
        assert text[text.index(begin):text.index(end)]==changed[changed.index(begin):changed.index(end)]
        assert len(headings)==19
    path.write_text(changed,encoding='utf-8')
print('Published',len(files),'initial306 members; no acceptance claimed; full plan scope/notes prefix preserved')
