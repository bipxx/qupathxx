import ast,importlib.util,json,os,py_compile,subprocess,sys,tempfile
from pathlib import Path
r=Path('/mnt/c/Users/HEQ/Projects/qupathxx');s=r/'build/ci/scientific-grid-159';ci=s/'tools/ci';checks=[]
with tempfile.TemporaryDirectory(prefix='qpx-audit207-') as temporary:
 root=Path(temporary)
 for path in sorted(ci.glob('*.py')):
  for node in ast.walk(ast.parse(path.read_text(encoding='utf-8'))):
   if not (isinstance(node,ast.Call) and isinstance(node.func,ast.Name) and node.func.id=='exec' and node.args and isinstance(node.args[0],ast.Call) and isinstance(node.args[0].func,ast.Name) and node.args[0].func.id=='compile' and '.origin' in ast.unparse(node)):continue
   target=root/'subject.py';target.write_bytes(b'VALUE = "old"\n');py_compile.compile(str(target),doraise=True);stamp=target.stat();target.write_bytes(b'VALUE = "new"\n');os.utime(target,ns=(stamp.st_atime_ns,stamp.st_mtime_ns))
   spec=importlib.util.spec_from_file_location('linux_source_witness',target);cached=importlib.util.module_from_spec(spec);spec.loader.exec_module(cached);assert cached.VALUE=='old'
   actual=importlib.util.module_from_spec(spec);origin=next(n for n in ast.walk(node) if isinstance(n,ast.Attribute) and n.attr=='origin');scope={'Path':Path,origin.value.id:spec,node.args[1].value.id:actual};exec(compile(ast.Expression(node),str(path),'eval'),scope);assert actual.VALUE=='new' and actual.__file__==str(target);checks.append(str(path.relative_to(s))+':'+str(node.lineno))
 done=subprocess.run([sys.executable,'-I','-O',str(ci/'accept_unified_engine.py'),'--help'],capture_output=True,timeout=15);assert done.returncode and b'artifact audit requires Python assertions enabled' in done.stderr;checks.append('optimized_entry')
 for key,value in [('PYTEST_ADDOPTS','--collect-only'),('PYTEST_ADDOPTS','-k nonexistent'),('PYTEST_PLUGINS','not_a_plugin')]:
  env=os.environ.copy();env.pop('PYTEST_ADDOPTS',None);env.pop('PYTEST_PLUGINS',None);env[key]=value;work=root/('work-'+str(len(checks)));done=subprocess.run([sys.executable,'-I',str(ci/'run.py'),'--mode','headless','--work',str(work)],env=env,capture_output=True,timeout=20);assert done.returncode and ('qualification rejects inherited '+key).encode() in done.stderr and not work.exists();checks.append(key+':'+value)
value=dict(scope='Linux stdlib execution-boundary witnesses; not a full pytest/platform campaign',python=sys.version,checks=checks,passed=len(checks));(r/'build/audit207-linux-boundaries.json').write_bytes((json.dumps(value,indent=2)+'\n').encode());print('Linux execution-boundary witnesses passed',len(checks))
