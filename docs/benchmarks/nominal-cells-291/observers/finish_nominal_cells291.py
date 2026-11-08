from pathlib import Path
import json
import subprocess
import sys

root=Path(__file__).resolve().parents[2];out=root/'build/nominal-cells-291';mode=sys.argv[1]
if mode=='native-repaired':
    args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release',
      '--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native-repaired.xml')]
elif mode=='install-repaired':
    python=root/'build/ci/nominal-cells-291-env/Scripts/python.exe';wheel,=list((out/'repaired-wheel').glob('*.whl'))
    args=[str(python),'-I','-m','pip','install','--no-deps','--force-reinstall',str(wheel)]
else:raise ValueError(mode)
log=out/(mode+'.log');assert not log.exists()
with log.open('w',encoding='utf-8') as stream:result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print(mode,result.returncode,flush=True);assert result.returncode==0
