from pathlib import Path
from PIL import Image
import numpy as np
p=Path(__file__).resolve().parents[1]/'grid172-resampled-rgb-debug/resampled_reference'
with Image.open(p/'input.png') as image:source=np.asarray(image.convert('RGB')).astype(float)
with Image.open(p/'processed-input.png') as image:actual=np.asarray(image.convert('RGB'))
for name,sx,sy in [('nominal',1.5,1.5),('realized',256/171,1.5)]:
 for origin in ['center','corner']:
  x=(np.arange(171)+.5)*sx-.5 if origin=='center' else np.arange(171)*sx
  y=(np.arange(128)+.5)*sy-.5 if origin=='center' else np.arange(128)*sy
  x=np.clip(x,0,255);y=np.clip(y,0,191);ix=np.floor(x).astype(int);iy=np.floor(y).astype(int);wx=(x-ix)[None,:,None];wy=(y-iy)[:,None,None]
  a=source[iy[:,None],ix[None,:]]*(1-wx)*(1-wy)+source[iy[:,None],np.minimum(ix+1,255)[None,:]]*wx*(1-wy)+source[np.minimum(iy+1,191)[:,None],ix[None,:]]*(1-wx)*wy+source[np.minimum(iy+1,191)[:,None],np.minimum(ix+1,255)[None,:]]*wx*wy
  for rounding,rounded in [('even',np.rint(a)),('halfup',np.floor(a+.5)),('floor',np.floor(a))]:
   print(name,origin,rounding,int(np.count_nonzero(rounded!=actual)),int(np.max(np.abs(rounded.astype(int)-actual))))
