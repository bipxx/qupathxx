from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
proof=json.loads((out/'source-runtime-proof.json').read_text(encoding='utf-8'));assert proof['source'].startswith('874b6342')
run=json.loads(subprocess.check_output(['gh','run','view','40792333489','--repo','bipxx/qupathxx','--json','databaseId,headSha,status,conclusion,jobs,url'],encoding='utf-8'))
assert run['headSha']=='9fbc9315e8c2417c35492cf505a8f676ecdfcf75'
(out/'original298-macos-observation.json').write_text(json.dumps(run,indent=2)+'\n',encoding='utf-8')
diagnostics=[dict(stage='initial capture observer',failure='Expected five named debug stages, but large sigma has the additional Refined boundaries stage',repair='Validate exact stage names/count from the pinned processing-sigma branch; preserve all captured bytes'),
 dict(stage='initial exact RGB assertion',failure='82 components differ by one byte in odd-origin step1.5 RGB',repair='Preserve the failure as an unresolved sampler limitation; report all cases without claiming input or segmentation parity'),
 dict(stage='initial alpha fixture',failure='Image.from_numpy requires RGB; HWC4 fixture refused before reaching sampler',repair='Use an openly licensed native TIFF RGBA source to test actual nominal opacity refusal; retain original three failures'),
 dict(stage='initial diagnostic processing ROI',failure='ROI extent exceeded the new diagnostic canvas',repair='Clip diagnostic ROI extent while preserving eligible centers; retain initial script and distinguish this new source from original-slide replay authority'),
 dict(stage='initial raw source proof',failure='New raw fixture prefix omitted, causing CRLF normalization of a pinned contract',repair='Add only the new raw fixture prefix; do not rewrite any capture bytes'),
 dict(stage='initial package proof',failure='Confused 43 wheel RECORD rows with installed RECORD rows',repair='Check wheel43 and installed49 against prior authenticated package; all39 product members remain identical')]
(out/'observer-diagnostics.json').write_text(json.dumps(dict(observed_failures=diagnostics,native_math_changed=False,captured_payloads_changed=False,tolerances_weakened=False),indent=2)+'\n',encoding='utf-8')
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: held-out workflow303 finds post-LoG gaps before qualification

- Do not extrapolate the nine zero-context synthetic matches302 into general
  detector parity. Capture13 new slide-edge, odd-origin crop/hole, fractional
  step and zero/large-sigma workflows through the pinned public plugin. Two
  executions repeat146 shared payloads exactly. Five new masks match; eight
  remain explicit gaps.32 new checks include characterization, opacity refusal,
  budget/calibration admission and source-supported negative extents. These are
  engineering observations, not biological acceptance.
- Isolate stages before changing math. Link the authenticated strict-FP300
  Gaussian object into one small diagnostic driver: all13 new LoG arrays are
  byte exact. Do not write a second Gaussian or rebuild the application. Captured
  processing RGB with a metric-preserving diagnostic source retains all eight
  final mask gaps. Reader correction alone cannot close them. Next compare the
  shared native post-LoG regions, splitting, binary/refinement and final labels,
  starting with sigma zero and exact-input step2.3 cases.
- Odd-origin step1.5 crops expose82 one-byte RGB component differences each.
  The public Java affine inverse gives first X center7.749999999999999 instead
  of the integer-decomposed7.75. A captured-inverse32.32/eight-bit-weight model
  matches13 new plus99 earlier reads exactly. Keep this diagnostic distinct from
  delivered sampling. Specify an additive request/recipe version and preserve
  version1 replay before exposing it through the existing shared consumers.
- The initial observer assumed five debug stages. Large sigma has an additional
  public refinement stage. Fix the observer from the pinned branch, preserving
  capture bytes. The initial alpha test used an RGB-only Image constructor;
  use actual RGBA TIFF input to reach the opacity boundary. Retain the three
  initial failures. Source admission also caught a missing raw fixture prefix;
  extend that prefix rather than normalizing pinned CRLF data. Distinguish43
  wheel RECORD rows from49 installed rows when reusing exact package evidence.
