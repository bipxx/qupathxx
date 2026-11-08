"""Freeze already-tested native bytes; package wheel/native/source without a rebuild."""
import base64,csv,hashlib,io,json,os,subprocess,sys,zipfile
from pathlib import Path
import qupathxx as q
ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT.parents[1]
work=Path(sys.argv[1]).resolve();delivery=work/'delivery';delivery.mkdir(parents=True,exist_ok=False)
package=Path(q.__file__).resolve().parent
expected='2875ca45645052c72636340337212c0ebdd5dba44057cfd237cefc5be38a2f2e'
assert q.build_environment().source_tree_sha256==expected
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
tracked=[p for p in subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode().split('\0') if p]
inputs=sorted(p for p in tracked if (p.startswith('cpp/') and Path(p).suffix in ('.cpp','.hpp','.h')) or (p.startswith('cmake/') and Path(p).suffix in ('.cmake','.in')) or (p.startswith('src/') and p.endswith('.py')) or p in ('CMakeLists.txt','pyproject.toml'))
manifest=''.join(p+'='+sha(ROOT/p)+'\n' for p in inputs).encode()
assert hashlib.sha256(manifest).hexdigest()==expected
for p in (ROOT/'src/qupathxx').iterdir():
 if p.is_file():assert p.read_bytes()==(package/p.name).read_bytes(),p
old=BUILD/'windows-candidate5/delivery/qupathxx-0.1.0-5-cp312-cp312-win_amd64.whl'
members={}
with zipfile.ZipFile(old) as z:
 for name in z.namelist():
  if name.startswith('qupathxx-0.1.0.dist-info/') and not name.endswith('/RECORD'):
   data=z.read(name)
   if name.endswith('/WHEEL'):data=data.replace(b'Build: 5',b'Build: 6').replace(b'Generator: scikit-build-core 1.1.1',b'Generator: qupathxx frozen-runtime packager (scikit-build-core metadata)')
   members[name]=data
for p in sorted(package.rglob('*')):
 if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc':members['qupathxx/'+p.relative_to(package).as_posix()]=p.read_bytes()
assert members['qupathxx/licenses/lodepng/LICENSE']==(ROOT/'cpp/vendor/lodepng/LICENSE').read_bytes()
assert members['qupathxx/licenses/qupathxx/LICENSE']==(ROOT/'LICENSE').read_bytes()
record=io.StringIO(newline='');writer=csv.writer(record,lineterminator='\n')
for name,data in sorted(members.items()):writer.writerow([name,'sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('='),len(data)])
writer.writerow(['qupathxx-0.1.0.dist-info/RECORD','',''])
members['qupathxx-0.1.0.dist-info/RECORD']=record.getvalue().encode()
wheel=delivery/'qupathxx-0.1.0-6-cp312-cp312-win_amd64.whl'
with zipfile.ZipFile(wheel,'x',zipfile.ZIP_DEFLATED) as z:
 for name,data in sorted(members.items()):z.writestr(name,data)
native=delivery/'qupathxx-windows-x64-candidate.zip'
with zipfile.ZipFile(native,'x',zipfile.ZIP_DEFLATED) as z:
 for name,data in sorted(members.items()):
  if not name.startswith('qupathxx/'):continue
  rel=name.removeprefix('qupathxx/')
  if rel.endswith(('.exe','.dll')) or rel.startswith(('fonts/','licenses/')):z.writestr(rel,data)
 z.writestr('RUN.txt',(Path(__file__).parent/'RUN.md').read_bytes())
revision=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
source=delivery/('qupathxx-source-'+revision[:8]+'.zip')
with zipfile.ZipFile(source,'x',zipfile.ZIP_DEFLATED) as z:
 for p in tracked:z.write(ROOT/p,p)
 z.writestr('compiler-inputs.sha256',manifest)
 z.writestr('BUILD-CANDIDATE.txt',f'Candidate6 freezes already-tested cached native bytes. Source commit {revision}.\nRaw compiler inputs {expected}. Native embeds {q.build_environment().git_revision}; dirty label is retained, raw source tree is authoritative.\nUse pyproject.toml/scikit-build-core, Release, GUI/PYTHON/OPENSLIDE/TIFF/GEOS/PROJECT/INTERCHANGE/CLASSIFIER_TRAINING=ON.\nMSVC19.44.35225, CPython3.12.8, nanobind3.1.0, CMake4.4.4. Pinned dependency bundle as supplied; LodePNG is vendored under cpp/vendor/lodepng.\nNo native rebuild or provenance-only relink occurred during packaging.\n')
with zipfile.ZipFile(source) as z:
 for p in inputs:assert hashlib.sha256(z.read(p)).hexdigest()==sha(ROOT/p),p
old=BUILD/'windows-candidate5/delivery'
for name in ('dependency-sources.json','qupathxx-dependency-sources.zip'):os.link(old/name,delivery/name)
deps=json.loads((delivery/'dependency-sources.json').read_text())
assert sha(delivery/deps['archive'])==deps['sha256']
with zipfile.ZipFile(delivery/deps['archive']) as z:
 for item in deps['sources']:assert hashlib.sha256(z.read(item['filename'])).hexdigest()==item['sha256']
vendor={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'cpp/vendor/lodepng').iterdir() if p.is_file()}
receipt=dict(candidate=6,source_commit=revision,embedded_git_revision=q.build_environment().git_revision,source_tree_sha256=expected,compiler_inputs=len(inputs),wheel=wheel.name,wheel_sha256=sha(wheel),native_sha256=sha(native),source_archive=source.name,source_sha256=sha(source),dependency_archives_verified=len(deps['sources']),vendored_lodepng=vendor,license='AGPL-3.0-only',native_bytes_rebuilt=False,package=str(package),support='Local Windows x64 CPython3.12 engineering prerelease; MSVC x64 runtime and OpenGL3.2 required. No fresh-host/signing/stable/other-platform/biology/general-speed claim.')
(delivery/'candidate.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
(delivery/'RUN.md').write_bytes((Path(__file__).parent/'RUN.md').read_bytes())
print(json.dumps(receipt,indent=2))
