from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/qupath-input-stages-301'
proof=json.loads((out/'source-runtime-proof.json').read_text());assert proof['source_paths']==1158
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: reuse exact OD/stain input stages301; fix isolated observer

- Capture public IJTools OD channels/sum and RGB stain separation before writing
  another native transform. Eleven cases include all pairwise byte-channel
  combinations, six white settings (including fractional/minimum/mixed whites),
  two-/three-vector and identity/signed bases, clipped ROI and nominal1.5 RGB.
  Existing StainTransform optical_densities/deconvolve match all77 captured
  arrays and33 normalized-vector checks exactly on Windows28c33573 and the
  authenticated Linux9fbc9315 wheel. Reuse this implementation; no new LUT or
  matrix engine, application rebuild, binding or stub regeneration is needed.
- OD-sum rounds each channel to float32 before two float32 additions. Stain
  separation mixes double OD before the final float32 cast. The independent
  float64-sum-once model differs on10 nontrivial cases; record that diagnostic,
  not a fitted epsilon. The NumPy oracle initially kept negative zero when its
  maximum operands tied. Java Math.max(+0,-0) yields positive zero. Correct
  only the oracle's zero sign and retain its diagnostic; captured/native bits
  remain untouched. No tolerance is weakened.
- Initial Groovy collectWithIndex is unsupported in this pinned runtime. Keep
  the failed log/script, use existing supported eachWithIndex and a new output
  directory. Two successful executions repeat92 numeric/model/input/recipe
  payloads exactly; the promoted generator additionally records runtime metadata.
-45 new checks pass each Windows/Linux host.806 scientific cases/13 declared
  skips and1096 controls pass. Bind1158 source paths,429 unchanged compiler
  inputs,71 unchanged application products, eight unchanged private-kernel
  products and39 unchanged Windows package members. Native math is unchanged.
  Install the authenticated Linux298 wheel into an owned durable venv with no
  index/dependencies, reusing pinned test dependencies and recording all package
  bytes/source/versions. This is not clean-machine evidence.
- Original298 engine is terminal failed: Windows GUI has5330 passes, three
  isolated reader-test helper import failures,11 skips and three strict xfails.
  The full original engine auditor rejects. Its software Python uses _pth and
  ignores PYTHONPATH; the locale test imported a repository helper before it
  reached any reader check. Headless Windows and all Linux roles passed; portable
  original acceptance remains valid. Mac arm64 is still pending, x86 archive
  authenticated. Preserve all10 current archives and original failure logs.
- Reproduce the frozen helper failure with Python isolation, then explicitly
  pass the tests directory and insert it inside the child after importing the
  installed product. Always use child -I. Four repaired cases pass in normal
  Windows and an actual copied _pth interpreter with a deliberately unusable
  PYTHONPATH; two pass in the new authenticated Linux environment. This exercises
  the failing isolation boundary without weakening it or rebuilding native code.
  Shared dependencies/rendering scope are explicit. Do not change frozen298,
  redispatch it or turn these local observer results into combined acceptance.
- Next integrate the exact input path and private Gaussian through one explicit
  processing policy, existing requests, historical identity, replay and all
  consumers. Broader grids/alpha/edges, biological, hardware/support/license/
  release gates and every M0-M7 exit remain open.
