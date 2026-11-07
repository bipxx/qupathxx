"""Three direct contour probes on one immutable saved raster, outside workflow runs."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import time

parser=argparse.ArgumentParser()
parser.add_argument('runtime',type=Path);parser.add_argument('project',type=Path);parser.add_argument('output',type=Path)
args=parser.parse_args();sys.path.insert(0,str(args.runtime.resolve()))
import qupathxx as q
project=q.Project.open(str(args.project.resolve()));result=project.document(1).cell_record(3).result
options=result.grid_parameters.contours
runs=[]
for iteration in range(3):
    trial={}
    for name,mask in [('nucleus',result.nuclei),('body',result.cells)]:
        start=time.perf_counter();cpu=time.process_time();contours=q.trace_tissue(mask,options)
        wall=(time.perf_counter()-start)*1000;cpu=(time.process_time()-cpu)*1000
        # Include ordering, labels, pixel counts and exact17-digit coordinate
        # representations. Hash outside the contour timer.
        shapes=[[c.label,c.pixels,c.geometry.wkt] for c in contours.components]
        encoded=json.dumps(shapes,separators=(',',':')).encode()
        trial[name]=dict(wall_ms=wall,cpu_ms=cpu,components=len(shapes),geometry_sha256=hashlib.sha256(encoded).hexdigest())
    runs.append(trial)
environment=q.build_environment()
args.output.write_text(json.dumps(dict(source_tree=environment.source_tree_sha256,runs=runs),indent=2),encoding='utf-8')
print(args.output)
