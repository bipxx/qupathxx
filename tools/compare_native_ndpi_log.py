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
        p.algorithm=q.CellAlgorithm.LOG_WATERSHED_BODIES;p.tile_size=64
        control=q.detect_cells(q.CellRequest(doc,target,transform,p))
        production=comparison.local_labels(control.nuclei,contract['roi'])
        results=[]
        for candidate in [False,True]:
            output=args.output_directory/('response-basins.qlr' if candidate else 'connected-regions.qlr')
            subprocess.run([str(args.executable)]+(['--response-basins'] if candidate else [])+[str(input),str(output)],check=True)
            raw=output.read_bytes()
            if raw[:4]!=b'QLR1' or struct.unpack_from('<II',raw,4)!=(rw,rh):raise ValueError('incorrect native label protocol')
            native=np.frombuffer(raw,dtype='<u4',offset=16).reshape(rh,rw)[padding:padding+height,padding:padding+width]
            # Match the production contract for ROI fragments after contextual filtering.
            counts=np.bincount(native.ravel());keep=(counts>=p.min_nucleus_pixels)&(counts<=p.max_nucleus_pixels);keep[0]=False
            dense=np.cumsum(keep,dtype=np.uint32)*keep;native=dense[native]
            if not candidate:np.testing.assert_array_equal(native,production)
            record=dict(response_basins=candidate,native_count=int(native.max()),
                labels_sha256=hashlib.sha256(native.astype('<u4',copy=False).tobytes()).hexdigest(),
                output_sha256=comparison.sha(output),metrics=metrics.metrics(labels,native,contract['comparison']))
            results.append(record)
            print(json.dumps(dict(response_basins=candidate,native_count=record['native_count'],foreground_iou=record['metrics']['foreground_iou'])),flush=True)
        report=dict(schema='qpx.native-ndpi-log-comparison/1',native_environment=environment,
            native_executable_sha256=comparison.sha(args.executable),feature_producer_git_revision=q.build_environment().git_revision,
            feature_producer_source_tree_sha256=q.build_environment().source_tree_sha256,
            tool_sha256=comparison.sha(Path(__file__)),input_sha256=comparison.sha(input),
            capture_manifest_sha256=comparison.sha(reference/'capture-manifest.json'),source_pixels_equal=True,
            legacy_kernel_matches_production=True,production_integrated=False,
            scientific_accepted=False,performance_accepted=False,results=results)
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_bytes((json.dumps(report,indent=2,allow_nan=False)+'\n').encode())
    finally:server.close()


if __name__=='__main__':main()
