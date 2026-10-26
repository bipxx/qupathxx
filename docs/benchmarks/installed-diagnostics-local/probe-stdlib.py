import argparse,ast,json,sys,subprocess,importlib.util
from pathlib import Path
from collections import Counter
parser=argparse.ArgumentParser();parser.add_argument('--repository',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args();args.output.mkdir(exist_ok=False)
source=args.repository/'tools/ci/run.py';spec=importlib.util.spec_from_file_location('probe_producer',source);producer=importlib.util.module_from_spec(spec);exec(compile(source.read_bytes(),str(source),'exec',dont_inherit=True),producer.__dict__)
tests=args.repository/'tests/test_ci_installed_diagnostics.py';node=next(n for n in ast.parse(tests.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='test_preparation_and_all_legacy_command_arguments_remain_intact');scope=dict(ROOT=args.repository,SOURCE=source,ast=ast,json=json,Counter=Counter);exec(compile(ast.Module(body=[node],type_ignores=[]),str(tests),'exec'),scope);scope[node.name]()
d=producer.InstalledDiagnostics(args.output/'stages.json');d.run([sys.executable,'-I','-c','raise SystemExit(1)'],args.output/'failed.log');d.run(['must-not-start'],args.output/'blocked.log',requires=('failed',));d.run([sys.executable,'-I','-c','import time;time.sleep(5)'],args.output/'timeout.log',timeout=.05);d.run([sys.executable,'-I','-c','print("independent")'],args.output/'independent.log')
try:d.finish()
except RuntimeError:pass
else:raise RuntimeError('failed diagnostics unexpectedly accepted')
state=json.loads((args.output/'stages.json').read_text());assert state['state']=='failed' and [x['state'] for x in state['stages']]==['failed','not_run','timed_out','passed'];assert not (args.output/'blocked.log').exists();assert 'independent' in (args.output/'independent.log').read_text()
report=dict(python=sys.version,preparation_and_58_command_sites_preserved=True,real_child_failure_timeout_and_independent_completion=True,overall_failure_retained=True,scope='Standard-library producer boundary probe; not native/product qualification');(args.output/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report,indent=2))
