"""Decode public real-slide nucleus/body masks; perform no expansion modeling."""
import argparse
import gzip
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=ROOT/'tests/fixtures/qupath-body-boundary-v2'


def generate(destination):
    spec=importlib.util.spec_from_file_location('body_boundary_reference',ROOT/'tools/compare_qupath_ndpi_cells.py')
    tool=importlib.util.module_from_spec(spec);spec.loader.exec_module(tool)
    source=ROOT/'tests/fixtures/qupath-ndpi-cells-0.7.0/capture'
    contract,reference=tool.verify_capture(source);tool.reference_labels(source,contract,reference)
    destination=Path(destination);destination.mkdir(parents=True,exist_ok=False)
    for name in ('nucleus','body'):
        (destination/(name+'.u32le')).write_bytes(gzip.decompress((source/(name+'.u32le.gz')).read_bytes()))
    manifest=dict(schema='qpx.qupath-body-boundary-reference/1',source_capture_sha256=tool.sha(source/'capture-manifest.json'),
        source_contract_sha256=tool.sha(tool.CONTRACT),generator_sha256=tool.sha(Path(__file__)),width=contract['roi'][2],height=contract['roi'][3],
        expansion_radius=contract['native']['cell_expansion'],labels=reference['count'],expansion_performed=False,
        files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(destination.iterdir())})
    (destination/'manifest.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode('utf-8'))
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    print(json.dumps(generate(args.output),indent=2))
