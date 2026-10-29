"""Characterize original-source nominal cells against the immutable nine-case capture.

Keep level-zero segmentation and processing-grid feature domains distinct.
This synthetic comparison does not establish biological or general parity.
"""
import argparse
from fractions import Fraction
from functools import lru_cache
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
FIXTURES=ROOT/'tests/fixtures/qupath-cells-0.7.0'


@lru_cache(maxsize=1)
def shared():
    path=ROOT/'tools/compare_qupath_cells.py'
    spec=importlib.util.spec_from_file_location('nominal_cells_shared',path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def nominal_core(raster,width,height,bounds,downsample):
    """Admit the declared original-source lattice before extracting any labels."""
    import numpy as np
    if (len(bounds)!=4 or any(type(v) is not int for v in (*bounds,width,height))
            or min(width,height)<=0 or min(bounds[:2])<0 or min(bounds[2:])<=0
            or bounds[0]+bounds[2]>width or bounds[1]+bounds[3]>height):
        raise ValueError('invalid comparison source bounds')
    if downsample not in (1,1.5):raise ValueError('comparison supports captured nominal steps1 and1.5 only')
    n=raster.nominal_sampling
    if n is None:raise ValueError('comparison requires a nominal sampling descriptor')
    r=n.source_request
    if (r.x_px,r.y_px,r.width_px,r.height_px,r.downsample)!=(*bounds,downsample):
        raise ValueError('nominal descriptor differs from declared source request')
    step=Fraction(downsample)
    expected=tuple((2*v*step.denominator+step.numerator)//(2*step.numerator) for v in bounds[2:])
    if (n.core_width,n.core_height)!=expected or min(expected)<=0:
        raise ValueError('nominal core counts differ from requested spacing')
    cx,cy=n.context_x,n.context_y
    if any(type(v) is not int or v<0 or v>4096 for v in (cx,cy)):
        raise ValueError('invalid nominal context')
    labels=raster.labels_numpy()
    cw,ch=expected
    if labels.shape!=(ch+2*cy,cw+2*cx) or labels.dtype.kind!='u' or labels.dtype.itemsize!=4:
        raise ValueError('nominal raster shape or label type differs')
    outside=labels.copy();outside[cy:cy+ch,cx:cx+cw]=0
    if np.any(outside):raise ValueError('cell labels escaped the nominal core')
    return labels[cy:cy+ch,cx:cx+cw].copy()


def nominal_canvas(raster,width,height,bounds,downsample):
    """Exact inverse mapping of level-zero pixel centers, without edge clipping."""
    import numpy as np
    core=nominal_core(raster,width,height,bounds,downsample);x,y,w,h=bounds
    step=Fraction(downsample)
    ix=((2*np.arange(w)+1)*step.denominator)//(2*step.numerator)
    iy=((2*np.arange(h)+1)*step.denominator)//(2*step.numerator)
    if np.any(ix>=core.shape[1]) or np.any(iy>=core.shape[0]):
        raise ValueError('nominal core does not cover declared level-zero centers')
    output=np.zeros((height,width),dtype=np.uint32)
    output[y:y+h,x:x+w]=core[iy[:,None],ix[None,:]]
    return output


def captured_processing_rgb(source,case,bounds,downsample):
    """Use original pixels at step1 or independently captured QuPath RGB at1.5."""
    import numpy as np
    x,y,w,h=bounds
    if downsample==1:return source[y:y+h,x:x+w].copy(),None
    if case['name']!='resampled_reference' or bounds!=[0,0,256,192] or downsample!=1.5:
        raise ValueError('no independent processed RGB capture for this request')
    debug=shared().load_tool('nominal_processing_capture',ROOT/'tools/qupath_resampled_cell_debug.py')
    _,_,rgb,original,labels,_=debug.verified_data(debug.FIXTURE)
    np.testing.assert_array_equal(source,original)
    return rgb,labels


def feature_checks(result,masks,rgb,white,calibration,downsample,tolerance):
    import numpy as np
    od=np.maximum(0,np.log10(np.asarray(white)/np.maximum(rgb.astype(float),1)))
    features={f.label:f for f in result.features}
    for mask in masks.values():
        if set(np.unique(mask[mask>0]))!=set(features):raise ValueError('feature and processing label identities differ')
    checks=[]
    for label,f in features.items():
        calibrated=result.calibrated_feature(label)
        for name,prefix in [('nucleus','nucleus'),('body','cell')]:
            selected=masks[name]==label;count=int(selected.sum())
            accepted=int(getattr(f,prefix+'_pixels'))==count
            accepted=accepted and np.allclose(getattr(f,prefix+'_rgb'),rgb[selected].mean(axis=0),atol=tolerance['feature_absolute'],rtol=tolerance['feature_relative'])
            accepted=accepted and np.allclose(getattr(f,prefix+'_channels'),od[selected].mean(axis=0),atol=tolerance['feature_absolute'],rtol=tolerance['feature_relative'])
            area=count*downsample**2*calibration[0]*calibration[1]
            accepted=accepted and np.isclose(getattr(calibrated,prefix+'_sample_area_um2'),area,atol=tolerance['feature_absolute'],rtol=tolerance['feature_relative'])
            if not accepted:raise ValueError('nominal feature differs from independent processing-pixel arithmetic')
            checks.append(dict(label=label,compartment=name,samples=count,processing_arithmetic_accepted=True))
    return checks


def compare_case(folder,contract,case,context=32,units='pixels',grid_scope='reference-roi',processing='quantitative',sampling='integer-origin'):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    if units not in ('pixels','micrometres'):raise ValueError('unknown comparison units')
    if sampling not in ('integer-origin','affine-inverse'):raise ValueError('unknown sampling policy')
    if grid_scope not in ('full-image','reference-roi'):raise ValueError('unknown comparison grid scope')
    px,py=case['pixel_size_um']
    if units=='micrometres' and px!=py:raise ValueError('equivalent physical recipe requires isotropic calibration')
    comparison=shared();path=Path(folder)/case['name'];width,height=contract['width'],contract['height']
    reference=comparison.read(path/'reference.json')
    if reference['qupath_version']!='0.7.0' or (reference['width'],reference['height'])!=(width,height):raise ValueError('reference runtime/dimensions differ')
    rgb=np.asarray(Image.open(path/'input.png').convert('RGB'))
    phantom=comparison.load_tool('nominal_phantom',comparison.PHANTOMS)
    np.testing.assert_array_equal(rgb,phantom.pixels(contract,case))
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(px,py)
    roi=case.get('roi',[0,0,width,height])
    def ring(b):
        x,y,w,h=b
        return [q.Point(x,y),q.Point(x+w,y),q.Point(x+w,y+h),q.Point(x,y+h)]
    obj=doc.add_roi(q.Roi.polygon(ring(roi),[ring(case['hole'])])) if 'hole' in case else doc.add_rectangle(q.Rect(*roi))
    transform=q.StainTransform(contract['vectors'],contract['white'])
    downsample=reference['parameters']['requestedPixelSizeMicrons']/((px+py)/2)
    native=contract['native'];p=q.GridCellParameters();scale=1 if units=='pixels' else px
    if processing=='imagej154p':p.processing=q.CellProcessingPolicy.IMAGEJ_154P
    elif processing=='qupath070':p.processing=q.CellProcessingPolicy.QUPATH_070
    elif processing!='quantitative':raise ValueError('unknown cell processing policy')
    p.units=q.GridCellUnits.LEVEL_ZERO_PIXELS if units=='pixels' else q.GridCellUnits.MICROMETRES
    p.threshold=native['threshold'];p.sigma=native['sigma']*scale;p.cell_expansion=native['cell_expansion']*scale
    p.min_nucleus_area=native['min_nucleus_pixels']*scale**2;p.max_nucleus_area=native['max_nucleus_pixels']*scale**2
    bounds=roi if grid_scope=='reference-roi' else [0,0,width,height]
    coordinates=q.RgbNominalSampling.AFFINE_INVERSE if sampling=='affine-inverse' else q.RgbNominalSampling.INTEGER_ORIGIN
    if grid_scope=='reference-roi':request=q.CellRequest.for_nominal_roi(doc,obj,transform,downsample,context,context,p,coordinates)
    else:request=q.CellRequest(doc,obj,transform,q.NominalCellSampling(q.RgbNominalRequest(*bounds,downsample,coordinates),context,context),p)
    processing_rgb,processing_labels=captured_processing_rgb(rgb,case,bounds,downsample)
    native_rgb=q.read_rgb_nominal_region(doc.server,request.nominal_sampling.source_request).numpy()
    np.testing.assert_array_equal(native_rgb[:,:,:3],processing_rgb);assert np.all(native_rgb[:,:,3]==255)
    result=q.detect_cells(request)
    expected_algorithm={'quantitative':'qpx.nominal-rgb-log-watershed-bodies/1','imagej154p':'qpx.nominal-rgb-imagej154p-log-watershed-bodies/1','qupath070':'qpx.nominal-rgb-qupath070-cell-bodies/1'}[processing]
    if sampling=='affine-inverse':expected_algorithm=expected_algorithm.replace('nominal-rgb','nominal-affine-rgb')
    if result.algorithm!=expected_algorithm:raise ValueError('unexpected cell recipe')
    masks={name:nominal_core(getattr(result,attr),width,height,bounds,downsample) for name,attr in [('nucleus','nuclei'),('body','cells')]}
    canvases={name:nominal_canvas(getattr(result,attr),width,height,bounds,downsample) for name,attr in [('nucleus','nuclei'),('body','cells')]}
    expected={name:np.frombuffer(gzip.decompress((path/(name+'.u32le.gz')).read_bytes()),dtype='<u4').reshape(height,width) for name in masks}
    metrics={name:comparison.metrics(expected[name],canvas,contract['comparison']) for name,canvas in canvases.items()}
    checks=feature_checks(result,masks,processing_rgb,contract['white'],(px,py),downsample,contract['comparison'])
    foreground_differences={}
    for name,actual in canvases.items():
        extra=(actual>0)&(expected[name]==0);missing=(expected[name]>0)&(actual==0)
        foreground_differences[name]={}
        for side,mask in [('reference_only',missing),('native_only',extra)]:
            coordinates=np.argwhere(mask)
            foreground_differences[name][side]=dict(count=len(coordinates),first_xy=[[int(x),int(y)] for y,x in coordinates[:32]],truncated=len(coordinates)>32)
    processing_metrics=None if processing_labels is None else {name:comparison.metrics(processing_labels[name],mask,contract['comparison']) for name,mask in masks.items()}
    regions={(r['compartment'],r['label']):r for r in reference['regions']};features={f.label:f for f in result.features};differences=[]
    for name,prefix in [('nucleus','nucleus'),('body','cell')]:
        for match in metrics[name]['matches']:
            f=features[match['native_label']];region=regions[name,match['reference_label']]
            a=np.asarray(getattr(f,prefix+'_rgb'));b=np.asarray(getattr(f,prefix+'_channels'));tol=contract['comparison']
            differences.append(dict(compartment=name,reference_label=match['reference_label'],native_label=match['native_label'],
                rgb_max_absolute_error=float(np.max(np.abs(a-region['rgb_mean']))),od_max_absolute_error=float(np.max(np.abs(b-region['od_mean']))),
                within_frozen_tolerance=bool(np.allclose(a,region['rgb_mean'],atol=tol['feature_absolute'],rtol=tol['feature_relative']) and np.allclose(b,region['od_mean'],atol=tol['feature_absolute'],rtol=tol['feature_relative'])),
                same_sampling_domain=downsample==1))
    return dict(case_name=case['name'],algorithm=result.algorithm,units=units,grid_scope=grid_scope,original_source_request=True,
        source_bounds=bounds,nominal_downsample=downsample,core_shape=list(next(iter(masks.values())).shape),context=[context,context],
        independent_processing_rgb_exact=True,feature_checks=checks,level_zero_segmentation=metrics,processing_segmentation=processing_metrics,
        foreground_pixel_differences=foreground_differences,
        reference_feature_domain='level-zero pixel centers',native_feature_domain='nominal processing pixel centers',matched_reference_feature_differences=differences,
        processing_label_sha256={k:hashlib.sha256(v.astype('<u4').tobytes()).hexdigest() for k,v in masks.items()},
        level_zero_label_sha256={k:hashlib.sha256(v.astype('<u4').tobytes()).hexdigest() for k,v in canvases.items()},input_sha256=result.nuclei.input_sha256)


def compare(folder=FIXTURES/'capture',context=32,units='pixels',grid_scope='reference-roi',processing='quantitative',sampling='integer-origin'):
    import qupathxx as q
    import qupathxx._native as native
    comparison=shared();contract=comparison.verify_capture(folder)
    if units not in ('pixels','micrometres'):raise ValueError('unknown comparison units')
    if sampling not in ('integer-origin','affine-inverse'):raise ValueError('unknown sampling policy')
    selected=[case for case in contract['cases'] if units=='pixels' or case['pixel_size_um'][0]==case['pixel_size_um'][1]]
    return dict(schema='qpx.qupath-nominal-cells-comparison/1',state='characterized-partial',
        build_git_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256,native_sha256=comparison.sha(native.__file__),
        comparison_tool_sha256=comparison.sha(__file__),shared_comparison_tool_sha256=comparison.sha(comparison.__file__),
        capture_manifest_sha256=comparison.sha(Path(folder)/'capture-manifest.json'),units=units,grid_scope=grid_scope,context=context,
        frozen_comparison_tolerances=contract['comparison'],omitted_anisotropic_physical_cases=[c['name'] for c in contract['cases'] if c not in selected],
        segmentation_parity_accepted=False,biological_validation=False,general_speed_claim=False,
        cases=[compare_case(folder,contract,case,context,units,grid_scope,processing,sampling) for case in selected])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,default=FIXTURES/'capture')
    parser.add_argument('--context',type=int,default=32)
    parser.add_argument('--units',choices=['pixels','micrometres'],default='pixels')
    parser.add_argument('--grid-scope',choices=['full-image','reference-roi'],default='reference-roi')
    parser.add_argument('--processing',choices=['quantitative','imagej154p','qupath070'],default='quantitative')
    parser.add_argument('--sampling',choices=['integer-origin','affine-inverse'],default='integer-origin')
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args();report=compare(args.reference,args.context,args.units,args.grid_scope,args.processing,args.sampling)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print(json.dumps(dict(state=report['state'],cases=len(report['cases']),report=str(args.report))))


if __name__=='__main__':main()
