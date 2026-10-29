from pathlib import Path
import json
import subprocess
import sys
root=Path(__file__).resolve().parents[2];out=root/'build/original-source-305';mode=sys.argv[1]
args=[str(root/'build/ci/original305-env/Scripts/python.exe'),'-I','-X','utf8',str(root/'build/ci/observe_original305.py'),mode]
with (out/(mode+'-comparison.log')).open('x',encoding='utf-8') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT)
with (out/(mode+'-comparison-command.json')).open('x',encoding='utf-8') as stream:json.dump(dict(argv=args,returncode=r.returncode),stream,indent=2);stream.write('\n')
print(mode,r.returncode,flush=True);raise SystemExit(r.returncode)
