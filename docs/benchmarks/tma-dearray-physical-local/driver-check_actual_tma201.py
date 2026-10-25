from pathlib import Path
import importlib.util,json,os,subprocess,xml.etree.ElementTree as ET
r=Path(__file__).resolve().parents[2];s=r/'build/ci/scientific-grid-159';out=r/'build/tma201-actual-protocol';out.mkdir(exist_ok=False);python=r/'build/ci/tma200-python-env/Scripts/python.exe';env=os.environ.copy();env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8'
spec=importlib.util.spec_from_file_location('actual_dearray_evidence',s/'tools/ci/tma_dearray_evidence.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
commands=[('tma-dearray-typing-positive.log','tests/typing/tma_dearray_valid.py',0),*[(v['log'],'tests/typing/'+name,1) for name,v in m.CONTRACT['typing_negative'].items()]]
for log,source,expected in commands:
 with (out/log).open('wb') as f:p=subprocess.run([str(python),'-I','-m','mypy','--strict','--no-incremental',source],cwd=s,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
 assert p.returncode==expected,(out/log).read_text(encoding='utf-8')
report={}
for host in ['windows','linux']:
 files=[r/'build/tma200-windows/tma_dearray.xml'] if host=='windows' else [r/'build/tma200-linux/tests.xml'];cases=[c for p in files for c in ET.parse(p).getroot().iter('testcase')];report[host]=m.native(cases)
for name,gui in [('runtime',False),('dearray-desktop',True)]:
 cases=list(ET.parse(r/('build/tma200-evidence/'+name+'.xml')).getroot().iter('testcase'));report[name]=m.desktop(cases,gui=gui)
 if not gui:report[name]['installed']=m.python(cases)
report['typing']=m.typing(out)
for label,path,gate in [('old_native',r/'build/tma197-windows/tma_dearray.xml',m.native),('old_installed',r/'build/tma198-evidence-retry/runtime.xml',m.python)]:
 try:gate(list(ET.parse(path).getroot().iter('testcase')))
 except ValueError as e:report[label+'_rejected']=str(e)
 else:raise AssertionError(label+' falsely accepted')
(out/'actual-protocol.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
