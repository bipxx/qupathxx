from pathlib import Path
import base64,csv,hashlib,io,json,os,subprocess,sys,zipfile
r=Path(__file__).resolve().parents[2];w=r/'build/ci/scientific-grid-159';o=r/'build/object-properties-r5'
python=r/'build/ci/object-properties-r5-env/Scripts/python.exe'
assert not python.exists()
subprocess.run([str(r/'.venv/Scripts/python.exe'),'-m','venv',str(python.parents[1])],check=True)
(python.parents[1]/'Lib/site-packages/shared_test_dependencies.pth').write_text(str(r/'.venv/Lib/site-packages')+'\n',encoding='utf-8')
wheel,=list((o/'wheel-final').glob('*.whl'));stage=o/'stub-stage';stage.mkdir()
digest=lambda b:hashlib.sha256(b).hexdigest()
initial=digest(wheel.read_bytes())
with zipfile.ZipFile(wheel) as archive:archive.extractall(stage)
native,=list((stage/'qupathxx').glob('_native*.pyd'));native_hash=digest(native.read_bytes())
env=os.environ.copy();env['PYTHONPATH']=str(stage)
# Full package bytes from the new wheel supply the exact compiled binding.
# Stub generation runs before the sole final product installation.
argv=[str(python),'-X','utf8',str(w/'tools/generate_stubs.py')]
with (o/'stub-generation.log').open('x',encoding='utf-8') as f:subprocess.run(argv,cwd=w,env=env,stdout=f,stderr=subprocess.STDOUT,check=True)
original=o/'wheel-before-stubs';original.mkdir();retained=original/wheel.name;wheel.replace(retained)
with zipfile.ZipFile(retained) as archive:
    entries={n:archive.read(n) for n in archive.namelist()}
    for name in ('qupathxx/_native.pyi','qupathxx/__init__.pyi'):entries[name]=(w/'src'/name).read_bytes()
    record,=list(n for n in entries if n.endswith('.dist-info/RECORD'))
    output=io.StringIO(newline='');writer=csv.writer(output,lineterminator='\n')
    for name,data in entries.items():
        writer.writerow((name,'','') if name==record else (name,'sha256='+base64.urlsafe_b64encode(hashlib.sha256(data).digest()).decode().rstrip('='),len(data)))
    entries[record]=output.getvalue().encode()
    with zipfile.ZipFile(wheel,'w') as result:
        for info in archive.infolist():result.writestr(info,entries[info.filename])
assert digest(native.read_bytes())==native_hash
with zipfile.ZipFile(wheel) as archive:assert digest(archive.read('qupathxx/'+native.name))==native_hash
argv=[str(python),'-I','-m','pip','install','--ignore-installed','--no-index','--no-deps',str(wheel)]
with (o/'install.log').open('x',encoding='utf-8') as f:subprocess.run(argv,cwd=w,stdout=f,stderr=subprocess.STDOUT,check=True)
(o/'package.json').write_bytes((json.dumps(dict(initial_wheel_sha256=initial,final_wheel_sha256=digest(wheel.read_bytes()),native_sha256=native_hash,stub_generation_from_new_wheel=True,stub_generation_argv=[str(python),'-X','utf8',str(w/'tools/generate_stubs.py')],install_argv=argv,native_rebuild_for_stubs=False,final_product_installs=1),indent=2)+'\n').encode())
print('Generated current stubs, repackaged retained native bytes, installed final wheel once.',flush=True)
