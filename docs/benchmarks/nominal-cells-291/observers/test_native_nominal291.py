from pathlib import Path
import json
import subprocess

root=Path(__file__).resolve().parents[2];out=root/'build/nominal-cells-291'
ctest=root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'
args=[str(ctest),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release',
      '--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000',
      '--output-junit',str(out/'native-initial.xml')]
assert not (out/'native-initial.log').exists()
with (out/'native-initial.log').open('w',encoding='utf-8') as stream:
    result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
(out/'native-initial-command.json').write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print('Initial native291 exit',result.returncode,flush=True)
raise SystemExit(result.returncode)
