"""Isolate the native response-basin stage on pinned real-slide feature pixels."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess

ROOT=Path(__file__).resolve().parents[1]


def load(name,filename):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/filename)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def main():
    import numpy as np
    import qupathxx as q
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--output-directory',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--imagej-runtime-cache',type=Path,help='Also isolate shape splitting with verified pinned ImageJ APIs')
    parser.add_argument('--context-padding',type=int,choices=range(257),help='Explicit diagnostic override; default uses the frozen contract')
    parser.add_argument('--imagej-dual-background',action='store_true',help='Use the dual-background candidate mask for the ImageJ shape probe')
    args=parser.parse_args()
    comparison=load('qpx_native_ndpi_capture','compare_qupath_ndpi_cells.py')
    metrics=load('qpx_native_ndpi_labels','label_comparison.py')
    reference=ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0/capture'
    contract,upstream=comparison.verify_capture(reference)
    comparison.identify_image(args.image,contract)
    labels=comparison.reference_labels(reference,contract,upstream)['nucleus']
    environment=json.loads(subprocess.check_output([str(args.executable),'--environment']))
    if '+dirty' in environment['git_revision']:raise ValueError('native candidate must have committed source')
    args.output_directory.mkdir(parents=True,exist_ok=False)
    x,y,width,height=contract['roi'];padding=contract['native']['context_padding']
    if args.context_padding is not None:padding=args.context_padding
    rw,rh=width+2*padding,height+2*padding
    server=q.open_slide(str(args.image))
    try:
        pixels=server.read_region(q.RegionRequest(x-padding,y-padding,0,rw,rh))
        transform=q.StainTransform(contract['vectors'],contract['white'])
        values=transform.optical_densities(pixels)
        channels=np.frombuffer(values.data,dtype=np.float32).reshape(rh,rw,3)
        score=channels.astype(np.float64).sum(axis=2).astype('<f4')
        domain=np.zeros((rh,rw),dtype=np.uint8);domain[padding:padding+height,padding:padding+width]=1
        input=args.output_directory/'real-context.qlp'
        input.write_bytes(b'QLP1'+struct.pack('<II',rw,rh)+score.tobytes()+domain.tobytes())
        # Validate pixel parity independently of both candidate and native LoG.
        from PIL import Image
        expected=np.asarray(Image.open(reference/'input.png').convert('RGB'))
        actual=pixels.numpy()[padding:padding+height,padding:padding+width]
        if not np.array_equal(actual[...,:3],expected) or not np.all(actual[...,3]==255):
            raise ValueError('real-slide input pixels differ from the capture')
        doc=q.Document(server);target=doc.add_rectangle(q.Rect(*contract['roi']))
        p=q.CellParameters()
        for key,value in contract['native'].items():setattr(p,key,value)
        p.algorithm=q.CellAlgorithm.LOG_WATERSHED_BODIES;p.tile_size=64;p.context_padding=padding
        control=q.detect_cells(q.CellRequest(doc,target,transform,p))
        production=comparison.local_labels(control.nuclei,contract['roi'])
        results=[]
        native_masks={}
        for mode,options in [('legacy',[]),('response_basins',['--response-basins']),('dual_background',['--dual-background']),('sampled_shape',['--sampled-shape'])]:
            output=args.output_directory/(mode+'.qlr')
            subprocess.run([str(args.executable)]+options+[str(input),str(output)],check=True)
            raw=output.read_bytes()
            if raw[:4]!=b'QLR1' or struct.unpack_from('<II',raw,4)!=(rw,rh):raise ValueError('incorrect native label protocol')
            native=np.frombuffer(raw,dtype='<u4',offset=16).reshape(rh,rw)[padding:padding+height,padding:padding+width]
            # Match the production contract for ROI fragments after contextual filtering.
            counts=np.bincount(native.ravel());keep=(counts>=p.min_nucleus_pixels)&(counts<=p.max_nucleus_pixels);keep[0]=False
            dense=np.cumsum(keep,dtype=np.uint32)*keep;native=dense[native]
            if mode=='legacy':np.testing.assert_array_equal(native,production)
            native_masks[mode]=native
            record=dict(mode=mode,response_basins=mode=='response_basins',eight_connected_background=mode in ['dual_background','sampled_shape'],native_count=int(native.max()),
                labels_sha256=hashlib.sha256(native.astype('<u4',copy=False).tobytes()).hexdigest(),
                output_sha256=comparison.sha(output),metrics=metrics.metrics(labels,native,contract['comparison']))
            if mode=='sampled_shape':record['shape_parameters']=dict(prominence=.5,minimum_radius=0,minimum_spacing=0,edges_are_background=False)
            results.append(record)
            print(json.dumps(dict(mode=mode,native_count=record['native_count'],foreground_iou=record['metrics']['foreground_iou'])),flush=True)
        report=dict(schema='qpx.native-ndpi-log-comparison/1',native_environment=environment,
            native_executable_sha256=comparison.sha(args.executable),feature_producer_git_revision=q.build_environment().git_revision,
            feature_producer_source_tree_sha256=q.build_environment().source_tree_sha256,
            tool_sha256=comparison.sha(Path(__file__)),input_sha256=comparison.sha(input),
            capture_manifest_sha256=comparison.sha(reference/'capture-manifest.json'),source_pixels_equal=True,
            legacy_kernel_matches_production=True,production_integrated=False,
            scientific_accepted=False,performance_accepted=False,context_padding=padding,results=results)
        if args.imagej_runtime_cache:
            runtime=load('qpx_native_ndpi_runtime','fetch_qupath.py').fetch(args.imagej_runtime_cache)
            stage=ROOT/'tools/upstream/imagej_edm_probe.groovy'
            region_maps=[]
            for option in ['--regions','--response-regions','--dual-regions']:
                path=args.output_directory/(option[2:]+'.qlr')
                subprocess.run([str(args.executable),option,str(input),str(path)],check=True)
                region_maps.append(path)
            report['response_basin_region_mask_equal']=region_maps[0].read_bytes()==region_maps[1].read_bytes()
            selected=region_maps[2 if args.imagej_dual_background else 0]
            output=args.output_directory/'imagej-edm.qlr'
            capture=subprocess.run([str(runtime),'script','--args',str(selected.resolve()),
                '--args',str(output.resolve()),'--args',f'{padding},{padding},{width},{height}',str(stage)],
                stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
            log=args.output_directory/'imagej-edm.log';log.write_bytes(capture.stdout);capture.check_returncode()
            raw=output.read_bytes()
            if raw[:4]!=b'QLR1' or struct.unpack_from('<II',raw,4)!=(rw,rh):raise ValueError('incorrect ImageJ stage protocol')
            grid=np.frombuffer(raw,dtype='<u4',offset=16).reshape(rh,rw)
            local=grid[padding:padding+height,padding:padding+width]
            counts=np.bincount(local.ravel());sums=np.bincount(local.ravel(),weights=score[padding:padding+height,padding:padding+width].ravel())
            means=np.divide(sums,counts,out=np.zeros_like(sums),where=counts>0)
            keep=(counts>=p.min_nucleus_pixels)&(counts<=p.max_nucleus_pixels)&(means>=p.threshold);keep[0]=False
            dense=np.cumsum(keep,dtype=np.uint32)*keep;local=dense[local]
            report['imagej_shape_stage']=dict(input_sha256=comparison.sha(selected),eight_connected_background=args.imagej_dual_background,
                output_sha256=comparison.sha(output),script_sha256=comparison.sha(stage),
                log_sha256=comparison.sha(log),native_count=int(local.max()),
                metrics=metrics.metrics(labels,local,contract['comparison']),
                runtime_upstream=comparison.read((comparison.CONTRACT.parent/contract['upstream_contract']).resolve())['upstream'])
            if args.imagej_dual_background:
                exact=metrics.metrics(local,native_masks['sampled_shape'],contract['comparison'])
                report['sampled_shape_vs_imagej']=exact
                if (exact['foreground_iou']!=1 or exact['unmatched_reference'] or exact['unmatched_native'] or
                        any(m['iou']!=1 or m['centroid_error_px']!=0 for m in exact['matches'])):
                    raise ValueError('native sampled shape pipeline differs from the pinned ImageJ stage')
            print(json.dumps(dict(stage='imagej_shape',native_count=int(local.max()),foreground_iou=report['imagej_shape_stage']['metrics']['foreground_iou'])),flush=True)
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    finally:server.close()


if __name__=='__main__':main()
