from pathlib import Path
import hashlib
import importlib.util
import json
import os
import subprocess
import sys
import zipfile
import qupathxx as q

root=Path(__file__).resolve().parents[2];work=root/'build/ci/scientific-grid-159';out=root/'build/nominal-cell-frontends-294'
manifest=json.loads((out/'compiler-inputs-initial.json').read_text(encoding='utf-8'))
assert q.build_environment().git_revision==manifest['source']
wheel,=list((out/'final-wheel').glob('*.whl'))
spec=importlib.util.spec_from_file_location('producer',work/'tools/ci/run.py')
producer=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),producer.__dict__)
destination=out/'standalone-delivery';destination.mkdir(exist_ok=False)
stage=producer.archive_windows(wheel,destination)
assert not list(stage.rglob('*.pyd')) and not list(stage.rglob('*.py'))
members=[]
with zipfile.ZipFile(wheel) as z:
    for p in stage.rglob('*'):
        if not p.is_file() or p.name=='README.txt':continue
        data=p.read_bytes();assert data==z.read('qupathxx/'+p.relative_to(stage).as_posix())
        members.append(dict(path=p.relative_to(stage).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
assert (stage/'fonts/NotoSansCJKsc-Regular.otf').is_file() and (stage/'fonts/NotoEmoji.ttf').is_file()
sys.path.insert(0,str(work/'tests'))
from test_nominal_cell_batches import cohort,flags
project,docs,_,_,_=cohort();source=destination/'original-原始𠀋.qpxproj';project.save(str(source));original=source.read_bytes()
env=os.environ.copy();env['PATH']=os.pathsep.join([str(Path(os.environ['SystemRoot'])/'System32'),os.environ['SystemRoot']])
commands=[]
for label in ('fresh','resume'):
    args=[str(stage/'qupathxx_batch.exe'),str(source),str(destination/'results'),*flags()]
    result=subprocess.run(args,cwd=destination,env=env,capture_output=True,timeout=120)
    (out/('standalone-'+label+'.stdout')).write_bytes(result.stdout)
    (out/('standalone-'+label+'.stderr')).write_bytes(result.stderr)
    commands.append(dict(label=label,argv=args,returncode=result.returncode))
    assert result.returncode==0,result.stderr
    report=json.loads(result.stdout.decode('utf-8').splitlines()[-1])
    assert report['saved']==(2 if label=='fresh' else 0) and report['reused']==(0 if label=='fresh' else 2)
    for item,(image,_,_) in zip(report['images'],docs):
        doc=q.Project.open(item['path']).document(image);run=doc.cell_records[0].result
        assert run.nominal_sampling.source_request.downsample==1.5
        if image==docs[0][0]:assert run.cell_count==3 and run.nuclei.foreground_pixels==379 and run.cells.foreground_pixels==825
    assert source.read_bytes()==original
layout=destination/'workspace.ini'
for label in ('layout','layout-reload'):
    args=[str(stage/'qupathxx_app.exe'),'--project',str(source),'--layout',str(layout),'--smoke']
    with (out/('standalone-'+label+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(args,cwd=destination,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
    commands.append(dict(label=label,argv=args,returncode=result.returncode));assert result.returncode==0
assert layout.read_bytes().startswith(b'; qupathxx workspace layout v1\n') and b'[Docking][Data]' in layout.read_bytes()
archive=destination/'qupathxx-windows-x64-candidate.zip'
(out/'standalone-proof.json').write_text(json.dumps(dict(native_source=manifest['source'],members=members,commands=commands,
    clean_path=True,python_files_absent=True,gui_smokes=2,images=2,source_unchanged=True,
    archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),clean_machine=False,release_ready=False),indent=2)+'\n',encoding='utf-8')
print('Actual standalone archive: two nominal images, resume and two layout/font smokes pass on clean PATH',flush=True)
