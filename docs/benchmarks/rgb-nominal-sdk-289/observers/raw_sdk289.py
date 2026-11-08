from pathlib import Path
import importlib.util
import json
import os
import subprocess
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/rgb-nominal-sdk-289'
python=root/'build/ci/rgb-nominal-sdk-289-env/Scripts/python.exe'
spec=importlib.util.spec_from_file_location('raw',work/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec)
spec.loader.exec_module(raw)
for label,gui in [('raw-gui',True),('raw-nonrendering',False)]:
    args=[str(python),'-I','-X','utf8','-m','pytest',*['tests/'+n+'.py' for n in raw.CONTRACT['installed']],'-q',
        '--junitxml='+str(out/(label+'.xml')),'--basetemp',str(out/(label+'-temp'))]
    env=os.environ.copy();env['QUPATHXX_TEST_GUI']='1' if gui else '0'
    log=out/(label+'.log');assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    (out/(label+'-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),gui=gui,returncode=result.returncode),indent=2)+'\n')
    print(label,result.returncode,flush=True)
    assert result.returncode==0
    admitted=raw.python(ET.parse(out/(label+'.xml')).findall('.//testcase'),gui=gui)
    assert (admitted['passed'],admitted['declared_skips'])==((395,1) if gui else (348,48))
    (out/(label+'-admission.json')).write_text(json.dumps(admitted,indent=2)+'\n')
