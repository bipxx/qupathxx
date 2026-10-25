from pathlib import Path
import json,os,subprocess,sys
r=Path(__file__).resolve().parents[2];source=r/'build/ci/tma-axis-workflow-196';manifest=json.loads((source/'tools/ci/qptma-cases.json').read_text(encoding='utf-8'));python=r/'build/ci/tma197-python-env/Scripts/python.exe';out=r/'build/tma199-cohort-source';out.mkdir(exist_ok=False)
command=[str(python),'-I','-m','pytest',*[str(source/'tests'/(name+'.py')) for name in manifest],'--collect-only','-q','--noconftest','--color=no','-o','addopts='];env=os.environ.copy();env.pop('PYTEST_ADDOPTS',None);env['PYTEST_DISABLE_PLUGIN_AUTOLOAD']='1';env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8';result=subprocess.run(command,cwd=source,env=env,capture_output=True,timeout=60)
(out/'collection.log').write_bytes(result.stdout+result.stderr);assert result.returncode==0,result.stdout.decode('utf-8',errors='replace')
collected={name:[] for name in manifest}
for line in result.stdout.decode('utf-8').splitlines():
 if '::' not in line:continue
 filename,name=line.split('::',1);module=Path(filename).stem;assert module in collected;collected[module].append(name)
for name,values in collected.items():assert values and len(values)==len(set(values));values.sort()
(out/'actual-cases.json').write_text(json.dumps(collected,indent=2)+'\n',encoding='utf-8');(out/'command.json').write_text(json.dumps(dict(command=command,exit=result.returncode,source='ad4aa2c8cfeaad652e4dd920f81ba8787eed0384'),indent=2)+'\n',encoding='utf-8')
print({k:len(v) for k,v in collected.items()});print('Manifest matches actual collection:',collected==manifest)
