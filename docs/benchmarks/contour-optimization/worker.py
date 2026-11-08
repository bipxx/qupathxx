"""Persistent native contour worker for a bounded interleaved comparison."""
import hashlib
import json
from pathlib import Path
import sys
import time
import qupathxx as q

project=q.Project.open_without_journal_recovery(str(Path(sys.argv[1]).resolve()))
document=project.document(1)
result=document.cell_record(3).result
masks=[('nucleus',result.nuclei),('body',result.cells)]
options=result.grid_parameters.contours
mode=sys.argv[2] if len(sys.argv)>2 else 'trace'
request=q.CellRequest.replay(document,3) if mode=='detect' else None

def trace():
    value={}
    active=masks
    detected=None
    if mode=='detect':
        begin=time.perf_counter();detected=q.detect_cells(request)
        value['detector_ms']=(time.perf_counter()-begin)*1000
        assert detected.cell_count==2237
        active=[('nucleus',detected.nuclei),('body',detected.cells)]
    for name,mask in active:
        if detected is None:
            begin=time.perf_counter();geometry=q.trace_tissue(mask,options)
            wall_ms=(time.perf_counter()-begin)*1000
        else:
            geometry=detected.nucleus_contours if name=='nucleus' else detected.cell_contours
            wall_ms=0
        digest=hashlib.sha256(json.dumps([[c.label,c.pixels,c.geometry.wkt] for c in geometry.components],separators=(',',':')).encode()).hexdigest()
        value[name]=dict(wall_ms=wall_ms,geometry=digest,edges=geometry.edge_count,vertices=geometry.vertex_count,
                         work_bytes=geometry.estimated_work_bytes,retained_bytes=geometry.retained_bytes)
    return value

for _ in range(4):trace()
print(json.dumps(dict(ready=True,source_tree=q.build_environment().source_tree_sha256)),flush=True)
for line in sys.stdin:
    if line.strip()=='stop':break
    print(json.dumps(trace()),flush=True)
document.server.close()
