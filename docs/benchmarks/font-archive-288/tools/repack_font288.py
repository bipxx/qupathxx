from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import zipfile
import qupathxx as q

root=Path(__file__).resolve().parents[2]
work=root/'build/ci/scientific-grid-159'
out=root/'build/font-archive-288'
artifact=root/'build/q285-all/Windows-gui-py3.12'
wheel,=list((artifact/'wheels').glob('*.whl'))
old=artifact/'qupathxx-windows-x64-candidate.zip'
original=out/'original-standalone'
original.mkdir(exist_ok=False)
with zipfile.ZipFile(old) as z:
    for entry in z.infolist():
        assert (original/entry.filename).resolve().is_relative_to(original.resolve())
    z.extractall(original)
assert not (original/'fonts').exists()
spec=importlib.util.spec_from_file_location('producer',work/'tools/ci/run.py')
producer=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),producer.__dict__)
repack=out/'repacked'
repack.mkdir(exist_ok=False)
stage=producer.archive_windows(wheel,repack)
assets=[]
with zipfile.ZipFile(wheel) as z:
    for name in ('fonts/NotoSansCJKsc-Regular.otf','fonts/NotoEmoji.ttf'):
        data=z.read('qupathxx/'+name)
        assert (stage/name).read_bytes()==data
        assets.append(dict(path=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    for path in stage.rglob('*'):
        if path.is_file() and path.name!='README.txt':
            assert path.read_bytes()==z.read('qupathxx/'+path.relative_to(stage).as_posix())
for path in original.rglob('*'):
    if path.is_file() and path.name!='README.txt':
        assert path.read_bytes()==(stage/path.relative_to(original)).read_bytes()
assert q.build_environment().git_revision=='7231d95c25fbc9515d937036986e8c92b19da261'
project=q.Project()
doc=q.Document(q.Image(256,192,[35,60,90]*256*192))
doc.add_rectangle(q.Rect(40,40,120,80),'A_雪𠀋🔬')
project.add_document(doc,'Unicode font archive')
path=out/'standalone-input.qpxproj'
assert not path.exists()
project.save(str(path))
layout=out/'standalone-workspace.ini'
clean=os.environ.copy()
clean['PATH']=os.pathsep.join([str(Path(os.environ['SystemRoot'])/'System32'),os.environ['SystemRoot']])
commands=[('original-missing-font',[str(original/'qupathxx_app.exe'),'--smoke'],1),
    ('fixed-layout',[str(stage/'qupathxx_app.exe'),'--project',str(path),'--layout',str(layout),'--smoke'],0),
    ('fixed-layout-reload',[str(stage/'qupathxx_app.exe'),'--project',str(path),'--layout',str(layout),'--smoke'],0),
    ('fixed-slide',[str(stage/'qupathxx_app.exe'),str(root/'build/fixtures/CMU-1-Small-Region.svs'),'--smoke'],0)]
results=[]
for name,args,expected in commands:
    log=out/(name+'.log')
    assert not log.exists()
    with log.open('w',encoding='utf-8') as stream:
        result=subprocess.run(args,cwd=out,env=clean,stdout=stream,stderr=subprocess.STDOUT,timeout=120)
    results.append(dict(name=name,argv=args,returncode=result.returncode,expected=expected))
    (out/'commands.json').write_text(json.dumps(results,indent=2)+'\n')
    print(name,result.returncode,flush=True)
    assert result.returncode==expected,log
assert 'Missing GUI font asset:' in (out/'original-missing-font.log').read_text()
assert layout.read_bytes().startswith(b'; qupathxx workspace layout v1\n')
assert b'[Docking][Data]' in layout.read_bytes()
source=subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip()
result=dict(product_source='1f71eb59e13fba272113c1119ee2b608fa26d01a',archiver_source=source,
    wheel_sha256=hashlib.sha256(wheel.read_bytes()).hexdigest(),
    original_archive_sha256=hashlib.sha256(old.read_bytes()).hexdigest(),
    repacked_archive_sha256=hashlib.sha256((repack/'qupathxx-windows-x64-candidate.zip').read_bytes()).hexdigest(),
    assets=assets,old_native_bytes_unchanged=True,commands=results,
    product_rebuilt=False,binary_relabelled=False,clean_path=True,clean_machine=False,
    hosted_run_repeated=False,qualification_transferred=False)
(out/'receipt-local.json').write_text(json.dumps(result,indent=2)+'\n')
print('Exact original CI standalone fails without fonts; same native bytes pass layout/reload/slide after resource-only repack',flush=True)
