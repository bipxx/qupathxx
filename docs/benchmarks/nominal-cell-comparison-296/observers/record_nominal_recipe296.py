"""Reconstruct and record the compared native requests without rerunning detection."""
from pathlib import Path
import hashlib
import importlib.util
import json
import sys
import numpy as np
from PIL import Image
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-comparison-296'
host=sys.argv[1];assert host in ('windows','linux')
spec=importlib.util.spec_from_file_location('recipe_comparison',work/'tools/compare_qupath_nominal_cells.py');tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
contract=tool.shared().verify_capture(tool.FIXTURES/'capture');cases={c['name']:c for c in contract['cases']};records=[]
for profile in ('roi-context0','roi-context32','full-context0','physical-context32'):
    report=json.loads((out/host/(profile+'.json')).read_text(encoding='utf-8'))
    assert report['build_git_revision']==q.build_environment().git_revision
    assert report['comparison_tool_sha256']==hashlib.sha256((work/'tools/compare_qupath_nominal_cells.py').read_bytes()).hexdigest()
    for captured in report['cases']:
        case=cases[captured['case_name']];rgb=np.asarray(Image.open(tool.FIXTURES/'capture'/case['name']/'input.png').convert('RGB'))
        doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(*case['pixel_size_um']);bounds=case.get('roi',[0,0,256,192])
        def ring(b):
            x,y,w,h=b
            return [q.Point(x,y),q.Point(x+w,y),q.Point(x+w,y+h),q.Point(x,y+h)]
        obj=doc.add_roi(q.Roi.polygon(ring(bounds),[ring(case['hole'])])) if 'hole' in case else doc.add_rectangle(q.Rect(*bounds))
        scale=1 if captured['units']=='pixels' else case['pixel_size_um'][0];p=q.GridCellParameters();n=contract['native']
        p.units=q.GridCellUnits.LEVEL_ZERO_PIXELS if captured['units']=='pixels' else q.GridCellUnits.MICROMETRES
        p.threshold=n['threshold'];p.sigma=n['sigma']*scale;p.cell_expansion=n['cell_expansion']*scale
        p.min_nucleus_area=n['min_nucleus_pixels']*scale**2;p.max_nucleus_area=n['max_nucleus_pixels']*scale**2
        transform=q.StainTransform(contract['vectors'],contract['white']);d=captured['nominal_downsample'];cx,cy=captured['context']
        if captured['grid_scope']=='reference-roi':request=q.CellRequest.for_nominal_roi(doc,obj,transform,d,cx,cy,p)
        else:request=q.CellRequest(doc,obj,transform,q.NominalCellSampling(q.RgbNominalRequest(*captured['source_bounds'],d),cx,cy),p)
        descriptor=request.nominal_sampling;source=descriptor.source_request
        assert [source.x_px,source.y_px,source.width_px,source.height_px]==captured['source_bounds']
        assert [descriptor.core_height,descriptor.core_width]==captured['core_shape']
        requested=request.grid_parameters;resolved=request.resolved_grid_parameters
        parameters={name:getattr(requested,name) for name in ('threshold','sigma','log_prominence','min_peak_radius','min_seed_distance','cell_expansion','min_nucleus_area','max_nucleus_area','max_cells','work_bytes')}
        parameters.update(units=requested.units.name,feature=requested.feature.name,sampling_policy=requested.sampling_policy.name,
            contours={name:getattr(requested.contours,name) for name in ('clip_to_roi','max_components','max_edges','max_vertices','work_bytes')})
        records.append(dict(profile=profile,case_name=case['name'],parameters=parameters,
            resolved={name:getattr(resolved,name) for name in ('step_x','step_y','sigma_x','sigma_y','min_nucleus_samples','max_nucleus_samples')},
            source_bounds=captured['source_bounds'],roi=bounds,hole=case.get('hole'),calibration_um=case['pixel_size_um'],
            downsample=d,context=[cx,cy],core_shape=captured['core_shape'],white=contract['white'],vectors=contract['vectors'],estimated_work_bytes=request.estimated_work_bytes))
target=out/host/'request-recipes.json';assert not target.exists()
target.write_text(json.dumps(dict(native_source=q.build_environment().git_revision,comparison_tool_sha256=report['comparison_tool_sha256'],
    reconstructed_from_immutable_inputs=True,native_detection_rerun=False,requests=records),indent=2)+'\n',encoding='utf-8')
print(host,len(records),'native request recipes recorded without detector re-execution',flush=True)
