"""Generate independent integer-ellipse TMA masks before implementing detection.

These synthetic fixtures contain no patient data. Native and upstream detector
observations are evaluated against the declared lattice, never each other alone.
"""
import argparse
import hashlib
import json
from pathlib import Path


def fixtures():
    width,height,rows,columns=480,352,3,4
    definitions=[
        ('aligned',(60,0),(0,60),set(),32,32,'full_grid'),
        ('interior_missing',(60,0),(0,60),{5},32,32,'full_grid'),
        ('rotated_missing',(60,8),(-8,60),{3,6},32,32,'full_grid'),
        ('skewed_missing',(60,4),(8,60),{5},32,32,'full_grid'),
        ('anisotropic',(60,0),(0,60),{6},32,48,'full_grid'),
        ('unobserved_row',(60,0),(0,60),{4,5,6,7},32,32,'review_or_refuse'),
        ('blank',(60,0),(0,60),set(range(12)),32,32,'review_or_refuse'),
    ]
    files={};cases=[]
    for name,column_step,row_step,missing,core_width,core_height,expected in definitions:
        cores=[];pixels=bytearray(width*height)
        for row in range(rows):
            for column in range(columns):
                slot=row*columns+column
                x=112+column*column_step[0]+row*row_step[0]
                y=96+column*column_step[1]+row*row_step[1]
                absent=slot in missing
                cores.append(dict(row=row,column=column,center=[x,y],present=not absent))
                if absent:continue
                for iy in range(y-core_height//2,y+core_height//2):
                    for ix in range(x-core_width//2,x+core_width//2):
                        dx=2*ix+1-2*x;dy=2*iy+1-2*y
                        if dx*dx*core_height**2+dy*dy*core_width**2<=core_width**2*core_height**2:
                            pixels[iy*width+ix]=255
        raw=f'P5\n{width} {height}\n255\n'.encode()+pixels
        files[name+'.pgm']=raw
        cases.append(dict(name=name,mask=name+'.pgm',mask_sha256=hashlib.sha256(raw).hexdigest(),width=width,height=height,
            rows=rows,columns=columns,core_width=core_width,core_height=core_height,
            upstream_core_diameter=40 if name=='anisotropic' else 32,
            expected=expected,cores=cores))
    contract=dict(schema='qpx.tma-dearray-fixtures/1',synthetic=True,patient_data=False,
        geometry='Integer level-zero ellipse centers; foreground iff the exact rational pixel center is in the closed ellipse.',
        rows_and_columns_supplied=True,missing_density_threshold=.05,
        acceptance=dict(full_grid=dict(rows=rows,columns=columns,max_center_error_pixels=2.0),
            review_or_refuse='Do not silently accept an unobservable requested axis or an empty image.'),
        scope='Independent synthetic geometry and upstream characterization; not biological or supported-release acceptance.',
        upstream_revision='04ccfa4fb7d43e9b566393e08e83690b72248d44',cases=cases)
    files['contract.json']=(json.dumps(contract,indent=2)+'\n').encode()
    return files


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'tests/fixtures/tma-dearray-v1')
    parser.add_argument('--check',action='store_true');args=parser.parse_args()
    files=fixtures()
    if args.check:
        for name,raw in files.items():
            if (args.output/name).read_bytes()!=raw:raise SystemExit('fixture differs: '+name)
    else:
        args.output.mkdir(parents=True,exist_ok=False)
        for name,raw in files.items():(args.output/name).write_bytes(raw)
    print(('Verified' if args.check else 'Generated')+' seven independent TMA masks and preregistered geometry/tolerances')


if __name__=='__main__':main()
