"""Native contour CLI with independent installed Python geometry oracle."""
import json
import os
from pathlib import Path
import signal
import subprocess
import pytest
import qupathxx as q
from test_raw_pixel_cli import source, arguments, run, final, require_training, CLI
from test_raw_pixel_python import tiff

pytestmark=pytest.mark.skipif(not q.has_tiff or not q.has_project or not q.has_geos,reason='typed TIFF/project and GEOS')


@pytest.fixture
def case(source,tmp_path):
    require_training();final(run(*arguments(source,tmp_path)))
    path,image,_,_,pixels=source
    project=q.Project.open(str(path));doc=project.document(image);root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,8,6)))
    augmented=tmp_path/'contour-input.qpx';project.save(str(augmented))
    model=tmp_path/'模型.qrm'
    yield augmented,image,doc,root,pixels,model
    doc.server.close()


def command(case,output):
    path,image,_,root,_,model=case
    return ['--contour-raw-pixels',path,image,model,output,'--x-axis','0,8,7,2','--y-axis','0,6,5,2','--tile-width',2,'--tile-height',2,'--work-bytes',16*1024**2,'--object',root]


@pytest.mark.parametrize('crop',[False,True])
def test_cli_streamed_geometry_saves_new_project_without_mutating_inputs(case,tmp_path,crop):
    path,image,doc,root,pixels,model_path=case;output=tmp_path/'contours.qpx'
    originals={p:p.read_bytes() for p in [path,pixels,model_path]}
    args=command(case,output)
    region=q.RawPixelStreamRegion(1,1,3,2) if crop else None
    if crop:args+=['--region','1,1,3,2']
    result=final(run(*args,'--name-prefix','CLI contour'))
    assert result['schema']=='qpx.raw-pixel-contours/1' and result['state']=='saved'
    model=q.load_raw_pixel_classifier(str(model_path));lattice=q.ScientificRawLattice(8,6,q.ScientificGridAxis(0,8,7,2),q.ScientificGridAxis(0,6,5,2))
    stream=q.RawPixelStreamOptions();stream.tile_width=stream.tile_height=2;stream.work_bytes=16*1024**2
    request=q.RawPixelTiledPredictionRequest(doc,model,lattice,stream,region,root);expected=q.trace_raw_pixel_stream(request)
    assert result['model_sha256']==model.sha256 and result['request_sha256']==request.sha256
    assert result['input_sha256']==expected.input_sha256 and result['prediction_sha256']==expected.prediction_sha256
    assert result['class_counts']==expected.class_counts and result['samples']==expected.completed_samples
    assert result['completed_windows']==result['total_windows']==request.total_windows
    saved=q.Project.open(str(output))
    try:
        current=saved.document(image);assert current.children(root)==result['object_ids']
        assert result['components']==len(result['object_ids'])==len(expected.contours.components)>0
        for obj,component in zip(result['object_ids'],expected.contours.components):
            annotation=current.annotation(obj)
            assert annotation.geometry.equals(component.geometry) and annotation.parent_id==root
            assert annotation.class_name==model.forest.classes[component.label-1] and annotation.name.startswith('CLI contour ')
    finally:saved.document(image).server.close()
    assert doc.object_count==3
    assert all(p.read_bytes()==data for p,data in originals.items())


@pytest.mark.parametrize('target',['project','pixels','model','existing','sidecar'])
def test_cli_streamed_contours_refuse_collisions(case,tmp_path,target):
    path,_,_,_,pixels,model=case
    output={'project':path,'pixels':pixels,'model':model,'sidecar':Path(str(path)+'-wal'),'existing':tmp_path/'sentinel.qpx'}[target]
    if target=='existing':output.write_bytes(b'keep prior output')
    original={p:p.read_bytes() for p in [path,pixels,model]}
    before=output.read_bytes() if output.exists() else None
    result=run(*command(case,output));assert result.returncode==2,result.stdout+result.stderr
    assert all(p.read_bytes()==data for p,data in original.items())
    assert (output.read_bytes() if output.exists() else None)==before


@pytest.mark.parametrize('extra',[['--resume','unused.qws'],['--replace-existing'],['--file-bytes','1024'],['--max-new-windows','1'],['--name-prefix','']])
def test_cli_streamed_contours_reject_inapplicable_flags(case,tmp_path,extra):
    output=tmp_path/'invalid.qpx';done=run(*command(case,output),*extra)
    assert done.returncode==2,done.stdout+done.stderr
    assert not output.exists()


@pytest.mark.parametrize('flag',['--total-work-bytes','--publication-work-bytes','--snapshot-bytes','--project-bytes'])
def test_cli_streamed_contours_quota_failure_leaves_no_project(case,tmp_path,flag):
    path,_,_,_,pixels,model=case;original={p:p.read_bytes() for p in [path,pixels,model]};output=tmp_path/'refused.qpx'
    report=final(run(*command(case,output),flag,1),1)
    assert report['state']=='failed' and report['error']
    assert not output.exists() and not list(tmp_path.glob('.qpx-save-new-*'))
    assert all(p.read_bytes()==data for p,data in original.items())


def test_cli_streamed_contours_interrupt_joins_and_does_not_publish(case,tmp_path):
    _,_,_,_,_,model=case
    image_path=tmp_path/'large.tif';tiff(image_path,width=1024,height=768)
    project=q.Project();image=project.add_image(str(image_path),'tiff');path=tmp_path/'large.qpx';project.save(str(path));project.document(image).server.close();original=path.read_bytes();output=tmp_path/'cancelled.qpx'
    extra={'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=='nt' else {}
    child=subprocess.Popen([str(CLI),'--contour-raw-pixels',str(path),str(image),str(model),str(output),'--x-axis','0,1024,896,2','--y-axis','0,768,640,2','--tile-width','16','--tile-height','16','--work-bytes',str(16*1024**2)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',**extra)
    try:
        initial=json.loads(child.stdout.readline());assert initial['state']=='running' and initial['completed_windows']<initial['total_windows'] and child.poll() is None
        child.send_signal(signal.CTRL_BREAK_EVENT if os.name=='nt' else signal.SIGINT)
        stdout,stderr=child.communicate(timeout=40)
        assert child.returncode==130,stdout+stderr
        assert json.loads(stdout.splitlines()[-1])['state']=='cancelled'
        assert not output.exists() and path.read_bytes()==original
        image_path.rename(tmp_path/'released.tif')
    finally:
        if child.poll() is None:child.kill();child.communicate(timeout=20)


def test_cli_no_clip_preserves_center_mask_but_omits_roi_intersection(case,tmp_path):
    path,image,_,root,pixels,model_path=case
    project=q.Project.open(str(path));doc=project.document(image);doc.update_roi(root,q.Roi.rectangle(q.Rect(1.1,1.3,4.2,3.1)))
    partial=tmp_path/'partial-roi.qpx';project.save(str(partial));current=(partial,image,doc,root,pixels,model_path)
    try:
        clipped=final(run(*command(current,tmp_path/'clipped.qpx')))
        uncut=final(run(*command(current,tmp_path/'uncut.qpx'),'--no-clip'))
        assert clipped['class_counts']==uncut['class_counts']
        a=q.Project.open(str(tmp_path/'clipped.qpx'));b=q.Project.open(str(tmp_path/'uncut.qpx'))
        try:
            area=lambda p,ids:sum(p.document(image).annotation(i).geometry.area for i in ids)
            assert area(b,uncut['object_ids'])>area(a,clipped['object_ids'])>0
        finally:a.document(image).server.close();b.document(image).server.close()
    finally:doc.server.close()
