"""Held-out public QuPath captures for the opt-in nominal numerical policy."""
import argparse
import gzip
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-cell-policy-edges-0.7.0'
PROBE=ROOT/'tools/upstream/qupath_cell_policy_edges.groovy'


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def read(path):return json.loads(Path(path).read_text(encoding='utf-8'))


def contract():
    base=read(ROOT/'tests/fixtures/qupath-cells-0.7.0/contract.json')
    cases=[]
    edge=[[0,40,12,9],[255,145,14,12],[100,0,11,10],[180,191,14,11],[128,96,10,10]]
    crop=[[7,50,12,9],[244,145,10,10],[120,11,12,12],[160,175,14,14],[100,95,9,9]]
    for prefix,nuclei,bounds in [('slide-edge',edge,None),('odd-crop',crop,[7,11,241,173])]:
        for step in (1.,1.25,1.5,2.3):
            case=dict(name=prefix+'-'+str(step),pixel_size_um=[.5,.5],nuclei=nuclei,
                      requested_pixel_size_um=.5*step)
            if bounds:case['roi']=bounds
            cases.append(case)
    cases.append(dict(name='odd-hole-1.5',pixel_size_um=[.5,.5],nuclei=crop,
                      roi=[7,11,241,173],hole=[90,85,20,20],requested_pixel_size_um=.75))
    for sigma in (0.,8.5,8.500001,16.):
        recipe=dict(base['native'],sigma=sigma,max_nucleus_pixels=4096)
        cases.append(dict(name='sigma-'+str(sigma),pixel_size_um=[.5,.5],
                          nuclei=[[54,60,23,20],[170,118,26,22]],native=recipe))
    return dict(base,cases=cases,intent='Held-out synthetic edge/lattice/kernel characterization; no biological or general segmentation acceptance')


def generator():
    comparison=load('edge_base_reference',ROOT/'tools/compare_qupath_cells.py')
    debug=load('edge_resampled_reference',ROOT/'tools/qupath_resampled_cell_debug.py')
    text=debug.generator(comparison)
    needle='    def folder=root.resolve(item.name)'
    if text.count(needle)!=1:raise ValueError('reference folder hook changed')
    text=text.replace(needle,needle+'\n    captureFolder=folder\n    def recipe=item.native ?: contract.native')
    return text.replace('contract.native.','recipe.')


def identity():
    return dict(schema='qpx.qupath-cell-policy-edge-capture/1',
        upstream=read(ROOT/'tests/fixtures/qupath-0.7.0/contract.json')['upstream'],
        contract=contract(),probe_sha256=sha(PROBE),
        reference_sha256=hashlib.sha256(generator().encode()).hexdigest(),
        phantom_generator_sha256=sha(ROOT/'tools/qupath_cells_fixture.py'))


def files():
    common={'contract.json','probe.groovy','reference.groovy','driver.log'}
    payload=('input.png','reference.json','cells.geojson','nucleus.u32le.gz','body.u32le.gz',
             'nucleus-decoded.u32le.gz','body-decoded.u32le.gz','processed-input.png',
             'processed-input.json','processing-stack.tif','processing-stack.json')
    return common|{case['name']+'/'+name for case in contract()['cases'] for name in payload}


