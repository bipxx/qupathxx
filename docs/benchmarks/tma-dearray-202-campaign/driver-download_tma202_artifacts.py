from pathlib import Path
import hashlib,json,re,sys,urllib.request,zipfile
items=json.load(sys.stdin);root=Path('build/tmaci202-hosted/artifacts').resolve();root.mkdir(parents=True,exist_ok=True)
allowed={'Windows-gui-py3.12','Windows-headless-py3.11','Linux-headless-py3.12','Linux-minimal-py3.12','Linux-sanitize-py3.12','macos-native-arm64','macos-native-x86_64','manylinux-3.11','manylinux-3.12','manylinux-consumer-3.11','manylinux-consumer-3.12'}
for item in items:
 a=item['metadata'];assert a['workflow_run']['head_sha']=='19371b74e0fabb3a91bd77ffe67ded04f46a5220' and not a['expired']
 name=a['name'];assert name in allowed;target=root/(name+'.zip');part=root/(name+'.zip.part');assert target.resolve().is_relative_to(root) and part.resolve().is_relative_to(root)
 expected=a['digest'];assert re.fullmatch(r'sha256:[0-9a-f]{64}',expected)
 if not target.exists():
  assert not part.exists(),'Inspect retained partial transfer before retry'
  with urllib.request.urlopen(item['url'],timeout=45) as source,part.open('xb') as destination:
   while True:
    block=source.read(1024*1024)
    if not block:break
    destination.write(block)
  with part.open('rb') as file:actual=hashlib.file_digest(file,'sha256').hexdigest()
  assert actual==expected.split(':')[1] and part.stat().st_size==a['size_in_bytes']
  part.rename(target)
 with target.open('rb') as file:assert hashlib.file_digest(file,'sha256').hexdigest()==expected.split(':')[1]
 assert target.stat().st_size==a['size_in_bytes'];dest=(root/name).resolve();assert dest.is_relative_to(root);dest.mkdir(exist_ok=True)
 with zipfile.ZipFile(target) as archive:
  assert all((dest/e.filename).resolve().is_relative_to(dest) for e in archive.infolist())
  archive.extractall(dest)
 (root/(name+'-artifact.json')).write_bytes((json.dumps(a,indent=2)+'\n').encode('utf-8'))
 print(name,'archive bytes/digest/source verified',flush=True)
