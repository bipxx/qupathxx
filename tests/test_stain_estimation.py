"""Independent SVD/quantiles, known absorbers, sample bounds and shared GUI use."""
import gc
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np
import pytest
import qupathxx as q

WHITE=np.array([255.,250.,245.])
TRUE=np.array([[.65,.70,.29],[.07,.99,.11]])
TRUE/=np.linalg.norm(TRUE,axis=1)[:,None]


def phantom(size=128):
    y,x=np.indices((size,size));a=np.where(x%8==0,0,.1+.6*x/(size-1));b=np.where(x%8==1,0,.1+.7*y/(size-1))
    od=a[...,None]*TRUE[0]+b[...,None]*TRUE[1]
    return np.rint(WHITE*10**(-od)).clip(0,255).astype(np.uint8)


def setup(size=128,parameters=None,roi=None,nucleus=False):
    rgb=phantom(size);doc=q.Document(q.Image(size,size,rgb.ravel().tolist()))
    shape=roi or q.Roi.rectangle(q.Rect(0,0,size,size))
    obj=doc.add_cell(shape,q.Roi.rectangle(q.Rect(size/4,size/4,size/2,size/2))) if nucleus else doc.add_roi(shape)
    request=q.StainEstimateRequest(doc,obj,parameters or q.StainEstimateParameters(),WHITE.tolist(),nucleus)
    return doc,obj,request,rgb


def retained(rgb,p,mask=None):
    """Tile traversal + independently written integer rejection reservoir."""
    keep=[];accepted=0;state=p.seed;mask=np.ones(rgb.shape[:2],bool) if mask is None else mask
    bits=(1<<64)-1
    for y in range(0,rgb.shape[0],p.tile_size):
        for x in range(0,rgb.shape[1],p.tile_size):
            block=rgb[y:y+p.tile_size,x:x+p.tile_size]
            od=np.maximum(0,-np.log10(np.maximum(1,block.astype(float))/WHITE))
            domain=mask[y:y+p.tile_size,x:x+p.tile_size] & np.all(od>=p.min_od,axis=2) & np.any(od!=0,axis=2)
            for point in od[domain]:
                accepted+=1
                if len(keep)<p.sample_limit:keep.append(point)
                else:
                    threshold=(1<<64)%accepted
                    while True:
                        state=(state+0x9e3779b97f4a7c15)&bits;z=state
                        z=((z^(z>>30))*0xbf58476d1ce4e5b9)&bits;z=((z^(z>>27))*0x94d049bb133111eb)&bits;z^=z>>31
                        if z>=threshold:break
                    index=z%accepted
                    if index<len(keep):keep[index]=point
    return np.array(keep),accepted


def reference(points,p):
    # SVD of the full sampled matrix independently validates the native 3x3
    # Jacobi eigensolver of its uncentered second moment.
    _,singular,axes=np.linalg.svd(points,full_matrices=False)
    normal=np.cross(axes[0],axes[1]);normal/=np.linalg.norm(normal)
    if normal[np.argmax(np.abs(normal))]<0:normal=-normal
    first=axes[:2].T@(axes[:2]@points.mean(axis=0));first/=np.linalg.norm(first)
    second=np.cross(normal,first);second/=np.linalg.norm(second)
    angles=np.arctan2(points@second,points@first)
    low,high=np.quantile(angles,[p.trim_quantile,1-p.trim_quantile])
    stains=np.array([first*np.cos(v)+second*np.sin(v) for v in [low,high]])
    stains=np.maximum(stains,0);stains/=np.linalg.norm(stains,axis=1)[:,None]
    if tuple(stains[0])<tuple(stains[1]):stains=stains[::-1]
    residual=np.cross(stains[0],stains[1]);residual/=np.linalg.norm(residual)
    return np.vstack([stains,residual]),singular**2/len(points)