def capture(folder,runtime_cache):
    if os.name!='nt':raise ValueError('pinned capture requires Windows')
    folder=Path(folder).resolve()
    load('edge_phantoms',ROOT/'tools/qupath_cells_fixture.py').generate(folder,contract())
    (folder/'reference.groovy').write_bytes(generator().encode())
    shutil.copyfile(PROBE,folder/'probe.groovy')
    runtime=load('edge_runtime',ROOT/'tools/fetch_qupath.py').fetch(runtime_cache)
    args=[str(runtime),'script','--args',str(folder),str(folder/'probe.groovy')]
    with (folder/'driver.log').open('xb') as stream:
        result=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=240,
                              creationflags=subprocess.CREATE_NO_WINDOW)
    result.check_returncode()
    manifest=identity();manifest['files']={name:dict(bytes=(folder/name).stat().st_size,sha256=sha(folder/name)) for name in sorted(files())}
    (folder/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
    return verify(folder)


def data(folder,case):
    import numpy as np
    from PIL import Image
    path=Path(folder)/case['name'];metadata=read(path/'processing-stack.json')
    stages=[]
    with Image.open(path/'processing-stack.tif') as image:
        for index in range(image.n_frames):
            image.seek(index);stages.append(np.asarray(image).copy())
    with Image.open(path/'processed-input.png') as image:rgb=np.asarray(image.convert('RGB')).copy()
    return metadata,stages,rgb


def geometry_labels(features,key,shape,origin,step):
    """Independent even-odd center membership on captured GeoJSON polygons."""
    import numpy as np
    y,x=np.mgrid[:shape[0],:shape[1]]
    x=origin[0]+(x+.5)*step;y=origin[1]+(y+.5)*step
    output=np.zeros(shape,dtype=np.uint32)
    for feature in features:
        geometry=feature[key]
        polygons=[geometry['coordinates']] if geometry['type']=='Polygon' else geometry['coordinates']
        if geometry['type'] not in ('Polygon','MultiPolygon'):raise ValueError('unsupported captured geometry')
        inside=np.zeros(shape,dtype=bool)
        for polygon in polygons:
            selected=np.zeros(shape,dtype=bool)
            for ring in polygon:
                points=np.asarray(ring,dtype=float)
                if len(points)<4 or not np.array_equal(points[0],points[-1]) or not np.all(np.isfinite(points)):
                    raise ValueError('invalid captured polygon')
                for a,b in zip(points,points[1:]):
                    if a[1]==b[1]:continue
                    crossing=((a[1]>y)!=(b[1]>y))&(x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0])
                    selected^=crossing
            inside|=selected
        if np.any(inside&(output!=0)):raise ValueError('captured compartment overlap')
        label=int(feature['properties']['name'].removeprefix('cell_'))
        if label<1:raise ValueError('invalid captured label')
        output[inside]=label
    return output


def verify(folder=FIXTURE):
    import numpy as np
    from PIL import Image
    folder=Path(folder).resolve();manifest=read(folder/'manifest.json')
    if {key:manifest.get(key) for key in identity()}!=identity() or set(manifest.get('files',{}))!=files():
        raise ValueError('edge capture identity differs')
    if {p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}!=files()|{'manifest.json'}:
        raise ValueError('edge capture members differ')
    for name,item in manifest['files'].items():
        path=folder/name
        if path.stat().st_size!=item['bytes'] or sha(path)!=item['sha256']:raise ValueError('edge payload changed: '+name)
    if read(folder/'contract.json')!=contract() or (folder/'reference.groovy').read_bytes()!=generator().encode() or (folder/'probe.groovy').read_bytes()!=PROBE.read_bytes():
        raise ValueError('edge generator/recipe differs')
    phantom=load('edge_verify_pixels',ROOT/'tools/qupath_cells_fixture.py');checks=[]
    for case in contract()['cases']:
        path=folder/case['name'];metadata,stages,rgb=data(folder,case)
        with Image.open(path/'input.png') as image:np.testing.assert_array_equal(np.asarray(image),phantom.pixels(contract(),case))
        if metadata['slices']!=['Input image','Laplacian of Gaussian filtered','Watershed labels','Binary','Labeled ROIs']:
            raise ValueError('edge stage names differ')
        shape=(metadata['height'],metadata['width'])
        if len(stages)!=5 or any(a.shape!=shape or a.dtype!=np.float32 or not np.all(np.isfinite(a)) for a in stages) or rgb.shape!=(*shape,3):
            raise ValueError('edge stage shape/type differs')
        reference=read(path/'reference.json');request=read(path/'processed-input.json')
        if reference['qupath_version']!='0.7.0' or request['width']!=shape[1] or request['height']!=shape[0]:raise ValueError('edge runtime/request differs')
        channels=np.maximum(0,np.log10(255/np.maximum(rgb.astype(float),1))).astype(np.float32)
        score=(channels[:,:,0]+channels[:,:,1])+channels[:,:,2]
        np.testing.assert_array_equal(stages[0].view(np.uint32),score.view(np.uint32))
        if np.any(stages[-1]<0) or np.any(stages[-1]!=np.floor(stages[-1])):raise ValueError('edge labels are not integers')
        features=read(path/'cells.geojson')['features'];bounds=case.get('roi',[0,0,256,192])
        masks={name:geometry_labels(features,key,shape,bounds[:2],request['downsample']) for name,key in [('nucleus','nucleusGeometry'),('body','geometry')]}
        # Geometry and debug IDs may differ in ordering; compare through shared assignment.
        comparison=load('edge_metrics',ROOT/'tools/compare_qupath_cells.py')
        metric=comparison.metrics(stages[-1].astype(np.uint32),masks['nucleus'],contract()['comparison'])
        if metric['foreground_iou']!=1 or any(m['iou']!=1 for m in metric['matches']):raise ValueError('debug and exported processing nuclei differ')
        checks.append(dict(name=case['name'],shape=list(shape),cells=reference['count'],input_exact=True,exported_debug_nuclei_exact=True))
    return dict(schema='qpx.qupath-cell-policy-edge-verification/1',cases=checks,biological_validation=False)


