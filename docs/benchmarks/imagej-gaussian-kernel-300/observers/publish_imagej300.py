from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/imagej-gaussian-kernel-300'
proof=json.loads((out/'source-runtime-proof.json').read_text());assert proof['exact_reference_arrays']==74
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: private Gaussian300 compiles only consumed inputs

- Implement the captured float32 Gaussian and unit-grid LoG as a private C++
  primitive, outside the current detector. Keep X/Y order, truncated-tail edge
  weights, finite-kernel endpoint smoothing, float32 accumulators and large-sigma
  reduction/reconstruction explicit. Do not replace the quantitative Gaussian.
- Compile only its standalone test target, its source and the existing SHA256
  source. No binding translation unit, full application link, stub regeneration
  or packaging is needed. A CMake applicability proof removes the one guarded
  test-target block and recovers the previous CMake file exactly. Production
  targets and all424 other prior compiler inputs remain unchanged. The global
  compiler inventory is429 because four new private/test files are recorded.
- MSVC strict floating point and GCC13.3 with contraction disabled reproduce
  every one of the74 captured blur/LoG arrays across37 cases byte-for-byte. The
  GCC profile uses ASan/UBSan; retain its binary, three objects, all arrays and
  logs before WSL /tmp disappears. Preserve four MSVC products separately.
- First GCC run reached all37 exact cases, then the overflow rejection fixture
  failed:2x2 edges do not add two maximum float neighbors. Correct the fixture
  to3x3; retain the original log, executable and outputs. A preliminary MSVC
  typed-zero conversion warning is corrected before final builds. These are
  observer/test corrections, not a tolerance change or numeric kernel repair.
- Prove actual MSBuild FloatingPointModel=Strict in all configurations. The
  initial observer incorrectly searched for literal /fp:strict in the XML;
  CMake maps that option into the property. Preserve the original observer,
  inspect the structured property and reuse the successful native results.
-761 scientific cases/13 declared skips and1095 audit controls pass with the
  retained native28c33573 installed application. Authenticate all71 app products
  and39 package members; no new private code is linked into that runtime. Bind
  source647576b4,1059 source paths,429 compiler inputs and both private profiles
  separately. The primitive has no Python, CLI, ImGui or archived recipe yet.
- Frozen298 now has accepted original complete portable builders/fresh consumers
  at9fbc9315. Eight archives authenticate, including the successful Windows
  headless role whose four locale/CLI regressions pass. Full engine remains live,
  Mac queued at observation. New scientific/native requirements do not transfer
  to that frozen source or justify restarting it. Keep its original auditors.
- Next capture and reproduce exact RGB-to-float32-OD arithmetic, including
  non-default white/stain paths and requested sampling. Then one versioned opt-in
  processing policy must traverse existing nominal requests, archive/replay,
  Python, CLI/batch and direct/managed UI consumers. Private37-case stage success
  does not close biological, broader input/scale, platform, hardware, support,
  project-license/release or any M0-M7 criterion.
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
entries=[(p,p.relative_to(out).as_posix()) for p in sorted(out.rglob('*')) if p.is_file() and p.suffix in {'.json','.log','.xml','.hpp','.vcxproj','.txt'} and 'windows-products' not in p.parts and 'products' not in p.parts]
for name in ('prepare_imagej300.py','validate_imagej300.py','linux_imagej300.py','prove_imagej300_initial.py','prove_imagej300.py','publish_imagej300.py'):
    entries.append((root/'build/ci'/name,'observers/'+name))
