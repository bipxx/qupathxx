"""Installed native CLI uses the same bounded export and historical verification."""
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import threading
import queue
import numpy as np
import pytest
import qupathxx as q
from qupathxx import _native

pytestmark=pytest.mark.skipif(not q.has_project,reason='project backend required for export fixtures')
CLI=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
FIXTURE=Path(__file__).parent/'data/pixel-streams/rectangle-v1-windows.qps'


def run(*args):
    return subprocess.run([str(CLI),*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=40)


def fixture(tmp_path,size=64):
    model=q.PixelStreamFile(str(FIXTURE)).read_block(0).model
    rgb=np.empty((size,size,3),np.uint8);rgb[:,:size//2]=[220,30,10];rgb[:,size//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));resolution=model.resolution
    if resolution.pixel_width_um is not None:doc.set_calibration(resolution.pixel_width_um,resolution.pixel_height_um)
    doc.add_roi(q.Roi.rectangle(q.Rect(0,0,1,1)),'Unused target')
    root=doc.add_roi(q.Roi.rectangle(q.Rect(4,4,size-8,size-8)),'Export target')
    project=q.Project();project.add_document(q.Document(q.Image(1,1,[0,0,0])),'Other image');image=project.add_document(doc,'Pixel image')
    source=tmp_path/'项目.qpxproj';project.save(str(source));path=tmp_path/'模型.qpc';q.save_pixel_classifier(model,str(path))
    return source,image,root,path,doc,model


@pytest.mark.parametrize('block',[16,128])
@pytest.mark.parametrize('window',[None,(17.25,11.2,28.5,20.7)])
def test_cli_export_matches_independent_python_blocks_and_preserves_project(tmp_path,block,window):
    source,image,root,model_path,doc,model=fixture(tmp_path);old=source.read_bytes();out=tmp_path/'完整.qps'
    args=['--export-pixels',source,image,root,model_path,out,'--block-size',block,'--work-bytes',8*1024*1024,'--tile-size',16,'--min-vote','.75']
    if window:args+=['--window',','.join(map(str,window))]
    done=run(*args);assert done.returncode==0,done.stdout+done.stderr
    lines=[json.loads(s) for s in done.stdout.splitlines()];result=lines[-1]
    assert all(s['schema']=='qpx.pixel-export/1' for s in lines) and result['state']=='saved' and not result['error']
    assert result['image_id']==image and result['object_id']==root and result['model_sha256']==model.sha256
    assert result['source_tree_sha256']==q.build_environment().source_tree_sha256 and result['estimated_work_bytes']<=8*1024*1024
    file=q.PixelStreamFile(str(out));file.verify();summary=file.declared_summary
    assert file.verified and file.windowed==bool(window) and result['summary']['content_sha256']==summary.sha256
    assert result['completed_blocks']==result['total_blocks']==summary.blocks and result['summary']['bytes']==out.stat().st_size
    p=q.PixelPredictionOptions();p.tile_size=16;p.min_vote_fraction=.75
    expected=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,p,q.Rect(*window) if window else None))
    assert summary.class_counts==expected.class_counts and summary.valid_pixels==expected.valid_pixels
    for i in range(summary.blocks):
        b=file.read_block(i);x=int(b.level_bounds.x-expected.level_bounds.x);y=int(b.level_bounds.y-expected.level_bounds.y)
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:
            np.testing.assert_array_equal(getattr(b,method)(),getattr(expected,method)()[y:y+b.height,x:x+b.width])
    checked=run('--verify-pixels',out);assert checked.returncode==0,checked.stdout+checked.stderr
    verified=json.loads(checked.stdout);assert verified['state']=='verified' and verified['summary']==result['summary']
    assert source.read_bytes()==old and q.Project.open(str(source)).document(image).pixel_classifier_record_count==0
    assert not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize('replace',[False,True])
def test_cli_never_overwrites_unrelated_output_or_input_files(tmp_path,replace):
    source,image,root,model,_,_=fixture(tmp_path)
    for path in [source,model,tmp_path/'unrelated.svs']:
        if not path.exists():path.write_bytes(b'preserve unrelated source')
        old=path.read_bytes();args=['--export-pixels',source,image,root,model,path]
        if replace:args+=['--replace-existing']
        done=run(*args);assert done.returncode==1,done.stdout+done.stderr
        result=json.loads(done.stdout.splitlines()[-1]);assert result['state']=='failed' and result['summary'] is None
        assert result['completed_blocks']==0 and path.read_bytes()==old


