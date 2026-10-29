from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/gaussian-characterization-299'
proof=json.loads((out/'source-runtime-proof.json').read_text());assert proof['source_paths']==1054 and proof['preflight_unique_passes']==1088
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: isolate the remaining cell mismatch by stage299

- While frozen298 qualifies, investigate with public ImageJ operations in the
  already authenticated QuPath0.7.0 runtime. Reuse the original clipped-ROI
  float32 input, public Gaussian/LoG calls and six deterministic small patterns.
  Capture37 cases including thin axes and both sides of the8.5-sigma transition.
  Do not rebuild or change production code to investigate a four-pixel mismatch.
- Two original executions repeat150 payloads exactly. The final public generator
  adds runtime metadata and bridges148 raw input/blur/LoG/kernel payloads to both
  originals. Compare case recipes semantically because the prototype wrote CRLF
  JSON while the final byte-authenticated fixture uses LF. Preserve the initial
  observer assertion failure; never normalize float payloads to repair a proof.
- Independent nearest-edge neighbor differences reproduce all37 ImageJ LoG
  arrays byte-for-byte from captured blur, and the ROI LoG matches the old plugin
  debug stack exactly. At sigma1, ImageJ requested kernel reaches five pixels;
  the native validity-normalized Gaussian reaches three. Extending diagnostic
  support makes the four responses positive but does not reproduce all arithmetic.
  Do not fit an epsilon or silently change the quantitative/default recipe.
- An unchanged native-header GCC probe uses captured float32 OD promoted to
  float64, not the production float64 OD input. Preserve that domain distinction.
  The requested-sigma kernel is also diagnostic when ImageJ uses its large-sigma
  rescale route. A compatible policy must specify input quantization, support,
  edge extension, intermediates and large-sigma behavior together.
- The60 new checks pass on Windows and Linux;760 scientific cases/13 declared
  skips and1088 distinct auditor controls pass. Run only the newly added actual
  collection guard after adding its parameter: all previous control bodies are
  byte-equivalent after removing that one parameter. No need to repeat1087
  unchanged controls. Bind1054 source paths and425 unchanged compiler inputs,
  authenticate71 retained products and39 unchanged installed Windows members.
  No application rebuild or stub regeneration follows this reference-only work.
- Reuse the existing authenticated Linux environment and record package bytes,
  interpreter/dependency versions and the absence of native calls for these
  reference checks. Shared dependencies are explicit; no clean-machine claim.
- Frozen298 keeps its original9fbc9315 source and full original auditors. New
  observerf788d082 needs60 cases its source does not contain; never use the new
  observer to audit old artifacts or restart that running campaign. Three Linux
  archives have authenticated; Windows/portable remain live and Mac queued at
  the recorded observation. No combined qualification or biological/speed/release
  acceptance follows. Preserve every M0-M7 criterion and all remaining workflows.
