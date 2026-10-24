from pathlib import Path
import argparse,hashlib,json,statistics,time
import numpy as np
import qupathxx as q
import qupathxx._native as native
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--label',required=True);a=p.parse_args()
rng=np.random.default_rng(154);rgba=rng.integers(0,256,(1024,2048,4),dtype=np.uint8)
rows=[]
for name,alpha,width,height in [('opaque_identity',255,2048,1024),('mixed_identity',None,2048,1024),('mixed_fractional',None,641,359)]:
    data=rgba.copy()
    if alpha is not None:data[:,:,3]=alpha
    source=q.PixelBuffer.from_numpy(data);expected=data.copy();expected[:,:,:3][expected[:,:,3]==0]=0
    expected_hash=hashlib.sha256(expected.tobytes()).hexdigest() if width==2048 and height==1024 else None
    observations=[];digest=None
    for i in range(11):
        start=time.perf_counter_ns();result=q.resample_area(source,width,height);elapsed=time.perf_counter_ns()-start
        observed=hashlib.sha256(result.data).hexdigest();del result
        if digest is None:digest=observed
        assert observed==digest and (expected_hash is None or observed==expected_hash)
        observations.append(dict(warmup=i<2,elapsed_ns=elapsed,sha256=observed))
    values=[o['elapsed_ns'] for o in observations if not o['warmup']]
    rows.append(dict(case=name,input_sha256=hashlib.sha256(data.tobytes()).hexdigest(),output_shape=[height,width,4],
        output_sha256=digest,independent_identity_hash=expected_hash,observations=observations,median_ns=statistics.median(values)))
result=dict(schema='qpx.resampling-component-profile/1',label=a.label,seed=154,shape=[1024,2048,4],warmup=2,samples=9,
    native_sha256=hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest(),rows=rows,
    hardware_or_workflow_acceptance=False,scope='Local resample_area call only; retained warmups and timed observations. No decoder, UI, RSS or hardware-budget acceptance.')
Path(a.output).write_bytes((json.dumps(result,indent=2)+'\n').encode('utf-8'))
print(json.dumps({row['case']:row['median_ns'] for row in rows}))
