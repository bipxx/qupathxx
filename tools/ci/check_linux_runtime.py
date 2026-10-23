"""Verify bundled Linux libraries and a relocated installed package without SDK paths."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def mapped_libraries():
    return {Path(parts[5]).resolve() for line in Path('/proc/self/maps').read_text(encoding='utf-8').splitlines()
            if len(parts:=line.split(maxsplit=5))==6 and Path(parts[5]).name.startswith(('libgeos','libopenslide'))}


def geometry(q):
    a=q.Roi.rectangle(q.Rect(0,0,10,10));b=q.Roi.rectangle(q.Rect(5,0,10,10))
    assert a.boolean_operation(b,q.BooleanOp.UNION).area==150


def relocated_child(path):
    sys.path.insert(0,str(path))
    import qupathxx as q
    from qupathxx import _native
    native=Path(_native.__file__).resolve();assert native.is_relative_to(path.resolve())
    geometry(q)
    expected={p.resolve() for p in native.parent.glob('libgeos*.so*')}|{(native.parent/'libopenslide.so.1').resolve()}
    assert len(expected)==3 and mapped_libraries()==expected
    print(json.dumps(dict(native=str(native),mapped=[str(p) for p in sorted(expected)],boolean_area=150)))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel',type=Path,required=True)
    parser.add_argument('--geos-sdk',type=Path,required=True)
    parser.add_argument('--openslide-sdk',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='linux' or os.environ.get('LD_LIBRARY_PATH'):
        parser.error('requires Linux with LD_LIBRARY_PATH unset')
    import qupathxx as q
    from qupathxx import _native
    native=Path(_native.__file__).resolve();package=native.parent
    capi=args.geos_sdk/'lib/libgeos_c.so.1'
    dynamic=subprocess.check_output(['readelf','-d',str(capi)],text=True)
    needed=re.findall(r'Shared library: \[(libgeos\.so\.[^\]]+)\]',dynamic)
    assert len(needed)==1 and re.search(r'\((?:RUNPATH|RPATH)\).*\[\$ORIGIN\]',dynamic)
    expected={'libgeos_c.so.1':capi,needed[0]:args.geos_sdk/'lib'/needed[0],
              'libopenslide.so.1':args.openslide_sdk/'lib/libopenslide.so.1'}
    with zipfile.ZipFile(args.wheel) as archive:
        assert hashlib.sha256(archive.read('qupathxx/'+native.name)).hexdigest()==sha(native)
        for name,source in expected.items():
            assert sha(package/name)==sha(source)==hashlib.sha256(archive.read('qupathxx/'+name)).hexdigest()
        assert archive.read('qupathxx/licenses/geos/COPYING')==(args.geos_sdk/'share/licenses/geos/COPYING').read_bytes()
    geometry(q)
    assert mapped_libraries()=={(package/name).resolve() for name in expected}
    clean=os.environ.copy()
    for name in ['LD_LIBRARY_PATH','LD_PRELOAD','PYTHONPATH','PYTHONHOME']:clean.pop(name,None)
    with tempfile.TemporaryDirectory(prefix='qpx-linux-runtime-') as folder:
        root=Path(folder);moved=root/'relocated package';shutil.copytree(package,moved/'qupathxx')
        child=subprocess.run([sys.executable,'-I',str(Path(__file__).resolve()),'--relocated-child',str(moved)],
                             env=clean,capture_output=True,text=True,timeout=30)
        assert child.returncode==0,child.stdout+child.stderr
        child_receipt=json.loads(child.stdout)
        ppm=root/'input.ppm';ppm.write_bytes(b'P6\n2 1\n255\n'+bytes([40,80,160,80,120,200]))
        pam=root/'output.pam'
        region=subprocess.run([str(moved/'qupathxx/qupathxx_region'),'--thumbnail',str(ppm),'1','1',str(pam),'--source-pixels','2'],
                              env=clean,capture_output=True,timeout=30)
        assert region.returncode==0,region.stderr
        assert pam.read_bytes().split(b'ENDHDR\n',1)[1]==bytes([60,100,180,255])
        project=q.Project();doc=q.Document(q.Image(32,24,[16]*32*24*3));doc.add_rectangle(q.Rect(1,2,20,16),'Portable ROI')
        project.add_document(doc);source=root/'input.qpxproj';project.save(str(source))
        batch=subprocess.run([str(moved/'qupathxx/qupathxx_batch'),str(source),str(root/'batch'),'--kind','stains'],
                             env=clean,capture_output=True,timeout=60)
        assert batch.returncode==0,batch.stderr
        checkpoints=list((root/'batch').rglob('*.qpxproj'))
        assert len(checkpoints)==1,batch.stdout.decode('utf-8',errors='replace')
        restored=q.Project.open(str(checkpoints[0]))
        assert len(restored.document(restored.images[0].id).analysis_records)==1
        csv=root/'batch.csv'
        exported=subprocess.run([str(moved/'qupathxx/qupathxx_batch'),'--export-csv',str(checkpoints[0]),str(csv)],
                                env=clean,capture_output=True,timeout=30)
        assert exported.returncode==0 and csv.stat().st_size>100,exported.stderr
    receipt=dict(schema='qpx.linux-runtime/1',source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=sha(native),runtimes={name:sha(package/name) for name in expected},
        mapped_from_package=True,geos_license_matches_sdk=True,relocated_python_geometry=True,
        relocated_region_pixels=True,relocated_batch=True,ld_library_path_used=False,
        relocated_child=child_receipt,
        scope='Installed and relocated Linux package resolves its bundled GEOS/OpenSlide runtimes without SDK search paths. System glibc/libstdc++ compatibility and manylinux policy remain separate gates.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(receipt,indent=2))


if __name__=='__main__':
    if len(sys.argv)==3 and sys.argv[1]=='--relocated-child':relocated_child(Path(sys.argv[2]))
    else:main()
