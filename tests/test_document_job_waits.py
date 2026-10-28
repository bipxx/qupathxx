"""Live Document checks stay under the GIL; only native readiness releases it."""
import concurrent.futures
import gc
from pathlib import Path
import subprocess
import sys
import threading

import numpy as np
import pytest
import qupathxx as q

KINDS=('AnalysisJob','TissueJob','CellJob','StainEstimateJob','ObjectClassifierJob','PixelStreamExportJob')
SELECTED=set(KINDS[:4])


def idle(kind):
    doc=q.Document(q.Image(8,8,[80]*8*8*3));obj=doc.add_rectangle(q.Rect(0,0,8,8))
    return doc,obj,getattr(q,kind)()


def call(job,kind,method,doc,obj):
    return getattr(job,method)(doc,obj) if kind in SELECTED else getattr(job,method)(doc)


@pytest.mark.parametrize('kind',KINDS)
@pytest.mark.parametrize('operation',('busy','poll','wait'))
def test_live_document_job_rejects_foreign_controller_use(kind,operation):
    doc,obj,job=idle(kind)
    def foreign():
        if operation=='busy':return job.busy
        return call(job,kind,operation,doc,obj)
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):
            pool.submit(foreign).result(timeout=5)
    assert not job.busy and call(job,kind,'poll',doc,obj).state=='idle'


@pytest.mark.parametrize('kind',KINDS)
def test_live_document_job_allows_foreign_cancellation(kind):
    doc,obj,job=idle(kind)
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:pool.submit(job.cancel).result(timeout=5)
    assert not job.busy
    assert call(job,kind,'wait',doc,obj).state in ('idle','cancelled')


class Case:
    def __init__(self,kind,doc,obj,job,start,outputs=None):
        self.kind,self.doc,self.obj,self.job,self.start=kind,doc,obj,job,start
        self.outputs=outputs if outputs is not None else []
    def wait(self):return call(self.job,self.kind,'wait',self.doc,self.obj)


def case_for(kind,tmp_path):
    if kind in KINDS[4:] and not q.has_classifier_training:pytest.skip('optional classifier training backend')
    job=getattr(q,kind)()
    if kind=='StainEstimateJob':
        from test_stain_estimation import setup,WHITE
        doc,obj,_,_=setup(size=1024)
        return Case(kind,doc,obj,job,lambda:job.request(q.StainEstimateRequest(doc,obj,white=WHITE.tolist())))
    if kind=='ObjectClassifierJob':
        from test_object_classifier import workspace,parameters
        training,labels=workspace();parameters_=parameters();parameters_.trees=512
        model=q.train_object_forest(q.extract_object_features(q.ObjectFeatureRequest(training,labels)),parameters_)
        doc=q.Document(q.Image(32,32,[80]*32*32*3));doc.set_history_limits(0,0)
        roi=q.Roi.rectangle(q.Rect(1,1,2,2));doc.execute([q.ObjectEdit.create_geometry(roi) for _ in range(20000)])
        ids=[a.id for a in doc.annotations]
        return Case(kind,doc,ids[0],job,lambda:job.predict(q.ObjectFeatureRequest(doc,ids),model))
    if kind=='PixelStreamExportJob':
        from test_pixel_prediction_windows import setup
        from test_pixel_stream_export_job import options
        _,_,model=setup();size=1024;rgb=np.full((size,size,3),80,dtype=np.uint8)
        doc=q.Document(q.dense_image_server(q.Image.from_numpy(rgb)));obj=doc.add_rectangle(q.Rect(0,0,size,size))
        paths=[]
        def start():
            path=tmp_path/('stream-'+str(len(paths))+'.qps');paths.append(path)
            return job.request(doc,obj,model,str(path),options(32))
        return Case(kind,doc,obj,job,start,paths)
    size=4096 if kind=='AnalysisJob' else 1024
    rgb=np.full((size,size,3),80,dtype=np.uint8)
    doc=q.Document(q.dense_image_server(q.Image.from_numpy(rgb)));obj=doc.add_rectangle(q.Rect(0,0,size,size))
    transform=q.StainTransform([[1,0,0],[0,1,0]])
    if kind=='AnalysisJob':return Case(kind,doc,obj,job,lambda:job.request(q.AnalysisRequest(doc,obj,transform)))
    if kind=='TissueJob':return Case(kind,doc,obj,job,lambda:job.request(q.TissueRequest(doc,obj,transform)))
    parameters=q.CellParameters();parameters.feature=q.TissueFeature.OPTICAL_DENSITY_SUM;parameters.threshold=.8;parameters.cell_expansion=0
    return Case(kind,doc,obj,job,lambda:job.request(q.CellRequest(doc,obj,transform,parameters)))


