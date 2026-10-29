from pathlib import Path
import gzip
import hashlib
import json
import shutil
import subprocess
import numpy as np
import qupathxx as q
root=Path(__file__).resolve().parents[2];initial=root/'build/qupath-input-stages-301';out=initial/'capture2';out.mkdir(exist_ok=False)
for name in ('cases.json','pairwise.rgb','roi.rgb','resampled.rgb'):shutil.copyfile(initial/name,out/name)
text=(initial/'probe.groovy').read_text().replace("def vector=item.vectors.collectWithIndex {v,index -> StainVector.createStainVector('S'+index,*(v.collect {it as double}))}","def vector=[];item.vectors.eachWithIndex {v,index -> vector.add(StainVector.createStainVector('S'+index,*(v.collect {it as double})))}")
assert 'collectWithIndex' not in text
(out/'probe.groovy').write_text(text,encoding='utf-8')
args=json.loads((initial/'command.json').read_text())['argv'];args=[v.replace(str(initial),str(out)) for v in args]
with (out/'driver.log').open('xb') as stream:r=subprocess.run(args,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
(out/'command.json').write_text(json.dumps(dict(argv=args,returncode=r.returncode),indent=2)+'\n');assert r.returncode==0,out/'driver.log'
cases=json.loads((out/'cases.json').read_text());images={}
for c in cases:images[c['image']]=np.frombuffer((out/(c['image']+'.rgb')).read_bytes(),dtype='u1').reshape(c['height'],c['width'],3).copy()
source=(root/'build/ci/diagnose_input301.py').read_text();tail=source[source.index('reports=[]'):]
exec(compile(tail,str(root/'build/ci/diagnose_input301.py'),'exec'),globals())
