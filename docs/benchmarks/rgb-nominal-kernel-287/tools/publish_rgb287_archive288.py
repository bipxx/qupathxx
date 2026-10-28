from pathlib import Path
import hashlib
import json
import shutil
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
kernel=root/'build/rgb-nominal-287'
archive=root/'build/font-archive-288'
campaign=root/'build/gui-font-qualification-285'
observation=root/'build/q285-all/20261028T190527Z'
def outcomes(path):
    cases=ET.parse(path).findall('.//testcase')
    return dict(passed=sum(all(c.find(t) is None for t in ('failure','error','skipped')) for c in cases),
        failures=sum(c.find('failure') is not None or c.find('error') is not None for c in cases),
        skipped=sum(c.find('skipped') is not None for c in cases))
assert outcomes(kernel/'native.xml')==dict(passed=1,failures=0,skipped=0)
assert outcomes(kernel/'linux-native.xml')==dict(passed=1,failures=0,skipped=0)
assert outcomes(kernel/'scientific.xml')==dict(passed=371,failures=0,skipped=7)
assert outcomes(kernel/'preflight.xml')==dict(passed=1057,failures=0,skipped=0)
assert outcomes(archive/'old-archiver.xml')==dict(passed=1,failures=1,skipped=0)
assert outcomes(archive/'new-archiver.xml')==dict(passed=2,failures=0,skipped=0)
assert outcomes(archive/'preflight.xml')==dict(passed=1059,failures=0,skipped=0)
assert json.loads((kernel/'global-domain-model.json').read_text())['exact']==99
assert len(json.loads((kernel/'holdout-repeat-proof.json').read_text())['files'])==72
kernel_proof=json.loads((kernel/'source-runtime-proof.json').read_text())
archive_proof=json.loads((archive/'source-runtime-proof.json').read_text())
repair=json.loads((archive/'receipt-local.json').read_text())
assert kernel_proof['native_source']=='23edd7ed1f15c87438602ec3b22e5506793a3a59'
assert repair['archiver_source']=='01c3b9aadd36ee4e06ba759929c19a9e2073507c'

