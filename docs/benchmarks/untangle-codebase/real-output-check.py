
import os,sys,importlib.util,json,hashlib,time
from pathlib import Path
binary=Path(sys.argv[1]).resolve();dll=os.add_dll_directory(str(binary.parent))
spec=importlib.util.spec_from_file_location('_native',binary);q=importlib.util.module_from_spec(spec);sys.modules[spec.name]=q;spec.loader.exec_module(q)
base=Path(sys.argv[2]).resolve();output=Path(sys.argv[3]);expected_tree=sys.argv[4]
assert q.build_environment().source_tree_sha256==expected_tree
# Independently retained legacy enum names, values and registration order.
legacy=json.loads((output.parent/'legacy-replay-enum.json').read_text())
observed=[(name,int(value.value)) for name,value in q.GuiInputWait.__members__.items()]
assert [list(v) for v in observed]==legacy
project=q.Project.open(str(base));doc=project.document(1);request=q.CellRequest.replay(doc,3)
frozen=json.loads((output.parent/'frozen-detector.json').read_text())['runs'][0];runs=[]
for iteration in range(4):
 began=time.perf_counter();result=q.detect_cells(request);elapsed=(time.perf_counter()-began)*1000
 masks={key:hashlib.sha256(mask.labels_numpy().astype('<u4',copy=False).tobytes()).hexdigest() for key,mask in [('nucleus',result.nuclei),('body',result.cells)]}
 geometry={key:hashlib.sha256(json.dumps([[c.label,c.pixels,c.geometry.wkt] for c in contours.components],separators=(',',':')).encode()).hexdigest() for key,contours in [('nucleus',result.nucleus_contours),('body',result.cell_contours)]}
 assert result.cell_count==2237 and masks==frozen['masks'] and geometry==frozen['geometry']
 runs.append(dict(iteration=iteration,cells=2237,masks=masks,geometry=geometry,wall_ms=elapsed))
output.write_text(json.dumps(dict(source_tree=expected_tree,compiler_inputs=530,enum_values=observed,runs=runs),indent=2),encoding='utf-8')
print(json.dumps({'cells':2237,'runs':len(runs),'exact_masks_geometry':True,'enum_members':len(observed),'source_tree':expected_tree}))
