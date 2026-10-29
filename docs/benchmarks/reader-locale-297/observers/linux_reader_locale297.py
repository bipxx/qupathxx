from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/reader-locale-297/linux'
source=Path('/tmp/qupathxx-reader297-source');build=Path('/tmp/qupathxx-reader297-build')
manifest=json.loads((out.parent/'compiler-inputs-initial.json').read_text());commit=manifest['source']
assert commit.startswith('28c33573') and not source.exists() and not build.exists()
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
targets=['qupathxx_raw_image_tests','qupathxx_region']
commands=[('linux-clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),
    ('linux-autocrlf',['git','config','core.autocrlf','false'],source),
    ('linux-checkout',['git','checkout','--detach',commit],source),
    ('linux-configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all',
        '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',
        *['-DQUPATHXX_BUILD_'+name+'=OFF' for name in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON'],root),
    ('linux-build',['cmake','--build',str(build),'--target',*targets,'--parallel','2'],root),
    ('linux-native',['ctest','--test-dir',str(build),'-R','^raw_image$',
        '--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'linux-native.xml')],root)]
results=[]
for name,args,cwd in commands:
    log=out/(name+'.log');assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:result=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,cwd=str(cwd),returncode=result.returncode))
    (out/'linux-command-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,result.returncode,flush=True);assert result.returncode==0,log
    if name=='linux-checkout':
        names=sorted(manifest['files']);data=subprocess.check_output(['git','cat-file','--batch'],input=('\n'.join(commit+':'+p for p in names)+'\n').encode(),cwd=source)
        offset=0;proof={}
        for p in names:
            end=data.index(b'\n',offset);header=data[offset:end].split();assert header[1]==b'blob'
            size=int(header[2]);blob=data[end+1:end+1+size];offset=end+size+2;raw=(source/p).read_bytes()
            assert raw==blob,p
            proof[p]=dict(checked_out_sha256=hashlib.sha256(raw).hexdigest(),git_blob_sha256=hashlib.sha256(blob).hexdigest())
        assert offset==len(data) and len(proof)==425
        (out/'linux-compiler-inputs.json').write_text(json.dumps(dict(source=commit,files=proof),indent=2)+'\n')
        fingerprint=hashlib.sha256(''.join(p+'='+proof[p]['checked_out_sha256']+'\n' for p in names).encode()).hexdigest()
        (out/'linux-source-fingerprint.json').write_text(json.dumps(dict(source=commit,fingerprint=fingerprint),indent=2)+'\n')
        (out/'linux-compiler-version.txt').write_text(subprocess.check_output(['c++','--version'],text=True))
retained=out/'linux-products';retained.mkdir(exist_ok=False);products=[]
for name in [*targets,'libqupathxx_core.a']:
    src=build/name;dst=retained/name;shutil.copyfile(src,dst);assert src.read_bytes()==dst.read_bytes()
    products.append(dict(path=str(dst),bytes=dst.stat().st_size,sha256=hashlib.sha256(dst.read_bytes()).hexdigest()))
(out/'linux-native-products.json').write_text(json.dumps(products,indent=2)+'\n')
shutil.copyfile(build/'generated/build_manifest.hpp',out/'linux-build-manifest.hpp')
shutil.copyfile(build/'Testing/Temporary/LastTest.log',out/'linux-native-complete.log')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
print('Affected raw-image suite passed under ASan/UBSan; three products and full logs retained before exit',flush=True)
