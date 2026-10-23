"""Installed standalone process, bounded output blocks and exact Python oracle."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time
import uuid
import numpy as np
import qupathxx as q
from qupathxx import _native


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as file:
        for b in iter(lambda:file.read(65536),b''):h.update(b)
    return h.hexdigest()


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=1024);ap.add_argument('--work',type=Path);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    if not 64<=args.size<=4096:ap.error('size must be64..4096')
    root=args.work or Path('build/pixel-stream-cli')/uuid.uuid4().hex;root.mkdir(parents=True,exist_ok=False)
    n=args.size;rgb=np.empty((n,n,3),np.uint8);rgb[:,:n//2]=[220,30,10];rgb[:,n//2:]=[10,30,220];doc=q.Document(q.Image.from_numpy(rgb));del rgb
    a=doc.add_roi(q.Roi.rectangle(q.Rect(n//2-32,0,32,32)));b=doc.add_roi(q.Roi.rectangle(q.Rect(n//2,0,32,32)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');target=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n,n)))
    training=q.PixelTrainingOptions();f=q.PixelFeatureOptions();f.sigmas=[0,1];f.gradient_magnitude=True;f.laplacian=True;training.features=f;training.max_samples=128;training.max_samples_per_class=64
    p=q.pixel_forest_parameters();p.trees=16;p.max_depth=4;model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),p)
    project=q.Project();image=project.add_document(doc);source=root/'source.qpxproj';project.save(str(source));model_path=root/'model.qpc';q.save_pixel_classifier(model,str(model_path));output=root/'mask.qps'
    source_hash=sha(source);model_file_hash=sha(model_path);exe=Path(_native.__file__).with_name('qupathxx_batch.exe' if os.name=='nt' else 'qupathxx_batch')
    prediction=q.PixelPredictionOptions();prediction.work_bytes=8*1024*1024
    if n>=2048:
        try:q.PixelPredictionRequest(doc,target,model,prediction)
        except (ValueError,RuntimeError):pass
        else:raise AssertionError('probe must exceed full-mask budget')
    began=time.perf_counter();done=subprocess.run([str(exe),'--export-pixels',str(source),str(image),str(target),str(model_path),str(output),'--block-size','128','--work-bytes',str(prediction.work_bytes)],capture_output=True,text=True,encoding='utf-8',timeout=90);export_ms=(time.perf_counter()-began)*1000
    assert done.returncode==0,done.stdout+done.stderr;lines=[json.loads(line) for line in done.stdout.splitlines()];final=lines[-1];assert final['state']=='saved' and not final['error']
    assert final['estimated_work_bytes']<=prediction.work_bytes and final['summary']['valid_pixels']==n*n and final['total_blocks']==(n//128)**2
    began=time.perf_counter();checked=subprocess.run([str(exe),'--verify-pixels',str(output)],capture_output=True,text=True,encoding='utf-8',timeout=90);verify_ms=(time.perf_counter()-began)*1000
    assert checked.returncode==0,checked.stdout+checked.stderr;verified=json.loads(checked.stdout);assert verified['state']=='verified' and verified['summary']==final['summary']
    saved=q.PixelStreamFile(str(output));saved.verify();maximum=0
    for i in range(final['total_blocks']):
        block=saved.read_block(i);maximum=max(maximum,block.width*block.height*9)
        oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,target,model,prediction,block.bounds))
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(block,method)(),getattr(oracle,method)())
        del block,oracle
    assert saved.verified and sha(source)==source_hash and sha(model_path)==model_file_hash
    env=q.build_environment();assert final['source_tree_sha256']==verified['source_tree_sha256']==env.source_tree_sha256
    out=dict(schema='qpx.pixel-stream-cli-benchmark/1',size=n,blocks=final['total_blocks'],file_bytes=output.stat().st_size,file_sha256=sha(output),content_sha256=saved.declared_summary.sha256,
        export_process_ms=export_ms,verify_process_ms=verify_ms,worker_ms=final['elapsed_ms'],progress_lines=len(lines),stdout_bytes=len(done.stdout.encode('utf-8')),
        estimated_producer_work_bytes=final['estimated_work_bytes'],budget_bytes=prediction.work_bytes,max_block_array_bytes=maximum,valid_pixels=final['summary']['valid_pixels'],class_counts=final['summary']['class_counts'],
        executable_sha256=sha(exe),native_sha256=sha(Path(_native.__file__)),model_sha256=model.sha256,project_sha256=source_hash,model_file_sha256=model_file_hash,
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Single shared-workstation synthetic embedded RGB project. Separate installed native export/verify processes, at most128px block output, all label/valid/vote arrays compared against Python window predictions, original project/model bytes unchanged. Process timing includes project/model load and OS flush. Source/model/metadata/thread bookkeeping outside producer budget; not RSS, vendor WSI, resume, project-wide batch, hardware or biological acceptance. All files retained.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
