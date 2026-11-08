from pathlib import Path
import hashlib
import json
import shutil
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/qupath-rgb-resampling-286'
public=work/'docs/benchmarks/qupath-rgb-resampling-286'
identity=json.loads((out/'source-runtime-proof.json').read_text())
assert identity['comparison_source'].startswith('83cd41df')
assert identity['native_source']=='7231d95c25fbc9515d937036986e8c92b19da261'
def outcomes(name):
    cases=ET.parse(out/(name+'.xml')).findall('.//testcase')
    assert not any(c.find('failure') is not None or c.find('error') is not None for c in cases)
    return dict(passed=sum(c.find('skipped') is None for c in cases),
        skipped=sum(c.find('skipped') is not None for c in cases),
        expected_failures=sum(c.find('skipped') is not None and c.find('skipped').get('type')=='pytest.xfail' for c in cases))
assert outcomes('installed2')==dict(passed=173,skipped=3,expected_failures=3)
assert outcomes('scientific')==dict(passed=359,skipped=7,expected_failures=0)
assert outcomes('preflight')==dict(passed=1054,skipped=0,expected_failures=0)
public.mkdir(exist_ok=False)
files=[]
def copy(src,name):
    target=public/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(src,target)
    assert src.read_bytes()==target.read_bytes()
    files.append(dict(path=name,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for name in ('capture.log','capture-exit.json','capture2.log','capture2-exit.json',
             'capture3-runtime.log','repeat-command.log','repeat-native-report.json',
             'installed.log','installed.xml','installed2.log','installed2.xml',
             'scientific.log','scientific.xml','scientific-command.json','scientific-exit.json','scientific-admission.json',
             'preflight.log','preflight.xml','preflight-command.json','preflight-exit.json',
             'scientific-source-proof.json','source-runtime-proof.json','repeat-proof.json','repeat-file-proof.json'):
    copy(out/name,name)
for p in (out/'attempt1').iterdir():
    copy(p,'initial-capture-source/'+p.name)
for name in ('probe_rgb_resampling_286.py','install_rgb_fixture286.py','register_rgb_cohort286.py',
             'validate_rgb286.py','prove_rgb286.py','publish_rgb286.py'):
    copy(root/'build/ci'/name,'tools/'+name)
receipt=dict(schema='qpx.qupath-rgb-resampling-local/1',state='local-reference-characterization-accepted',
    comparison_source=identity['comparison_source'],native_source=identity['native_source'],
    captures=2,synthetic_inputs=4,read_requests=36,exact_repeated_payloads_including_manifest=43,
    new_installed_cases=82,related_installed=outcomes('installed2'),
    complete_scientific_cohort=outcomes('scientific'),audit_controls=outcomes('preflight'),
    counts_overlap=True,source_paths=661,unchanged_compiler_inputs=415,unchanged_retained_products=71,
    native_sampling_changed=False,product_rebuilt=False,local_binary_relabelled=False,
    scope='Opaque RGB public WrappedBufferedImageServer requests; no pyramid/vendor/alpha/biological acceptance.',
    initial_capture_failure='Public script used unavailable getQuPathVersion(); corrected to existing public GeneralTools.getVersion(). Initial payload/logs preserved.',
    initial_test_collection_failure='pytest reserves request as a parameter name; corrected to request_name before successful run.',
    legacy_strict_expected_failures_preserved=3,shared_test_dependencies=True,
    sampler_equivalence_accepted=False,biological_validation=False,
    own_source_platform_qualified=False,general_speed_claim=False,release_ready=False,files=files)
(public/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    stream.write('\ndocs/benchmarks/qupath-rgb-resampling-286/** -text -whitespace\n')
paragraph='''**RGB input resampling286 is locally characterized at comparison83cd41df.**
The [receipt](benchmarks/qupath-rgb-resampling-286/receipt.json) binds36 pinned
public QuPath RGB reads, two complete byte-identical captures and82 new installed
checks. Related comparisons pass173 with the three legacy strict expected
failures unchanged; the full named scientific cohort passes359/seven declared
GUI skips and1054 audit controls pass. Counts overlap.661 scientific source paths
authenticate;415 compiler inputs and71 retained products remain unchanged at
native7231d95. No rebuild, native sampler change or acceptance transfer follows.
The [production route](qupath-rgb-resampling.md) now requires separate nominal
spacing, enlargement kernel, edge behavior and byte-rounding controls; the
earlier single1.5 case cannot define a general bilinear compatibility API.
Keep default quantitative policies and capture/replay authority explicit.
Frozen font285 continues independently with its original auditors: newer
scientific auditors require82 cases that its unchanged source does not contain.
Broader specimen/tolerance, physical UI, hardware/support/release and M0-M7 gates
remain open.
'''
for name in ('current-task.md','project-plan.md','feature-matrix.md','api-delivery-plan.md'):
    path=work/'docs'/name
    text=path.read_text(encoding='utf-8').replace('complete own-source qualification is queued.','complete own-source qualification is running.')
    if name=='current-task.md':
        marker='**Current qualified feature baseline:'
        position=text.index(marker)
        text=text[:position]+paragraph+'\n'+text[position:]
    else:
        title,rest=text.split('\n',1)
        text=title+'\n\n'+paragraph+'\n'+rest.lstrip('\n')
    path.write_text(text,encoding='utf-8')
path=work/'docs/qupath-rgb-resampling.md'
text=path.read_text(encoding='utf-8')
text=text.replace('## What the captures establish',
    'The [local receipt](benchmarks/qupath-rgb-resampling-286/receipt.json) binds\nsource83cd41df, retained native7231d95, complete scientific admission and\nunchanged compiler/product evidence. New-source platform acceptance remains open.\n\n## What the captures establish',1)
path.write_text(text,encoding='utf-8')
note=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
prefix=note.read_bytes()
addition='''

## RGB resampling286: avoid generalizing one matching synthetic case (2026-10-28)

The legacy distance recipe's three strict expected failures do not describe all
cell modes: the existing explicit-grid LoG isolated gates already pass. Target
the real remaining input-recipe gap instead of weakening those failures.
Extend the original1.5 processed-cell example to36 public QuPath RGB requests on
four synthetic inputs. Enlargement matches nominal nearest, odd-sized2x has
black destination edges, and2.3 reductions retain one-byte bilinear differences.
A single matched bilinear example is insufficient to specify a production API.
Two complete captures reproduce43 payload/manifest files exactly;82 new named
checks,359 scientific passes/seven declared GUI skips and1054 controls verify.
Related173 passes retain all three legacy expected failures. All415 compiler
inputs and71 retained products stay unchanged; no product rebuild or broad CI
dispatch is needed for this reference/observer change. Frozen font285 retains
its own original auditors, because this newer auditor requires82 additional cases.
Carry nominal spacing, interpolation/edge/rounding policy and source authority
through any future explicit native compatibility descriptor. Preserve existing
quantitative policies; do not infer biological, vendor/pyramid or speed parity.
The initial public script metadata method was unavailable; use the existing
GeneralTools.getVersion() public call. A reserved pytest parameter name stopped
initial collection; preserve both errors separately from product behavior.
'''
note.write_bytes(prefix+addition.encode('utf-8'))
assert note.read_bytes()[:len(prefix)]==prefix
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(prefix_bytes=len(prefix),
    prefix_sha256=hashlib.sha256(prefix).hexdigest(),prefix_unchanged=True),indent=2)+'\n')
print('Published RGB286:',len(files),'evidence files; product unchanged; full scientific cohort359/7 and1054 controls',flush=True)
