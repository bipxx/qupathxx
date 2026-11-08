from pathlib import Path
import hashlib,json,os,shutil,subprocess,time,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/raw-stream-history-248/final';py=r/'build/ci/raw-stream-history-248-env/Scripts/python.exe';cache=r/'build/ci/gridfrontends167-package'
snapshot=json.loads((out/'compiler-inputs.json').read_text());commands=[]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def unchanged():
 for name,digest in snapshot['files'].items():assert sha(s/name)==digest,name
unchanged();(out/'products').mkdir();products={}
for name in json.loads((out/'previous-products.json').read_text()):
 p=cache/'Release'/name;shutil.copyfile(p,out/'products'/name);products[name]={'source':snapshot['source'],'sha256':sha(p),'bytes':p.stat().st_size}
(out/'products.json').write_text(json.dumps(products,indent=2)+'\n',encoding='utf-8')
observer={'source':subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),'files':{name:sha(s/name) for name in ['tests/test_raw_stream_contour_python.py','tests/raw_stream_contour_shutdown_child.py','tests/typing/raw_stream_contour_valid.py','tests/typing/raw_stream_contour_invalid.py','tools/generate_stubs.py','tests/test_gui_raw_stream_contours.py','tests/test_raw_stream_contour_cli.py','tests/test_raw_pixel_cli.py','tests/test_raw_stream_contour_batch.py','tests/test_gui_batch.py','tests/test_gui_pixel_batch.py','tests/test_raw_file_contours.py','tests/raw_file_contour_shutdown_child.py']},'compiled_runtime_source':snapshot['source'],'compiler_inputs_unchanged':True}
(out/'consumer-observer.json').write_text(json.dumps(observer,indent=2)+'\n',encoding='utf-8')
base_env=os.environ.copy();
for key in ['PYTEST_ADDOPTS','PYTEST_PLUGINS']:
 if base_env.get(key):raise RuntimeError('unbound pytest configuration: '+key)
base_env['QUPATHXX_HISTORY_EVIDENCE_DIR']=str(out/'history-captures');base_env['QUPATHXX_HISTORY_SHUTDOWN_DIR']=str(out/'history-shutdown');base_env['QUPATHXX_STREAM_BATCH_EVIDENCE_DIR']=str(out/'batch-captures');base_env['QUPATHXX_TEST_GUI']='1';base_env['QUPATHXX_STREAM_FRONTEND_EVIDENCE_DIR']=str(out/'captures');base_env['QUPATHXX_STREAM_CONTOUR_EVIDENCE_DIR']=str(out/'active-shutdown')
def run(name,args,expected=0):
 unchanged();start=time.monotonic()
 with (out/(name+'.log')).open('wb') as f:done=subprocess.run(list(map(str,args)),stdout=f,stderr=subprocess.STDOUT,env=base_env)
 commands.append({'name':name,'command':list(map(str,args)),'exit':done.returncode,'expected':expected,'seconds':time.monotonic()-start});(out/'verification-commands.json').write_text(json.dumps(commands,indent=2)+'\n',encoding='utf-8')
 unchanged()
 if done.returncode!=expected:raise RuntimeError(name+' failed; retained '+str(out/(name+'.log')))
 print(name+' passed',flush=True)
cmake=r/'.venv/Lib/site-packages/cmake/data/bin/ctest.exe'
run('native-tests',[cmake,'--test-dir',cache,'-C','Release','-R','^(project_batch|pixel_features|pixel_workflows|classifier|viewer_session|gui_replay|overlay_surface|resampling|tissue_contours)$','--output-on-failure','--test-output-size-passed','1048576','--no-tests=error','--output-junit',out/'native-tests.xml'])
cases=list(ET.parse(out/'native-tests.xml').getroot().iter('testcase'));assert len(cases)==9 and not any(any(t.find(k) is not None for k in ['failure','error','skipped']) for t in cases)
tests=['test_raw_file_contours','test_raw_stream_contour_batch','test_gui_batch','test_gui_pixel_batch','test_raw_stream_contour_cli','test_raw_pixel_batch','test_project_batch','test_raw_pixel_cli','test_raw_pixel_python','test_pixel_stream_cli','test_project_pixel_batch','test_raw_batch','test_public_stubs','test_gui_raw_pixel','test_gui_pixel_classifier','test_viewer_session','test_raw_stream_contour_python','test_raw_contour_seams','test_gui_raw_stream_contours']
run('python-tests',[py,'-I','-m','pytest',*[s/'tests'/(name+'.py') for name in tests],'-q','--junitxml='+str(out/'python-tests.xml')])
new=[t for t in ET.parse(out/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_raw_stream_contour_python')]
assert len(new)==7 and not any(any(t.find(k) is not None for k in ['failure','error','skipped']) for t in new)
gui=[t for t in ET.parse(out/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_gui_raw_stream_contours')]
assert len(gui)==6 and not any(any(t.find(k) is not None for k in ['failure','error','skipped']) for t in gui)
cli=[t for t in ET.parse(out/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_raw_stream_contour_cli')]
assert len(cli)==18 and not any(any(t.find(k) is not None for k in ['failure','error','skipped']) for t in cli)
batch=[t for t in ET.parse(out/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_raw_stream_contour_batch')]
assert len(batch)==16 and all(all(t.find(k) is None for k in ['skipped','failure','error']) for t in batch)
history=[t for t in ET.parse(out/'python-tests.xml').getroot().iter('testcase') if t.get('classname','').endswith('test_raw_file_contours')]
assert len(history)==23 and all(all(t.find(k) is None for k in ['skipped','failure','error']) for t in history)
run('stubs-check',[py,'-I',s/'tools/generate_stubs.py','--check'])
for kind in ['valid','invalid']:
 old=json.loads((r/'build/rawgui234-final'/('pyright-'+kind+'.json')).read_text())
 paths=[(r/'build/rawgui234-final'/name).resolve() for name in old['include']]+[s/'tests/typing'/('raw_stream_contour_'+kind+'.py')]
 old['include']=[Path(os.path.relpath(p,out)).as_posix() for p in paths]
 old['venvPath']=str(py.parent.parent.parent);old['venv']=py.parent.parent.name
 config=out/('pyright-'+kind+'.json');config.write_text(json.dumps(old,indent=2)+'\n',encoding='utf-8')
 run('typing-'+kind,['uv','tool','run','--from','pyright==1.1.407','pyright','--project',config,'--pythonpath',py,'--outputjson'],expected=int(kind=='invalid'))
 report=json.loads((out/('typing-'+kind+'.log')).read_text());errors=[d for d in report['generalDiagnostics'] if d.get('severity')=='error']
 if kind=='valid':assert not errors and report['summary']['filesAnalyzed']==5
 else:
  assert len(errors)==20 and report['summary']['filesAnalyzed']==4 and sum(Path(d['file']).name=='raw_stream_contour_invalid.py' for d in errors)==8
print('Final installed consumer/native/typing closure passes with runtime and observer sources kept separate.',flush=True)
