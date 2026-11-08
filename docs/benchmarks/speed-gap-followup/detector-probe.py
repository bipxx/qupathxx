import sys,time,json,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(sys.argv[1]).resolve()))
import qupathxx as q
assert q.build_environment().source_tree_sha256==sys.argv[4]
project=q.Project.open(str(Path(sys.argv[2]).resolve()));doc=project.document(1);request=q.CellRequest.replay(doc,3)
runs=[]
for iteration in range(4):
 begin=time.perf_counter();cpu=time.process_time();result=q.detect_cells(request)
 wall=(time.perf_counter()-begin)*1000;cpu=(time.process_time()-cpu)*1000
 masks={key:hashlib.sha256(mask.labels_numpy().astype('<u4',copy=False).tobytes()).hexdigest() for key,mask in [('nucleus',result.nuclei),('body',result.cells)]}
 geometry={key:hashlib.sha256(json.dumps([[c.label,c.pixels,c.geometry.wkt] for c in contours.components],separators=(',',':')).encode()).hexdigest() for key,contours in [('nucleus',result.nucleus_contours),('body',result.cell_contours)]}
 assert result.cell_count==2237
 runs.append(dict(iteration=iteration,wall_ms=wall,cpu_ms=cpu,cells=2237,masks=masks,geometry=geometry))
Path(sys.argv[3]).write_text(json.dumps(dict(source_tree=q.build_environment().source_tree_sha256,runs=runs),indent=2),encoding='utf-8')
print(json.dumps([round(r['wall_ms'],1) for r in runs]))
