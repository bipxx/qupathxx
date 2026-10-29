from pathlib import Path
import hashlib
import json
import re
import shutil

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
proof=json.loads((out/'source-runtime-proof.json').read_text(encoding='utf-8'))
destination=work/'docs/benchmarks/original-source-cells-305';assert not destination.exists()
def sha(raw):return hashlib.sha256(raw).hexdigest()
summary='''[Original-source305](benchmarks/original-source-cells-305/receipt.json) locally
delivers independent affine RGB sampling and QuPath070 cell processing through
native requests, QCL8/QCA8 history/replay/classifier provenance, CSV11/12, Python,
CLI, batch/resume, direct/batch ImGui and managed capture. All13 held-out and nine
prior zero-context original-slide nucleus/body workflows match exactly; actual
processing RGB and its independent feature arithmetic pass unchanged comparisons.
The old quantitative/ImageJ recipes and their explicit limits remain intact.
Native26cae2af plus observer6d8bb150 bind434 unchanged compiler inputs,1428 source
paths,79 retained Windows products,25 GCC products and39 installed package members.
Windows passes57 native suites,943 scientific checks/23 declared skips,1099 final
auditor controls and24 actual desktop/managed checks; typing/generated stubs pass.
Six affected GCC ASan/UBSan suites pass, including112 captured affine RGB reads.
Next qualify one complete combined source through the required hosted profiles.
Broader context/specimen/feature-domain, whole-slide seams/resources, hardware,
general speed, support/license/release and all M0-M7 gates remain open. Preserve
qualified281 and original terminal298 outcomes without acceptance transfer.
'''
prepared={}
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    text=(work/'docs'/name).read_text(encoding='utf-8');first,rest=text.split('\n\n',1)
    prepared[name]=first+'\n\n'+summary+'\n'+rest