def publication(folder):
    target=work/'docs/benchmarks'/folder
    target.mkdir(exist_ok=False)
    entries=[]
    def copy(path,name):
        dest=target/name
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,dest)
        assert path.read_bytes()==dest.read_bytes()
        entries.append(dict(path=name,bytes=dest.stat().st_size,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
    return target,entries,copy

public,files,copy=publication('rgb-nominal-kernel-287')
for p in kernel.iterdir():
    if p.is_file() and p.suffix in ('.json','.xml','.log','.hpp','.txt'):
        copy(p,p.name)
for name in ('rgb_quantization_probe287.py','holdout_rgb287.py','rgb_global_model287.py','install_holdout287.py',
             'register_rgb287.py','build_rgb287.py','linux_rgb287.py','repeat_holdout287.py','prove_rgb287.py',
             'validate_rgb286.py','publish_rgb287_archive288.py'):
    copy(root/'build/ci'/name,'tools/'+name)
kernel_receipt=dict(schema='qpx.rgb-nominal-private-kernel/1',state='windows-and-gcc-sanitized-private-kernel-accepted',
    native_source=kernel_proof['native_source'],observer_source=kernel_proof['observer_source'],
    exact_captured_native_cases=99,baseline_cases=36,held_out_cases=63,
    held_out_complete_captures=2,exact_repeated_holdout_payloads=72,
    source_paths=752,canonical_compiler_inputs=418,retained_new_products=4,retained_gui_products_unchanged=71,
    windows_native_suites=1,gcc_asan_ubsan_native_suites=1,python_reference_cases=12,
    python_scientific_fixture_cohort=dict(passed=371,declared_gui_skips=7),audit_controls=1057,
    python_fixture_runtime_source='7231d95c25fbc9515d937036986e8c92b19da261',
    counts_overlap=True,private_kernel_only=True,public_python_bindings_added=False,
    reader_cli_gui_consumers_delivered=False,default_sampling_changed=False,
    opaque_rgb_only=True,scientific_acceptance=False,general_speed_claim=False,
    own_source_platform_qualified=False,release_ready=False,files=files)
(public/'receipt.json').write_text(json.dumps(kernel_receipt,indent=2)+'\n',encoding='utf-8')

public,files,copy=publication('font-archive-288')
for p in archive.iterdir():
    if p.is_file() and p.suffix in ('.json','.xml','.log'):
        copy(p,p.name)
for name in ('portable-acceptance.json','portable-audit-exit.json','portable-complete-audit.log',
             'portable-run-terminal.json','engine-audit-exit.json','engine-complete-audit.log','complete-audit-plan.json'):
    copy(campaign/name,'original285/'+name)
for p in observation.iterdir():
    if p.is_file() and p.suffix=='.json':copy(p,'original285/observation/'+p.name)
for name in ('123012238234-failed-job.log',):copy(root/'build/q285-all'/name,'original285/'+name)
for name in ('native-tests.xml','python-tests.xml','diagnostic-stages.json','standalone-gui.log','standalone-slide-gui.log'):
    copy(root/'build/q285-all/Windows-gui-py3.12'/name,'original285/windows-gui/'+name)
for name in ('audit_engine285.py','audit_portable285.py','repack_font288.py','prove_archive288.py',
             'preflight_font288.py','publish_rgb287_archive288.py'):
    copy(root/'build/ci'/name,'tools/'+name)
receipt=dict(schema='qpx.standalone-font-archive-repair/1',state='resource-only-repack-locally-verified',
    archiver_source=repair['archiver_source'],product_source=repair['product_source'],
    native_bytes_unchanged=True,product_rebuilt=False,binary_relabelled=False,
    original_standalone_missing_fonts_reproduced=True,fixed_layout_reload_and_slide_smokes=3,
    unit_red=dict(passed=1,failed=1),unit_green=dict(passed=2,failed=0),audit_controls=1059,
    observer_source_paths=archive_proof['source_paths'],clean_path=True,clean_machine=False,
    original_windows_gui_counts=archive_proof['windows_gui_counts'],
    original_engine='terminal-failure',original_engine_full_auditor_returncode=1,
    original_portable_full_auditor_returncode=0,macos_last_observed='queued',
    authenticated_original_archives=archive_proof['authenticated_archives'],
    combined_candidate285_accepted=False,hosted_runs_repeated=False,
    own_source_platform_qualified=False,general_speed_claim=False,release_ready=False,files=files)
(public/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
with (work/'.gitattributes').open('a',encoding='utf-8') as stream:
    stream.write('\ndocs/benchmarks/rgb-nominal-kernel-287/** -text -whitespace\n')
    stream.write('docs/benchmarks/font-archive-288/** -text -whitespace\n')

font_status='''**Candidate285 engine is unaccepted; portable independently passes; Mac is pending.**
The [archive repair288](benchmarks/font-archive-288/receipt.json) retains all nine
currently available authenticated archives, the original engine rejection and
complete original-source portable acceptance at frozen1f71eb5. Windows GUI passes
55 native/4866 Python checks with14 declared skips including three legacy xfails;
its standalone layout/slide stages fail because archive_windows omitted fonts.
Observer01c3b9aa copies packaged font resources. The original CI archive fails
locally with missing fonts; a resource-only repack of the same CI wheel/native
bytes passes layout, reload and slide startup with clean PATH. Two unit cases
and1059 controls pass. No native rebuild, CI redispatch, binary relabeling or
combined acceptance follows. Observe the existing Mac run40743845473; preserve
the frozen ref/checkout. Candidate281 remains the qualified feature baseline.
'''
kernel_status='''**Private RGB nominal kernel287 passes99 pinned reads on Windows and GCC sanitizers.**
The [receipt](benchmarks/rgb-nominal-kernel-287/receipt.json) binds native23edd7ed,
36 original and63 held-out public QuPath reads. All72 held-out payloads repeat
exactly.32.32 coordinates, eight-bit interpolation fractions and one final
rounding reproduce every captured pixel. Crop interpolation uses global source
context; a cropped-input model fails eight held-out cases. The private planner
includes that halo, bounds buffers before IO and preserves large integer origins.
The native resampling suite passes each Windows minimal/GCC13.3 ASan+UBSan
profile, including opacity, admission, cancellation and malformed-window checks.
12 reference checks,371 scientific fixture passes/seven declared GUI skips and
1057 controls pass separately on the retained7231d95 Python runtime.418 canonical
compiler inputs,752 source paths and four new retained products authenticate;
all71 GUI products remain unchanged. SDK/readers/CLI/UI consumers and complete
own-source qualification remain open. Default quantitative sampling is unchanged.
'''
for name in ('current-task.md','project-plan.md','feature-matrix.md','api-delivery-plan.md'):
    path=work/'docs'/name
    text=path.read_text(encoding='utf-8')
    start=text.index('**Font candidate285 is frozen; complete own-source qualification is running.**')
    end=text.index('\n\n',start)
    text=text[:start]+font_status.rstrip()+text[end:]
    if name=='current-task.md':
        marker='**Current qualified feature baseline:'
        position=text.index(marker)
        text=text[:position]+kernel_status+'\n'+text[position:]
    else:
        title,rest=text.split('\n',1)
        text=title+'\n\n'+kernel_status+'\n'+rest.lstrip('\n')
    path.write_text(text,encoding='utf-8')
path=work/'docs/qupath-rgb-resampling.md'
text=path.read_text(encoding='utf-8')
marker='## Easiest production route'
section='''## Private native kernel and held-out context

[Kernel287](benchmarks/rgb-nominal-kernel-287/receipt.json) adds63 independent
public reads on new dimensions, nondyadic factors, offset crops and thin/single-
pixel images. Two captures reproduce72 holdout payloads exactly. Native golden
RGBA hashes derive directly from those PNGs and the original36 outputs; their
generator performs no interpolation. The C++ kernel matches all99 under MSVC
and GCC13.3 with ASan/UBSan.

The observed fixed-point recipe uses32.32 center coordinates, truncated eight-
bit interpolation fractions and one final half-up division by65536. Enlargement
uses nearest centers. Centers outside the full source image become opaque black;
bilinear edges inside that image clamp to its border. Integer origins stay
separate from fractional offsets, including the tested3e12-pixel coordinates.

Eight held-out crop cases fail if the source is cut to the requested rectangle
before interpolation. Including neighboring source pixels resolves them exactly.
The private planner therefore returns the needed level-zero source window,
including interpolation context beyond the crop. It admits nominal extents,
output and source-window buffers against the existing256MiB limit, including
the halo. Window plus output can coexist; decoder costs remain additional.
It rejects nonopaque/malformed windows and checks native cancellation throughout.

This kernel is private. A public immutable request, ImageServer read adapter,
nanobind interface, CLI and relevant UI/workflow consumers remain to be delivered.
Opaque RGB fixture agreement does not establish general Java rendering,
transparency, vendor/pyramid behavior or biological acceptance.

'''
assert marker in text
text=text.replace(marker,section+marker,1)
text=text.replace('Keep the existing area and typed scientific sampling policies. First implement\nan explicitly versioned RGB compatibility kernel against this immutable corpus,\nincluding the currently failing boundary and nondyadic cases.',
    'Keep the existing area and typed scientific sampling policies. Reuse the\nprivate kernel287 for the native reader adapter and an explicitly versioned\npublic RGB request; its99 captured boundary/nondyadic/context cases now pass.')
path.write_text(text,encoding='utf-8')

note=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md'
prefix=note.read_bytes()
addition='''

## RGB kernel287 and standalone font repair288 (2026-10-28)

32.32 coordinates, truncated eight-bit interpolation fractions and one final
rounding match the original36 RGB reads. Fresh held-out dimensions/factors/crops
find eight failures in a crop-only model. Interpolation needs global source
context, not just the requested crop. The bounded source-window planner resolves
all99 reads. All72 held-out payloads repeat exactly. MSVC and GCC13.3 ASan/UBSan
pass the native golden controls and inherited resampling suite. Keep global
integer origins separate from fixed-point offsets and admit the halo before IO.
12 reference tests,371 scientific fixture passes/seven GUI skips and1057 controls
pass on the explicitly retained7231d95 Python runtime; the kernel itself is
native23edd7ed.418 canonical compiler inputs and four native products authenticate;
all71 earlier GUI products remain unchanged. Public consumers are still required.

Original font285 engine is terminal/failed: standalone archive filtering copies
executables/DLLs/notices but drops fonts. Its wheel tests pass55 native/4866 Python
checks before standalone startup rejects missing assets. Preserve the nine
authenticated archives and original full auditor rejection. Portable independently
passes its complete original auditor; Mac is still pending. Candidate281 remains
qualified. Observer01c3b9aa copies the packaged fonts. An actual old CI archive
fails locally; a resource-only repack of the exact CI wheel/native bytes passes
layout, layout reload and real-slide startup on clean PATH. Two red/green archive
cases and1059 controls verify. No native rebuild, binary relabeling or CI rerun.
Model-based producer tests stubbed archive_windows and could not find this data
loss: keep a direct actual-archiver test with packaged non-binary resources and
test the archive independently from the installed wheel.
'''
note.write_bytes(prefix+addition.encode('utf-8'))
assert note.read_bytes()[:len(prefix)]==prefix
(archive/'compilexx-append-proof.json').write_text(json.dumps(dict(prefix_bytes=len(prefix),
    prefix_sha256=hashlib.sha256(prefix).hexdigest(),prefix_unchanged=True),indent=2)+'\n')
print('Published private RGB287 and resource-only archive288 evidence; combined font qualification stays unaccepted',flush=True)
