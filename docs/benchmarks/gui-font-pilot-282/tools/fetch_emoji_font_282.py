from pathlib import Path
import hashlib
import json
import subprocess
import urllib.request

ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'build/gui-fonts-282/inputs/emoji';OUT.mkdir(exist_ok=False)
def query(path):return json.loads(subprocess.check_output(['gh','api',path],encoding='utf-8',timeout=45))
commits=query('repos/google/fonts/commits?path=ofl/notoemoji&per_page=1');assert len(commits)==1
commit=commits[0]['sha']
contents=query('repos/google/fonts/contents/ofl/notoemoji?ref='+commit)
fonts=[item for item in contents if item['name'].endswith('.ttf')];assert len(fonts)==1
selected=fonts+[next(item for item in contents if item['name']==name) for name in ['OFL.txt','METADATA.pb']]
entries=[]
for item in selected:
    data=urllib.request.urlopen(item['download_url'],timeout=45).read()
    assert len(data)==item['size']
    assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==item['sha']
    target=OUT/item['name'];target.write_bytes(data)
    entries.append(dict(name=item['name'],path=item['path'],url=item['download_url'],bytes=len(data),git_blob=item['sha'],sha256=hashlib.sha256(data).hexdigest()))
license=(OUT/'OFL.txt').read_text(encoding='utf-8');assert 'SIL OPEN FONT LICENSE Version 1.1' in license
(OUT/'input-manifest.json').write_text(json.dumps(dict(repository='https://github.com/google/fonts',commit=commit,commit_url=commits[0]['html_url'],license='SIL Open Font License1.1',files=entries),indent=2)+'\n')
print('Authenticated monochrome emoji font',fonts[0]['name'],fonts[0]['size'],'bytes at',commit,flush=True)
