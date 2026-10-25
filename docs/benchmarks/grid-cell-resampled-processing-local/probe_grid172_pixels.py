from pathlib import Path
from PIL import Image
import numpy as np
p=Path(__file__).resolve().parents[1]/'grid172-resampled-rgb-debug/resampled_reference'
with Image.open(p/'input.png') as image:source=np.asarray(image.convert('RGB'));src=image.convert('RGB')
with Image.open(p/'processed-input.png') as image:actual=np.asarray(image.convert('RGB'))
print('unique input/output',len(np.unique(source.reshape(-1,3),axis=0)),len(np.unique(actual.reshape(-1,3),axis=0)))
for mode in ['NEAREST','BOX','BILINEAR','BICUBIC','LANCZOS']:
 a=np.asarray(src.resize((171,128),getattr(Image.Resampling,mode)))
 print(mode,int(np.count_nonzero(a!=actual)),int(np.max(np.abs(a.astype(int)-actual))))
y=np.floor((np.arange(128)+.5)*1.5).astype(int);x=np.floor((np.arange(171)+.5)*1.5).astype(int)
print('nominal nearest',int(np.count_nonzero(source[y[:,None],x[None,:]]!=actual)))
y=np.floor((np.arange(128)+.5)*192/128).astype(int);x=np.floor((np.arange(171)+.5)*256/171).astype(int)
print('realized nearest',int(np.count_nonzero(source[y[:,None],x[None,:]]!=actual)))
