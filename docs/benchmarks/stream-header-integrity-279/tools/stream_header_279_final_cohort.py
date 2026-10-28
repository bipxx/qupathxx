from pathlib import Path
import importlib.util,json,os,subprocess,xml.etree.ElementTree as ET
root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159';out=root/'build/stream-header-integrity-279'
spec=importlib.util.spec_from_file_location('raw',work/'tools/ci/raw_classifier_evidence.py')
raw=importlib.util.module_from_spec(spec);spec.loader.exec_module(raw)
for mode,gui in [('gui',True),('nonrendering',False)]:
    log=out/(mode+'-final-cohort.log');assert not log.exists()
    args=[str(root/'build/ci/stream-header-integrity-279-env/Scripts/python.exe'),'-I','-X','utf8','-m','pytest',
          *['tests/'+name+'.py' for name in raw.CONTRACT['installed']],'-q',
          '--junitxml='+str(out/(mode+'-final-cohort.xml')),'--basetemp',str(out/(mode+'-final-cohort-temp'))]
    env=os.environ.copy();env['QUPATHXX_TEST_GUI']='1' if gui else '0'
    (out/(mode+'-final-cohort-command.json')).write_text(json.dumps(dict(argv=args,cwd=str(work),gui_opt_in=gui,
        runtime_source='3bd6591930baa64ae5826d8ba3e2efaee37e8376',runtime_gui_capability=True),indent=2)+'\n')
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT)
    print(mode,'pytest exit',result.returncode,flush=True)
    assert result.returncode==0
    tree=ET.parse(out/(mode+'-final-cohort.xml'))
    admitted=raw.python(tree.findall('.//testcase'),gui=gui)
    assert (admitted['passed'],admitted['declared_skips'])==((395,1) if gui else (348,48))
    (out/(mode+'-final-cohort-admission.json')).write_text(json.dumps(admitted,indent=2)+'\n')
    print(mode,'admission',admitted['passed'],admitted['declared_skips'],flush=True)
