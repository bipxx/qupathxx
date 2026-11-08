from pathlib import Path
import hashlib
import json
import re
import shutil
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';campaign=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T022825Z'
source='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
verified=json.loads((obs/'verified.json').read_text(encoding='utf-8'));assert verified['source']==source and len(verified['files'])==11
mac=json.loads((campaign/'macos-acceptance.json').read_text(encoding='utf-8'));assert mac['source']==source and mac['native_engineering_accepted']
for role,expected in [('engine',1),('macos',0),('portable',0)]:assert json.loads((campaign/(role+'-audit-exit.json')).read_text(encoding='utf-8'))['returncode']==expected
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

### Original298 terminal observation during303

All11 frozen9fbc9315 archives now authenticate. Original Mac40792333489 is
terminal successful; its complete original auditor executes once and accepts
both native architectures. Original portable acceptance and engine rejection
are unchanged. All three original complete auditors have now executed; the
campaign remains terminal/unaccepted overall. Preserve the frozen ref/checkout
and failed engine evidence. No redispatch or acceptance transfer to policy302
or held-out303 follows; qualified281 remains the combined feature baseline.
'''.encode()
assert b'Original298 terminal observation during303' not in before
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
append=dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True)
(campaign/'terminal303-notes-proof.json').write_text(json.dumps(append,indent=2)+'\n',encoding='utf-8')
plan_before=(work/'docs/project-plan.md').read_text(encoding='utf-8')
paragraph='''**Frozen298 is terminal and unaccepted overall.** The
[terminal303 receipt](benchmarks/reader-locale-qualification-298/terminal303/receipt.json)
authenticates all11 original archives at9fbc9315. Complete original Mac-native
and portable auditors accept their roles; the original engine auditor rejects
three isolated-Python helper imports. All three original complete auditors have
executed. The local observer repair changes no old-source outcome. No redispatch,
main promotion or acceptance transfer follows; qualified281 remains the combined
feature baseline.'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text(encoding='utf-8');headings=re.findall(r'^#{1,3} .+$',text,re.M)
    start=text.index('[Held-out303]');end=text.index('\n\n',start)
    text=text[:end]+'\n\n'+paragraph+text[end:]
    marker='**Windows Unicode reader selection is repaired locally; candidate295 is terminal/unaccepted.**'
    assert text.count(marker)==1;text=text.replace(marker,'**Historical repair297/candidate298 observation; current receipts below supersede campaign status.**')
    assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text(encoding='utf-8');anchor=text.index('## Execution order from the present state')
old='''historical recipes and298's source-bound outcomes; observe its original Mac
handle without redispatch. Broader scientific and release lanes remain open.'''
new='''historical recipes and298's terminal source-bound outcomes: Mac/portable accept
and engine rejects, with all11 archives authenticated. No redispatch or acceptance
transfer follows. Broader scientific and release lanes remain open.'''
assert text[anchor:].count(old)==1;text=text[:anchor]+text[anchor:].replace(old,new)
begin='## Current execution map';end='## Execution order from the present state'
assert plan_before[plan_before.index(begin):plan_before.index(end)]==text[text.index(begin):text.index(end)]
assert re.findall(r'^#{1,3} .+$',plan_before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
path.write_text(text,encoding='utf-8',newline='\n')
(campaign/'terminal303-plan-scope-proof.json').write_text(json.dumps(dict(all_headings_preserved=True,heading_count=19,execution_map_and_full_milestone_scope_preserved=True,before_sha256=hashlib.sha256(plan_before.encode()).hexdigest(),after_sha256=hashlib.sha256(text.encode()).hexdigest()),indent=2)+'\n',encoding='utf-8')
target=work/'docs/benchmarks/reader-locale-qualification-298/terminal303';target.mkdir(exist_ok=True);assert not any(target.iterdir())
entries=[(campaign/name,name) for name in ('macos-run-terminal.json','macos-complete-audit.log','macos-audit-exit.json','macos-acceptance.json','complete-audit-plan.json','terminal303-notes-proof.json','terminal303-plan-scope-proof.json')]
entries += [(obs/name,name) for name in ('verified.json','macos-run.json','macos-artifacts.json','macos-native-arm64-artifact.json','macos-native-x86_64-artifact.json')]
counts={}
for architecture in ('arm64','x86_64'):
    folder=root/('build/q298-all/macos-native-'+architecture)
    cases=ET.parse(folder/'native-tests.xml').findall('.//testcase')
    declared=json.loads((folder/'result.json').read_text(encoding='utf-8'))
    assert declared['commit']==source and declared['tests']['passed']==len(cases)==54
    assert declared['tests']['names']==sorted(case.get('name') for case in cases)
    assert all(all(case.find(tag) is None for tag in ('failure','error','skipped')) for case in cases)
    counts[architecture]=len(cases)
    for name in ('native-tests.xml','native-tests.log','result.json','binary-architecture.log','macos-native-file-inventory.json','macos-native-relocation.log','macos-relocated-runtime.json'):
        entries.append((folder/name,architecture+'/'+name))
entries += [(root/'build/ci'/name,'observers/'+name) for name in ('collect_candidate298.py','audit_macos298_terminal303.py','closeout_298_terminal303.py')]
entries.append((campaign/'closeout303-initial-count-observer.py','observers/closeout303-initial-count-observer.py'))
files=[]
for original,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(original,dest);assert original.read_bytes()==dest.read_bytes()
    files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'receipt.json').write_text(json.dumps(dict(schema='qpx.reader-locale-original-campaign-terminal/1',state='terminal-unaccepted-overall',source=source,authenticated_archives=11,complete_original_auditors_executed=['engine','macos','portable'],macos_run=40792333489,macos_native_suites=counts,engine_accepted=False,macos_accepted=True,portable_accepted=True,combined_accepted=False,main_promoted=False,release_ready=False,files=files),indent=2)+'\n',encoding='utf-8')
print('Frozen298 terminal closeout: all11 archives, original Mac/portable accepted, engine rejected;',len(files),'receipt members')