def compare(folder=FIXTURE):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    import qupathxx._native as native
    verified=verify(folder);comparison=load('edge_native_metrics',ROOT/'tools/compare_qupath_cells.py')
    nominal=load('edge_native_features',ROOT/'tools/compare_qupath_nominal_cells.py');reports=[]
    for case in contract()['cases']:
        path=Path(folder)/case['name'];metadata,stages,processing=data(folder,case)
        with Image.open(path/'input.png') as image:source=np.asarray(image.convert('RGB')).copy()
        doc=q.Document(q.Image.from_numpy(source));doc.set_calibration(*case['pixel_size_um'])
        bounds=case.get('roi',[0,0,256,192]);x,y,w,h=bounds
        def ring(b):
            a,b_,c,d=b;return [q.Point(a,b_),q.Point(a+c,b_),q.Point(a+c,b_+d),q.Point(a,b_+d)]
        obj=doc.add_roi(q.Roi.polygon(ring(bounds),[ring(case['hole'])])) if 'hole' in case else doc.add_rectangle(q.Rect(*bounds))
        recipe=case.get('native',contract()['native']);p=q.GridCellParameters();p.processing=q.CellProcessingPolicy.IMAGEJ_154P
        p.threshold=recipe['threshold'];p.sigma=recipe['sigma'];p.cell_expansion=recipe['cell_expansion']
        p.min_nucleus_area=recipe['min_nucleus_pixels'];p.max_nucleus_area=recipe['max_nucleus_pixels']
        transform=q.StainTransform(contract()['vectors'],contract()['white'])
        step=read(path/'processed-input.json')['downsample'];request=q.CellRequest.for_nominal_roi(doc,obj,transform,step,0,0,p)
        rgb=q.read_rgb_nominal_region(doc.server,request.nominal_sampling.source_request).numpy()
        np.testing.assert_array_equal(rgb[:,:,:3],processing);assert np.all(rgb[:,:,3]==255)
        result=q.detect_cells(request);features=read(path/'cells.geojson')['features'];shape=processing.shape[:2]
        reference={name:geometry_labels(features,key,shape,(x,y),step) for name,key in [('nucleus','nucleusGeometry'),('body','geometry')]}
        masks={'nucleus':result.nuclei.labels_numpy(),'body':result.cells.labels_numpy()}
        metrics={name:comparison.metrics(reference[name],mask,contract()['comparison']) for name,mask in masks.items()}
        checks=nominal.feature_checks(result,masks,processing,contract()['white'],case['pixel_size_um'],step,contract()['comparison'])
        reports.append(dict(name=case['name'],input_rgb_exact=True,shape=list(shape),sigma=recipe['sigma'],downsample=step,
                            metrics=metrics,features=checks,input_sha256=result.nuclei.input_sha256))
    return dict(schema='qpx.qupath-cell-policy-edge-comparison/1',state='characterized-partial',
        native_source=q.build_environment().git_revision,native_sha256=sha(native.__file__),
        reference_manifest_sha256=sha(Path(folder)/'manifest.json'),cases=reports,
        biological_validation=False,general_speed_claim=False,segmentation_parity_accepted=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--capture',type=Path)
    parser.add_argument('--reference',type=Path,default=FIXTURE);parser.add_argument('--native',action='store_true')
    parser.add_argument('--runtime-cache',type=Path,default=ROOT/'build/upstream/qupath');parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args();report=capture(args.capture,args.runtime_cache) if args.capture else verify(args.reference)
    if args.native:report=compare(args.capture or args.reference)
    args.report.parent.mkdir(parents=True,exist_ok=True);args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    print('Verified',len(report['cases']),'held-out edge/lattice/kernel cases')


if __name__=='__main__':main()