publish('imagej-gaussian-kernel-300',entries,dict(**proof,state='private-kernel-msvc-and-gcc-sanitizers-locally-verified',initial_test_failure='2x2 overflow rejection fixture; corrected3x3',initial_observer_failure='Literal FP switch search failed; actual MSBuild Strict properties verified',remaining=['Exact RGB-to-float32 OD input stage and held-out grids/scales','Versioned opt-in request/archive/replay and all consumers','Broader biological/platform/resource/hardware/support/license/release gates and all M0-M7 exits']))
campaign=root/'build/reader-locale-qualification-298';obs=root/'build/q298-all/20261029T004550Z'
audit=json.loads((campaign/'portable-acceptance.json').read_text());assert audit['state']=='build-and-fresh-consumer-accepted'
entries=[(campaign/n,n) for n in ('portable-run-terminal.json','portable-complete-audit.log','portable-audit-exit.json','portable-acceptance.json')]
entries.extend((obs/n,n) for n in ('portable-run.json','portable-artifacts.json','verified.json','manylinux-3.11-artifact.json','manylinux-3.12-artifact.json','manylinux-consumer-3.11-artifact.json','manylinux-consumer-3.12-artifact.json','Windows-headless-py3.11-artifact.json'))
entries.append((root/'build/ci/audit_portable298_terminal300.py','observers/audit_portable298_terminal300.py'))
cases=ET.parse(root/'build/q298-all/Windows-headless-py3.11/python-tests.xml').findall('.//testcase')
selected=[c for c in cases if c.get('classname')=='tests.test_reader_locale'];assert len(selected)==4
assert all(all(c.find(t) is None for t in ('failure','error','skipped')) for c in selected)
path=out/'frozen298-windows-reader-checks.json';path.write_text(json.dumps(dict(source=audit['commit'],cases=[c.get('name') for c in selected],passed=4,python_junit_sha256=hashlib.sha256((root/'build/q298-all/Windows-headless-py3.11/python-tests.xml').read_bytes()).hexdigest(),complete_engine_accepted=False),indent=2)+'\n')
entries.append((path,path.name))
publish('reader-locale-qualification-298/portable300',entries,dict(schema='qpx.reader-locale-portable-role/1',state='original-complete-portable-role-accepted',source=audit['commit'],run=40792343129,authenticated_portable_archives=4,total_collected_archives=8,complete_original_auditor=True,windows_headless_reader_regressions_passed=4,engine_accepted=False,macos_accepted=False,combined_accepted=False,main_promoted=False,release_ready=False))
old='''[Gaussian299](benchmarks/gaussian-characterization-299/receipt.json) now adds37
public ImageJ stage captures and60 checks on each Windows/Linux host. The
captured blur reproduces all LoG arrays exactly and isolates the clipped-ROI
difference to Gaussian/input processing.760 scientific cases/13 declared skips
and1088 distinct controls pass;425 compiler inputs and application products are
unchanged. This reference-only observer inherits no298 qualification. The next
compatibility step is a private exact Gaussian/input kernel, then one versioned
opt-in policy through existing request/archive/replay and consumers.
'''
new='''[Private Gaussian300](benchmarks/imagej-gaussian-kernel-300/receipt.json) now
matches all74 blur/LoG arrays from37 captured cases on MSVC and GCC ASan/UBSan.
Its isolated target leaves71 application products and39 installed members
unchanged at native28c33573.761 scientific cases/13 declared skips and1095 audit
controls pass;1059 source paths and429 compiler inputs authenticate. Exact
RGB-to-float32-OD staging and a versioned opt-in request/archive/replay/consumer
policy remain. The original [portable298 audit](benchmarks/reader-locale-qualification-298/portable300/receipt.json)
accepts its frozen builders/fresh consumers; engine/Mac and broader gates remain
open. Private300 inherits no298 platform acceptance.
'''
for name in ('current-task.md','feature-matrix.md','project-plan.md','api-delivery-plan.md'):
    path=work/'docs'/name;text=path.read_text();headings=re.findall(r'^#{1,3} .+$',text,re.M)
    assert text.count(old)==1;text=text.replace(old,new)
    assert re.findall(r'^#{1,3} .+$',text,re.M)==headings
    path.write_text(text,encoding='utf-8',newline='\n')
path=work/'docs/project-plan.md';text=path.read_text();anchor=text.index('## Execution order from the present state')
old='''Gaussian299 isolates the clipped-ROI smoothing/input difference while298 runs.
Implement and validate the private compatible stages against all37 captures
before adding a versioned opt-in policy to the existing request, persistence and
consumer routes. Existing quantitative/default policies remain unchanged.'''
new='''Private Gaussian300 reproduces all37 captured stages on MSVC/GCC sanitizers
while298 qualifies. Next capture exact RGB-to-float32-OD arithmetic and held-out
grids/scales, then add one versioned opt-in processing policy through the existing
request, persistence and consumer routes. Existing quantitative/default policies
remain unchanged.'''
assert text[anchor:].count(old)==1;text=text[:anchor]+text[anchor:].replace(old,new)
original=subprocess.check_output(['git','show','adac8ca9124677ac8ee4230e9eec6821360a8acd:docs/project-plan.md'],cwd=work).decode();start='## Current execution map';end='## Execution order from the present state'
assert original[original.index(start):original.index(end)]==text[text.index(start):text.index(end)]
assert re.findall(r'^#{1,3} .+$',original,re.M)==re.findall(r'^#{1,3} .+$',text,re.M)
path.write_text(text,encoding='utf-8',newline='\n')
print('Published private300 and original portable298 acceptance; full plan scope preserved')
