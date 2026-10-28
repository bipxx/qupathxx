from pathlib import Path
import json
import os
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cells-291'
previous=json.loads((root/'build/record-cohort-observer-275/preflight-command.json').read_text(encoding='utf-8'))
tests=[v.replace('\\','/') for v in previous['argv'] if v.replace('\\','/').startswith('tests/') and v.endswith('.py')]
assert len(tests)==20
tests=[p for p in tests if p not in ('tests/test_scientific_grid_evidence.py','tests/test_raw_classifier_evidence.py')];assert len(tests)==18
env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
commands=[('preflight-remaining',[str(root/'build/ci/nominal-cells-291-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest',*tests,'-q',
    '--junitxml='+str(out/'preflight-remaining.xml'),'--basetemp',str(out/'preflight-remaining-temp')]),
    ('captured-fixture',[str(root/'.venv/Scripts/python.exe'),'-I','-X','utf8','tools/reference_nominal_cells.py'])]
for name,args in commands:
    log=out/(name+'.log');assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    (out/(name+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
    print(name,result.returncode,flush=True);assert result.returncode==0
seen=set()
for name in ('auditor.xml','preflight-remaining.xml'):
    for case in ET.parse(out/name).findall('.//testcase'):
        key=(case.get('classname'),case.get('name'));assert key not in seen;seen.add(key)
        assert all(case.find(t) is None for t in ('error','failure','skipped'))
assert len(seen)==1071
(out/'preflight-union.json').write_text(json.dumps(dict(passed=len(seen),files=20,overlap=0,repeated_completed_modules=False),indent=2)+'\n',encoding='utf-8')
