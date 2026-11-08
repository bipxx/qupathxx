import sys,time,json
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1]).resolve()))
import qupathxx as q
project=q.Project.open(str(Path(sys.argv[2]).resolve()));result=project.document(1).cell_record(3).result
runs=[]
for iteration in range(3):
 nucleus=q.trace_tissue(result.nuclei,result.grid_parameters.contours)
 body=q.trace_tissue(result.cells,result.grid_parameters.contours)
 pairs=[(a.geometry,b.geometry) for a,b in zip(body.components,nucleus.components)]
 begin=time.perf_counter();cold=[a.covers(b) for a,b in pairs];cold_ms=(time.perf_counter()-begin)*1000
 begin=time.perf_counter();warm=[a.covers(b) for a,b in pairs];warm_ms=(time.perf_counter()-begin)*1000
 assert cold==warm
 runs.append(dict(cold_ms=cold_ms,warm_ms=warm_ms,repairs=cold.count(False),cells=len(cold)))
Path(sys.argv[3]).write_text(json.dumps(dict(source_tree=q.build_environment().source_tree_sha256,runs=runs),indent=2),encoding='utf-8')
print(json.dumps(runs))