def test_cli_explicit_verified_replacement_and_default_refusal(tmp_path):
    source,image,root,model,_,_=fixture(tmp_path);out=tmp_path/'replace.qps';args=['--export-pixels',source,image,root,model,out]
    assert run(*args).returncode==0;old=out.read_bytes()
    denied=run(*args);assert denied.returncode==1 and out.read_bytes()==old
    saved=run(*args,'--replace-existing');assert saved.returncode==0,saved.stdout+saved.stderr
    assert run('--verify-pixels',out).returncode==0
    data=bytearray(out.read_bytes());data[-1]^=1;out.write_bytes(data);old=out.read_bytes()
    denied=run(*args,'--replace-existing');assert denied.returncode==1 and out.read_bytes()==old


@pytest.mark.parametrize('args',[
    [],['--block-size','15'],['--block-size','513'],['--file-bytes','127'],['--file-bytes',str((1<<46)+1)],
    ['--max-blocks','0'],['--max-pixels','0'],['--work-bytes','0'],['--tile-size','15'],
    ['--window','1,2,3'],['--window','1,2,3,4,5'],['--window','nan,0,1,1'],['--window','0,0,-1,1'],
    ['--window','999,999,2,2'],['--min-vote','nan'],['--min-vote','1.1'],['--min-vote','.5junk'],
    ['--replace-existing','--replace-existing'],['--unknown'],['--block-size'],
])
def test_cli_rejects_invalid_export_without_output(tmp_path,args):
    source,image,root,model,_,_=fixture(tmp_path);out=tmp_path/'none.qps'
    done=run('--export-pixels',source,image,root,model,out,*args) if args else run('--export-pixels')
    assert done.returncode==2,done.stdout+done.stderr
    error=json.loads(done.stderr);assert error['schema']=='qpx.batch-error/1' and error['error']
    assert not out.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))


@pytest.mark.parametrize('ids',[('0','1'),('1','0'),('99','2'),('2','99'),('-1','2'),('2','bad')])
def test_cli_missing_image_or_target_is_reported_before_work(tmp_path,ids):
    source,_,_,model,_,_=fixture(tmp_path);out=tmp_path/'none.qps'
    done=run('--export-pixels',source,*ids,model,out);assert done.returncode==2 and json.loads(done.stderr)['error']
    assert not out.exists()


@pytest.mark.parametrize('mode',['bad-checksum','truncated','unrelated','missing','byte-limit'])
def test_cli_verification_failure_never_reports_trusted_summary(tmp_path,mode):
    path=tmp_path/'broken.qps';data=bytearray(FIXTURE.read_bytes());args=[]
    if mode=='bad-checksum':data[-1]^=1
    elif mode=='truncated':data=data[:-1]
    elif mode=='unrelated':data=bytearray(b'unrelated')
    elif mode=='byte-limit':args=['--file-bytes','128']
    if mode!='missing':path.write_bytes(data)
    done=run('--verify-pixels',path,*args);assert done.returncode==1
    result=json.loads(done.stdout);assert result['state']=='failed' and result['error'] and result['summary'] is None


@pytest.mark.parametrize('args',[[],['--bad','1'],['--file-bytes','bad'],['--file-bytes','127'],['--file-bytes'],['--file-bytes','128','extra']])
def test_cli_verification_argument_errors(args):
    done=run('--verify-pixels',*([str(FIXTURE),*args] if args else []))
    assert done.returncode==2 and json.loads(done.stderr)['error']


def test_cli_interrupt_cancels_worker_and_leaves_no_partial_file(tmp_path):
    source,image,root,model,_,_=fixture(tmp_path,2048);out=tmp_path/'cancel.qps'
    extra={'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP} if os.name=='nt' else {}
    child=subprocess.Popen([str(CLI),'--export-pixels',str(source),str(image),str(root),str(model),str(out),'--block-size','16'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',**extra)
    lines=queue.Queue();thread=threading.Thread(target=lambda:lines.put(child.stdout.readline()),daemon=True);thread.start()
    try:
        first=json.loads(lines.get(timeout=20));assert first['state']=='running' and first['summary'] is None
        child.send_signal(signal.CTRL_BREAK_EVENT if os.name=='nt' else signal.SIGINT)
        stdout,stderr=child.communicate(timeout=25)
        assert child.returncode==130,stdout+stderr
        final=json.loads(stdout.splitlines()[-1]);assert final['state']=='cancelled' and final['summary'] is None
        assert not out.exists() and not list(tmp_path.glob('*.qpx-tmp-*'))
    finally:
        if child.poll() is None:child.kill();child.communicate(timeout=10)


def test_help_describes_pixel_commands_and_old_batch_options():
    done=run('--help');assert done.returncode==0
    for flag in ['--export-pixels','--verify-pixels','--export-csv','--kind od|stains|tissue|cells']:
        assert flag in done.stdout