@pytest.mark.parametrize('kind',KINDS)
@pytest.mark.parametrize('action',('edit','cancel'))
def test_live_document_wait_releases_gil_and_rechecks_before_delivery(tmp_path,kind,action):
    case=case_for(kind,tmp_path);assert case.start();revision=case.doc.revision
    ready,gate,done=threading.Event(),threading.Event(),threading.Event();errors=[]
    def change():
        ready.set();gate.wait()
        try:
            if action=='edit':case.doc.rename_annotation(case.obj,'Edited while native wait released the GIL')
            else:case.job.cancel()
        except BaseException as error:errors.append(error)
        finally:done.set()
    thread=threading.Thread(target=change);thread.start();assert ready.wait(5)
    interval=sys.getswitchinterval()
    try:
        # Prevent a Python bytecode switch between opening the gate and entering
        # wait. The waiting native call itself must let the producer run.
        sys.setswitchinterval(1);gate.set();stats=case.wait();ran_during_wait=done.is_set()
    finally:
        sys.setswitchinterval(interval);gate.set();thread.join(10)
    assert not thread.is_alive() and not errors and ran_during_wait
    assert not case.job.busy and case.doc.revision==revision+(action=='edit')
    if kind=='PixelStreamExportJob':
        assert stats.state in ('saved','cancelled')
        if action=='edit':assert not stats.current
        if stats.state=='cancelled':assert not case.outputs[-1].exists()
        else:assert q.PixelStreamFile(str(case.outputs[-1])).declared_summary.sha256==stats.summary.sha256
    else:
        assert stats.state==('stale' if action=='edit' else 'cancelled') and case.job.result is None
    assert case.start();fresh=case.wait()
    assert fresh.state==('saved' if kind=='PixelStreamExportJob' else 'ready'),fresh.error
    assert case.doc.revision==revision+(action=='edit')


@pytest.mark.parametrize('kind',KINDS)
def test_live_document_request_is_owner_only(tmp_path,kind):
    case=case_for(kind,tmp_path)
    with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):pool.submit(case.start).result(timeout=10)
    assert not case.job.busy


def destroy_on_foreign_thread(kind,tmp_path):
    case=case_for(kind,tmp_path);assert case.start();box=[case.job];case.job=None;del case;gc.collect()
    def retire():
        job=box.pop();del job;gc.collect()
    thread=threading.Thread(target=retire);thread.start();thread.join(15)
    assert not thread.is_alive() and not box
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize('kind',KINDS)
def test_live_document_job_shutdown_releases_gil_and_joins(tmp_path,kind):
    if kind in KINDS[4:] and not q.has_classifier_training:pytest.skip('optional classifier training backend')
    code="import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);import test_document_job_waits as t;t.destroy_on_foreign_thread(sys.argv[2],Path(sys.argv[3]));print('joined')"
    child=subprocess.run([sys.executable,'-I','-X','utf8','-c',code,str(Path(__file__).parent),kind,str(tmp_path)],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert child.returncode==0 and 'joined' in child.stdout,child.stdout+child.stderr
