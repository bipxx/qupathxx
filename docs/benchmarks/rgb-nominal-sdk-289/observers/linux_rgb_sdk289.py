"""Frozen native read/CLI check under Ubuntu ASan+UBSan; no Python-wheel claim."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx')
out=root/'build/rgb-nominal-sdk-289'
source=Path('/tmp/qupathxx-rgb289-source')
build=Path('/tmp/qupathxx-rgb289-build')
commit='11f70cd1133612fd059fd0c294f6b7eb60103679'
assert not source.exists() and not build.exists()
env=os.environ.copy()
env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1'
env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
commands=[('linux-clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),
    ('linux-autocrlf',['git','config','core.autocrlf','false'],source),
    ('linux-checkout',['git','checkout','--detach',commit],source),
    ('linux-configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo',
        '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all',
        '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',
        *['-DQUPATHXX_BUILD_'+name+'='+value for name,value in dict(PYTHON='OFF',GUI='OFF',TESTS='ON',
            OPENSLIDE='OFF',TIFF='OFF',GEOS='OFF',PROJECT='OFF',INTERCHANGE='OFF',CLASSIFIER_TRAINING='OFF').items()]],root),
    ('linux-build',['cmake','--build',str(build),'--target','qupathxx_resampling_tests','qupathxx_region','--parallel','2'],root),
    ('linux-native',['ctest','--test-dir',str(build),'-R','^resampling$','--output-on-failure',
        '--output-junit',str(out/'linux-native.xml')],root)]
results=[]
for label,args,cwd in commands:
    log=out/(label+'.log')
    assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
    results.append(dict(label=label,argv=args,cwd=str(cwd),returncode=result.returncode))
    (out/'linux-command-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(label,result.returncode,flush=True)
    assert result.returncode==0,log
    if label=='linux-checkout':
        assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()==commit
        assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
        inputs=json.loads((out/'compiler-inputs-initial.json').read_text())['files']
        names=sorted(inputs)
        data=subprocess.check_output(['git','cat-file','--batch'],input=('\n'.join(commit+':'+p for p in names)+'\n').encode(),cwd=source)
        offset=0;proof={}
        for name in names:
            end=data.index(b'\n',offset);header=data[offset:end].decode().split()
            assert header[1]=='blob'
            size=int(header[2]);blob=data[end+1:end+1+size];offset=end+size+2
            actual=(source/name).read_bytes()
            assert actual.replace(b'\r\n',b'\n')==blob.replace(b'\r\n',b'\n'),name
            proof[name]=dict(checked_out_sha256=hashlib.sha256(actual).hexdigest(),git_blob_sha256=hashlib.sha256(blob).hexdigest())
        assert offset==len(data) and len(proof)==418
        (out/'linux-compiler-inputs.json').write_text(json.dumps(dict(source=commit,files=proof),indent=2)+'\n')
        (out/'linux-compiler-version.txt').write_text(subprocess.check_output(['c++','--version'],text=True))
        fingerprint=hashlib.sha256(''.join(p+'='+proof[p]['checked_out_sha256']+'\n' for p in names).encode()).hexdigest()
        (out/'linux-source-fingerprint.json').write_text(json.dumps(dict(source=commit,fingerprint=fingerprint),indent=2)+'\n')
fixture=source/'tests/fixtures/rgb-nominal-v1'
cli_dir=build/'cli-captured-tests'
cli_dir.mkdir(exist_ok=False)
cli=[]
for line in (fixture/'index.txt').read_text().splitlines()[1:]:
    fields=line.split();name,path,sw,sh=fields[:4]
    original=b'P6\n'+sw.encode()+b' '+sh.encode()+b'\n255\n'+(fixture/path).read_bytes()
    image=cli_dir/'source-原始-𠀋.ppm';image.write_bytes(original)
    output=cli_dir/'result.pam'
    args=[str(build/'qupathxx_region'),'--rgb-nominal',str(image),*fields[4:9],str(output)]
    result=subprocess.run(args,env=env,capture_output=True)
    entry=dict(name=name,argv=args,returncode=result.returncode,stderr=result.stderr.decode('utf-8',errors='replace'))
    cli.append(entry)
    (out/'linux-cli-cases.json').write_text(json.dumps(cli,indent=2)+'\n')
    assert result.returncode==0,entry
    header,pixels=output.read_bytes().split(b'ENDHDR\n',1)
    assert hashlib.sha256(pixels).hexdigest()==fields[11],name
    assert image.read_bytes()==original
assert len(cli)==99
retained=out/'linux-products';retained.mkdir(exist_ok=False)
products=[]
for name in ('qupathxx_resampling_tests','qupathxx_region','libqupathxx_core.a'):
    src=build/name;target=retained/name;shutil.copyfile(src,target)
    assert src.read_bytes()==target.read_bytes()
    products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'linux-native-products.json').write_text(json.dumps(products,indent=2)+'\n')
shutil.copyfile(build/'generated/build_manifest.hpp',out/'linux-build-manifest.hpp')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
print('GCC ASan/UBSan public reader99 and CLI99 captured hashes passed; three native products retained',flush=True)
