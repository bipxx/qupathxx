from pathlib import Path
import hashlib
import json
import math
import os
import shutil
import subprocess
root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/postlog-trace-304/linux';out.mkdir(exist_ok=False)
source=Path('/tmp/qupathxx-postlog304-source');build=Path('/tmp/qupathxx-postlog304-build')
assert not source.exists() and not build.exists();commit='e9d71f28'
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
targets=['qupathxx_qupath_response_tests','qupathxx_log_watershed_tests','qupathxx_cell_expansion_tests']
commands=[('clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),('autocrlf',['git','config','core.autocrlf','false'],source),('checkout',['git','checkout','--detach',commit],source),
 ('configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all','-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',*['-DQUPATHXX_BUILD_'+name+'=OFF' for name in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON'],root),
 ('build',['cmake','--build',str(build),'--target',*targets,'--parallel','2'],root),
 ('native',['ctest','--test-dir',str(build),'-R','^(qupath_response|log_watershed|cell_expansion)$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')],root)]
results=[]
for name,args,cwd in commands:
    with (out/(name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,cwd=str(cwd),returncode=r.returncode));(out/'commands.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
    print(name,r.returncode,flush=True);assert r.returncode==0,out/(name+'.log')
probe=build/'nucleus-probe';kernel=root/'build/imagej-gaussian-kernel-300/linux/products/imagej_gaussian.cpp.o'
args=['g++','-std=c++17','-O1','-g1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all','-ffp-contract=off','-I'+str(source/'cpp'),str(root/'build/postlog-trace-304/nuclei-probe/main.cpp'),str(kernel),'-o',str(probe)]
with (out/'probe-build.log').open('x',encoding='utf-8') as stream:r=subprocess.run(args,env=env,stdout=stream,stderr=subprocess.STDOUT)
assert r.returncode==0
contract=json.loads((source/'tests/fixtures/qupath-cell-policy-edges-0.7.0/contract.json').read_text());arrays=out/'arrays';arrays.mkdir();checks=[]
for case in contract['cases']:
    name=case['name'];metadata=json.loads((source/'tests/fixtures/qupath-cell-policy-edges-0.7.0'/name/'processing-stack.json').read_text())
    step=case.get('requested_pixel_size_um',.5)/.5;p=case.get('native',contract['native']);prefix=arrays/name;inputs=root/'build/postlog-trace-304/arrays'/name
    args=[str(probe),*[str(inputs)+suffix for suffix in ('-score.f32le','-log.f32le','-domain.u8')],str(prefix),str(metadata['width']),str(metadata['height']),repr(p['sigma']/step),str(math.ceil(p['min_nucleus_pixels']/step**2)),str(math.floor(p['max_nucleus_pixels']/step**2)),repr(p['cell_expansion']/step),repr(p['threshold'])]
    r=subprocess.run(args,env=env,capture_output=True,text=True);assert r.returncode==0,r.stderr
    payloads=[]
    for suffix in ('-candidate-basins.u32le','-candidate-regions.u8','-candidate-binary.u8','-candidate-refinement.u8','-candidate-nuclei.u32le','-candidate-bodies.u32le'):
        target=Path(str(prefix)+suffix);expected=Path(str(inputs)+suffix);assert target.read_bytes()==expected.read_bytes(),name+suffix
        payloads.append(dict(name=target.name,bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    checks.append(dict(name=name,payloads=payloads))
(out/'cross-host-probe.json').write_text(json.dumps(dict(cases=checks,all78_stage_payloads_byte_exact=True,application_rebuilt=False),indent=2)+'\n',encoding='utf-8')
products=[];retained=out/'products';retained.mkdir()
for path in [*[build/name for name in targets],probe]:
    target=retained/path.name;shutil.copyfile(path,target);products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
for folder in ('qupathxx_qupath_response_tests.dir','qupathxx_log_watershed_tests.dir','qupathxx_cell_expansion_tests.dir'):
    for path in sorted((build/'CMakeFiles'/folder).rglob('*.o')):
        target=retained/(folder+'-'+path.name);shutil.copyfile(path,target);products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
shutil.copyfile(build/'Testing/Temporary/LastTest.log',out/'native-complete.log')
(out/'compiler.txt').write_text(subprocess.check_output(['g++','--version'],text=True),encoding='utf-8')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
print('Three sanitizer suites and78 cross-host stage payloads passed; products retained before WSL exit',flush=True)
