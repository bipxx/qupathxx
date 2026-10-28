"""Derive native cell fixtures from authenticated captured pixels and geometry."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]


def reference():
    path=ROOT/'tools/qupath_resampled_cell_debug.py'
    spec=importlib.util.spec_from_file_location('nominal_cell_capture',path)
    debug=importlib.util.module_from_spec(spec);spec.loader.exec_module(debug)
    _,_,rgb,original,labels,_=debug.verified_data(debug.FIXTURE)
    assert original.shape==(192,256,3) and rgb.shape==(128,171,3)
    payloads={'source.rgb':original.tobytes(),'processed.rgb':rgb.tobytes()}
    payloads.update({name+'.u32le':labels[name].astype('<u4').tobytes() for name in ('nucleus','body')})
    manifest=dict(schema='qpx.nominal-cell-captured-input/1',
        source_capture='qupath-cells-resampled-debug-0.7.0',
        capture_manifest_sha256=hashlib.sha256((debug.FIXTURE/'manifest.json').read_bytes()).hexdigest(),
        request=dict(x_px=0,y_px=0,width_px=256,height_px=192,downsample=1.5,context_x=0,context_y=0),
        output_shape=[128,171],sample_counts=dict(nucleus=379,body=825),
        files={name:dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in sorted(payloads.items())})
    payloads['manifest.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    return payloads


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();payloads=reference()
    if args.output:
        args.output.mkdir(parents=True,exist_ok=False)
        for name,data in payloads.items():(args.output/name).write_bytes(data)
    else:
        folder=ROOT/'tests/fixtures/nominal-cells-v1'
        assert {p.name for p in folder.iterdir()}==set(payloads)
        for name,data in payloads.items():assert (folder/name).read_bytes()==data,name
    print('Captured source/processed RGB and independently rasterized nucleus/body fixtures match')


if __name__=='__main__':main()
