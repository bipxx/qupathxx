from pathlib import Path
import hashlib,json,subprocess,sys
import qupathxx as q
r=Path(__file__).resolve().parents[2];out=r/'build/rawcli230-backend-off';exe=r/'build/ci/gridfrontends167-package/Release/qupathxx_batch.exe'
flags={line.split(':')[0]:line.split('=',1)[1] for line in (exe.parent.parent/'CMakeCache.txt').read_text(encoding='utf-8').splitlines() if line.startswith('QUPATHXX_BUILD_') and ':BOOL=' in line}
assert flags['QUPATHXX_BUILD_CLASSIFIER_TRAINING']=='OFF'
# Valid linked project from the actual installed quantitative reader, not an
# intentionally malformed argument used to stand in for backend admission.
fixture=out/'fixture';fixture.mkdir(exist_ok=False)
source=fixture/'source.ppm';source.write_bytes(b'P6\n8 8\n255\n'+bytes([30,60,90])*64)
project=q.Project();image=project.add_image(str(source),'ppm');path=fixture/'input.qpx';project.save(str(path));project.document(image).server.close()
paths=[fixture/n for n in ('model.qrm','training.qtc','rows.qtt')]
cmd=[str(exe),'--train-raw-pixels',str(path),str(image),str(paths[0]),'--checkpoint',str(paths[1]),'--archive',str(paths[2])]
before=path.read_bytes();done=subprocess.run(cmd,capture_output=True,timeout=30)
(out/'backend-off.stdout').write_bytes(done.stdout);(out/'backend-off.stderr').write_bytes(done.stderr)
result=json.loads(done.stdout);assert done.returncode==1 and result['state']=='failed' and 'OpenCV' in result['error']
assert all(not p.exists() for p in paths) and path.read_bytes()==before
proof=dict(command=cmd,exit=done.returncode,result=result,flags=flags,source_project_unchanged=True,no_outputs=True,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),consumer=sys.executable)
(out/'backend-off-proof.json').write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8',newline='\n');print('Backend-OFF executable refuses training before raw IO/output; original project unchanged')
