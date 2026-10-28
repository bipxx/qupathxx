from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-287'
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source.startswith('23edd7ed')
assert not subprocess.check_output(['git','status','--porcelain'],cwd=work)
paths=set()
for directory,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    paths.update(p.relative_to(work).as_posix() for p in (work/directory).rglob('*') if p.is_file() and p.suffix in suffixes)
paths.update(['CMakeLists.txt','pyproject.toml'])
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(paths)}
assert len(files)==418
fingerprint=hashlib.sha256(''.join(f'{p}={h}\n' for p,h in files.items()).encode()).hexdigest()
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'
ctest=root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'
build=root/'build/ci/rgb-nominal-287-native'
commands=[('build',[str(cmake),'--build',str(build),'--config','Release','--target','qupathxx_resampling_tests','--parallel','2']),
          ('native',[str(ctest),'--test-dir',str(build),'-C','Release','-R','^resampling$','--output-on-failure','--output-junit',str(out/'native.xml')])]
for name,args in commands:
    log=out/(name+'.log')
    assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
    (out/(name+'-exit.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n')
    print(name,'exit',result.returncode,flush=True)
    assert result.returncode==0,log
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
exe=build/'Release/qupathxx_resampling_tests.exe'
lib=build/'Release/qupathxx_core.lib'
(out/'native-products.json').write_text(json.dumps([dict(path=str(p),bytes=p.stat().st_size,
    sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in (exe,lib)],indent=2)+'\n')
print('Private RGB native kernel passes99 captured golden cases and inherited resampling suite;418 source inputs unchanged',flush=True)
