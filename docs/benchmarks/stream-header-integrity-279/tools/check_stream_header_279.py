"""Run installed checks once the single broad279 build completes."""
from pathlib import Path
import importlib.util
import json
import os
import subprocess
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'build/ci/scientific-grid-159'
OUT=ROOT/'build/stream-header-integrity-279'
PYTHON=ROOT/'build/ci/stream-header-integrity-279-env/Scripts/python.exe'
manifest=json.loads((OUT/'compiler-inputs-initial.json').read_text())
spec=importlib.util.spec_from_file_location('raw',WORK/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)

def run(name,args,gui=False,expected=0):
    log=OUT/(name+'.log');assert not log.exists()
    env=os.environ.copy();env['QUPATHXX_TEST_GUI']='1' if gui else '0'
    (OUT/(name+'-command.json')).write_text(json.dumps(dict(argv=[str(a) for a in args],cwd=str(WORK),gui_opt_in=gui,source=manifest['source']),indent=2)+'\n')
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=WORK,env=env,stdout=stream,stderr=subprocess.STDOUT)
    print(name,'exit',result.returncode,flush=True)
    assert result.returncode==expected,log

def pytest(name,files,gui=False):
    run(name,[PYTHON,'-I','-X','utf8','-m','pytest',*files,'-q','--junitxml='+str(OUT/(name+'.xml')),'--basetemp',OUT/(name+'-temp')],gui)
    cases=ET.parse(OUT/(name+'.xml')).findall('.//testcase')
    assert not any(c.find('failure') is not None or c.find('error') is not None for c in cases)
    return cases

run('native',[ROOT/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe','--test-dir',ROOT/'build/ci/gridfrontends167-package','-C','Release','--output-on-failure','--output-junit',OUT/'native.xml'])
cases=ET.parse(OUT/'native.xml').findall('.//testcase');assert len(cases)==55
for gui,name,expected in [(True,'gui-cohort',(377,1)),(False,'nonrendering-cohort',(330,48))]:
    cases=pytest(name,['tests/'+m+'.py' for m in raw.CONTRACT['installed']],gui)
    admission=raw.python(cases,gui=gui)
    assert (admission['passed'],admission['declared_skips'])==expected
    (OUT/(name+'-admission.json')).write_text(json.dumps(admission,indent=2)+'\n')
    print(name,expected,flush=True)
cases=pytest('stream-and-ui-regressions',['tests/test_pixel_stream_file.py','tests/test_pixel_stream_file_portable.py','tests/test_pixel_stream_read_job.py','tests/test_pixel_stream_export_job.py',
    *['tests/test_gui_'+suffix+'.py' for suffix in ['layout','docking','scale','overlay','lifecycle','workspace','window','replay','retirement']]],True)
stream=[c for c in cases if c.get('classname')=='tests.test_pixel_stream_file']
assert len(stream)==41 and all(c.find('skipped') is None for c in stream)
preflight=json.loads((ROOT/'build/record-cohort-observer-275/preflight-command.json').read_text())
args=[arg.replace('snapshot-waits-274-env','stream-header-integrity-279-env').replace('record-cohort-observer-275','stream-header-integrity-279') for arg in preflight['argv']]
run('preflight-final',args)
cases=ET.parse(OUT/'preflight-final.xml').findall('.//testcase');assert len(cases)==1039 and not any(c.find(k) is not None for c in cases for k in ['failure','error','skipped'])
print('All279 native, installed cohorts, stream/UI and audit checks passed',flush=True)
