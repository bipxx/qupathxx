from pathlib import Path
import hashlib
import json
import os
import subprocess

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/rgb-nominal-context-290'
exe=out/'windows-products/qupathxx_region.exe'
fixture=work/'tests/fixtures/rgb-nominal-v1'
temp=out/'windows-cli';temp.mkdir(exist_ok=False)
commands=[]
for line in (fixture/'index.txt').read_text(encoding='utf-8').splitlines()[1:]:
    f=line.split();image=temp/'source-原始-𠀋.ppm';output=temp/'result.pam'
    original=b'P6\n'+f[2].encode()+b' '+f[3].encode()+b'\n255\n'+(fixture/f[1]).read_bytes();image.write_bytes(original)
    args=[str(exe),'--rgb-nominal',str(image),*f[4:9],str(output)]
    result=subprocess.run(args,capture_output=True)
    commands.append(dict(name=f[0],argv=args,returncode=result.returncode,stderr=result.stderr.decode('utf-8',errors='replace')))
    (out/'windows-cli-cases.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf-8')
    assert result.returncode==0,commands[-1]
    _,pixels=output.read_bytes().split(b'ENDHDR\n',1)
    assert hashlib.sha256(pixels).hexdigest()==f[11] and image.read_bytes()==original
assert len(commands)==99
python=root/'build/ci/rgb-nominal-sdk-289-env/Scripts/python.exe'
args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_scientific_grid_evidence.py','-q','--junitxml='+str(out/'auditor.xml'),'--basetemp',str(out/'auditor-temp')]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
with (out/'auditor.log').open('w',encoding='utf-8') as log:
    result=subprocess.run(args,cwd=work,env=env,stdout=log,stderr=subprocess.STDOUT)
(out/'auditor-command.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode,product_source='11f70cd1133612fd059fd0c294f6b7eb60103679',scope='auditor controls/independent collection only; not the new native runtime'),indent=2)+'\n',encoding='utf-8')
assert result.returncode==0
print('Windows99 CLI hashes and scientific auditor controls passed',flush=True)
