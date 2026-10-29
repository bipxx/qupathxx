"""Derive affine-sampling golden bytes from retained public captures.

No coordinate or interpolation model participates in expected output generation.
The earlier 99 reads remain in rgb-nominal-v1; this adds the 13 cell edge reads.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURE = ROOT / 'tests/fixtures/rgb-affine-v2'


def generate(destination):
    from PIL import Image
    spec = importlib.util.spec_from_file_location('affine_edge_reference', ROOT / 'tools/qupath_cell_policy_edges.py')
    edges = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(edges)
    edges.verify()
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    (destination / 'sources').mkdir()
    lines = ['qpx.rgb-affine-golden/1']
    for case in edges.contract()['cases']:
        folder = edges.FIXTURE / case['name']
        with Image.open(folder / 'input.png') as image:
            source = image.convert('RGB')
            sw, sh = source.size
            raw = source.tobytes()
        source_file = 'sources/' + hashlib.sha256(raw).hexdigest() + '.rgb'
        (destination / source_file).write_bytes(raw)
        with Image.open(folder / 'processed-input.png') as image:
            output = image.convert('RGBA')
            ow, oh = output.size
            expected = hashlib.sha256(output.tobytes()).hexdigest()
        step = case.get('requested_pixel_size_um', .5) / .5
        bounds = case.get('roi', [0, 0, sw, sh])
        fields = [case['name'], source_file, sw, sh, *bounds, repr(step), ow, oh, expected]
        lines.append(' '.join(map(str, fields)))
    (destination / 'index.txt').write_bytes(('\n'.join(lines) + '\n').encode('utf-8'))
    manifest = dict(schema='qpx.rgb-affine-golden-manifest/1', cases=len(lines)-1,
        interpolation_performed=False, source_manifest_sha256=edges.sha(edges.FIXTURE / 'manifest.json'),
        generator_sha256=edges.sha(__file__),
        files={p.relative_to(destination).as_posix(): dict(bytes=p.stat().st_size, sha256=edges.sha(p))
               for p in sorted(destination.rglob('*')) if p.is_file()})
    (destination / 'manifest.json').write_bytes((json.dumps(manifest, indent=2)+'\n').encode('utf-8'))
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(generate(args.output), indent=2))
