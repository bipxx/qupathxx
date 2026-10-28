from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cells-291'
python=root/'build/ci/nominal-cells-291-env/Scripts/python.exe'
mode=sys.argv[1];env=os.environ.copy();env['QUPATHXX_TEST_GUI']='0';env.pop('MYPYPATH',None)
if mode=='native':
    args=[str(root/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'),'--test-dir',str(root/'build/ci/gridfrontends167-package'),'-C','Release',
        '--output-on-failure','--test-output-size-passed','10000000','--test-output-size-failed','10000000','--output-junit',str(out/'native-final.xml')]
elif mode=='install':
    assert not python.exists()
    subprocess.run([str(root/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
    (python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(root/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
    wheel,=list((out/'final-wheel').glob('*.whl'))
    args=[str(python),'-I','-m','pip','install','--no-deps','--force-reinstall',str(wheel)]
elif mode=='stubs':args=[str(python),'-I','-X','utf8','tools/generate_stubs.py','--check']
elif mode=='scientific':
    spec=importlib.util.spec_from_file_location('science',work/'tools/ci/scientific_grid_evidence.py')
    science=importlib.util.module_from_spec(spec);spec.loader.exec_module(science)
    args=[str(python),'-I','-X','utf8','-m','pytest',*['tests/'+n+'.py' for n in science.CASES],'-q','--junitxml='+str(out/'scientific.xml'),'--basetemp',str(out/'scientific-temp')]
elif mode=='consumers':
    tests=['test_cells','test_cell_records','test_cell_calibration','test_cell_area_limits','test_cell_domain_replay','test_cell_feature_links',
        'test_cell_tables','test_cell_object_tables','test_object_classifier','test_object_classifier_archives','test_tissue','test_tissue_levels','test_tissue_records','test_tissue_contours','test_tissue_cleanup']
    assert all((work/'tests'/(n+'.py')).exists() for n in tests)
    args=[str(python),'-I','-X','utf8','-m','pytest',*['tests/'+n+'.py' for n in tests],'-q','--junitxml='+str(out/'consumers.xml'),'--basetemp',str(out/'consumers-temp')]
elif mode=='auditor':
    args=[str(python),'-I','-X','utf8','-m','pytest','tests/test_scientific_grid_evidence.py','tests/test_raw_classifier_evidence.py','-q','--junitxml='+str(out/'auditor.xml'),'--basetemp',str(out/'auditor-temp')]
else:raise ValueError(mode)
log=out/(mode+'.log');assert not log.exists()
with log.open('w',encoding='utf-8') as stream:
    result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
(out/(mode+'-command.json')).write_text(json.dumps(dict(argv=args,returncode=result.returncode),indent=2)+'\n',encoding='utf-8')
print(mode,result.returncode,flush=True);assert result.returncode==0,log
if mode=='scientific':
    admitted=science.python(ET.parse(out/'scientific.xml').findall('.//testcase'),gui=False)
    (out/'scientific-admission.json').write_text(json.dumps(admitted,indent=2)+'\n',encoding='utf-8')
