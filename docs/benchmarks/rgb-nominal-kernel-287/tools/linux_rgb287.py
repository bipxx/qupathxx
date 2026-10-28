"""Run under Ubuntu WSL; build an immutable native Git checkout with ASan/UBSan."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx')
out=root/'build/rgb-nominal-287'
source=Path('/tmp/qupathxx-rgb287-source')
build=Path('/tmp/qupathxx-rgb287-build')
commit='23edd7ed1f15c87438602ec3b22e5506793a3a59'
assert not source.exists() and not build.exists()
commands=[('linux-clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),
          ('linux-autocrlf',['git','config','core.autocrlf','false'],source),
          ('linux-checkout',['git','checkout','--detach',commit],source),
          ('linux-configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo',
              '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all',
              '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',
              *['-DQUPATHXX_BUILD_'+name+'='+value for name,value in dict(
                  PYTHON='OFF',GUI='OFF',TESTS='ON',OPENSLIDE='OFF',TIFF='OFF',GEOS='OFF',
                  PROJECT='OFF',INTERCHANGE='OFF',CLASSIFIER_TRAINING='OFF').items()]],root),
          ('linux-build',['cmake','--build',str(build),'--target','qupathxx_resampling_tests','--parallel','2'],root),
          ('linux-native',['ctest','--test-dir',str(build),'-R','^resampling$','--output-on-failure',
              '--output-junit',str(out/'linux-native.xml')],root)]
results=[]
environment=os.environ.copy()
environment['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1'
environment['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
for label,argv,cwd in commands:
    log=out/(label+'.log')
    assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(argv,cwd=cwd,env=environment,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=label,argv=argv,cwd=str(cwd),returncode=result.returncode))
    (out/'linux-command-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(label,'exit',result.returncode,flush=True)
    assert result.returncode==0,log
    if label=='linux-checkout':
        assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
        assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
        windows=json.loads((out/'compiler-inputs-initial.json').read_text())
        hashes={}
        for name in windows['files']:
            data=(source/name).read_bytes()
            blob=subprocess.check_output(['git','show',commit+':'+name],cwd=source)
            assert data==blob or data.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),name
            hashes[name]=dict(checked_out_sha256=hashlib.sha256(data).hexdigest(),git_blob_sha256=hashlib.sha256(blob).hexdigest())
        (out/'linux-compiler-inputs.json').write_text(json.dumps(dict(source=commit,files=hashes),indent=2)+'\n')
        (out/'linux-compiler-version.txt').write_text(subprocess.check_output(['c++','--version'],text=True))
        manifest=hashlib.sha256(''.join(name+'='+hashes[name]['checked_out_sha256']+'\n' for name in sorted(hashes)).encode()).hexdigest()
        (out/'linux-source-fingerprint.json').write_text(json.dumps(dict(source=commit,fingerprint=manifest),indent=2)+'\n')
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
retained=out/'linux-products'
retained.mkdir(exist_ok=False)
products=[]
for name in ('qupathxx_resampling_tests','libqupathxx_core.a'):
    original=build/name
    target=retained/name
    shutil.copyfile(original,target)
    assert target.read_bytes()==original.read_bytes()
    products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'linux-native-products.json').write_text(json.dumps(products,indent=2)+'\n')
shutil.copyfile(build/'generated/build_manifest.hpp',out/'linux-build-manifest.hpp')
print('GCC13.3 ASan/UBSan native99 golden controls and inherited resampling suite passed; frozen checkout remains clean',flush=True)
