from pathlib import Path
import hashlib
import json
import os
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'build/gui-font-gl-284'
work = root / 'build/ci/scientific-grid-159'
manifest = json.loads((root / 'build/gui-font-delivery-283/compiler-inputs-compatibility.json').read_text())
assert all(hashlib.sha256((work / p).read_bytes()).hexdigest() == h for p, h in manifest['files'].items())
products = json.loads((root / 'build/gui-font-delivery-283/final-products.json').read_text())
library, = [v for v in products if v['path'] == 'Release/qupathxx_gui.lib']
assert hashlib.sha256(Path(library['retained']).read_bytes()).hexdigest() == library['sha256']
paths = [out / 'probe.cpp', out / 'CMakeLists.txt', Path(library['retained']),
         root / 'build/ci/gridfrontends167-package/_deps/glfw-build/src/Release/glfw3.lib',
         work / 'cpp/gui_fonts.hpp', root / 'build/cp312-cp312-win_amd64/_deps/glfw-src/include/GLFW/glfw3.h']
paths += list((root / 'build/gui-fonts-282/inputs/imgui193').glob('*.h'))
paths += list((root / 'build/gui-fonts-282/inputs/imgui193/backends').glob('*.h'))
identity = dict(native_source=manifest['source'], compiler_inputs_unchanged=len(manifest['files']),
    product_source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip(),
    upstream_imgui=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root/'build/gui-fonts-282/inputs/imgui193',text=True).strip(),
    files=[dict(path=str(p),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths])
(out / 'input-identity.json').write_text(json.dumps(identity,indent=2)+'\n')
cmake = root / '.venv/Lib/site-packages/cmake/data/bin/cmake.exe'
build = [str(cmake),'--build',str(out/'build'),'--config','Release','--parallel','2']
assert not (out / 'build.log').exists()
with (out / 'build.log').open('w',encoding='utf-8') as log:
    result = subprocess.run(build,stdout=log,stderr=subprocess.STDOUT)
(out / 'build-exit.json').write_text(json.dumps(dict(argv=build,returncode=result.returncode),indent=2)+'\n')
print('Build exit',result.returncode,flush=True)
assert result.returncode == 0
exe = out / 'build/Release/font_gl_probe.exe'
for name in ('NotoSansCJKsc-Regular.otf','NotoEmoji.ttf'):
    assert (exe.parent / 'fonts' / name).read_bytes() == (root/'build/gui-font-delivery-283/final-products/Release/fonts'/name).read_bytes()
with (out/'measurements.jsonl').open('w',encoding='utf-8') as stdout, (out/'run-stderr.log').open('w',encoding='utf-8') as stderr:
    result = subprocess.run([str(exe)],cwd=root,stdout=stdout,stderr=stderr,timeout=90)
(out / 'run-exit.json').write_text(json.dumps(dict(argv=[str(exe)],returncode=result.returncode,
    exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest()),indent=2)+'\n')
print('Probe exit',result.returncode,flush=True)
assert result.returncode == 0
rows = [json.loads(line) for line in (out/'measurements.jsonl').read_text().splitlines()]
assert [r['name'] for r in rows] == ['normal','fractional','large','maximal','quota','repeat_normal']
assert all(r['live_after_shutdown']==0 and r['readbacks']>0 for r in rows)
assert all(r['queried_gl_storage_peak_bytes']<=2*2048*2048*4 for r in rows)
summary = dict(native_source=manifest['source'],sessions=rows,
    real_gl_storage_not_physical_vram=True, synthetic_density_not_physical_monitor=True,
    rss_includes_gl_driver_and_probe_readback_buffers=True,
    font_files_persist_after_sessions=True, no_product_source_change=True)
(out/'receipt-local.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Six real-GL sessions and exact readback/deletion controls passed',flush=True)
