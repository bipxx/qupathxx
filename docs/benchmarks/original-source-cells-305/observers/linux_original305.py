from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

root=Path('/mnt/c/Users/HEQ/Projects/qupathxx');out=root/'build/original-source-305/linux';out.mkdir(exist_ok=False)
source=Path('/tmp/qupathxx-original305-source');build=Path('/tmp/qupathxx-original305-build')
assert not source.exists() and not build.exists()
commit='26cae2af9f8d1ee54b1b5871079e67646a9faf07'
env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=1:halt_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
targets=['qupathxx_resampling_tests','qupathxx_cell_tests','qupathxx_cell_record_tests','qupathxx_cell_table_tests','qupathxx_object_classifier_tests','qupathxx_qupath_response_tests']
commands=[('clone',['git','clone','--shared','--no-checkout',str(root),str(source)],root),
 ('autocrlf',['git','config','core.autocrlf','false'],source),('checkout',['git','checkout','--detach',commit],source),
 ('configure',['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=RelWithDebInfo','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
 '-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all',
 '-DCMAKE_CXX_FLAGS_RELWITHDEBINFO=-O1 -g1 -DNDEBUG',*['-DQUPATHXX_BUILD_'+name+'=OFF' for name in ('PYTHON','GUI','OPENSLIDE','TIFF','GEOS','PROJECT','INTERCHANGE','CLASSIFIER_TRAINING')],'-DQUPATHXX_BUILD_TESTS=ON'],root),
 ('build',['cmake','--build',str(build),'--target',*targets,'--parallel','2'],root),
 ('native',['ctest','--test-dir',str(build),'-R','^(resampling|cells|cell_records|cell_tables|object_classifier|qupath_response)$','--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native.xml')],root)]
results=[]
try:
    for label,args,cwd in commands:
        with (out/(label+'.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
        results.append(dict(label=label,argv=args,cwd=str(cwd),returncode=r.returncode))
        (out/'commands.json').write_text(json.dumps(results,indent=2)+'\n',encoding='utf-8')
        print(label,r.returncode,flush=True)
        if r.returncode:raise SystemExit(r.returncode)
finally:
    retained=out/'products';retained.mkdir();products=[]
    paths=[build/name for name in targets]
    for name in targets:
        paths.extend(sorted((build/'CMakeFiles'/(name+'.dir')).rglob('*.o')))
    paths.extend(build/'CMakeFiles/qupathxx_core.dir/cpp'/name for name in ('rgb_nominal.cpp.o','scientific_cells.cpp.o','imagej_gaussian.cpp.o'))
    for path in paths:
        if not path.is_file():continue
        relative=path.relative_to(build);target=retained/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        products.append(dict(path=target.relative_to(out).as_posix(),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    (out/'products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
    for original,name in [(build/'Testing/Temporary/LastTest.log','native-complete.log'),(build/'compile_commands.json','compile_commands.json')]:
        if original.is_file():shutil.copyfile(original,out/name)
    (out/'compiler.txt').write_text(subprocess.check_output(['g++','--version'],text=True),encoding='utf-8')
    if source.is_dir():assert not subprocess.check_output(['git','status','--porcelain'],cwd=source)
