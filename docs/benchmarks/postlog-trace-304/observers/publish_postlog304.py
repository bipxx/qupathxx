from pathlib import Path
import hashlib
import json
import re
import shutil
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304'
proof=json.loads((out/'source-runtime-proof.json').read_text(encoding='utf-8'));assert proof['native_source'].startswith('e9d71f28')
names=['app-build.log','app-build-exit.json','compiler-inputs-initial.json','prior-products.json','final-products.json','private-products.json','installed-proof.json','source-runtime-proof.json','raw_classifier_evidence-source-proof.json','scientific_grid_evidence-source-proof.json','trace.json','basin-experiments.json','new-basins-before-border.json','new-basins.json','candidate-nuclei.json','candidate-nuclei-final.json','qupath_response_basins-before-border.hpp','response-repeat-proof.json','unit-basins-comparison.json','commands.json','configure.log','build.log','basins-configure.log','basins-build.log','new-basins-configure.log','new-basins-build.log','new-basins-build-repaired.log','new-basins-build-border.log','nuclei-configure.log','nuclei-build.log','nuclei-final-build.log','unit-configure.log','unit-build.log','unit-native.log','unit-native.xml','unit-commands.json','reference.log','reference.xml','collection.log','scientific-admission.json','typing-admission.json','native-complete.log']
for mode in ('install','native','scientific','preflight','frontends','typing','stubs-check'):names += [mode+'.log',mode+'-command.json']
for mode in ('native','scientific','preflight','frontends'):names.append(mode+'.xml')
entries=[(out/name,name) for name in names]
entries += [(path,path.name) for path in sorted(out.glob('*-typing-negative.log'))]
entries += [(path,'linux/'+path.name) for path in sorted((out/'linux').iterdir()) if path.is_file()]
for folder in ('probe','basin-probe','new-basins','nuclei-probe'):
    entries += [(path,'diagnostics/'+folder+'/'+path.name) for path in sorted((out/folder).iterdir()) if path.is_file() and path.suffix in ('.cpp','.txt')]
