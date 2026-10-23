"""Single native worker, captured authority, atomic file results and owner lifetime."""
import gc
import subprocess
import sys
import time
from pathlib import Path
import numpy as np
import pytest
import qupathxx as q
from test_pixel_prediction_windows import setup

pytestmark=pytest.mark.skipif(not q.has_classifier_training,reason='optional training backend')


def options(block=16):
    o=q.PixelStreamOptions();o.block_size=block
    p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;o.prediction=p
    return o


@pytest.mark.parametrize('block',[16,256])
@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
def test_export_job_completes_exact_file_without_document_changes(tmp_path,block,window):
    doc,root,model=setup();revision=doc.revision;path=tmp_path/'native.qps';job=q.PixelStreamExportJob()
    assert job.poll(doc).state=='idle' and not job.busy
    rect=q.Rect(*window) if window else None
    assert job.request(doc,root,model,str(path),options(block),rect)
    assert not job.request(doc,root,model,str(path),options(block),rect)
    stats=job.wait(doc)
    assert stats.state=='saved' and not stats.error and stats.current and not job.busy and stats.generation==1
    assert stats.completed_blocks==stats.total_blocks==stats.summary.blocks and stats.estimated_work_bytes<=8*1024*1024
    file=q.PixelStreamFile(str(path));file.verify();assert file.verified and file.windowed==bool(window)
    assert stats.summary.sha256==file.declared_summary.sha256 and stats.valid_pixels==file.declared_summary.valid_pixels
    full=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,window=rect))
    assert stats.summary.class_counts==full.class_counts and stats.classified_pixels==full.classified_pixels
    job.cancel();assert job.poll(doc).state=='saved' and doc.revision==revision
    snapshot=stats.summary;del job;gc.collect();assert snapshot.sha256==file.declared_summary.sha256


def test_failed_job_can_restart_and_preserves_existing_file(tmp_path):
    doc,root,model=setup();path=tmp_path/'existing.qps';path.write_bytes(b'unrelated file');job=q.PixelStreamExportJob()
    assert job.request(doc,root,model,str(path),options(),replace_existing=True)
    failed=job.wait(doc);assert failed.state=='failed' and failed.error and failed.summary is None
    assert path.read_bytes()==b'unrelated file' and failed.completed_blocks==0
    output=tmp_path/'fresh.qps';assert job.request(doc,root,model,str(output),options())
    saved=job.wait(doc);assert saved.state=='saved' and saved.generation==2 and saved.summary
    assert failed.state=='failed' and failed.summary is None


def test_cancel_after_worker_commit_before_owner_poll_reports_saved(tmp_path):
    doc,root,model=setup();path=tmp_path/'committed.qps';job=q.PixelStreamExportJob()
    assert job.request(doc,root,model,str(path),options())
    deadline=time.monotonic()+15
    while not path.exists() and time.monotonic()<deadline:time.sleep(.001)
    assert path.exists();job.cancel();stats=job.wait(doc)
    assert stats.state=='saved' and stats.summary and not stats.error
    q.PixelStreamFile(str(path)).verify()


def test_captured_export_is_historical_after_document_edits(tmp_path):
    doc,root,model=setup();job=q.PixelStreamExportJob();path=tmp_path/'history.qps'
    assert job.request(doc,root,model,str(path),options())
    doc.rename_annotation(root,'edited');stats=job.wait(doc)
    assert stats.state=='saved' and not stats.current
    block=q.PixelStreamFile(str(path)).read_block(0)
    request=q.PixelPredictionRequest(doc,root,model,window=block.bounds)
    assert not block.matches(request)


@pytest.mark.parametrize('replace',[False,True])
def test_cancel_cleans_temporary_and_keeps_valid_old_file(tmp_path,replace):
    doc,root,model=setup();path=tmp_path/'cancel.qps';before=None
    if replace:
        q.save_pixel_stream(q.PixelPredictionStream(doc,root,model,options()),str(path));before=path.read_bytes()
    image=q.Document(q.Image.from_numpy(np.full((2048,2048,3),80,np.uint8)))
    target=image.add_roi(q.Roi.rectangle(q.Rect(0,0,2048,2048)));job=q.PixelStreamExportJob()
    assert job.request(image,target,model,str(path),options(),replace_existing=replace)
    job.cancel();stats=job.wait(image)
    assert stats.state=='cancelled' and stats.summary is None and not job.busy
    assert (path.read_bytes()==before) if replace else not path.exists()
    assert not list(tmp_path.glob('*.qpx-tmp-*'))
    assert job.request(doc,root,model,str(tmp_path/'retry.qps'),options());assert job.wait(doc).state=='saved'


@pytest.mark.parametrize('limit',[0,127,(1<<46)+1])
def test_bad_limit_is_synchronous_and_leaves_idle_job(tmp_path,limit):
    doc,root,model=setup();job=q.PixelStreamExportJob()
    with pytest.raises(ValueError):job.request(doc,root,model,str(tmp_path/'none.qps'),max_bytes=limit)
    assert not job.busy and job.poll(doc).state=='idle' and not list(tmp_path.iterdir())


def test_worker_finishes_with_only_native_source_model_owners(tmp_path):
    # A subprocess bounds any ownership/GIL shutdown regression.
    script='''
import gc,sys,numpy as np,qupathxx as q
sys.path.insert(0,sys.argv[1])
from test_pixel_prediction_windows import setup
doc,root,model=setup();other=q.Document(q.Image.from_numpy(np.zeros((1,1,3),np.uint8)))
job=q.PixelStreamExportJob();assert job.request(doc,root,model,sys.argv[2])
del doc,model;gc.collect();s=job.wait(other);assert s.state=='saved' and not s.current
del job;gc.collect();q.PixelStreamFile(sys.argv[2]).verify()
'''
    done=subprocess.run([sys.executable,'-I','-c',script,str(Path(__file__).parent),str(tmp_path/'owned.qps')],capture_output=True,text=True,timeout=25)
    assert done.returncode==0,done.stdout+done.stderr


def test_destroy_running_job_joins_without_gil_deadlock_or_partial_file(tmp_path):
    script='''
import gc,sys,numpy as np,qupathxx as q
from pathlib import Path
sys.path.insert(0,sys.argv[1])
from test_pixel_prediction_windows import setup
_,_,model=setup();doc=q.Document(q.Image.from_numpy(np.full((2048,2048,3),80,np.uint8)))
root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,2048,2048)));o=q.PixelStreamOptions();o.block_size=16
job=q.PixelStreamExportJob();assert job.request(doc,root,model,sys.argv[2],o)
del doc,model,job;gc.collect();assert not Path(sys.argv[2]).exists()
assert not list(Path(sys.argv[2]).parent.glob('*.qpx-tmp-*'))
'''
    done=subprocess.run([sys.executable,'-I','-c',script,str(Path(__file__).parent),str(tmp_path/'destroy.qps')],capture_output=True,text=True,timeout=25)
    assert done.returncode==0,done.stdout+done.stderr
