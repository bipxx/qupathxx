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
    if mask.level!=0 or mask.downsample!=1:
        raise ValueError('NDPI comparison requires the natural level-zero grid')
    grid=mask.level_bounds
    left,top=max(x,int(grid.x)),max(y,int(grid.y))
    right,bottom=min(x+width,int(grid.x)+mask.width),min(y+height,int(grid.y)+mask.height)
    output=np.zeros((height,width),dtype=np.uint32)
    if right>left and bottom>top:
        output[top-y:bottom-y,left-x:right-x]=mask.labels_numpy()[top-int(grid.y):bottom-int(grid.y),left-int(grid.x):right-int(grid.x)]
    return output


def compare(folder,image):
    import numpy as np
    from PIL import Image
    import qupathxx as q
    from qupathxx import _native
    contract,reference=verify_capture(folder)
    identify_image(image,contract)
    metrics=module('qpx_real_ndpi_metrics',ROOT/'tools/compare_qupath_cells.py')
    roi=contract['roi'];x,y,width,height=roi
    report=dict(schema='qpx.qupath-ndpi-cells-comparison/1',state='characterized-partial',
        build_git_revision=q.build_environment().git_revision,source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=sha(Path(_native.__file__)),tool_sha256=sha(Path(__file__)),
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
        refs={name:np.frombuffer(gzip.decompress((folder/(name+'.u32le.gz')).read_bytes()),dtype='<u4').reshape(height,width)
              for name in ['nucleus','body']}
        doc=q.Document(server);target=doc.add_rectangle(q.Rect(*roi))
        transform=q.StainTransform(contract['vectors'],contract['white'])
        for mode,algorithm in [('log',q.CellAlgorithm.LOG_WATERSHED),('log-bodies',q.CellAlgorithm.LOG_WATERSHED_BODIES)]:
            previous=None;results=[]
            for tile in contract['tile_sizes']:
                p=q.CellParameters()
                for key,value in contract['native'].items():setattr(p,key,value)
                p.algorithm=algorithm;p.tile_size=tile
                request=q.CellRequest(doc,target,transform,p);result=q.detect_cells(request)
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
                results.append(dict(tile_size=tile,cells=result.cell_count,algorithm=result.algorithm,
                    labels_sha256=identity,input_sha256=result.nuclei.input_sha256,
                    estimated_work_bytes=request.estimated_work_bytes,retained_bytes=result.retained_bytes,
                    segmentation={name:metrics.metrics(refs[name],mask,contract['comparison']) for name,mask in masks.items()}))
            report['runs'].append(dict(mode=mode,tile_invariant=True,independent_rgb_features_passed=True,results=results))
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
    args=parser.parse_args()
    if bool(args.capture)==bool(args.reference):parser.error('choose a new capture or an existing reference')
    folder=args.capture or args.reference
    if args.capture:capture(folder,args.image,args.runtime_cache)
    report=compare(folder,args.image)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    print(json.dumps(dict(state=report['state'],source_pixels_equal=report['source_pixels_equal'],report=str(args.report))))
    return 0 if report['source_pixels_equal'] else 1


if __name__=='__main__':
    raise SystemExit(main())
