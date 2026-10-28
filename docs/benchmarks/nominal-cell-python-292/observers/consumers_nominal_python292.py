from pathlib import Path
import json
import os
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build/nominal-cell-python-292';work=root/'build/ci/scientific-grid-159'
previous=json.loads((root/'build/nominal-cells-291/consumers-command.json').read_text(encoding='utf-8'))
args=[v.replace('nominal-cells-291','nominal-cell-python-292') for v in previous['argv']]
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
log=out/'consumers.log';assert not log.exists()
with log.open('w',encoding='utf-8') as stream:result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/'consumers-command.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print('Existing consumers292',result.returncode,flush=True);assert result.returncode==0
