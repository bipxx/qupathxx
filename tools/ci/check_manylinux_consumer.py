"""Run a real slide workflow from a qualified wheel in a fresh Linux consumer."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import zipfile


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def exercise_slide(q,slide,package):
    import numpy as np
    with tempfile.TemporaryDirectory(prefix='qpx-portable-consumer-') as folder:
        root=Path(folder);server=q.open_slide(str(slide.resolve()))
        try:
            doc=q.Document(server);obj=doc.add_rectangle(q.Rect(1000,1000,40,30),'Portable ROI μ')
            rgba=server.read_region(q.RegionRequest(1000,1000,0,40,30)).numpy()
            expected_mean=float(rgba[:,:,:3][rgba[:,:,3]>0].mean())
            assert abs(doc.mean_intensity(obj)-expected_mean)<1e-10
            request=q.AnalysisRequest(doc,obj,q.StainTransform([[1,0,0],[0,1,0]]))
            doc.keep_analysis(request,q.analyze(request))
            project=q.Project();image=project.add_document(doc,source_path=str(slide.resolve()),reader='openslide');source=root/'consumer.qpxproj';project.save(str(source))
            restored=q.Project.open(str(source));reopened=restored.document(image)
            assert reopened.annotation(obj).name=='Portable ROI μ' and len(reopened.analysis_records)==1
            assert abs(reopened.mean_intensity(obj)-expected_mean)<1e-10
            csv=root/'measurements.csv';q.save_measurements_csv(q.MeasurementTableRequest(reopened,q.MeasurementTableKind.ROI),str(csv))
            assert csv.stat().st_size>100
            region=root/'region.pam'
            done=subprocess.run([str(package/'qupathxx_region'),str(slide.resolve()),'1000','1000','0','40','30',str(region)],capture_output=True,timeout=30)
            assert done.returncode==0,done.stderr
            assert region.read_bytes().split(b'ENDHDR\n',1)[1]==rgba.tobytes()
            batch=subprocess.run([str(package/'qupathxx_batch'),str(source),str(root/'batch'),'--kind','stains'],capture_output=True,timeout=60)
            assert batch.returncode==0,batch.stderr
            checkpoints=list((root/'batch').rglob('*.qpxproj'));assert len(checkpoints)==1
            checkpoint=q.Project.open(str(checkpoints[0]));checkpoint_doc=checkpoint.document(image)
            assert len(checkpoint_doc.analysis_records)==2
            exported=subprocess.run([str(package/'qupathxx_batch'),'--export-csv',str(checkpoints[0]),str(root/'batch.csv')],capture_output=True,timeout=30)
            assert exported.returncode==0 and (root/'batch.csv').stat().st_size>100,exported.stderr
            checkpoint_doc.server.close();reopened.server.close()
        finally:server.close()
    return expected_mean


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--wheel',type=Path,required=True)
    parser.add_argument('--qualification',type=Path,required=True)
    parser.add_argument('--slide',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    report=json.loads(args.qualification.read_text(encoding='utf-8'))
    if not report.get('qualified') or report.get('policy')!='manylinux_2_28_x86_64':
        raise RuntimeError('requires a successfully qualified manylinux_2_28 wheel')
    if sha(args.wheel)!=report['qualified_wheel_sha256']:raise RuntimeError('consumer wheel differs from qualified artifact')
    if os.environ.get('GITHUB_SHA') and report.get('engine_commit')!=os.environ['GITHUB_SHA']:
        raise RuntimeError('qualification came from a different source commit')
    if os.environ.get('LD_LIBRARY_PATH') or os.environ.get('LD_PRELOAD'):raise RuntimeError('consumer must not use loader overrides')
    from packaging.utils import parse_wheel_filename
    tags=parse_wheel_filename(args.wheel.name)[3]
    interpreter='cp'+str(sys.version_info.major)+str(sys.version_info.minor)
    assert {t.interpreter for t in tags}=={interpreter} and {t.platform for t in tags}=={report['policy']}
    import qupathxx as q
    from qupathxx import _native
    native=Path(_native.__file__).resolve();package=native.parent
    assert 'site-packages' in native.parts and not q.has_gui
    assert q.has_geos and q.has_openslide and q.has_project and q.has_interchange and q.has_tiff and q.has_classifier_training
    with zipfile.ZipFile(args.wheel) as archive:
        for name,entry in report['elf_payloads'].items():
            assert name.startswith('qupathxx/') and '..' not in Path(name).parts
            path=package/Path(name).relative_to('qupathxx')
            assert sha(path)==entry['sha256']==hashlib.sha256(archive.read(name)).hexdigest()
        for notice in ['geos/COPYING','openslide-sdk/OpenSlide/COPYING.LESSER','opencv/LICENSE','libtiff/LICENSE.md']:
            assert len(archive.read('qupathxx/licenses/'+notice))>100
    expected={p.resolve() for p in package.glob('libgeos*.so*')}|{(package/'libopenslide.so.1').resolve()}
    mapped={Path(parts[5]).resolve() for line in Path('/proc/self/maps').read_text(encoding='utf-8').splitlines()
            if len(parts:=line.split(maxsplit=5))==6 and Path(parts[5]).name.startswith(('libgeos','libopenslide'))}
    assert len(expected)==3 and mapped==expected
    a=q.Roi.rectangle(q.Rect(0,0,10,10));b=q.Roi.rectangle(q.Rect(5,0,10,10))
    assert a.boolean_operation(b,q.BooleanOp.UNION).area==150
    expected_mean=exercise_slide(q,args.slide,package)
    result=dict(schema='qpx.manylinux-consumer/1',engine_commit=report.get('engine_commit'),policy=report['policy'],
        wheel_sha256=sha(args.wheel),native_sha256=sha(native),source_tree_sha256=q.build_environment().source_tree_sha256,
        python=sys.version,platform=platform.platform(),libc=platform.libc_ver(),
        mapped_runtimes=[p.name for p in sorted(mapped)],loader_overrides_used=False,geos_union_area=150,
        slide_sha256=sha(args.slide),real_slide_mean=expected_mean,unicode_annotation_reopened=True,
        retained_analysis_reopened=True,measurement_csv=True,native_region_exact=True,native_batch_checkpoint=True,native_batch_csv=True)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
