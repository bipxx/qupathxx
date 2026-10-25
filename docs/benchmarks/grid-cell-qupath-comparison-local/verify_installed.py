from pathlib import Path
import subprocess,json,sys
root=Path(__file__).resolve().parent/'scientific-grid-159'
out=root.parents[1]/'grid170-local';out.mkdir(exist_ok=True)
manifest=json.loads((root/'tools/ci/scientific-grid-cases.json').read_text(encoding='utf-8'))
files=['tests/'+name+'.py' for name in manifest]
files+=['tests/test_scientific_grid_evidence.py','tests/test_manylinux_qualification.py','tests/test_manylinux_acceptance.py','tests/test_manylinux_body_acceptance.py','tests/test_campaign_inputs.py']
commands=[('installed',[sys.executable,'-I','-m','pytest',*files,'-q','--override-ini','addopts=','--junitxml='+str(out/'tests.xml')])]
commands += [(units,[sys.executable,'-I','tools/compare_qupath_grid_cells.py','--units',units,'--report',str(out/(units+'.json'))]) for units in ['pixels','micrometres']]
for name,cmd in commands:
 print('Running',name,flush=True)
 with (out/(name+'.log')).open('w',encoding='utf-8') as f:
  result=subprocess.run(cmd,cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=180)
 print(name,'exit',result.returncode,flush=True)
 if result.returncode:
  print((out/(name+'.log')).read_text(encoding='utf-8',errors='replace')[-12000:]);raise SystemExit(result.returncode)