@pytest.mark.parametrize('limit,seed',[(200000,0),(257,0),(257,42),(64,2**64-1)])
def test_matches_independent_svd_quantile_and_reservoir_reference(limit,seed):
    p=q.StainEstimateParameters();p.sample_limit=limit;p.seed=seed;p.tile_size=32
    doc,obj,request,rgb=setup(parameters=p);actual=q.estimate_stains(request);points,accepted=retained(rgb,p);expected,spectrum=reference(points,p)
    np.testing.assert_allclose(actual.stains,expected,rtol=0,atol=2e-11)
    np.testing.assert_allclose(actual.eigenvalues,spectrum,rtol=2e-11,atol=2e-14)
    assert actual.eligible==accepted and actual.sampled==min(accepted,limit) and actual.samples==128**2 and actual.tiles==16
    assert actual.residual_fraction==pytest.approx(spectrum[-1]/spectrum.sum(),abs=2e-14)
    assert actual.provenance is request.provenance and actual.provenance.object_id==obj
    assert q.stain_model_sha256(actual.transform)==actual.model_sha256


def test_known_stains_are_recovered_and_input_digest_matches_independent_rgba_tiles():
    p=q.StainEstimateParameters();p.tile_size=32;doc,obj,request,rgb=setup(parameters=p);result=q.estimate_stains(request)
    angles=np.degrees(np.arccos(np.clip(np.sum(np.array(result.stains[:2])*TRUE,axis=1),-1,1)))
    assert np.max(angles)<.5 and result.angle_degrees>20
    digest=hashlib.sha256(b'qpx.rgba-tiles/1');digest.update(struct.pack('<7Q',128,128,0,0,128,128,32))
    for y in range(0,128,32):
        for x in range(0,128,32):
            digest.update(struct.pack('<4Q',x,y,32,32));rgba=np.full((32,32,4),255,np.uint8);rgba[:,:,:3]=rgb[y:y+32,x:x+32];digest.update(rgba.tobytes())
    assert result.input_sha256==digest.hexdigest()


def test_model_identity_json_atomic_export_and_exact_transform_reload(tmp_path):
    doc,obj,request,_=setup();result=q.estimate_stains(request);text=q.stain_estimate_json(result);data=json.loads(text)
    assert data['schema']=='qpx.stain-estimate/1' and data['algorithm']==result.algorithm and data['model_sha256']==result.model_sha256
    model=q.StainTransform.from_normalized(data['stains'],data['white']);assert q.stain_model_sha256(model)==result.model_sha256
    digest=hashlib.sha256(b'qpx.stain-model/1')
    for value in [*data['white'],*(v for stain in data['stains'] for v in stain)]:digest.update(struct.pack('<d',0. if value==0 else value))
    assert digest.hexdigest()==result.model_sha256
    path=tmp_path/'\u8272.json';q.save_stain_estimate(result,str(path));assert path.read_bytes()==text.encode()
    directory=tmp_path/'occupied';directory.mkdir()
    with pytest.raises(RuntimeError):q.save_stain_estimate(result,str(directory))
    assert directory.is_dir() and not list(tmp_path.glob('*.qpx-tmp-*'))
    means=q.analyze(q.AnalysisRequest(doc,obj,model));same=q.analyze(q.AnalysisRequest(doc,obj,result.transform))
    np.testing.assert_array_equal(means.means,same.means)
    del doc,request;gc.collect();assert q.stain_estimate_json(result)==text
    with pytest.raises(AttributeError):result.model_sha256='changed'
    params=result.parameters;params.seed=999;assert result.parameters.seed==0


@pytest.mark.parametrize('field,value',[('min_od',-1),('min_od',float('nan')),('trim_quantile',.3),('min_angle_degrees',0),('max_residual_fraction',.3),('min_samples',7),('sample_limit',63),('sample_limit',1000001),('tile_size',15),('work_bytes',0),('max_read_pixels',0),('level',-2)])
def test_invalid_parameters_rejected(field,value):
    p=q.StainEstimateParameters();setattr(p,field,value)
    with pytest.raises(ValueError):p.validate()


def test_exact_work_budget_and_read_limits_are_enforced_before_worker():
    doc,obj,request,_=setup();p=request.parameters;p.work_bytes=request.estimated_work_bytes
    assert q.StainEstimateRequest(doc,obj,p,WHITE.tolist()).estimated_work_bytes==p.work_bytes
    p.work_bytes-=1
    with pytest.raises(ValueError,match='limits'):q.StainEstimateRequest(doc,obj,p,WHITE.tolist())
    p=q.StainEstimateParameters();p.max_read_pixels=100
    with pytest.raises(ValueError,match='limits'):q.StainEstimateRequest(doc,obj,p,WHITE.tolist())
    p.level=99
    with pytest.raises(IndexError):q.StainEstimateRequest(doc,obj,p)


