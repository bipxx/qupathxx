from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
root=Path(__file__).resolve().parents[2];out=root/'build/qupath-input-stages-301';prefix=root/'build/ci/reader-isolated-pth-301';prefix.mkdir(exist_ok=False)
base=Path(sys.base_prefix);files=[]
for name in ('python.exe','python312.dll','python3.dll','vcruntime140.dll','vcruntime140_1.dll'):
    if (base/name).is_file():
        shutil.copyfile(base/name,prefix/name);assert (base/name).read_bytes()==(prefix/name).read_bytes()
        files.append(dict(path=name,sha256=hashlib.sha256((prefix/name).read_bytes()).hexdigest()))
pth=prefix/'python312._pth';pth.write_text('\n'.join([str(base/'Lib'),str(base/'DLLs'),str(root/'build/ci/reader-locale-297-env/Lib/site-packages'),str(root/'.venv/Lib/site-packages'),'import site'])+'\n')
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env['PYTHONPATH']='deliberately-ignored-isolated-reader-path'
args=[str(prefix/'python.exe'),'-I','-X','utf8','-m','pytest','tests/test_reader_locale.py','-q','--junitxml='+str(out/'reader-pth.xml')]
with (out/'reader-pth.log').open('x',encoding='utf-8') as stream:r=subprocess.run(args,cwd=root/'build/ci/scientific-grid-159',env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/'reader-pth-command.json').write_text(json.dumps(dict(argv=args,returncode=r.returncode,copied_interpreter_files=files,pth_sha256=hashlib.sha256(pth.read_bytes()).hexdigest(),pth=pth.read_text(),shared_test_dependencies=True,rendering_exercised=False,clean_machine=False),indent=2)+'\n')
print('Actual _pth isolated reader regressions exit',r.returncode,flush=True);assert r.returncode==0