'''.encode()
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n')
def publish(folder,entries,receipt):
    target=work/'docs/benchmarks'/folder;target.mkdir(exist_ok=False);files=[]
    for source,name in entries:
        dest=target/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(source,dest);assert source.read_bytes()==dest.read_bytes()
        files.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    (target/'receipt.json').write_text(json.dumps(dict(**receipt,files=files),indent=2)+'\n')
names=['command.json','driver.log','probe.groovy','cases.json','original-isolated-reader-red.json','reader-isolated.log','reader-isolated.xml','reader-pth.log','reader-pth.xml','reader-pth-command.json','reader-pth-identity.json','initial-comparison.json','final-native-comparison.json','windows-native-comparison.json','numpy-signed-zero-observer.json','repeat-proof.json','collect.log','api.xml','api.log','scientific.xml','scientific.log','scientific-command.json','preflight.xml','preflight.log','preflight-command.json','source-runtime-proof.json','raw_classifier_evidence-source-proof.json','scientific_grid_evidence-source-proof.json','compilexx-append-proof.json']
# The successful prototype comparison remains in its owned second capture.
entries=[((out/'capture2' if name=='initial-comparison.json' else out)/name,name) for name in names]
entries.extend((out/'capture2'/n,'prototype/'+n) for n in ('command.json','driver.log','probe.groovy'))
entries.extend((out/'linux'/n,'linux/'+n) for n in ('commands.json','venv.log','install.log','identity.log','installed-proof.json','api.log','api.xml','native-comparison.log','native-comparison.json'))
for name in ('diagnose_input301.py','resume_input301.py','promote_input301_probe.py','inspect_odzero301.py','collect_input301_cases.py','validate_input301.py','linux_input301.py','reproduce_reader_isolation301.py','reader_pth301.py','prove_input301.py','publish_input301.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
publish('qupath-input-stages-301',entries,dict(**proof,state='two-host-existing-native-input-stages-and-isolated-reader-observer-locally-verified',remaining=['Opt-in request/archive/replay/frontend processing policy','Held-out grids/alpha/edge and wider specimen validation','Source-bound platform/hardware/support/license/release gates and all M0-M7 exits']))
campaign=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T011206Z'
assert json.loads((campaign/'engine-audit-exit.json').read_text())['returncode']!=0
entries=[(campaign/n,n) for n in ('engine-run-terminal.json','engine-complete-audit.log','engine-audit-exit.json')]
entries.extend((obs/n,n) for n in ('engine-run.json','engine-artifacts.json','macos-run.json','verified.json','Windows-gui-py3.12-artifact.json'))
gui=root/'build/q298-all/Windows-gui-py3.12'
entries.extend((gui/n,n) for n in ('python-tests.xml','python-tests.log','native-tests.xml','native-tests.log','diagnostic-stages.json'))
entries.append((root/'build/q298-all/123180291207-failed-job.log','failed-job.log'))
entries.append((root/'build/ci/audit_engine298_terminal301.py','observers/audit_engine298_terminal301.py'))
cases=ET.parse(gui/'python-tests.xml').findall('.//testcase');failed=[c for c in cases if c.find('failure') is not None or c.find('error') is not None]
assert len(failed)==3 and all(c.get('classname')=='tests.test_reader_locale' and "No module named 'test_nominal_cell_batches'" in c.find('failure').get('message','') for c in failed)
publish('reader-locale-qualification-298/engine301',entries,dict(schema='qpx.reader-locale-engine-observation/1',state='original-engine-audit-rejected',source='9fbc9315e8c2417c35492cf505a8f676ecdfcf75',run=40792323979,authenticated_engine_archives=5,total_collected_archives=10,failed_gui_cases=[c.get('name') for c in failed],original_auditor=True,engine_accepted=False,portable_accepted=True,macos_accepted=False,combined_accepted=False,main_promoted=False,release_ready=False))
old='''[Private Gaussian300](benchmarks/imagej-gaussian-kernel-300/receipt.json) now
matches all74 blur/LoG arrays from37 captured cases on MSVC and GCC ASan/UBSan.
Its isolated target leaves71 application products and39 installed members
unchanged at native28c33573.761 scientific cases/13 declared skips and1095 audit
controls pass;1059 source paths and429 compiler inputs authenticate. Exact
RGB-to-float32-OD staging and a versioned opt-in request/archive/replay/consumer
policy remain. The original [portable298 audit](benchmarks/reader-locale-qualification-298/portable300/receipt.json)
accepts its frozen builders/fresh consumers; engine/Mac and broader gates remain
open. Private300 inherits no298 platform acceptance.
'''
new='''[Input301](benchmarks/qupath-input-stages-301/receipt.json) verifies existing
native OD/stain transforms against all77 captured arrays on Windows/Linux,
including non-default whites, bases and original ROI/resampled inputs.45 new
checks per host,806 scientific cases/13 declared skips and1096 controls pass.
429 compiler inputs,71 application and eight private products remain unchanged;
1158 source paths authenticate. Private Gaussian300 already matches74 arrays.
The [original298 engine audit](benchmarks/reader-locale-qualification-298/engine301/receipt.json)
rejects three GUI test-helper imports under isolated Python. The observer repair
passes normal Windows, actual _pth and Linux; no native repair or acceptance
transfer follows. Portable accepts; Mac arm64 remains pending with10 archives
authenticated. Next deliver the explicit processing policy through existing
requests, archive/replay and all consumers. Broader M0-M7 gates remain open.
'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text();headings=re.findall(r'^#{1,3} .+$',text,re.M)
    assert text.count(old)==1;text=text.replace(old,new);assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text();anchor=text.index('## Execution order from the present state')
old='''Private Gaussian300 reproduces all37 captured stages on MSVC/GCC sanitizers
while298 qualifies. Next capture exact RGB-to-float32-OD arithmetic and held-out
grids/scales, then add one versioned opt-in processing policy through the existing
request, persistence and consumer routes. Existing quantitative/default policies
remain unchanged.'''
new='''Private Gaussian300 reproduces all37 captured stages on MSVC/GCC sanitizers;
input301 verifies exact existing native OD/stain inputs on Windows/Linux.298's
original engine auditor rejects isolated test-helper imports; the observer is
locally repaired without rebuilding application code. Portable accepts; keep
observing the original Mac handle. Next add one versioned opt-in processing
policy through existing requests, persistence/replay and all consumers, with
held-out grids/alpha/edges. Existing quantitative/default policies stay unchanged.'''
assert text[anchor:].count(old)==1;text=text[:anchor]+text[anchor:].replace(old,new)
original=subprocess.check_output(['git','show','a3830c1d6b972f8aa61968da2933aa7d42cd830d:docs/project-plan.md'],cwd=work).decode();start='## Current execution map';end='## Execution order from the present state'
assert original[original.index(start):original.index(end)]==text[text.index(start):text.index(end)]
assert re.findall(r'^#{1,3} .+$',original,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
path.write_text(text,encoding='utf-8',newline='\n')
print('Published exact input301, isolated observer repair and original engine298 rejection; full plan scope preserved')