@pytest.mark.parametrize('rgb_value',[0,80,255])
def test_constant_or_blank_regions_fail_without_synthetic_vectors(rgb_value):
    doc=q.Document(q.Image(32,32,[rgb_value]*32*32*3));obj=doc.add_rectangle(q.Rect(0,0,32,32))
    with pytest.raises(ValueError):q.estimate_stains(q.StainEstimateRequest(doc,obj))
    job=q.StainEstimateJob();assert job.request(q.StainEstimateRequest(doc,obj));status=job.wait(doc,obj)
    assert status.state=='failed' and status.error and job.result is None


def test_nucleus_and_ellipse_use_selected_roi_pixel_centers():
    doc,obj,request,_=setup(nucleus=True);result=q.estimate_stains(request)
    assert result.provenance.nucleus and result.samples==64**2 and result.roi.bounds.x==32
    y,x=np.indices((128,128));mask=((x+.5-64)/50)**2+((y+.5-64)/45)**2<=1
    roi=q.Roi.ellipse(q.Rect(14,19,100,90));doc,obj,request,rgb=setup(roi=roi);result=q.estimate_stains(request)
    assert result.samples==int(mask.sum()) and result.roi.kind==q.RoiKind.ELLIPSE


def test_jobs_reject_stale_selection_source_and_owner_identity_and_can_cancel():
    doc,obj,request,_=setup();job=q.StainEstimateJob();assert job.request(request);assert job.wait(doc,obj).state=='ready'
    expected=job.result.model_sha256;doc.rename_annotation(obj,'changed');assert job.poll(doc,obj).state=='stale' and job.result is None
    fresh=q.StainEstimateRequest(doc,obj,white=WHITE.tolist());assert job.request(fresh);job.cancel();assert job.wait(doc,obj).state=='cancelled' and job.result is None
    assert job.request(fresh);assert job.wait(doc,obj).state=='ready' and job.result.model_sha256==expected
    doc.server.close();assert job.poll(doc,obj).state=='stale' and job.result is None


@pytest.mark.skipif(not q.has_openslide,reason='native pyramid')
def test_auto_level_bounds_read_work_and_uses_real_coarse_pixels(tmp_path):
    tifffile=pytest.importorskip('tifffile');path=tmp_path/'stains.tif'
    with tifffile.TiffWriter(path) as writer:
        for i,size in enumerate([512,128]):writer.write(phantom(size),tile=(64,64),photometric='rgb',metadata=None,subfiletype=0 if i==0 else 1)
    server=q.open_slide(str(path));doc=q.Document(server);obj=doc.add_rectangle(q.Rect(0,0,512,512));p=q.StainEstimateParameters();p.max_read_pixels=20000
    request=q.StainEstimateRequest(doc,obj,p,WHITE.tolist());result=q.estimate_stains(request)
    assert request.level==result.level==1 and result.downsample==4 and result.samples==128**2
    points,accepted=retained(phantom(128),p);expected,_=reference(points,p)
    np.testing.assert_allclose(result.stains,expected,rtol=0,atol=2e-11)
    digest=hashlib.sha256(b'qpx.rgba-level-tiles/1');digest.update(struct.pack('<5Qd5Q',512,512,1,128,128,4.,0,0,128,128,256));digest.update(struct.pack('<6Q',0,0,0,0,128,128))
    rgba=np.full((128,128,4),255,np.uint8);rgba[:,:,:3]=phantom(128);digest.update(rgba.tobytes());assert result.input_sha256==digest.hexdigest()


