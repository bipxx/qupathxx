from pathlib import Path
import hashlib
import json
import shutil

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/gui-font-qualification-285'
observation=root/'build/q285-all/20261028T181837Z'
public=work/'docs/benchmarks/gui-font-qualification-285'
state=json.loads((out/'campaign.json').read_text())
assert state['source']=='1f71eb59e13fba272113c1119ee2b608fa26d01a'
assert set(state['roles'])=={'engine','macos','portable'}
assert all(v['state']=='run_identified' and v['returncode']==0 for v in state['roles'].values())
for role in state['roles']:
    run=json.loads((observation/(role+'-run.json')).read_text())
    assert run['databaseId']==state['roles'][role]['run']['databaseId']
    assert run['headSha']==state['source'] and run['status']=='queued'
public.mkdir(exist_ok=False)
files=[]
def copy(src,name):
    target=public/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(src,target)
    assert src.read_bytes()==target.read_bytes()
    files.append(dict(path=name,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for p in out.iterdir():
    if p.is_file() and p.suffix=='.json':copy(p,p.name)
for p in observation.iterdir():
    if p.is_file() and p.suffix=='.json':copy(p,'initial/'+p.name)
for name in ('dispatch_font_285.py','collect_candidate285.py','prepare_campaign285.py','publish_campaign285.py'):
    copy(root/'build/ci'/name,'tools/'+name)
receipt=dict(schema='qpx.gui-font-qualification-campaign/1',source=state['source'],ref=state['ref'],
    state='three-full-roles-initially-queued',
    runs={r:v['run']['databaseId'] for r,v in state['roles'].items()},
    dispatches_per_role=1,required_archives=11,observed_archives=0,
    source_paths=547,canonical_compiler_blobs_equal=415,fresh_checkout_newline_only_differences=149,
    local_native_source=state['local_native_source'],local_binary_relabelled=False,
    hosted_roles_build_own_source=True,full_audits_prepared_not_executed=True,
    engineering_accepted=False,scientific_accepted=False,general_speed_claim=False,release_ready=False,files=files)
(public/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    stream.write('\ndocs/benchmarks/gui-font-qualification-285/** -text -whitespace\n')
paragraph='''**Font candidate285 is frozen; complete own-source qualification is queued.**
The [campaign](benchmarks/gui-font-qualification-285/receipt.json) binds source
`1f71eb5`, engine [40743832312](https://github.com/bipxx/qupathxx/actions/runs/40743832312),
Mac-native [40743845473](https://github.com/bipxx/qupathxx/actions/runs/40743845473)
and portable [40743859500](https://github.com/bipxx/qupathxx/actions/runs/40743859500).
Each full role was dispatched once.547 source paths authenticate;415 canonical
compiler Git blobs equal tested localnative7231d95, with149 newline-only fresh
checkout differences. Each hosted role builds its own frozen source; local
binaries keep7231d95 provenance. Observe these exact runs, authenticate all11
required archives and execute the prepared complete original-source auditors.
Do not redispatch or move the frozen ref/checkout. Candidate281 remains the
qualified feature baseline until combined285 acceptance exists; science,
physical-monitor DPI, actual new-font Name editing and release remain separate.
'''
for name in ('current-task.md','project-plan.md','feature-matrix.md','api-delivery-plan.md'):
    path=work/'docs'/name
    text=path.read_text(encoding='utf-8')
    if name=='current-task.md':
        marker='**Current qualified feature baseline:'
        assert marker in text
        position=text.index(marker)
        text=text[:position]+paragraph+'\n'+text[position:]
    else:
        title,rest=text.split('\n',1)
        text=title+'\n\n'+paragraph+'\n'+rest.lstrip('\n')
    path.write_text(text,encoding='utf-8')
note=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
prefix=note.read_bytes()
addition='''

## Font-source285 full qualification dispatched once (2026-10-28)

Freeze1f71eb5 with clean detached audit checkout.547 source paths authenticate;
415 canonical compiler blobs equal retained testednative7231d95;149 checkout
differences are newline-only. Do not relabel local binaries. Each hosted role
builds its own1f71eb5 source. The active numeric workflow and authoritative zero
prior-run observation precede one API POST per full role. Engine40743832312,
Mac-native40743845473 and portable40743859500 are confirmed initially queued.
Observe these exact IDs, authenticate11 archives and use the complete original
frozen-source auditors; preserve partial/failed observations and never redispatch
because a GET or artifact download times out. Candidate281 stays qualified until
combined285 acceptance. Physical UI, science, hardware/support/release and every
M0-M7 exit remain independently open.
'''
note.write_bytes(prefix+addition.encode('utf-8'))
assert note.read_bytes()[:len(prefix)]==prefix
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(prefix_bytes=len(prefix),
    prefix_sha256=hashlib.sha256(prefix).hexdigest(),prefix_unchanged=True),indent=2)+'\n')
print('Published queued font285 campaign:',len(files),'evidence files; no acceptance',flush=True)
