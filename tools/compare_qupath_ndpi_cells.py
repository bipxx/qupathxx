"""Capture pinned real-slide QuPath output and characterize native cell recipes."""
import argparse
from datetime import datetime, timezone
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
CONTRACT=ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0/contract.json'
SCRIPT=ROOT/'tools/upstream/qupath_ndpi_cells.groovy'
FILES=['contract.json','input.png','cells.geojson','reference.json','nucleus.u32le.gz','body.u32le.gz','qupath.log']


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


def identify_image(path,contract):
    fixture=contract['fixture']
    if path.stat().st_size!=fixture['bytes'] or sha(path)!=fixture['sha256']:
        raise ValueError('NDPI source does not match the pinned fixture')


def capture(folder,image,cache):
    contract=read(CONTRACT)
    identify_image(image,contract)
    folder.mkdir(parents=True,exist_ok=False)
    shutil.copyfile(CONTRACT,folder/'contract.json')
    runtime=module('qpx_ndpi_runtime',ROOT/'tools/fetch_qupath.py').fetch(cache)
    result=subprocess.run([str(runtime),'script','--args',str(folder.resolve()),'--args',str(image.resolve()),str(SCRIPT)],
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
    (folder/'qupath.log').write_bytes(result.stdout)
    result.check_returncode()
    upstream=read((CONTRACT.parent/contract['upstream_contract']).resolve())['upstream']
    manifest=dict(schema='qpx.qupath-ndpi-capture/1',captured_utc=datetime.now(timezone.utc).isoformat(),
        upstream=upstream,contract_sha256=sha(CONTRACT),generator_sha256=sha(SCRIPT),fixture=contract['fixture'],
        files={name:dict(bytes=(folder/name).stat().st_size,sha256=sha(folder/name)) for name in FILES})
    (folder/'capture-manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())


def verify_capture(folder):
    contract=read(CONTRACT)
    manifest=read(folder/'capture-manifest.json')
    upstream=read((CONTRACT.parent/contract['upstream_contract']).resolve())['upstream']
    if (manifest['schema']!='qpx.qupath-ndpi-capture/1' or manifest['upstream']!=upstream or
            manifest['contract_sha256']!=sha(CONTRACT) or manifest['generator_sha256']!=sha(SCRIPT) or
            manifest['fixture']!=contract['fixture'] or set(manifest['files'])!=set(FILES)):
        raise ValueError('NDPI capture identity differs')
    for name in FILES:
        path=folder/name;identity=manifest['files'][name]
        if path.stat().st_size!=identity['bytes'] or sha(path)!=identity['sha256']:
            raise ValueError('NDPI capture changed: '+name)
    if read(folder/'contract.json')!=contract:
        raise ValueError('NDPI captured contract differs')
    reference=read(folder/'reference.json')
    if (reference['schema']!='qpx.qupath-ndpi-cells-reference/1' or
            reference['qupath_version']!=upstream['version'] or reference['roi']!=contract['roi'] or
            [reference['source_width'],reference['source_height']]!=contract['fixture']['dimensions'] or
            'openslide' not in reference['server_class'].lower() or
            any(abs(a-b)>1e-12 for a,b in zip(reference['pixel_size_um'],contract['pixel_size_um'],strict=True))):
        raise ValueError('NDPI reference metadata differs from the contract')
    return contract,reference


def local_labels(mask,roi):
    import numpy as np
    x,y,width,height=roi
    nominal=getattr(mask,'nominal_sampling',None)
    if nominal is not None:
        request=nominal.source_request
        if request.downsample!=1:
            raise ValueError('NDPI reference canvas requires unit nominal sampling')
        gx,gy=request.x_px-nominal.context_x,request.y_px-nominal.context_y
    else:
        if mask.level!=0 or mask.downsample!=1:
            raise ValueError('NDPI comparison requires the natural level-zero grid')
        grid=mask.level_bounds;gx,gy=int(grid.x),int(grid.y)
    left,top=max(x,gx),max(y,gy)
    right,bottom=min(x+width,gx+mask.width),min(y+height,gy+mask.height)
    output=np.zeros((height,width),dtype=np.uint32)
    if right>left and bottom>top:
        output[top-y:bottom-y,left-x:right-x]=mask.labels_numpy()[top-gy:bottom-gy,left-gx:right-gx]
    return output


def reference_labels(folder,contract,reference):
    """Decode only the contracted canvas and check its independent count ledger."""
    import numpy as np
    width,height=contract['roi'][2:]
    expected=width*height*4
    output={}
    if reference['count']<=0 or reference['count']>width*height:
        raise ValueError('invalid NDPI reference cell count')
    for name in ['nucleus','body']:
        with gzip.open(folder/(name+'.u32le.gz'),'rb') as stream:
            raw=stream.read(expected+1)
        if len(raw)!=expected:
            raise ValueError('NDPI reference label canvas has incorrect length')
        labels=np.frombuffer(raw,dtype='<u4').reshape(height,width)
        if int(labels.max())!=reference['count']:
            raise ValueError('NDPI reference label count differs')
        counts=np.bincount(labels.ravel(),minlength=reference['count']+1)[1:]
        if (np.any(counts==0) or reference['collisions'][name]!=0 or
                counts.tolist()!=reference['sampled_pixels'][name]):
            raise ValueError('NDPI reference label ledger differs')
        output[name]=labels
    return output


def compare(folder,image,include_sampled=False,context_padding=None,include_nominal=False,nominal_revision=1):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    from qupathxx import _native
    if nominal_revision not in (1,2) or (nominal_revision!=1 and not include_nominal):
        raise ValueError('nominal revision requires an enabled nominal recipe and revision1 or2')
    contract,reference=verify_capture(folder)
    identify_image(image,contract)
    metrics=module('qpx_real_ndpi_metrics',ROOT/'tools/label_comparison.py')
    roi=contract['roi'];x,y,width,height=roi
    report=dict(schema='qpx.qupath-ndpi-cells-comparison/1',state='characterized-partial',
        build_git_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=sha(Path(_native.__file__)),tool_sha256=sha(Path(__file__)),
        metrics_tool_sha256=sha(ROOT/'tools/label_comparison.py'),
        capture_manifest_sha256=sha(folder/'capture-manifest.json'),contract_sha256=sha(CONTRACT),
        fixture_sha256=contract['fixture']['sha256'],source_pixels_equal=False,
        segmentation_parity_accepted=False,biological_validation=False,performance_accepted=False,runs=[])
    server=q.open_slide(str(image))
    try:
        pixels=server.read_region(q.RegionRequest(x,y,0,width,height)).numpy()
        expected=np.asarray(Image.open(folder/'input.png').convert('RGB'))
        report['source_pixels_equal']=bool(np.array_equal(pixels[...,:3],expected) and np.all(pixels[...,3]==255))
        report['source_rgb_sha256']=hashlib.sha256(pixels[...,:3].copy().tobytes()).hexdigest()
        report['qupath_rgb_sha256']=hashlib.sha256(expected.tobytes()).hexdigest()
        report['source_pixel_disagreements']=int(np.count_nonzero(pixels[...,:3]!=expected))
        if not report['source_pixels_equal']:
            report['state']='input-pixels-differ'
            return report
        refs=reference_labels(folder,contract,reference)
        doc=q.Document(server);target=doc.add_rectangle(q.Rect(*roi))
        transform=q.StainTransform(contract['vectors'],contract['white'])
        modes=[('log',q.CellAlgorithm.LOG_WATERSHED),('log-bodies',q.CellAlgorithm.LOG_WATERSHED_BODIES)]
        if include_sampled:
            modes.append(('log-sampled',q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES))
        if include_nominal:modes.append(('nominal-qupath070',None))
        for mode,algorithm in modes:
            previous=None;results=[]
            # Nominal grids have no tile-size control. Run that recipe once;
            # repeated identical requests would not establish tile invariance.
            for tile in ([None] if mode=='nominal-qupath070' else contract['tile_sizes']):
                padding=contract['native']['context_padding'] if context_padding is None else context_padding
                if mode=='nominal-qupath070':
                    p=q.GridCellParameters();native=contract['native']
                    p.processing=q.CellProcessingPolicy.QUPATH_070;p.units=q.GridCellUnits.LEVEL_ZERO_PIXELS
                    if nominal_revision==2:p.processing=q.CellProcessingPolicy.QUPATH_070_V2
                    p.threshold=native['threshold'];p.sigma=native['sigma'];p.cell_expansion=native['cell_expansion']
                    p.min_nucleus_area=native['min_nucleus_pixels'];p.max_nucleus_area=native['max_nucleus_pixels']
                    request=q.CellRequest.for_nominal_roi(doc,target,transform,1,padding,padding,p,q.RgbNominalSampling.AFFINE_INVERSE)
                else:
                    p=q.CellParameters()
                    for key,value in contract['native'].items():setattr(p,key,value)
                    p.algorithm=algorithm;p.tile_size=tile
                    if mode=='log-sampled':p.min_peak_radius=p.min_seed_distance=0
                    if context_padding is not None:p.context_padding=context_padding
                    request=q.CellRequest(doc,target,transform,p)
                result=q.detect_cells(request)
                masks={name:local_labels(getattr(result,attribute),roi) for name,attribute in [('nucleus','nuclei'),('body','cells')]}
                identity={name:hashlib.sha256(mask.astype('<u4',copy=False).tobytes()).hexdigest() for name,mask in masks.items()}
                if previous is not None and previous!=identity:
                    raise RuntimeError('real-slide native labels are not tile invariant')
                previous=identity
                # Source-pixel arithmetic is independent of native compartment aggregation.
                for feature in result.features:
                    for name,compartment in [('nucleus','nucleus'),('body','cell')]:
                        selected=pixels[...,:3][masks[name]==feature.label]
                        if len(selected)!=getattr(feature,compartment+'_pixels'):
                            raise RuntimeError('real-slide compartment count differs from its label map')
                        np.testing.assert_allclose(selected.mean(axis=0),getattr(feature,compartment+'_rgb'),rtol=0,atol=1e-10)
                results.append(dict(tile_size=tile,cells=result.cell_count,raw_peaks=result.raw_peaks,algorithm=result.algorithm,
                    labels_sha256=identity,input_sha256=result.nuclei.input_sha256,
                    estimated_work_bytes=request.estimated_work_bytes,retained_bytes=result.retained_bytes,
                    segmentation={name:metrics.metrics(refs[name],mask,contract['comparison']) for name,mask in masks.items()}))
                if mode=='nominal-qupath070':
                    results[-1]['requested_grid_recipe']=dict(sampling_algorithm=request.nominal_sampling.source_request.algorithm,
                        processing='qupath070/'+str(nominal_revision),units='level_zero_pixels',downsample=1,context_x=padding,context_y=padding,
                        threshold=p.threshold,sigma=p.sigma,cell_expansion=p.cell_expansion,min_nucleus_area=p.min_nucleus_area,
                        max_nucleus_area=p.max_nucleus_area,min_peak_radius=p.min_peak_radius,min_seed_distance=p.min_seed_distance)
            report['runs'].append(dict(mode=mode,context_padding=padding,tile_invariant=None if mode=='nominal-qupath070' else True,independent_rgb_features_passed=True,results=results))
        report['reference_count']=reference['count']
        report['reference_collisions']=reference['collisions']
    finally:
        server.close()
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--capture',type=Path)
    parser.add_argument('--reference',type=Path)
    parser.add_argument('--runtime-cache',type=Path,default=ROOT/'build/upstream/qupath')
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--include-sampled',action='store_true',help='Also characterize the explicit production sampled recipe')
    parser.add_argument('--include-nominal',action='store_true',help='Also characterize original-slide affine sampling and QuPath070 processing at unit downsample')
    parser.add_argument('--nominal-revision',type=int,choices=[1,2],default=1,help='Explicit nominal recipe revision; requires --include-nominal for revision2')
    parser.add_argument('--context-padding',type=int,help='Explicit contextual pixels for every compared recipe')
    args=parser.parse_args()
    if args.context_padding is not None and not 0<=args.context_padding<=4096:parser.error('context padding must be in 0..4096')
    if args.nominal_revision!=1 and not args.include_nominal:parser.error('nominal revision2 requires --include-nominal')
    if bool(args.capture)==bool(args.reference):parser.error('choose a new capture or an existing reference')
    folder=args.capture or args.reference
    if args.capture:capture(folder,args.image,args.runtime_cache)
    report=compare(folder,args.image,args.include_sampled,args.context_padding,args.include_nominal,args.nominal_revision)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    print(json.dumps(dict(state=report['state'],source_pixels_equal=report['source_pixels_equal'],report=str(args.report))))
    return 0 if report['source_pixels_equal'] else 1


if __name__=='__main__':
    raise SystemExit(main())
