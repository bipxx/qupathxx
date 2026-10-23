"""Historical file jobs need neither a live Document nor a training backend."""
import gc
from pathlib import Path
import struct
import subprocess
import sys
import numpy as np
import pytest
import qupathxx as q

FIXTURE=Path(__file__).parent/'data/pixel-streams/rectangle-v1-windows.qps'


def copy_file(tmp_path):
    path=tmp_path/'saved.qps';path.write_bytes(FIXTURE.read_bytes());return path


def opened(path,index=0):
    job=q.PixelStreamReadJob();assert job.open(str(path),index);stats=job.wait()
    assert stats.state=='ready' and stats.block is not None and not stats.error
    return job,stats


def test_idle_job_and_missing_reader_operations():
    job=q.PixelStreamReadJob();assert not job.busy and job.poll().state=='idle'
    job.cancel();assert job.wait().state=='idle' and job.close()
    with pytest.raises(RuntimeError):job.read(0)
    with pytest.raises(RuntimeError):job.verify()


@pytest.mark.parametrize('index',[0,1,5,11])
def test_open_reads_requested_block_and_declares_unverified_totals(index):
    job,stats=opened(FIXTURE,index);reference=q.PixelStreamFile(str(FIXTURE))
    assert not job.busy and stats.generation==1 and stats.block_index==index and not stats.verified
    assert stats.block_size==reference.block_size and not stats.windowed
    assert stats.declared_summary.sha256==reference.declared_summary.sha256
    assert stats.block.sha256==reference.read_block(index).sha256
    assert stats.block.labels_numpy().flags.writeable is False


def test_navigation_verification_and_close_keep_value_snapshots_alive(tmp_path):
    path=copy_file(tmp_path);job,first=opened(path);labels=first.block.labels_numpy();reference=labels.copy()
    assert job.read(11);last=job.wait();assert last.state=='ready' and last.block_index==11 and last.generation==2
    assert last.block.width==8 and last.block.height==8 and first.block.width==16
    assert job.verify();verified=job.wait();assert verified.verified and verified.block is last.block and verified.generation==3
    assert job.read(0);again=job.wait();assert again.state=='ready' and again.verified and again.block.sha256==first.block.sha256
    assert job.close();closed=job.poll();assert closed.state=='idle' and closed.block is None and closed.declared_summary is None
    assert closed.generation==4;path.unlink();del job,first,last,verified,again;gc.collect();np.testing.assert_array_equal(labels,reference)


def test_pending_request_has_no_queue_or_close_side_effect():
    job=q.PixelStreamReadJob();assert job.open(str(FIXTURE))
    assert job.busy and not job.open('unrelated.qps') and not job.read(1) and not job.verify() and not job.close()
    stats=job.wait();assert stats.state=='ready' and stats.generation==1 and stats.block_index==0


def test_out_of_range_read_clears_display_and_can_retry_same_reader():
    job,first=opened(FIXTURE);assert job.read(first.declared_summary.blocks)
    bad=job.wait();assert bad.state=='failed' and bad.error and bad.block is None and not bad.verified
    assert job.read(1);good=job.wait();assert good.state=='ready' and good.block_index==1 and not good.error
    assert first.block is not None and first.block_index==0


def test_failed_open_replaces_old_reader_and_can_reopen(tmp_path):
    job,first=opened(FIXTURE);assert job.open(str(tmp_path/'missing.qps'))
    failed=job.wait();assert failed.state=='failed' and failed.error and failed.block is None and failed.declared_summary is None
    with pytest.raises(RuntimeError):job.read(0)
    assert job.open(str(FIXTURE),11);again=job.wait();assert again.state=='ready' and again.block_index==11 and first.block_index==0


@pytest.mark.parametrize('limit',[0,127,(1<<46)+1])
def test_invalid_file_limit_does_not_launch_worker(limit):
    job=q.PixelStreamReadJob()
    with pytest.raises(ValueError):job.open(str(FIXTURE),max_bytes=limit)
    assert not job.busy and job.poll().state=='idle'