for name in ('trace_postlog304.py','experiment_basins304.py','check_new_basins304.py','resume_new_basins304.py','resume_border_basins304.py','check_nucleus_recipe304.py','capture_basins304.py','promote_response304.py','preflight_response304.py','collect_response304_cases.py','prepare_postlog304_build.py','build_postlog304.py','postlog304_backend.py','linux_postlog304.py','prepare_postlog304_validation.py','validate_postlog304.py','final_recipe304.py','prove_postlog304.py','publish_postlog304.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
assert len(entries)==len({name for _,name in entries})
for path,_ in entries:assert path.is_file(),path
replacement='''[Private post-LoG304](benchmarks/postlog-trace-304/receipt.json) supplies a native
compatibility candidate matching all13 captured nucleus/body workflows and33
independent public response cases.78 stage payloads match Windows/GCC sanitizer
bytes;102 oracle payloads repeat exactly. Shared region-fill/label helpers are
reused, and existing recipes retain their outputs/counters. Sourcee9d71f28 binds
433 compiler inputs,1421 source paths,79 retained Windows products and39 installed
members.57 native suites,896 scientific checks/18 declared skips,1099 auditor
controls,19 desktop/managed checks and typing/stub checks pass; counts overlap.
Three affected standalone GCC ASan/UBSan suites pass with eight retained products.
The candidate uses captured processing input and has no public request selector.
Next deliver the versioned affine sampling and new recipe through original-source
capture, history/replay, feature/contour publication and all existing consumers;
see the [integration route](cell-policy-heldout-gaps.md). Public policies and the
three legacy scientific xfails remain unchanged. No new platform, biological,
general-speed, hardware/support/license/release or M0-M7 acceptance follows.
Preserve qualified281 and terminal298 outcomes without redispatch or transfer.'''
prepared={};old=None
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text(encoding='utf-8');start=text.index('[Held-out303]');end=text.index('\n\n',start)
    if old is None:old=text[start:end]
    assert text[start:end]==old
    prepared[name]=text[:start]+replacement+text[end:]
plan_before=(work/'docs/project-plan.md').read_text(encoding='utf-8');text=prepared['project-plan.md'];anchor=text.index('## Execution order from the present state')
old_order='''Policy302 remains locally delivered through all consumers. Held-out303 exposes
eight additional detector mask gaps while all13 new LoG stages match exactly.
First isolate shared post-LoG regions/splitting/refinement against captured
stages; specify and deliver the separately versioned affine-sampling correction
through existing requests and consumers. Close the named workflow gaps before
freezing one combined own-source qualification. Preserve defaults, version1
historical recipes and298's terminal source-bound outcomes: Mac/portable accept
and engine rejects, with all11 archives authenticated. No redispatch or acceptance
transfer follows. Broader scientific and release lanes remain open.'''
new_order='''Policy302 remains locally delivered. Private304 now reproduces the13 captured
held-out workflows through shared native basin, sampled shape/refinement and body
stages on Windows/GCC sanitizers. Deliver its additive original-source recipe and
the separately versioned affine sampler through existing request, archive/replay,
feature/contour and consumer paths. Preserve legacy policies, explicit units,
source-supported context and declared budgets; keep diagnostic intermediates out
of ordinary retained records. Verify all13 original-source workflows and existing
defaults before one combined own-source qualification.298 remains terminal:
Mac/portable accept and engine rejects, with all11 archives authenticated. No
redispatch or acceptance transfer follows. All broader M0-M7 lanes remain open.'''
assert text[anchor:].count(old_order)==1;text=text[:anchor]+text[anchor:].replace(old_order,new_order)
begin='## Current execution map';end='## Execution order from the present state'
assert plan_before[plan_before.index(begin):plan_before.index(end)]==text[text.index(begin):text.index(end)]
assert re.findall(r'^#{1,3} .+$',plan_before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
prepared['project-plan.md']=text
old_queue='''Policy302 completes the explicit numerical-policy consumer slice locally. Reuse
its native request/archive/controller path. Held-out303 now identifies eight
post-LoG mask gaps and a separate affine-phase sampling discrepancy. Follow the
[measured gap route](cell-policy-heldout-gaps.md): isolate shared post-LoG stages,
deliver additive versioned sampling and detector behavior through existing
consumers, then freeze one complete own-source qualification. Do not repeat the
already exact transforms/Gaussian or completed SDK/frontend delivery. Preserve
original298 outcomes; all broader research/performance/release lanes remain.'''
new_queue='''Policy302 remains locally delivered through existing consumers. Private304 now
matches all13 held-out processing workflows and33 public response vectors across
the measured native stages. Follow the [integration route](cell-policy-heldout-gaps.md):
add a versioned affine sampling option and explicit new cell recipe to existing
original-owner requests; carry their identity through archive/replay/classifiers,
CSV and batch resume. Reuse the shared feature/contour/result path and thin
Python/CLI/ImGui/managed adapters. Generate public stubs only when the new API is
installed. Complete original-source parity, units, context, resource and lifecycle
checks before one combined own-source qualification. Keep defaults and terminal298
outcomes intact; all broader research/performance/release lanes remain required.'''
assert prepared['api-delivery-plan.md'].count(old_queue)==1
prepared['api-delivery-plan.md']=prepared['api-delivery-plan.md'].replace(old_queue,new_queue)
for name,text in prepared.items():
    before=(work/'docs'/name).read_text(encoding='utf-8');assert re.findall(r'^#{1,3} .+$',before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';prefix=notes.read_bytes()
addition='''

## 2026-10-28: private native post-LoG304 resolves captured-stage differences

- Trace the existing production path and reuse its private candidates before
  adding segmentation engines. The sampled watershed already fixes sigma zero
  and both step2.3 nucleus masks. Region admission needs filled response-basin
  means rather than one positive-component mean, followed by sampled shape and
  the existing sigma1 refinement arithmetic. Reuse region filling, labeling,
  Gaussian/LoG and sampled body expansion; do not duplicate them.
- A regional plateau connected to the image border is not a seed. The public
  float processor also returns NaN outside its bounds; the initial watershed
  frontier consequently includes positive boundary pixels before they have a
  labeled neighbor. Preserve that captured behavior explicitly in a separate
  compatibility primitive. Row-major initial frontier, raw float32 priorities
  and FIFO ties reproduce all13 full basin label arrays exactly. The first
  non-border prototype and its comparisons remain retained.
- Two independent public executions of33 small response cases repeat102 shared
  payloads. Generated native vectors contain captured values only. Tests cover
  flat/one-dimensional/threshold/plateau/FIFO/noise inputs, budgets, finite data,
  candidate limits and cancellation. Four resealed semantic faults are refused.
  Prepare helper scripts before starting dependent commands in parallel; a
  validation harness initially launched before its helper file existed. That
  command never entered validation and was then executed in the correct order.
- The private nucleus/body candidate matches all13 held-out workflows, binary
  and applicable refinement masks. All78 stage payloads repeat byte-for-byte on
  Windows and GCC ASan/UBSan. Intermediate arrays are diagnostic outputs, not
  original-slide replay authority. Public request selection is still pending.
- Freeze complete native inputs once before the application build. Source
  e9d71f28 binds433 compiler inputs and1421 source paths;79 Windows products,
  eight GCC products and39 installed members authenticate.57 native suites,
 896 scientific checks/18 declared skips,1099 controls and19 desktop/managed
  checks pass; counts overlap. Three affected standalone sanitizer suites pass.
  Public stubs/type contracts are unchanged, so check them without regeneration.
- Next integrate the already verified affine model and private recipe through
  versioned existing requests/history and all consumers. Keep full stage copies
  optional and budgeted for diagnostics; ordinary records need results and
  recipe identity. Do not reopen old campaigns or use private/captured-input
  success as source-bound platform, biological, general-speed or release
  acceptance. All M0-M7 exits and broader research/resource/support gates remain.
'''.encode()
assert b'post-LoG304 resolves' not in prefix
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==prefix+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(prefix),prefix_sha256=hashlib.sha256(prefix).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n',encoding='utf-8')
for name,text in prepared.items():(work/'docs'/name).write_text(text,encoding='utf-8',newline='\n')
(out/'plan-scope-proof.json').write_text(json.dumps(dict(all_headings_preserved=True,heading_count=19,execution_map_and_full_milestone_scope_preserved=True,before_sha256=hashlib.sha256(plan_before.encode()).hexdigest(),after_sha256=hashlib.sha256(prepared['project-plan.md'].encode()).hexdigest()),indent=2)+'\n',encoding='utf-8')
sources=[dict(url='https://github.com/qupath/qupath/blob/v0.7.0/qupath-core-processing/src/main/java/qupath/imagej/processing/'+name,sha256=hashlib.sha256((out/name).read_bytes()).hexdigest()) for name in ('MorphologicalReconstruction.java','Watershed.java')]
(out/'source-references.json').write_text(json.dumps(dict(primary_sources=sources,source_copied_into_native_implementation=False),indent=2)+'\n',encoding='utf-8')
entries += [(out/name,name) for name in ('compilexx-append-proof.json','plan-scope-proof.json','source-references.json')]
target=work/'docs/benchmarks/postlog-trace-304';target.mkdir(exist_ok=False);files=[]
for source,name in entries:
    destination=target/name;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,destination);assert source.read_bytes()==destination.read_bytes()
    files.append(dict(path=name,bytes=destination.stat().st_size,sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
(target/'receipt.json').write_text(json.dumps(dict(**proof,state='private-sampled-compatibility-candidate-and-existing-recipe-regressions-locally-verified',remaining=['Versioned original-source affine sampling and new recipe selection','Shared history/replay/classifier/table identity and all existing consumers','Complete own-source platform, broader science/resource/hardware/support/license/release gates and all M0-M7 exits'],files=files),indent=2)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8',newline='\n') as stream:stream.write('\ndocs/benchmarks/postlog-trace-304/** -text -whitespace\n')
print('Published private304 evidence and integration plan; full milestone scope preserved;',len(files),'receipt members')