plan_before=(work/'docs/project-plan.md').read_text(encoding='utf-8')
old='''Original-source integration is in progress locally. The private affine sampler
passes112 captured RGB reads plus tile/context and signed/large-origin checks
on Windows. Public request/history/consumer wiring and GCC verification remain
open; see the [contract and delivery sequence](original-source-cell-integration.md).
This checkpoint is not a new installed application or qualification candidate.
'''
assert prepared['project-plan.md'].count(old)==1;prepared['project-plan.md']=prepared['project-plan.md'].replace(old,'')
old='''The current local integration checkpoint delivers the private affine sampler's
112 captured RGB comparisons and tile/context/precision checks on Windows.
Follow the [explicit request/history/consumer sequence](original-source-cell-integration.md)
to deliver both sampling and detector choices together; no public opt-in or
new combined qualification is accepted yet.
'''
assert prepared['api-delivery-plan.md'].count(old)==1;prepared['api-delivery-plan.md']=prepared['api-delivery-plan.md'].replace(old,'')
queue=prepared['api-delivery-plan.md'];start=queue.index('Policy302 remains locally delivered through existing consumers. Private304 now')
end=queue.index('\n\nThe next RGB cell-analysis slice',start)
prepared['api-delivery-plan.md']=queue[:start]+'''Original-source305 locally delivers the complete sampling/detector consumer
slice. Keep the explicit recipe choices and old numerical defaults. The
[source-bound receipt](benchmarks/original-source-cells-305/receipt.json) verifies
22 zero-context original-slide mask workflows, processing-grid feature arithmetic,
history/replay/classifier provenance, batch resume and all existing consumer paths.
Freeze the complete observer/stub/reference source once for the required hosted
qualification. Do not redispatch terminal298 or transfer its acceptance. Continue
broader context/specimen/feature-domain, whole-slide resource/seam, hardware/speed,
support/license/release and full M0-M7 work under their existing requirements.'''+queue[end:]
for name,text in prepared.items():
    before=(work/'docs'/name).read_text(encoding='utf-8')
    assert re.findall(r'^#{1,3} .+$',before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
    if name=='project-plan.md':
        begin='## Current execution map';end='## Execution order from the present state'
        assert before[before.index(begin):before.index(end)]==text[text.index(begin):text.index(end)]
    (work/'docs'/name).write_text(text,encoding='utf-8')
plan_after=prepared['project-plan.md']
with (out/'plan-scope-proof.json').open('x',encoding='utf-8') as stream:json.dump(dict(all_headings_preserved=True,heading_count=len(re.findall(r'^#{1,3} .+$',plan_before,re.M)),execution_map_and_full_milestone_scope_preserved=True,before_sha256=sha(plan_before.encode()),after_sha256=sha(plan_after.encode())),stream,indent=2);stream.write('\n')
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';prefix=notes.read_bytes()
addition='''

## Original-source305 completed local integration

- Independent sampling and processing choices now travel through one native source-authoritative path, QCL8/QCA8 history/replay/classifier provenance, CSV11/12, Python/stubs, region/batch CLI, batch resume, direct/batch ImGui and managed capture. Keep old defaults and old archive bytes; do not multiply separate detector implementations across consumers.
- Source26cae2af and observer6d8bb150 authenticate434 unchanged compiler inputs,1428 source paths,79 retained Windows products,25 retained GCC products and39 installed members. Build the combined application once after the shared public headers/consumers settle. Bindings recompilation dominated the wait; the verified live build was allowed to finish, without unnecessary restarts or source changes.
- All13 held-out and nine prior zero-context original-slide nucleus/body masks are exact.112 captured RGB reads agree on Windows/GCC; independent arithmetic over the actual processing-grid feature domain passes fixed tolerances. Do not turn these facts into a biological, universal QuPath feature-domain, arbitrary-context, whole-slide or general-speed claim.
- Windows57 native suites,943 scientific checks/23 declared skips,24 actual desktop/managed cases and strict typing/generated stubs pass. Six affected GCC ASan/UBSan suites pass. Ordinary detector jobs do not retain diagnostic basin/region/binary/refinement stages after use.
- A stale auditor count expected224 RGB API cases after coverage grew to238. Preserve the original1095 passes/four failures, repair the explicit independent count, pass its four refusal controls, and retain the final complete1099-control pass. This was an audit fixture repair, not a relaxed numerical comparison or a native rebuild.
- Correct the reference generator's platform-dependent manifest newline with UTF-8 write_bytes. Regeneration proves all four numerical/index members unchanged. Patch only the two generated stubs and wheel RECORD after installation; all native/font/notice bytes and434 compiler inputs remain unchanged.
- Preserve all old failures and source identities. Next freeze the complete combined observer/stub/reference source for hosted qualification once; terminal298 is not live and must not be redispatched. Broader biology, context/feature-domain, whole-slide seams/resources, representative hardware/speed, supported installs, licensing and release gates remain open, as do all M0-M7 exits.
'''.encode('utf-8')
with notes.open('ab') as stream:stream.write(addition)
after=notes.read_bytes();assert after==prefix+addition
with (out/'compilexx-append-proof.json').open('x',encoding='utf-8') as stream:json.dump(dict(path=str(notes),prefix_bytes=len(prefix),prefix_sha256=sha(prefix),append_bytes=len(addition),append_sha256=sha(addition),prefix_preserved=True),stream,indent=2);stream.write('\n')
destination.mkdir()
entries=[]
for path in sorted(out.iterdir()):
    if path.is_file() and path.suffix in ('.json','.xml','.log','.png'):entries.append((path,path.name))
for name in ('commands.json','products.json','native.xml','native-complete.log','compiler.txt','build.log','configure.log','clone.log','autocrlf.log','checkout.log','native.log'):
    entries.append((out/'linux'/name,'linux/'+name))
for name in ('build_original305.py','original305_backend.py','validate_original305.py','patch_original305_stubs.py','observe_original305.py','run_observation305.py','linux_original305.py','normalize_affine_manifest305.py','preflight_final305.py','prove_original305.py','publish_original305.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
files=[]
for path,name in entries:
    target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
    raw=target.read_bytes();files.append(dict(path=name,bytes=len(raw),sha256=sha(raw)))
assert len({v['path'] for v in files})==len(files)
receipt=dict(schema='qpx.original-source-cells-local/1',**proof,files=files,full_plan_scope_preserved=True,
    known_limits=['zero-context comparison corpus; broader context remains','processing-grid feature arithmetic; no universal level-zero feature equivalence','isotropic QuPath070 metric and zero radius/spacing filters','no hosted own-source qualification yet','no biological/general-speed/supported-release acceptance'])
(destination/'receipt.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('utf-8'))
print('Published',len(files),'receipt members;19 plan headings/full scope and compilexx prefix preserved',flush=True)
