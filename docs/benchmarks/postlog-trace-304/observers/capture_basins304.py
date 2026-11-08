from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import subprocess
import sys
import numpy as np
root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/postlog-trace-304';folder=out/sys.argv[1];folder.mkdir(exist_ok=False)
patterns={}
for name,shape in [('single',(1,1)),('row',(1,7)),('column',(7,1)),('flat',(7,9))]:patterns[name]=np.ones(shape,dtype='<f4')
a=np.zeros((7,9),dtype='<f4');a[2:5,3:6]=1;patterns['interior-plateau']=a.copy();a[0:3,3]=1;patterns['border-plateau']=a.copy()
a=np.zeros((7,9),dtype='<f4');a[2,2]=a[4,6]=1;a[1:6,1:8]+=np.float32(.2);patterns['fifo-peaks']=a
a=np.zeros((7,9),dtype='<f4');a[2:5,3:6]=np.float32(.001);patterns['minimum-peak']=a.copy();a*=np.float32(.999);patterns['below-minimum']=a
for seed in range(24):
    rng=np.random.default_rng(304000+seed);shape=(7+seed%3,9+seed%4)
    patterns['noise-'+str(seed)]=(rng.integers(-2,5,size=shape)/8).astype('<f4')
cases=[]
for name,values in patterns.items():
    raw=values.tobytes();(folder/(name+'-input.f32le')).write_bytes(raw)
    cases.append(dict(name=name,width=values.shape[1],height=values.shape[0],input_sha256=hashlib.sha256(raw).hexdigest()))
(folder/'cases.json').write_bytes((json.dumps(cases,indent=2)+'\n').encode())
probe=work/'tools/upstream/qupath_response_basins_probe.groovy';shutil.copyfile(probe,folder/'probe.groovy')
spec=importlib.util.spec_from_file_location('basin304_runtime',work/'tools/fetch_qupath.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
runtime=module.fetch(root/'build/upstream/qupath');args=[str(runtime),'script','--args',str(folder),str(folder/'probe.groovy')]
with (folder/'driver.log').open('xb') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180,creationflags=subprocess.CREATE_NO_WINDOW)
assert r.returncode==0
files={p.name:dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(folder.iterdir()) if p.is_file()}
(folder/'manifest.json').write_bytes((json.dumps(dict(schema='qpx.response-basins-reference/1',probe_sha256=hashlib.sha256(probe.read_bytes()).hexdigest(),cases=cases,files=files),indent=2)+'\n').encode())
print('Captured',len(cases),'independent public basin cases',flush=True)
