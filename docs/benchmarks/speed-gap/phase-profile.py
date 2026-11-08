import json,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1]).resolve()))
import qupathxx as q
project=q.Project.open(str(Path(sys.argv[2]).resolve()));doc=project.document(1)
request=q.CellRequest.replay(doc,3)
runs=[]
for iteration in range(4):
 job=q.CellJob();started=time.perf_counter();last=started;phase='idle';times={}
 assert job.request(request)
 while job.busy:
  status=job.poll(doc,1);now=time.perf_counter()
  if status.phase!=phase:
   times[phase]=times.get(phase,0)+(now-last)*1000;phase=status.phase;last=now
  time.sleep(.001)
 now=time.perf_counter();times[phase]=times.get(phase,0)+(now-last)*1000
 assert status.state=='ready' and job.result.cell_count==2237
 runs.append(dict(iteration=iteration,phase_ms=times,elapsed_ms=(now-started)*1000))
Path(sys.argv[3]).write_text(json.dumps(dict(method='Public CellJob phase polling every1ms; coarse grouped stages, diagnostic only',runs=runs),indent=2),encoding='utf-8')
print(json.dumps(runs))
