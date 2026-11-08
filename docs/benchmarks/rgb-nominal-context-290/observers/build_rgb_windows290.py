from pathlib import Path
import hashlib
import json
import shutil
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-context-290';out.mkdir(exist_ok=False)
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
assert source.startswith('65c03e4e') and not subprocess.check_output(['git','status','--porcelain'],cwd=work)
old=json.loads((root/'build/rgb-nominal-287/windows-retained-products.json').read_text())
for p in old:assert hashlib.sha256(Path(p['path']).read_bytes()).hexdigest()==p['sha256']
names=set(['CMakeLists.txt','pyproject.toml'])
for folder,suffixes in [('cpp',{'.cpp','.hpp'}),('cmake',{'.cmake','.in'}),('src',{'.py'})]:
    names.update(p.relative_to(work).as_posix() for p in (work/folder).rglob('*') if p.is_file() and p.suffix in suffixes)
files={p:hashlib.sha256((work/p).read_bytes()).hexdigest() for p in sorted(names)}
assert len(files)==418
fingerprint=hashlib.sha256(''.join(p+'='+h+'\n' for p,h in files.items()).encode()).hexdigest()
(out/'compiler-inputs-initial.json').write_text(json.dumps(dict(source=source,fingerprint=fingerprint,files=files),indent=2)+'\n')
cmake=root/'.venv/Lib/site-packages/cmake/data/bin/cmake.exe'
ctest=cmake.with_name('ctest.exe');build=root/'build/ci/rgb-nominal-287-native'
commands=[('windows-build',[str(cmake),'--build',str(build),'--config','Release','--target','qupathxx_resampling_tests','qupathxx_region','--parallel','2']),
          ('windows-native',[str(ctest),'--test-dir',str(build),'-C','Release','-R','^resampling$','--output-on-failure','--output-junit',str(out/'windows-native.xml')])]
results=[]
for name,args in commands:
    with (out/(name+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
    results.append(dict(label=name,argv=args,returncode=result.returncode))
    (out/'windows-command-results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,result.returncode,flush=True);assert result.returncode==0
assert all(hashlib.sha256((work/p).read_bytes()).hexdigest()==h for p,h in files.items())
retained=out/'windows-products';retained.mkdir()
products=[]
for relative in ('Release/qupathxx_resampling_tests.exe','Release/qupathxx_region.exe','Release/qupathxx_core.lib'):
    src=build/relative;target=retained/src.name;shutil.copy2(src,target)
    assert src.read_bytes()==target.read_bytes()
    products.append(dict(path=str(target),bytes=target.stat().st_size,sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
(out/'windows-native-products.json').write_text(json.dumps(products,indent=2)+'\n')
shutil.copy2(build/'generated/build_manifest.hpp',out/'windows-build-manifest.hpp')
# Reuse the prior audited sanitizer orchestration, but freeze a separate source
# and output directory. The original289 checkout/products remain untouched.
template=(root/'build/ci/linux_rgb_sdk289.py').read_text()
linux=root/'build/ci/linux_rgb_context290.py';assert not linux.exists()
template=template.replace('rgb-nominal-sdk-289','rgb-nominal-context-290').replace('qupathxx-rgb289','qupathxx-rgb290').replace('11f70cd1133612fd059fd0c294f6b7eb60103679',source)
linux.write_text(template,encoding='utf-8')
print('Windows native context checks passed and three products retained; sanitizer script prepared',flush=True)
