from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-processing-policy-302'
proof=json.loads((out/'source-runtime-proof.json').read_text())
assert proof['source_paths']==1159 and proof['compiler_inputs']==429
linux=json.loads((out/'linux/linux-command-results.json').read_text())
assert len(linux)==6 and all(item['returncode']==0 for item in linux)
products=json.loads((out/'linux/linux-native-products.json').read_text())
assert len(products)==9
for item in products:
    path=Path(item['path'].replace('/mnt/c/','C:/',1))
    assert path.stat().st_size==item['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']
cases=ET.parse(out/'linux/linux-native.xml').findall('.//testcase')
assert len(cases)==8 and all(all(c.find(tag) is None for tag in ('failure','error','skipped')) for c in cases)
inputs=json.loads((out/'linux/linux-compiler-inputs.json').read_text())
assert inputs['source']==proof['native_source'] and len(inputs['files'])==429

notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: integrate one numerical policy through existing cell workflows302

- Reuse the exact native float32 OD/stain transform and private Gaussian rather
  than duplicating a detector or adding ImageJ as a runtime dependency. Add an
  opt-in processing enum to the existing copied parameters. Default quantitative
  requests preserve their old arithmetic, archive encodings and recipe keys.
  New QCL7/QCA7, algorithm IDs, CSV9/10 and resume identity distinguish the policy.
- Finish native history/replay, Python, CLI/batch, direct/batch ImGui and managed
  consumers before the next qualification. One Windows application build covers
  all429 compiler inputs at2b4786ed; observer/stub source28e062dd reuses those
  inputs. Generate stubs once from the installed wheel and change only two stub
  members plus RECORD. Preserve prior71 products before replacing the cache.
- A comparison helper initially reused its processing-policy argument name for
  the captured RGB array. Keep distinct policy/data names and exercise both
  policies through the actual comparison entry point. Existing35 quantitative
  comparisons pass. All nine pinned synthetic masks match with zero context;
  real source context32 retains the known24-pixel clipped-ROI nucleus difference.
  Preserve that semantic boundary and feature-domain differences, not an epsilon.
-56 Windows native suites,114 focused installed cases,19 desktop/managed cases,
 826 scientific cases/18 declared skips,1097 auditor controls and strict typing
  pass. Counts overlap. Eight affected GCC ASan/UBSan suites pass; retain nine
  Linux products before WSL exit. Authenticate1159 source paths,76 Windows
  products and39 installed members. No clean-machine, biological, general-speed,
  supported-release or full-platform acceptance follows from local evidence.
- Preserve frozen298's engine rejection and portable acceptance. Its original
  Mac arm64 job remains queued at the last live observation. Never redispatch
  the old source or transfer acceptance. Next finish held-out numerical/edge
  evidence and freeze the completed integration for one own-source campaign;
  broader M0-M7, research, hardware, license and release gates remain open.
