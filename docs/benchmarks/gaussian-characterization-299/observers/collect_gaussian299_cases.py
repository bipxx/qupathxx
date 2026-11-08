from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/gaussian-characterization-299'
args=[str(root/'build/ci/reader-locale-297-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest','tests/test_qupath_gaussian.py','--collect-only','-q','--noconftest']
result=subprocess.run(args,cwd=work,capture_output=True,text=True);assert result.returncode==0,result.stderr
(out/'collect.log').write_text(result.stdout+result.stderr,encoding='utf-8')
names=sorted(line.split('::',1)[1] for line in result.stdout.splitlines() if line.startswith('tests/test_qupath_gaussian.py::'))
assert len(names)==len(set(names))==60
path=work/'tools/ci/scientific-grid-cases.json';cases=json.loads(path.read_text());assert 'test_qupath_gaussian' not in cases
cases['test_qupath_gaussian']=names
path.write_text(json.dumps(cases,indent=2)+'\n',encoding='utf-8')
print('Bound60 independently collected Gaussian reference checks')
