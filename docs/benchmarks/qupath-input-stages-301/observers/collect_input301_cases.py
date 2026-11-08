from pathlib import Path
import json
import subprocess
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/qupath-input-stages-301'
args=[str(root/'build/ci/reader-locale-297-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest','tests/test_qupath_cell_inputs.py','--collect-only','-q','--noconftest']
r=subprocess.run(args,cwd=work,capture_output=True,text=True);assert r.returncode==0
names=sorted(line.split('::',1)[1] for line in r.stdout.splitlines() if line.startswith('tests/test_qupath_cell_inputs.py::'))
assert len(names)==45
(out/'collect.log').write_text(r.stdout+r.stderr)
path=work/'tools/ci/scientific-grid-cases.json';cases=json.loads(path.read_text());assert 'test_qupath_cell_inputs' not in cases;cases['test_qupath_cell_inputs']=names
path.write_text(json.dumps(cases,indent=2)+'\n')
before=out/'capture2';after=out/'capture3';files=[]
for name in ['cases.json','pairwise.rgb','roi.rgb','resampled.rgb',*[c['name']+'-model.json' for c in json.loads((before/'cases.json').read_text())],*[p.name for p in sorted(before.glob('*.f32le.gz'))]]:
    assert (before/name).read_bytes()==(after/name).read_bytes(),name
    files.append(name)
(out/'repeat-proof.json').write_text(json.dumps(dict(original_successful_capture='capture2',second_successful_capture='capture3',byte_identical_payloads=len(files),files=files,final_generator_adds_runtime_metadata=True),indent=2)+'\n')
print('45 independently collected cases;92 repeated payloads exact')