@pytest.mark.skipif(not q.has_project,reason='native PPM factory')
def test_model_export_preserves_its_source_file_and_alias(tmp_path):
    rgb=phantom();path=tmp_path/'source.ppm';path.write_bytes(b'P6\n128 128\n255\n'+rgb.tobytes());project=q.Project();image=project.add_image(str(path),reader='ppm');doc=project.document(image);obj=doc.add_rectangle(q.Rect(0,0,128,128));result=q.estimate_stains(q.StainEstimateRequest(doc,obj,white=WHITE.tolist()))
    before=path.read_bytes()
    with pytest.raises(ValueError):q.save_stain_estimate(result,str(path))
    alias=tmp_path/'alias.ppm';os.link(path,alias)
    with pytest.raises(ValueError):q.save_stain_estimate(result,str(alias))
    assert path.read_bytes()==before


def test_worker_source_ownership_releases_on_python_owner_thread(tmp_path):
    script=r'''
import gc,sys
import qupathxx as q
rgb=[]
for y in range(64):
 for x in range(64):rgb.extend([50+x,50+y,100+x//2])
doc=q.Document(q.Image(64,64,rgb));obj=doc.add_rectangle(q.Rect(0,0,64,64));p=q.StainEstimateParameters();p.max_residual_fraction=.25
request=q.StainEstimateRequest(doc,obj,p);job=q.StainEstimateJob();assert job.request(request);del request;gc.collect();job.wait(doc,obj);del job,doc;gc.collect()
'''
    child=subprocess.run([sys.executable,'-I','-c',script],capture_output=True,text=True,timeout=20)
    assert child.returncode==0,child.stdout+child.stderr


@pytest.mark.skipif(not q.has_gui or os.environ.get('QUPATHXX_TEST_GUI')!='1',reason='desktop replay')
def test_gui_estimates_applies_and_analyzes_the_exact_native_model():
    doc,obj,request,_=setup();expected=q.estimate_stains(request);options=q.GuiOptions();options.analysis_white=WHITE.tolist()
    def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
        e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
    options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(8,q.GuiInputKind.ESTIMATE_STAINS_KEY),event(10,q.GuiInputKind.APPLY_STAIN_ESTIMATE_KEY,q.GuiInputWait.STAINS_ESTIMATED),event(12,q.GuiInputKind.ANALYZE_KEY,q.GuiInputWait.STAINS_APPLIED)]
    report=q.show(doc,frame_limit=40,options=options)
    assert report.stain_estimate_state=='ready' and not report.stain_estimate_error and report.stain_estimate_applied
    assert report.stain_estimate_result.model_sha256==expected.model_sha256 and report.analysis_state=='ready'
    direct=q.analyze(q.AnalysisRequest(doc,obj,expected.transform));np.testing.assert_array_equal(report.analysis_means,direct.means)


def test_estimation_and_job_wait_release_the_python_gil():
    import threading
    doc,obj,request,_=setup(size=1024);calls=[0];stop=threading.Event()
    def count():
        while not stop.is_set():calls[0]+=1
    thread=threading.Thread(target=count);thread.start()
    try:
        before=calls[0];q.estimate_stains(request);assert calls[0]>before
        job=q.StainEstimateJob();assert job.request(request);before=calls[0];assert job.wait(doc,obj).state=='ready' and calls[0]>before
    finally:stop.set();thread.join()


def test_request_identity_and_selected_id_are_reconciled_before_publication():
    doc,obj,request,_=setup();other=q.Document(doc.server);same_id=other.add_rectangle(q.Rect(0,0,128,128));assert same_id==obj
    job=q.StainEstimateJob();assert job.request(request) and not job.request(request)
    assert job.wait(other,same_id).state=='stale' and job.result is None
    assert job.request(request);assert job.wait(doc,0).state=='stale' and job.result is None


@pytest.mark.skipif(not q.has_project,reason='native project')
def test_estimated_model_used_in_analysis_survives_project_restart(tmp_path):
    doc,obj,request,_=setup();model=q.estimate_stains(request)
    analysis=q.AnalysisRequest(doc,obj,model.transform);result=q.analyze(analysis);record=doc.keep_analysis(analysis,result)
    project=q.Project();image=project.add_document(doc);path=tmp_path/'estimated.qpxproj';project.save(str(path))
    loaded=q.Project.open(str(path)).document(image);saved=loaded.analysis_record(record)
    restored=q.StainTransform.from_normalized(saved.result.stains,saved.result.white)
    assert q.stain_model_sha256(restored)==model.model_sha256
    np.testing.assert_array_equal(saved.result.means,result.means)
