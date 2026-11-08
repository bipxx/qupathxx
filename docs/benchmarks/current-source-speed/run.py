"""Bounded alternating cross-engine measurement; waits excluded from stage timers."""
import json
from pathlib import Path
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT.parents[1]
BENCH=Path(__file__).resolve().parent
work=Path(sys.argv[1]).resolve();work.mkdir(exist_ok=False)
measure=ROOT/'docs/benchmarks/post-consolidation-speed/measure.py'
commands={
 'native':[sys.executable,'-I',str(BENCH/'native.py'),str(BUILD/'fixtures/CMU-1.svs'),str(work/'native'),str(BUILD/'consolidation/development'),'--steady','--expected-tree','2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e','--label','contour-optimized-development'],
 'qupath':[str(BUILD/'upstream/qupath/runtime/QuPath-0.7.0 (console).exe'),'script',str(BENCH/'qupath.groovy'),'--args',str(BUILD/'fixtures/CMU-1.svs'),'--args',str(work/'qupath'),'--args','steady','--args','24','--args','6']}
children={};logs={};rows=[]
def wait_ready(name,index):
 deadline=time.monotonic()+300
 path=work/name/('ready-'+str(index))
 while not path.exists():
  if children[name].poll() is not None:raise RuntimeError(name+' exited early')
  if time.monotonic()>deadline:raise TimeoutError(str(path))
  time.sleep(.02)
try:
 for name in ('native','qupath'):
  logs[name]=(work/(name+'-monitor.log')).open('w',encoding='utf-8')
  children[name]=subprocess.Popen([sys.executable,'-I',str(measure),str(work/name),'--',*commands[name]],stdout=logs[name],stderr=subprocess.STDOUT)
  wait_ready(name,6 if name=='native' else 24)
 for pair in range(6):
  order=['native','qupath'] if pair%2==0 else ['qupath','native']
  row=dict(pair=pair,order=order,windows={})
  for name in order:
   index=(6 if name=='native' else 24)+pair
   start=time.time_ns();(work/name/('go-'+str(index))).touch()
   if pair<5:wait_ready(name,index+1)
   else:
    code=children[name].wait(timeout=300)
    if code:raise RuntimeError(name+' failed '+str(code))
   row['windows'][name]=dict(start_unix_ns=start,end_unix_ns=time.time_ns())
  rows.append(row)
  (work/'schedule.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
  print('Completed pair '+str(pair+1),flush=True)
finally:
 for child in children.values():
  if child.poll() is None:child.terminate();child.wait(timeout=20)
 for log in logs.values():log.close()