def test_valid_too_small_limit_fails_without_decoded_values():
    job=q.PixelStreamReadJob();assert job.open(str(FIXTURE),max_bytes=128)
    stats=job.wait();assert stats.state=='failed' and stats.error and stats.block is None and stats.declared_summary is None


def test_cancel_open_discards_even_a_completed_uncollected_result():
    job=q.PixelStreamReadJob();assert job.open(str(FIXTURE));job.cancel();stats=job.wait()
    assert stats.state=='cancelled' and stats.block is None and not stats.verified
    assert job.open(str(FIXTURE));assert job.wait().state=='ready'


@pytest.mark.parametrize('operation',['read','verify'])
def test_cancel_pending_reader_is_terminal_until_reopened(operation):
    job,first=opened(FIXTURE)
    assert job.read(11) if operation=='read' else job.verify()
    job.cancel();stats=job.wait();assert stats.state=='cancelled' and stats.block is None and not stats.verified
    with pytest.raises(RuntimeError):job.read(0)
    with pytest.raises(RuntimeError):job.verify()
    assert first.block.valid_pixels>0
    assert job.open(str(FIXTURE));assert job.wait().state=='ready'


def test_changed_file_invalidates_result_and_verification_without_losing_old_views(tmp_path):
    path=copy_file(tmp_path);job,first=opened(path);labels=first.block.labels_numpy();reference=labels.copy()
    data=bytearray(path.read_bytes());data[-1]^=1;path.write_bytes(data)
    assert job.verify();failed=job.wait();assert failed.state=='failed' and failed.error and failed.block is None and not failed.verified
    np.testing.assert_array_equal(labels,reference)
    path.write_bytes(FIXTURE.read_bytes());assert job.open(str(path));assert job.wait().state=='ready'


def test_unrequested_corrupt_block_is_rejected_on_read_and_complete_scan(tmp_path):
    path=copy_file(tmp_path);data=bytearray(path.read_bytes());at=132+struct.unpack_from('<Q',data,60)[0]+136;data[at]^=1;path.write_bytes(data)
    job,first=opened(path);assert not first.verified
    assert job.read(1);assert job.wait().state=='failed' and job.poll().block is None
    assert job.open(str(path));assert job.wait().state=='ready'
    assert job.verify();failed=job.wait();assert failed.state=='failed' and not failed.verified and failed.block is None


def test_result_and_summary_copies_cannot_rewrite_reader_state():
    job,first=opened(FIXTURE);counts=first.declared_summary.class_counts;counts[0]=0
    assert job.poll().declared_summary.class_counts!=counts
    with pytest.raises(AttributeError):first.state='forged'
    with pytest.raises(AttributeError):first.block=None
    assert not first.block.labels_numpy().flags.writeable


def test_loaded_block_restores_no_live_publication_authority():
    job,stats=opened(FIXTURE);block=stats.block;model=block.model
    doc=q.Document(q.Image.from_numpy(np.full((48,64,3),80,np.uint8)))
    resolution=model.resolution
    if resolution.pixel_width_um is not None:
        assert resolution.pixel_height_um is not None;doc.set_calibration(resolution.pixel_width_um,resolution.pixel_height_um)
    target=doc.add_roi(block.roi);request=q.PixelPredictionRequest(doc,target,model,window=block.bounds)
    assert not block.matches(request)
    if q.has_geos:
        with pytest.raises(RuntimeError):q.publish_pixel_contours(doc,request,q.trace_pixel_prediction(block))
    assert doc.object_count==1


def test_python_owner_shutdown_during_read_and_verification_does_not_deadlock():
    script='''
import gc,sys,numpy as np,qupathxx as q
for operation in ['read','verify']:
    job=q.PixelStreamReadJob();assert job.open(sys.argv[1]);first=job.wait();view=first.block.labels_numpy();expected=view.copy()
    assert job.read(11) if operation=='read' else job.verify()
    del job;gc.collect();np.testing.assert_array_equal(view,expected)
'''
    done=subprocess.run([sys.executable,'-I','-c',script,str(FIXTURE)],capture_output=True,text=True,timeout=20)
    assert done.returncode==0,done.stdout+done.stderr
