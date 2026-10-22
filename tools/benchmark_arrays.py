"""Warm native/NumPy imports, views, copies and retained lifetime checks."""
import argparse
import gc
import hashlib
import json
from pathlib import Path
import platform
import time

import numpy as np
import qupathxx as q
from qupathxx import _native


def measure(fn,n=100):
    values=[]
    for _ in range(n):
        start=time.perf_counter();result=fn();values.append((time.perf_counter()-start)*1000)
    return {'p50_ms':sorted(values)[n//2],'p95_ms':sorted(values)[int(n*.95)-1]},result


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=1024);ap.add_argument('--output',type=Path,default=Path('build/arrays-benchmark.json'));args=ap.parse_args();assert 64<=args.size<=2048
    source=np.arange(args.size*args.size*3,dtype=np.uint8).reshape(args.size,args.size,3)
    q.Image.from_numpy(source);imports,image=measure(lambda:q.Image.from_numpy(source),30)
    doc=q.Document(image);p=doc.server.read_region(q.RegionRequest(0,0,0,args.size,args.size));q.StainTransform([[1,0,0],[0,1,0]]).deconvolve(p)
    view,array=measure(p.numpy,1000);copy,_=measure(lambda:p.numpy(copy=True));legacy,_=measure(lambda:np.frombuffer(p.data,dtype=np.uint8).reshape(args.size,args.size,4));strided,_=measure(lambda:q.Image.from_numpy(source[::-1,::2]),30)
    transformed=q.StainTransform([[1,0,0],[0,1,0]]).deconvolve(p);float_view,_=measure(transformed.numpy,1000)
    assert np.shares_memory(array,p.numpy()) and not array.flags.writeable
    expected=array.copy();doc.server.close();del image,doc,p;gc.collect();np.testing.assert_array_equal(array,expected)
    result=dict(platform=platform.platform(),numpy_version=np.__version__,native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,size=args.size,region_bytes=array.nbytes,import_rgb=imports,import_strided_rgb=strided,region_view=view,region_copy=copy,region_bytes_copy_legacy=legacy,float_view=float_view)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
