from pathlib import Path
import hashlib,json,sys,zipfile
import qupathxx as q
from qupathxx import _native
r=Path(__file__).resolve().parents[2];out=r/'build/rawpython229-final-b';m=json.loads((out/'compiler-inputs.json').read_text());wheel,=list((out/'wheel').glob('*.whl'));package=Path(q.__file__).parent
assert package.parent.resolve()==Path(sys.prefix,'Lib/site-packages').resolve()
assert q.has_tiff and q.has_project and q.has_classifier_training and not q.has_gui and not q.has_geos
environment=q.build_environment();assert environment.git_revision==m['source'] and environment.source_tree_sha256==m['fingerprint']
entries=[]
with zipfile.ZipFile(wheel) as z:
 for member in z.namelist():
  if not member.startswith('qupathxx/') or member.endswith('/'):continue
  data=z.read(member);p=package.parent/member;assert p.read_bytes()==data,member
  entries.append(dict(path=member,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
names=json.loads((r/'build/rawpython229-public-api.json').read_text());assert len(names)==71 and all(hasattr(q,n) and n in q.__all__ for n in names)
result=dict(package=str(package),native=str(_native.__file__),python=sys.version,source=environment.git_revision,fingerprint=environment.source_tree_sha256,wheel=str(wheel),wheel_bytes=wheel.stat().st_size,wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest(),wheel_members=entries,new_public_exports=names,shared_test_dependencies=str(r/'.venv/Lib/site-packages'),gui=False,geos=False,training=True,tiff=True,project=True)
(out/'installed-proof.json').write_text(json.dumps(result,indent=2)+'\n');print('Installed wheel members/source/runtime/capabilities and all 71 exports authenticated')
