from pathlib import Path
import importlib.util
import json
import sys

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/original-source-305'
mode=sys.argv[1]
path=work/'tools'/('qupath_cell_policy_edges.py' if mode=='edges' else 'compare_qupath_nominal_cells.py')
spec=importlib.util.spec_from_file_location('original305_'+mode,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report=module.compare(processing_policy='qupath070',sampling='affine-inverse') if mode=='edges' else module.compare(context=0,processing='qupath070',sampling='affine-inverse')
with (out/(mode+'-original-source-comparison.json')).open('x',encoding='utf-8') as stream:json.dump(report,stream,indent=2,allow_nan=False);stream.write('\n')
if mode=='edges':
    print(json.dumps([dict(name=c['name'],rgb=c['input_component_differences'],masks={n:m['foreground_iou'] for n,m in c['metrics'].items()}) for c in report['cases']]))
else:
    print(json.dumps([dict(name=c['case_name'],masks={n:m['foreground_iou'] for n,m in c['level_zero_segmentation'].items()}) for c in report['cases']]))
