"""Owner serialization for older Document-free Python job controllers."""
from concurrent.futures import ThreadPoolExecutor
import csv
import gc
import hashlib
import io
from pathlib import Path
import subprocess
import sys
import threading

import numpy as np
import pytest
import qupathxx as q


JOBS=[q.FileFingerprintJob,q.CellTableExportJob,q.MeasurementTableExportJob,
      q.ThumbnailJob,q.PixelStreamReadJob]
KINDS=[t.__name__ for t in JOBS]


@pytest.mark.parametrize('job_type',JOBS,ids=lambda t:t.__name__)
@pytest.mark.parametrize('operation',['busy','poll','wait'])
def test_snapshot_controller_rejects_foreign_collection(job_type,operation):
    job=job_type()
    def foreign():
        value=getattr(job,operation)
        return value() if callable(value) else value
    with ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):
            pool.submit(foreign).result(timeout=10)
    assert job.poll().state=='idle'


class Case:
    def __init__(self,job,start,verify):
        self.job,self.start,self.verify=job,start,verify


def case_for(kind,tmp_path,*,slow=False):
    job=getattr(q,kind)()
    if kind=='FileFingerprintJob':
        block=b'snapshot-job-owner\x00'
        data=block*((64*1024*1024//len(block)) if slow else 262144)
        path=tmp_path/'input.bin';path.write_bytes(data)
        def verify(stats):
            assert stats.state=='succeeded' and job.result.sha256==hashlib.sha256(data).hexdigest()
            assert path.read_bytes()==data
        return Case(job,lambda:job.request(str(path)),verify)
    if kind=='ThumbnailJob':
        pixels=np.full((1024,1536,3),[20,40,60],dtype=np.uint8)
        server=q.Document(q.Image.from_numpy(pixels)).server
        def verify(stats):
            assert stats.state=='succeeded'
            actual=job.result().pixels.numpy()
            np.testing.assert_array_equal(actual,np.broadcast_to([20,40,60,255],actual.shape))
        return Case(job,lambda:job.request(server),verify)
    if kind=='PixelStreamReadJob':
        from test_pixel_stream_read_job import FIXTURE
        path=tmp_path/'source.qps';data=FIXTURE.read_bytes();path.write_bytes(data)
        def verify(stats):
            assert stats.state=='ready' and stats.block is not None and not stats.error
            assert stats.block.sha256==q.PixelStreamFile(str(path)).read_block(0).sha256
            assert path.read_bytes()==data
        return Case(job,lambda:job.open(str(path)),verify)
    if kind=='CellTableExportJob':
        from test_cell_tables import setup
        doc,_,_,_=setup();request=q.CellTableRequest(doc)
        expected=q.cell_measurements_csv(request)
    else:
        from test_measurement_tables import setup
        doc,_,_,_=setup();request=q.MeasurementTableRequest(doc,q.MeasurementTableKind.ROI)
        expected=q.measurements_csv(request)
    assert request.row_count>0
    outputs=[]
    def start():
        path=tmp_path/('table-'+str(len(outputs))+'.csv');outputs.append(path)
        return job.request(request,str(path))
    def verify(stats):
        assert stats.state=='succeeded' and stats.rows==request.row_count
        assert outputs[-1].read_bytes()==expected.encode('utf-8')
        assert len(list(csv.DictReader(io.StringIO(expected))))==request.row_count
    return Case(job,start,verify)


@pytest.mark.parametrize('kind',KINDS)
def test_snapshot_request_is_owner_only(tmp_path,kind):
    case=case_for(kind,tmp_path)
    with ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):
            pool.submit(case.start).result(timeout=10)
    assert case.job.poll().state=='idle'
    assert case.start();case.verify(case.job.wait())


@pytest.mark.parametrize('kind',KINDS)
@pytest.mark.parametrize('action',['poll','cancel'])
def test_snapshot_wait_serializes_collection_and_allows_cancel(tmp_path,kind,action):
    check_liveness=kind=='FileFingerprintJob' and action=='poll'
    case=case_for(kind,tmp_path,slow=check_liveness);assert case.start()
    gate=threading.Event();done=threading.Event();errors=[]
    def other():
        gate.wait()
        try:
            if action=='cancel':case.job.cancel()
            else:
                with pytest.raises(RuntimeError,match='owner thread'):case.job.poll()
        except BaseException as error:errors.append(error)
        finally:done.set()
    thread=threading.Thread(target=other);thread.start();interval=sys.getswitchinterval()
    try:
        sys.setswitchinterval(1);gate.set();stats=case.job.wait();ran_during_wait=done.is_set()
    finally:
        sys.setswitchinterval(interval);gate.set();thread.join(10)
    assert not thread.is_alive() and not errors
    # An already completed future may return before another thread is scheduled.
    # The bounded larger hash exercises shared-bridge GIL liveness; the other
    # cases check serialization/cancellation whether work finishes early or late.
    if check_liveness:assert ran_during_wait
    if action=='poll':case.verify(stats)
    else:assert stats.state in ('succeeded','ready','cancelled'),stats.error
    busy=case.job.busy
    assert not (busy() if callable(busy) else busy)
    assert case.start();case.verify(case.job.wait())
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


def destroy_on_foreign_thread(kind,tmp_path):
    case=case_for(kind,tmp_path);assert case.start();box=[case.job];case.job=None
    del case;gc.collect()
    def retire():
        job=box.pop();del job;gc.collect()
    thread=threading.Thread(target=retire);thread.start();thread.join(15)
    assert not thread.is_alive() and not box
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize('kind',KINDS)
def test_snapshot_job_foreign_final_reference_joins(tmp_path,kind):
    code="import sys;from pathlib import Path;sys.path.insert(0,sys.argv[1]);import test_snapshot_job_waits as t;t.destroy_on_foreign_thread(sys.argv[2],Path(sys.argv[3]));print('joined')"
    child=subprocess.run([sys.executable,'-I','-X','utf8','-c',code,str(Path(__file__).parent),kind,str(tmp_path)],capture_output=True,text=True,encoding='utf-8',timeout=30)
    assert child.returncode==0 and 'joined' in child.stdout,child.stdout+child.stderr


@pytest.mark.parametrize('job_type',JOBS,ids=lambda t:t.__name__)
def test_snapshot_controller_allows_foreign_cancel(job_type):
    job=job_type()
    with ThreadPoolExecutor(max_workers=1) as pool:
        pool.submit(job.cancel).result(timeout=10)
    assert job.poll().state=='idle'


@pytest.mark.parametrize('kind,operation',[
    ('FileFingerprintJob','result'),('FileFingerprintJob','association'),
    ('FileFingerprintJob','request_source'),('ThumbnailJob','result'),
    ('PixelStreamReadJob','read'),('PixelStreamReadJob','verify'),
    ('PixelStreamReadJob','close'),('PixelStreamReadJob','open_link'),
])
def test_snapshot_remaining_entry_points_are_owner_only(kind,operation):
    job=getattr(q,kind)()
    source=q.Document(q.Image.demo()).server.reader_source
    from test_pixel_stream_read_job import FIXTURE
    link=q.prepare_pixel_stream_link(str(FIXTURE)) if operation=='open_link' else None
    def foreign():
        value=getattr(job,operation)
        if operation=='request_source':return value(source)
        if operation=='read':return value(0)
        if operation=='open_link':return value(link)
        return value() if callable(value) else value
    with ThreadPoolExecutor(max_workers=1) as pool:
        with pytest.raises(RuntimeError,match='owner thread'):
            pool.submit(foreign).result(timeout=10)
    assert job.poll().state=='idle'
