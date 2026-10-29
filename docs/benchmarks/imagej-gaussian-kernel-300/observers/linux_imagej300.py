from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/imagej-gaussian-kernel-300/linux';out.mkdir(exist_ok=False)
source=Path('/tmp/qupathxx-imagej300-source');build=Path('/tmp/qupathxx-imagej300-build');commit='647576b458144d5ca1e032eb108f9e62930fba99'
assert not source.exists() and not build.exists()
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
commands=[('clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),
    ('autocrlf',['git','config','core.autocrlf','false'],source),('checkout',['git','checkout','--detach',commit],source),
    ('configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all','-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',
        *['-DQUPATHXX_BUILD_'+n+'=OFF' for n in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON'],root),
    ('build',['cmake','--build',str(build),'--target','qupathxx_imagej_gaussian_tests','--parallel','2'],root),
    ('native',['ctest','--test-dir',str(build),'-R','^imagej_gaussian$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')],root)]
results=[]
for name,args,cwd in commands:
    with (out/(name+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,cwd=str(cwd),returncode=r.returncode));(out/'command-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,r.returncode,flush=True);assert r.returncode==0
    if name=='checkout':
        names={'CMakeLists.txt','pyproject.toml'}
        for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:names.update(p.relative_to(source).as_posix() for p in (source/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
        files={n:hashlib.sha256((source/n).read_bytes()).hexdigest() for n in sorted(names)};assert len(files)==429
        (out/'compiler-inputs.json').write_text(json.dumps(dict(source=commit,files=files),indent=2)+'\n')
        (out/'compiler-version.txt').write_text(subprocess.check_output(['c++','--version'],text=True))
retained=out/'products';retained.mkdir(exist_ok=False);products=[]
for path in [build/'qupathxx_imagej_gaussian_tests',*sorted((build/'CMakeFiles/qupathxx_imagej_gaussian_tests.dir').rglob('*.o'))]:
    target=retained/path.name;shutil.copyfile(path,target);assert path.read_bytes()==target.read_bytes()
    products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'native-products.json').write_text(json.dumps(products,indent=2)+'\n')
shutil.copytree(build/'imagej-gaussian-test-artifacts',out/'artifacts')
shutil.copyfile(build/'Testing/Temporary/LastTest.log',out/'native-complete.log')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
print('Standalone Gaussian suite passed ASan/UBSan; artifacts and products retained before exit')
