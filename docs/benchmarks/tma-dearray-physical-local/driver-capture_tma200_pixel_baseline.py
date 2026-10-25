from pathlib import Path
import hashlib,json,numpy as np
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];fixture=r/'build/ci/scientific-grid-159/tests/fixtures/tma-dearray-v1/anisotropic.pgm';raw=fixture.read_bytes();header=b'P5\n480 352\n255\n';assert raw.startswith(header);gray=np.frombuffer(raw[len(header):],dtype=np.uint8).reshape(352,480);doc=q.Document(q.Image.from_numpy(np.repeat((255-gray)[...,None],3,axis=2)));o=q.TmaDearrayOptions();o.core_width=32;o.core_height=48;preview=doc.server.read_region(q.RegionRequest(0,0,0,480,352));results={}
for name,result in [('preview',q.detect_tma_grid(preview,480,352,o)),('server',q.detect_tma_grid(doc.server,o))]:results[name]={'input_sha256':result.input_sha256,'recipe_sha256':result.recipe_sha256}
v={'source':q.build_environment().git_revision,'compiler_fingerprint':q.build_environment().source_tree_sha256,'native_module_sha256':hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(raw).hexdigest(),'results':results};assert v['source']=='a492ea5a6f7cea2f069fcc5899b6f4a20cd9554e';out=r/'build/tma200-pixel-baseline.json';out.write_text(json.dumps(v,indent=2)+'\n',encoding='utf-8');print(json.dumps(v,indent=2))
