"""Stream independent analytic TMA ellipses into a tiled synthetic OME pyramid.

No qupathxx imports: geometry is specified by the preregistered contract.
Each level samples the same level-zero scene at exact rational pixel centers.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

import numpy as np
import tifffile

CONTRACT = Path(__file__).resolve().parents[1]/'tests/fixtures/tma-large-pyramid-v1/contract.json'


def tiles(contract, downsample):
    """Yield one owning RGB tile at a time, including white edge padding."""
    width = math.ceil(contract['width']/downsample)
    height = math.ceil(contract['height']/downsample)
    size = contract['tile_size']
    cw, ch = contract['core_width'], contract['core_height']
    for top in range(0, height, size):
        for left in range(0, width, size):
            tile = np.full((size, size, 3), 255, dtype=np.uint8)
            valid_w, valid_h = min(size, width-left), min(size, height-top)
            for core in contract['cores']:
                if not core['present']:
                    continue
                cx, cy = core['center']
                # Clip to the ellipse's bounding box before squaring. This
                # keeps arithmetic in int64 even for far-away base tiles.
                x0 = max(left, math.floor((cx-cw/2)/downsample))
                x1 = min(left+valid_w, math.ceil((cx+cw/2)/downsample))
                y0 = max(top, math.floor((cy-ch/2)/downsample))
                y1 = min(top+valid_h, math.ceil((cy+ch/2)/downsample))
                if x0 >= x1 or y0 >= y1:
                    continue
                dx = downsample*(2*np.arange(x0,x1,dtype=np.int64)+1)-2*cx
                dy = downsample*(2*np.arange(y0,y1,dtype=np.int64)+1)-2*cy
                inside = dy[:,None]**2*cw**2 + dx[None,:]**2*ch**2 <= cw**2*ch**2
                tile[y0-top:y1-top,x0-left:x1-left][inside] = 0
            yield tile


def generate(path, contract):
    if path.exists():
        raise FileExistsError(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with tifffile.TiffWriter(path, bigtiff=True, ome=True) as writer:
        for index, scale in enumerate(contract['downsamples']):
            metadata = None if index else dict(
                axes='YXS', PhysicalSizeX=contract['pixel_width_um'],
                PhysicalSizeY=contract['pixel_height_um'],
                PhysicalSizeXUnit='\u00b5m', PhysicalSizeYUnit='\u00b5m')
            writer.write(tiles(contract,scale),
                         shape=(math.ceil(contract['height']/scale),math.ceil(contract['width']/scale),3),
                         dtype=np.uint8, tile=(contract['tile_size'],)*2,
                         photometric='rgb', compression='deflate', metadata=metadata,
                         subifds=len(contract['downsamples'])-1 if not index else None,
                         subfiletype=0 if not index else 1, maxworkers=1)
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            digest.update(block)
    return dict(file=str(path.resolve()),bytes=path.stat().st_size,
                sha256=digest.hexdigest(),contract_sha256=hashlib.sha256(
                    json.dumps(contract,sort_keys=True,separators=(',',':')).encode('utf-8')).hexdigest(),
                tifffile_version=tifffile.__version__,numpy_version=np.__version__)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output',type=Path)
    args=parser.parse_args()
    manifest=generate(args.output,json.loads(CONTRACT.read_text(encoding='utf-8')))
    args.output.with_suffix(args.output.suffix+'.json').write_bytes((json.dumps(manifest,indent=2)+'\n').encode())
    print(json.dumps(manifest,indent=2))


if __name__=='__main__':
    main()
