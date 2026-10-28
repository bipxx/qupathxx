from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/rgb-nominal-context-290'
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'));source=manifest['source']
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in manifest['files'].items())
linux=json.loads((out/'linux-compiler-inputs.json').read_text(encoding='utf-8'))
assert len(linux['files'])==len(manifest['files'])==418 and linux['source']==source
proofs={}
for name in ('raw_classifier_evidence','scientific_grid_evidence'):
    spec=importlib.util.spec_from_file_location(name,work/('tools/ci/'+name+'.py'))
    mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
    proofs[name]=mod.source_contract(work,source)
    (out/(name+'-source-proof.json')).write_text(json.dumps(proofs[name],indent=2)+'\n',encoding='utf-8')
merged={}
for proof in proofs.values():
    for p,v in proof.items():
        assert p not in merged or merged[p]==v
        merged[p]=v
for p in manifest['files']:assert merged[p]['git_blob_sha256']==linux['files'][p]['git_blob_sha256']
logs={
 'windows':(root/'build/ci/rgb-nominal-287-native/Testing/Temporary/LastTest.log').read_bytes(),
 'linux':subprocess.check_output(['wsl.exe','-d','Ubuntu','--','cat','/tmp/qupathxx-rgb290-build/Testing/Temporary/LastTest.log'])}
marker='99 RGB nominal captured outputs stitch at three tile sizes with unchanged context cores; six negative-context translations and admission passed'
for platform,raw in logs.items():
    (out/(platform+'-native-complete.log')).write_bytes(raw)
    text=raw.decode('utf-8');assert text.count(marker)==1 and 'Test Passed.' in text
    cases=ET.parse(out/(platform+'-native.xml')).findall('.//testcase')
    assert len(cases)==1 and all(cases[0].find(v) is None for v in ('failure','error','skipped'))
    for p in json.loads((out/(platform+'-native-products.json')).read_text(encoding='utf-8')):
        path=Path(p['path']) if platform=='windows' else out/'linux-products'/Path(p['path']).name
        assert path.stat().st_size==p['bytes'] and hashlib.sha256(path.read_bytes()).hexdigest()==p['sha256']
    cli=json.loads((out/(platform+'-cli-cases.json')).read_text(encoding='utf-8'))
    assert len(cli)==99 and all(c['returncode']==0 for c in cli)
old=json.loads((root/'build/rgb-nominal-sdk-289/final-products.json').read_text(encoding='utf-8'))
for p in old:
    assert hashlib.sha256(Path(p['retained']).read_bytes()).hexdigest()==p['sha256']
    assert hashlib.sha256((root/'build/ci/gridfrontends167-package'/p['path']).read_bytes()).hexdigest()==p['sha256']
assert len(old)==71
summary=dict(native_source=source,windows_fingerprint=manifest['fingerprint'],
    linux_fingerprint=json.loads((out/'linux-source-fingerprint.json').read_text(encoding='utf-8'))['fingerprint'],
    canonical_compiler_inputs=418,source_paths=len(merged),retained_windows_products=3,retained_gcc_products=3,
    sdk289_gui_products_unchanged=71,windows_native_suites=1,gcc_asan_ubsan_native_suites=1,
    pinned_captured_cases_per_toolchain=99,stitch_tile_sizes=[1,7,32],core_context_cases_per_toolchain=99,
    independent_negative_context_translations=6,cli_hashes_per_toolchain=99,
    auditor_controls=136,auditor_runtime_source='11f70cd1133612fd059fd0c294f6b7eb60103679',
    full_native_output_recovered_from_existing_ctest_logs=True,test_rerun_for_output_recovery=False,
    public_api_changed=False,cell_recipe_delivered=False,own_source_platform_qualified=False,
    scientific_acceptance=False,general_speed_claim=False,release_ready=False)
(out/'source-runtime-proof.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
notes=root.parent/'compilexx/docs/notes/qupathxx-build-inefficiencies.md';before=notes.read_bytes()
addition='''

## 2026-10-28: preserve the nominal lattice before cell-workflow integration290

- Existing scientific-grid spacing is extent/sample-count, which cannot represent
  every rounded nominal RGB output. Keep the new recipe explicit; do not silently
  reinterpret rational area grids or QCL5 bytes to make a comparison pass.
- Plan output-index windows from the original request, including negative detector
  context. Re-cropping each tile to a rounded integer source origin shifts its
  interpolation phase. All99 pinned outputs now stitch at tile sizes1/7/32 and
  retain their core under added context; six integer-translated exact-factor cases
  independently check negative context. Both MSVC and GCC ASan/UBSan pass.
- Split signed sample-index arithmetic into integer/fractional parts and preserve
  large integer source origins. Keep the256MiB input/output limits and4096-sample
  context bound. Detector context is distinct from the decoder interpolation halo.
- Reuse one native input primitive, the existing physical LoG/expansion kernels,
  result/publication controller and project blob storage. Next add original-owner
  capture and explicit nominal result geometry, then versioned archive/replay and
  all consumers. A new preview document is not original-slide workflow completion.
- Windows native build/test passed before a post-test observer tried decoding an
  UTF-8 helper through Windows GBK. Preserve the error; explicitly decode UTF-8
  and prepare the separate sanitizer run without rebuilding/retesting Windows.
- CTest truncated XML system-out at1024 bytes. Recover the full output from its
  existing LastTest.log and preserve both records, instead of repeating completed
  tests just for logging. This targeted check is not full platform acceptance.
- Preserve all71 SDK289 GUI products while building only the affected minimal
  native suite/CLI. Both new toolchains pass99 real CLI hashes;136 auditor controls
  run on the explicitly identified prior installed product. No new wheel/GUI or
  original-document cell/archive/replay qualification is claimed.
'''.encode('utf-8')
with notes.open('ab') as stream:stream.write(addition)
assert notes.read_bytes()==before+addition
(out/'compilexx-append-proof.json').write_text(json.dumps(dict(path=str(notes),prefix_bytes=len(before),prefix_sha256=hashlib.sha256(before).hexdigest(),appended_bytes=len(addition),final_sha256=hashlib.sha256(notes.read_bytes()).hexdigest(),prefix_preserved=True),indent=2)+'\n',encoding='utf-8')
print('Both native sources/products,99 CLI cases each, full existing CTest logs and71 unchanged GUI products authenticated',flush=True)