'''.encode()
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n')
target=work/'docs/benchmarks/gaussian-characterization-299';target.mkdir(exist_ok=False);entries=[]
names=['initial-result.json','repeat-proof.json','native-and-repeat-proof.json','analysis.json','cases.json','command.json','driver.log','probe.groovy','native-probe.cpp','native-probe.log','native-compiler-version.txt','collect.log','api.xml','scientific.xml','scientific.log','scientific-command.json','preflight.xml','preflight.log','preflight-command.json','preflight-collection.xml','preflight-collection.log','final-verification.json','windows-verification.json','source-runtime-proof.json','raw_classifier_evidence-source-proof.json','scientific_grid_evidence-source-proof.json','compilexx-append-proof.json']
entries.extend((out/name,name) for name in names)
entries.extend((out/'linux'/name,'linux/'+name) for name in ('api.log','api.xml','runtime-proof.json','verification.json'))
entries.extend((out/'repeat'/name,'repeat/'+name) for name in ('command.json','driver.log'))
for name in ('diagnose_gaussian299.py','analyze_gaussian299.py','repeat_gaussian299.py','finalize_gaussian_probe299.py','collect_gaussian299_cases.py','validate_gaussian299.py','linux_gaussian299_proof.py','prove_gaussian299.py','publish_gaussian299.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
obs=root/'build/q298-all/20261029T002747Z'
entries.extend((obs/name,'qualification-observation/'+name) for name in ('engine-run.json','macos-run.json','portable-run.json','verified.json','Linux-headless-py3.12-artifact.json','Linux-minimal-py3.12-artifact.json','Linux-sanitize-py3.12-artifact.json'))
cases=ET.parse(root/'build/q298-all/Linux-headless-py3.12/python-tests.xml').findall('.//testcase')
selected=[case for case in cases if case.get('classname')=='tests.test_reader_locale']
assert sorted(c.get('name') for c in selected)==['test_installed_region_dispatch_accepts_unicode_uppercase_ppm','test_unicode_reader_dispatch_and_nominal_resume[C]']
assert all(all(c.find(tag) is None for tag in ('failure','error','skipped')) for c in selected)
(out/'frozen298-linux-reader-checks.json').write_text(json.dumps(dict(source='9fbc9315e8c2417c35492cf505a8f676ecdfcf75',artifact='Linux-headless-py3.12',python_junit_sha256=hashlib.sha256((root/'build/q298-all/Linux-headless-py3.12/python-tests.xml').read_bytes()).hexdigest(),cases=[c.get('name') for c in selected],passed=2,complete_engine_accepted=False),indent=2)+'\n')
entries.append((out/'frozen298-linux-reader-checks.json','qualification-observation/linux-reader-checks.json'))
files=[]
for source,name in entries:
    dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
    files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(target/'receipt.json').write_text(json.dumps(dict(**proof,state='two-host-gaussian-stage-characterization-locally-verified',repeated_original_payloads=150,final_raw_payload_bridge=148,numeric_native_probe_domain='ImageJ float32 OD promoted to float64',remaining=['Private compatible Gaussian and exact RGB-to-OD arithmetic','Versioned opt-in request/archive/replay/frontend policy','Broader biological/scientific/hardware/support/license/release gates and all M0-M7 exits'],files=files),indent=2)+'\n')
paragraph='''[Gaussian299](benchmarks/gaussian-characterization-299/receipt.json) now adds37
public ImageJ stage captures and60 checks on each Windows/Linux host. The
captured blur reproduces all LoG arrays exactly and isolates the clipped-ROI
difference to Gaussian/input processing.760 scientific cases/13 declared skips
and1088 distinct controls pass;425 compiler inputs and application products are
unchanged. This reference-only observer inherits no298 qualification. The next
compatibility step is a private exact Gaussian/input kernel, then one versioned
opt-in policy through existing request/archive/replay and consumers.

'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text();headings=re.findall(r'^#{1,3} .+$',text,re.M)
    marker='**Native original-source nominal cells291';at=text.index(marker);text=text[:at]+paragraph+text[at:]
    assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
plan=work/'docs/project-plan.md';text=plan.read_text();anchor=text.index('## Execution order from the present state')
old='''remaining SCI1-B workflows, the decision lane and REL1 through every M0-M7 exit.'''
new='''remaining SCI1-B workflows, the decision lane and REL1 through every M0-M7 exit.
Gaussian299 isolates the clipped-ROI smoothing/input difference while298 runs.
Implement and validate the private compatible stages against all37 captures
before adding a versioned opt-in policy to the existing request, persistence and
consumer routes. Existing quantitative/default policies remain unchanged.'''
assert text[anchor:].count(old)==1;text=text[:anchor]+text[anchor:].replace(old,new)
original=subprocess.check_output(['git','show','45ca8c59904224d180d527b54fe24ba4166bc374:docs/project-plan.md'],cwd=work).decode()
start='## Current execution map';end='## Execution order from the present state'
assert original[original.index(start):original.index(end)]==text[text.index(start):text.index(end)]
assert re.findall(r'^#{1,3} .+$',original,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
plan.write_text(text,encoding='utf-8',newline='\n')
print('Published299 characterization, preserved plan scope and appended compilexx evidence')
