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
out=root/'build/rgb-nominal-sdk-289'
wheel,=list((out/'final-wheel').glob('*.whl'))
assert q.build_environment().git_revision=='11f70cd1133612fd059fd0c294f6b7eb60103679'
spec=importlib.util.spec_from_file_location('producer',work/'tools/ci/run.py')
producer=importlib.util.module_from_spec(spec)
exec(compile(Path(spec.origin).read_bytes(),spec.origin,'exec',dont_inherit=True),producer.__dict__)
destination=out/'standalone-delivery';destination.mkdir(exist_ok=False)
stage=producer.archive_windows(wheel,destination)
assert not list(stage.glob('*.pyd')) and not list(stage.glob('*.py'))
members=[]
with zipfile.ZipFile(wheel) as z:
    for p in stage.rglob('*'):
        if not p.is_file() or p.name=='README.txt':continue
        key='qupathxx/'+p.relative_to(stage).as_posix()
        assert p.read_bytes()==z.read(key)
        members.append(dict(path=p.relative_to(stage).as_posix(),bytes=p.stat().st_size,
            sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
assert (stage/'fonts/NotoSansCJKsc-Regular.otf').is_file() and (stage/'fonts/NotoEmoji.ttf').is_file()
env=os.environ.copy()
env['PATH']=os.pathsep.join([str(Path(os.environ['SystemRoot'])/'System32'),os.environ['SystemRoot']])
fixture=work/'tests/fixtures/rgb-nominal-v1'
commands=[]
for line in (fixture/'index.txt').read_text().splitlines()[1:]:
    f=line.split();image=destination/'source-原始-𠀋.ppm';output=destination/'read.pam'
    original=b'P6\n'+f[2].encode()+b' '+f[3].encode()+b'\n255\n'+(fixture/f[1]).read_bytes()
    image.write_bytes(original)
    args=[str(stage/'qupathxx_region.exe'),'--rgb-nominal',str(image),*f[4:9],str(output)]
    result=subprocess.run(args,env=env,capture_output=True,timeout=30)
    commands.append(dict(name=f[0],argv=args,returncode=result.returncode,stderr=result.stderr.decode('utf-8',errors='replace')))
    assert result.returncode==0,commands[-1]
    _,pixels=output.read_bytes().split(b'ENDHDR\n',1)
    assert hashlib.sha256(pixels).hexdigest()==f[11]
    assert image.read_bytes()==original
assert len(commands)==99
project=q.Project();doc=q.Document(q.Image.demo());doc.add_rectangle(q.Rect(10,10,100,80),'RGB_雪𠀋🔬');project.add_document(doc)
project_path=destination/'input.qpxproj';project.save(str(project_path))
layout=destination/'workspace.ini'
for name,args in [('layout',[str(stage/'qupathxx_app.exe'),'--project',str(project_path),'--layout',str(layout),'--smoke']),
                  ('layout-reload',[str(stage/'qupathxx_app.exe'),'--project',str(project_path),'--layout',str(layout),'--smoke'])]:
    with (out/('standalone-'+name+'.log')).open('w',encoding='utf-8') as log:
        result=subprocess.run(args,cwd=destination,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=120)
    commands.append(dict(name=name,argv=args,returncode=result.returncode))
    assert result.returncode==0
assert layout.read_bytes().startswith(b'; qupathxx workspace layout v1\n') and b'[Docking][Data]' in layout.read_bytes()
archive=destination/'qupathxx-windows-x64-candidate.zip'
(out/'standalone-proof.json').write_text(json.dumps(dict(native_source=q.build_environment().git_revision,
    members=members,commands=commands,exact_cli_hashes=99,gui_smokes=2,clean_path=True,python_files_absent=True,
    archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),clean_machine=False,release_ready=False),indent=2)+'\n')
print('Independent standalone99 captured CLI hashes and two layout/font smokes pass on clean PATH',flush=True)