-858 scientific checks/18 declared skips and1098 auditor controls pass. Bind1310
  source paths and prove429 compiler inputs,76 application products, eight prior
  private products and39 installed members unchanged. Only the small diagnostic
  driver is compiled; its three products remain separate. No application/stub
  rebuild, old CI restart, platform acceptance transfer or speed claim follows.
  Full M0-M7, broader research/resources/hardware/support/license/release gates
  remain open. Reorder the next work around measured post-LoG and affine gaps
  before one complete own-source qualification.
'''.encode()
assert b'workflow303 finds' not in before
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n',encoding='utf-8')

new='''[Held-out303](benchmarks/cell-policy-edges-303/receipt.json) captures13 new
slide-edge, odd-crop/hole, fractional-step and zero/large-sigma workflows.146
shared payloads repeat exactly. Five new masks match and eight remain unresolved.
All13 LoG arrays match the retained private kernel; a captured-affine diagnostic
matches112 RGB reads. Captured processing RGB retains the eight downstream mask
gaps.32 new checks,858 scientific cases/18 declared skips and1098 audit controls
pass; counts overlap.1310 source paths authenticate;429 compiler inputs,76
application products, eight private products and39 installed members remain
unchanged at native2b4786ed. No application or stub rebuild occurs. Next isolate
post-LoG differences and specify the additive affine recipe through existing
requests/consumers; see the [gap route](cell-policy-heldout-gaps.md). The delivered
[policy302](benchmarks/cell-processing-policy-302/receipt.json), default policies
and three legacy scientific xfails remain intact. Own-source platform, broader
science/resources/hardware/support/license/release gates and all M0-M7 exits stay
open. Preserve qualified281 and original298 outcomes without redispatch or
acceptance transfer.'''
plan_before=(work/'docs/project-plan.md').read_text(encoding='utf-8');old=None
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text(encoding='utf-8');headings=re.findall(r'^#{1,3} .+$',text,re.M)
    start=text.index('[Policy302]');end=text.index('\n\n',start)
    if old is None:old=text[start:end]
    assert text[start:end]==old
    text=text[:start]+new+text[end:];assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text(encoding='utf-8');anchor=text.index('## Execution order from the present state')
old_order='''Policy302 now integrates the exact input/Gaussian path through existing
requests, persistence/replay and all consumers, with local Windows and affected
GCC sanitizer evidence. Finish held-out grids/alpha/edge characterization and
freeze one complete successor for own-source qualification. Existing default
quantitative results and historical recipes remain unchanged.298's original
engine rejection and portable acceptance stay source-bound; observe its original
Mac handle without redispatch. Broader scientific and release lanes remain open.'''
new_order='''Policy302 remains locally delivered through all consumers. Held-out303 exposes
eight additional detector mask gaps while all13 new LoG stages match exactly.
First isolate shared post-LoG regions/splitting/refinement against captured
stages; specify and deliver the separately versioned affine-sampling correction
through existing requests and consumers. Close the named workflow gaps before
freezing one combined own-source qualification. Preserve defaults, version1
historical recipes and298's source-bound outcomes; observe its original Mac
handle without redispatch. Broader scientific and release lanes remain open.'''
assert text[anchor:].count(old_order)==1;text=text[:anchor]+text[anchor:].replace(old_order,new_order)
begin='## Current execution map';end='## Execution order from the present state'
assert plan_before[plan_before.index(begin):plan_before.index(end)]==text[text.index(begin):text.index(end)]
assert re.findall(r'^#{1,3} .+$',plan_before,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
path.write_text(text,encoding='utf-8',newline='\n')
(out/'plan-scope-proof.json').write_text(json.dumps(dict(all_headings_preserved=True,heading_count=19,execution_map_and_full_milestone_scope_preserved=True,before_sha256=hashlib.sha256(plan_before.encode()).hexdigest(),after_sha256=hashlib.sha256(text.encode()).hexdigest()),indent=2)+'\n',encoding='utf-8')
path=work/'docs/api-delivery-plan.md';text=path.read_text(encoding='utf-8')
old_queue='''Policy302 completes the explicit numerical-policy consumer slice locally. Reuse
its native request/archive/controller path; do not restart SDK or frontend work.
Next characterize held-out numerical/edge cases, then freeze one combined
own-source qualification. Keep original298's source-specific outcomes separate;
all broader research, performance and release lanes remain required.'''
new_queue='''Policy302 completes the explicit numerical-policy consumer slice locally. Reuse
its native request/archive/controller path. Held-out303 now identifies eight
post-LoG mask gaps and a separate affine-phase sampling discrepancy. Follow the
[measured gap route](cell-policy-heldout-gaps.md): isolate shared post-LoG stages,
deliver additive versioned sampling and detector behavior through existing
consumers, then freeze one complete own-source qualification. Do not repeat the
already exact transforms/Gaussian or completed SDK/frontend delivery. Preserve
original298 outcomes; all broader research/performance/release lanes remain.'''
assert text.count(old_queue)==1;path.write_text(text.replace(old_queue,new_queue),encoding='utf-8',newline='\n')

target=work/'docs/benchmarks/cell-policy-edges-303';target.mkdir(exist_ok=False)
names=['source-runtime-proof.json','installed-proof.json','raw_classifier_evidence-source-proof.json','scientific_grid_evidence-source-proof.json','repeat-proof.json','capture2-verification.json','native-characterization.json','affine-probe.groovy','affine-reference.json','affine-driver.log','affine-command.json','affine-model-comparison.json','affine-heldout-requests.json','affine-heldout-probe.groovy','affine-heldout-reference.json','affine-heldout-driver.log','affine-heldout-command.json','affine-heldout-comparison.json','kernel-probe-proof.json','kernel-configure.log','kernel-build.log','postlog-comparison.json','collection.log','api-final.log','api-final.xml','scientific.log','scientific.xml','scientific-command.json','preflight.log','preflight.xml','preflight-command.json','api-initial-image-factory-red.log','api-initial-image-factory-red.xml','api.log','api.xml','initial-five-stage-observer.py','postlog-initial-unclipped-roi.py','prove-initial-raw-prefix-red.py','observer-diagnostics.json','compilexx-append-proof.json','plan-scope-proof.json','original298-macos-observation.json']
entries=[(out/name,name) for name in names]
for name in ('probe.cpp','CMakeLists.txt'):entries.append((out/'kernel-probe'/name,'kernel-probe/'+name))
entries.append((out/'kernel-probe/build/gaussian303.vcxproj','kernel-probe/gaussian303.vcxproj'))
for name in ('characterize_policy303.py','diagnose_affine303.py','affine_holdout303.py','probe_kernel303.py','diagnose_postlog303.py','collect_policy303_cases.py','validate_policy303.py','prove_policy303.py','publish_policy303.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
files=[]
for source,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
    files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
assert len(files)==len({item['path'] for item in files})
(target/'receipt.json').write_text(json.dumps(dict(**proof,state='held-out-workflows-characterized-with-eight-explicit-segmentation-gaps',remaining=['Native post-LoG stage diagnosis and versioned compatibility behavior','Additive affine sampling option through existing requests, history and consumers','Complete own-source platform and all broader M0-M7 research/resource/hardware/support/license/release gates'],files=files),indent=2)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8',newline='\n') as stream:stream.write('\ndocs/benchmarks/cell-policy-edges-303/** -text -whitespace\n')
print('Published303 characterization and revised the next work around verified causes;',len(files),'receipt members; original Mac298',run['status'],run['conclusion'])
