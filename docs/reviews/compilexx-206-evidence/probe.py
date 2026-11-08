"""Bounded review witnesses; never imports an auditor's CLI or modifies product inputs."""
import ast
from collections import Counter
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import py_compile
import subprocess
import sys
import xml.etree.ElementTree as ET

OUT=Path(__file__).resolve().parent
SOURCE=OUT.parents[1]/'build/ci/scientific-grid-159'
results={}

# Compile the exact maintained JUnit function in isolation, including its
# original assertion. No network, run lookup or artifact acceptance is invoked.
auditor=SOURCE/'tools/ci/accept_unified_engine.py'
tree=ast.parse(auditor.read_text(encoding='utf-8'))
function=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='junit')
isolated=ast.Module(body=[function],type_ignores=[])
xml=OUT/'failed.xml'
xml.write_text('<testsuite tests="1" failures="1" errors="0" skipped="0"><testcase name="broken"><failure>failed</failure></testcase></testsuite>',encoding='utf-8')
for optimized in (0,1):
    scope=dict(ET=ET,Counter=Counter,sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest())
    exec(compile(isolated,str(auditor),'exec',optimize=optimized),scope)
    try:
        value=scope['junit'](xml)
        results['junit_optimize_'+str(optimized)]=dict(rejected=False,result=value)
    except AssertionError:
        results['junit_optimize_'+str(optimized)]=dict(rejected=True)

# Same standard loader used in the evidence helpers; preserve equal source
# size and mtime so a timestamp pyc is considered valid by CPython.
module=OUT/'loader_subject.py';old=b'VALUE = "old"\n';new=b'VALUE = "new"\n'
module.write_bytes(old)
py_compile.compile(str(module),doraise=True)
stamp=module.stat();module.write_bytes(new);os.utime(module,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
def load():
    spec=importlib.util.spec_from_file_location('review_loader_subject',module)
    value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value)
    return value.VALUE
prior=sys.dont_write_bytecode;sys.dont_write_bytecode=True
try: value=load()
finally: sys.dont_write_bytecode=prior
results['timestamp_cache']=dict(source_sha256=hashlib.sha256(module.read_bytes()).hexdigest(),source_value='new',loaded_value=value,dont_write_bytecode=True)

# -I ignores Python's environment configuration; it does not disable pytest's
# own environment inputs. Prove selection drift through the installed runner.
test=OUT/'test_selected.py';test.write_text('def test_selected():\n    assert True\n',encoding='utf-8')
for name,addopts in [('baseline',None),('inherited_collect_only','--collect-only')]:
    env=os.environ.copy()
    for key in ('PYTEST_ADDOPTS','PYTEST_PLUGINS','PYTEST_DISABLE_PLUGIN_AUTOLOAD'):env.pop(key,None)
    env['PYTEST_DISABLE_PLUGIN_AUTOLOAD']='1'
    if addopts:env['PYTEST_ADDOPTS']=addopts
    output=OUT/(name+'.xml')
    done=subprocess.run([sys.executable,'-I','-m','pytest',str(test),'-q','--junitxml='+str(output)],env=env,capture_output=True,timeout=30)
    (OUT/(name+'.log')).write_bytes(done.stdout+done.stderr)
    results[name]=dict(exit=done.returncode,cases=sum(1 for _ in ET.parse(output).getroot().iter('testcase')),addopts=addopts)

results['source']=subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()
(OUT/'witnesses.json').write_bytes((json.dumps(results,indent=2)+'\n').encode())
print(json.dumps(results,indent=2))
