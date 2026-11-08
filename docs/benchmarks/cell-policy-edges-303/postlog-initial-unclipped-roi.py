from pathlib import Path
import importlib.util
import json
import numpy as np
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/cell-policy-edges-303'
spec=importlib.util.spec_from_file_location('postlog_edges',work/'tools/qupath_cell_policy_edges.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
metric=tool.load('postlog_metrics',work/'tools/compare_qupath_cells.py');checks=[]
for case in tool.contract()['cases']:
    metadata,stages,rgb=tool.data(out/'capture1',case);step=case.get('requested_pixel_size_um',.5)/.5
    bounds=case.get('roi',[0,0,256,192]);x,y,w,h=bounds
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(step,step)
    def ring(rect):
        a,b,c,d=rect;return [q.Point(a,b),q.Point(a+c,b),q.Point(a+c,b+d),q.Point(a,b+d)]
    roi=[0,0,w/step,h/step]
    if 'hole' in case:
        a,b,c,d=case['hole'];hole=[(a-x)/step,(b-y)/step,c/step,d/step]
        obj=doc.add_roi(q.Roi.polygon(ring(roi),[ring(hole)]))
    else:obj=doc.add_rectangle(q.Rect(*roi))
    recipe=case.get('native',tool.contract()['native']);p=q.GridCellParameters()
    p.processing=q.CellProcessingPolicy.IMAGEJ_154P;p.units=q.GridCellUnits.MICROMETRES
    p.threshold=recipe['threshold'];p.sigma=recipe['sigma'];p.cell_expansion=recipe['cell_expansion']
    p.min_nucleus_area=recipe['min_nucleus_pixels'];p.max_nucleus_area=recipe['max_nucleus_pixels']
    transform=q.StainTransform(tool.contract()['vectors'],tool.contract()['white'])
    descriptor=q.NominalCellSampling(q.RgbNominalRequest(0,0,rgb.shape[1],rgb.shape[0],1),0,0)
    result=q.detect_cells(q.CellRequest(doc,obj,transform,descriptor,p))
    origin=(x,y);features=tool.read(out/'capture1'/case['name']/'cells.geojson')['features']
    reference={name:tool.geometry_labels(features,key,rgb.shape[:2],origin,step) for name,key in [('nucleus','nucleusGeometry'),('body','geometry')]}
    actual={'nucleus':result.nuclei.labels_numpy(),'body':result.cells.labels_numpy()}
    checks.append(dict(name=case['name'],captured_processing_input=True,
        metric_steps=[result.resolved_grid_parameters.step_x,result.resolved_grid_parameters.step_y],
        metrics={name:metric.metrics(reference[name],labels,tool.contract()['comparison']) for name,labels in actual.items()}))
(out/'postlog-comparison.json').write_text(json.dumps(dict(schema='qpx.captured-input-postlog-diagnostic/1',
    native_source=q.build_environment().git_revision,cases=checks,
    input_authority='New diagnostic processing-pixel document; original-source replay authority is not transferred',
    metric='Synthetic per-axis calibration preserves original source-pixel distances/areas while presenting captured processing RGB at unit spacing',
    biological_validation=False,segmentation_parity_accepted=False),indent=2)+'\n',encoding='utf-8')
print([(case['name'],{name:value['foreground_iou'] for name,value in case['metrics'].items()}) for case in checks])