'''.encode()
append_proof=out/'compilexx-append-proof.json'
if append_proof.exists():
    recorded=json.loads(append_proof.read_text(encoding='utf-8'))
    assert hashlib.sha256(before).hexdigest()==recorded['final_sha256']
    assert before[recorded['prefix_bytes']:]==addition
    assert hashlib.sha256(before[:recorded['prefix_bytes']]).hexdigest()==recorded['prefix_sha256']
else:
    assert b'workflows302' not in before
    with notes.open('ab') as stream:stream.write(addition)
    assert notes.read_bytes()==before+addition
    append_proof.write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n')

old=None
new='''[Policy302](benchmarks/cell-processing-policy-302/receipt.json) locally delivers
the opt-in `CellProcessingPolicy.IMAGEJ_154P` through native nominal requests,
QCL7/QCA7 history/replay, copied Python parameters, CLI/batch resume, ImGui and
managed commands. Native2b4786ed and observer28e062dd bind429 compiler inputs,
1159 source paths,76 retained Windows products and39 installed members.56 native
suites,114 focused cases,19 desktop/managed cases,826 scientific cases/18 declared
skips,1097 controls and typing pass; counts overlap. Eight affected GCC ASan/UBSan
suites pass with nine retained products. All nine pinned synthetic masks match
with zero context; context32 retains the24-pixel clipped-ROI nucleus difference.
Defaults and the three legacy scientific xfails stay unchanged. This reuses the
exact [input301](benchmarks/qupath-input-stages-301/receipt.json) and private
Gaussian300 stages. Full own-source qualification, held-out/biological/resource/
hardware/support/license/release gates and all M0-M7 exits remain open. Preserve
qualified281, original298 engine rejection and portable acceptance; continue
observing its existing Mac handle without redispatch or acceptance transfer.'''
plan_before=(work/'docs/project-plan.md').read_text()
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text();headings=re.findall(r'^#{1,3} .+$',text,re.M)
    start=text.index('[Input301]');end=text.index('\n\n',start)
    if old is None:old=text[start:end]
    assert text[start:end]==old
    text=text[:start]+new+text[end:]
    assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text();anchor=text.index('## Execution order from the present state')
old_order='''Private Gaussian300 reproduces all37 captured stages on MSVC/GCC sanitizers;
input301 verifies exact existing native OD/stain inputs on Windows/Linux.298's
original engine auditor rejects isolated test-helper imports; the observer is
locally repaired without rebuilding application code. Portable accepts; keep
observing the original Mac handle. Next add one versioned opt-in processing
policy through existing requests, persistence/replay and all consumers, with
held-out grids/alpha/edges. Existing quantitative/default policies stay unchanged.'''
new_order='''Policy302 now integrates the exact input/Gaussian path through existing
requests, persistence/replay and all consumers, with local Windows and affected
GCC sanitizer evidence. Finish held-out grids/alpha/edge characterization and
freeze one complete successor for own-source qualification. Existing default
quantitative results and historical recipes remain unchanged.298's original
engine rejection and portable acceptance stay source-bound; observe its original
Mac handle without redispatch. Broader scientific and release lanes remain open.'''
assert text[anchor:].count(old_order)==1
text=text[:anchor]+text[anchor:].replace(old_order,new_order)
begin='## Current execution map';end='## Execution order from the present state'
assert plan_before[plan_before.index(begin):plan_before.index(end)]==text[text.index(begin):text.index(end)]
assert re.findall(r'^#{1,3} .+$',plan_before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
path.write_text(text,encoding='utf-8',newline='\n')
(out/'plan-scope-proof.json').write_text(json.dumps(dict(all_headings_preserved=True,heading_count=len(re.findall(r'^#{1,3} .+$',text,re.M)),execution_map_and_full_milestone_scope_preserved=True,before_sha256=hashlib.sha256(plan_before.encode()).hexdigest(),after_sha256=hashlib.sha256(text.encode()).hexdigest()),indent=2)+'\n')
path=work/'docs/api-delivery-plan.md';text=path.read_text();anchor='## Dependency lanes and immediate queue\n'
assert text.count(anchor)==1
text=text.replace(anchor,anchor+'''
Policy302 completes the explicit numerical-policy consumer slice locally. Reuse
its native request/archive/controller path; do not restart SDK or frontend work.
Next characterize held-out numerical/edge cases, then freeze one combined
own-source qualification. Keep original298's source-specific outcomes separate;
all broader research, performance and release lanes remain required.
''')
path.write_text(text,encoding='utf-8',newline='\n')

target=work/'docs/benchmarks/cell-processing-policy-302';target.mkdir(exist_ok=False)
names=['build.log','build-exit.json','compiler-inputs-initial.json','prior-products.json','final-products.json','installed-proof.json','source-runtime-proof.json','raw_classifier_evidence-source-proof.json','scientific_grid_evidence-source-proof.json','stub-only-wheel-proof.json','imagej-context0.json','imagej-context32.json','imagej-desktop.png','compilexx-append-proof.json','plan-scope-proof.json','native-complete.log','typing-admission.json','scientific-admission.json','test_imagej_cell_processing-collection.log','test_nominal_cell_frontends-collection.log','quantitative-comparison-prior-runtime.xml','quantitative-comparison-prior-runtime.log']
for mode in ('install','install-final','native','policy','frontends','scientific','preflight','generate-stubs','stubs-check','typing'):
    names += [mode+'.log',mode+'-command.json']
for mode in ('native','policy','frontends','scientific','preflight'):names.append(mode+'.xml')
entries=[(out/name,name) for name in names]
entries += [(p,p.name) for p in sorted(out.glob('*-typing-negative.log'))]
entries += [(p,'linux/'+p.name) for p in sorted((out/'linux').iterdir()) if p.is_file()]
entries += [(p,'publisher-encoding-failure/'+p.name) for p in sorted((out/'publisher-encoding-failure').iterdir()) if p.is_file()]
for name in ('build_cell_policy302.py','cell_policy302_backend.py','validate_cell_policy302.py','collect_policy302_cases.py','prepare_linux_policy302.py','linux_cell_policy302.py','prepare_policy302_stub_patch.py','patch_policy302_stubs.py','prove_cell_policy302.py','publish_cell_policy302.py','publish_cell_policy302_initial.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
files=[]
for source,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
    files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
assert len(files)==len({item['path'] for item in files})
(target/'receipt.json').write_text(json.dumps(dict(**proof,state='opt-in-nominal-processing-policy-all-consumers-locally-verified',linux_affected_sanitizer_suites=8,linux_retained_products=9,remaining=['Held-out numerical/grid/edge characterization','Complete own-source platform qualification','Broader biological, whole-slide resource, representative speed/hardware, support/license/release gates and all M0-M7 exits'],files=files),indent=2)+'\n')
attrs=work/'.gitattributes'
with attrs.open('a',encoding='utf-8',newline='\n') as stream:stream.write('\ntests/test_imagej_cell_processing.py text eol=lf\ndocs/benchmarks/cell-processing-policy-302/** -text -whitespace\n')
print('Published local policy302 evidence and appended compilexx notes; full plan scope preserved',len(files))
