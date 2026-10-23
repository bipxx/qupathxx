"""Durable single-model stream output and bounded complete/indexed verification."""
import argparse
import hashlib
import json
import itertools
from collections import deque
from pathlib import Path
import platform
import time
import uuid
import numpy as np
import qupathxx as q
from qupathxx import _native


def sha(path):
    digest=hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda:file.read(65536),b''):digest.update(chunk)
    return digest.hexdigest()


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=1024);ap.add_argument('--output',type=Path,required=True);ap.add_argument('--file',type=Path);ap.add_argument('--job',action='store_true');args=ap.parse_args()
    if not 64<=args.size<=4096:ap.error('size must be 64..4096')
    n=args.size;rgb=np.empty((n,n,3),np.uint8);rgb[:,:n//2]=[220,30,10];rgb[:,n//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));del rgb
    a=doc.add_roi(q.Roi.rectangle(q.Rect(n//2-32,0,32,32)));b=doc.add_roi(q.Roi.rectangle(q.Rect(n//2,0,32,32)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n,n)))
    training=q.PixelTrainingOptions();f=q.PixelFeatureOptions();f.sigmas=[0,1];f.gradient_magnitude=True;f.laplacian=True;training.features=f;training.max_samples=128;training.max_samples_per_class=64
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),parameters)
    options=q.PixelStreamOptions();options.block_size=128;p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;options.prediction=p
    producer=None if args.job else q.PixelPredictionStream(doc,root,model,options);revision=doc.revision
    path=args.file or Path('build/pixel-stream-files')/(uuid.uuid4().hex+'.qps');path.parent.mkdir(parents=True,exist_ok=True)
    job_metrics=None;started=time.perf_counter()
    if args.job:
        job=q.PixelStreamExportJob();capture=time.perf_counter();assert job.request(doc,root,model,str(path),options);capture_ms=(time.perf_counter()-capture)*1000
        polls=0;maximum=0.;samples=deque(maxlen=256)
        while job.busy:
            began=time.perf_counter();stats=job.poll(doc);elapsed=(time.perf_counter()-began)*1000;maximum=max(maximum,elapsed);samples.append(elapsed);polls+=1
            if time.perf_counter()-started>90:job.cancel();job.wait(doc);raise RuntimeError('export job deadline exceeded')
            if job.busy:time.sleep(.001)
        assert stats.state=='saved' and stats.summary and stats.current and not stats.error
        summary=stats.summary;total_blocks=stats.total_blocks;estimated_work=stats.estimated_work_bytes
        job_metrics=dict(capture_ms=capture_ms,polls=polls,maximum_poll_ms=maximum,last_256_poll_p95_ms=float(np.percentile(samples,95)),retained_poll_samples=len(samples),state=stats.state)
    else:
        summary=q.save_pixel_stream(producer,str(path));total_blocks=producer.total_blocks;estimated_work=producer.estimated_work_bytes
    write_ms=(time.perf_counter()-started)*1000
    started=time.perf_counter();file=q.PixelStreamFile(str(path));open_ms=(time.perf_counter()-started)*1000
    assert not file.verified and summary.bytes==path.stat().st_size and summary.blocks==total_blocks
    started=time.perf_counter();file.verify();verify_ms=(time.perf_counter()-started)*1000;assert file.verified
    last_odd=summary.blocks-1 if summary.blocks%2==0 else summary.blocks-2
    order=itertools.chain(range(0,summary.blocks,2),range(last_odd,0,-2));maximum=0;valid=0;counts=np.zeros(len(model.classes),dtype=np.uint64)
    started=time.perf_counter()
    for i in order:
        block=file.read_block(i);maximum=max(maximum,block.width*block.height)
        oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,p,block.bounds))
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(block,method)(),getattr(oracle,method)())
        valid+=block.valid_pixels;counts+=np.asarray(block.class_counts,dtype=np.uint64);del block,oracle
    indexed_oracle_ms=(time.perf_counter()-started)*1000
    assert valid==summary.valid_pixels==n*n and counts.tolist()==summary.class_counts and doc.revision==revision
    assert file.declared_summary.sha256==summary.sha256
    with path.open('rb') as binary:
        prefix=binary.read(68);header_bytes=int.from_bytes(prefix[60:68],'little')
    env=q.build_environment();result=dict(schema='qpx.pixel-stream-file-benchmark/1',size=n,pixels=n*n,blocks=summary.blocks,block_size=file.block_size,
        budget_bytes=p.work_bytes,estimated_producer_work_bytes=estimated_work,job=job_metrics,file_bytes=summary.bytes,raw_mask_bytes=n*n*9,header_archive_bytes=header_bytes,
        record_overhead_bytes=200*(summary.blocks-1),max_block_array_bytes=maximum*9,write_ms=write_ms,open_ms=open_ms,verify_ms=verify_ms,indexed_oracle_ms=indexed_oracle_ms,
        file_sha256=sha(path),content_sha256=summary.sha256,model_sha256=model.sha256,class_counts=summary.class_counts,valid_pixels=summary.valid_pixels,
        file=path.name,native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        verified=['atomic complete export and unchanged document','all record checksums/numeric masks/aggregate counts and content commitment','out-of-order block reads equal independent window requests','one model/provenance header and no full output array'],
        scope='Single shared-workstation synthetic in-memory RGB source and local disk observation. Producer budget excludes source/model; file header has separate128MiB encoded/256MiB retained artifact caps, plus a block and64KiB I/O buffer. Timing includes OS flush, not a power-loss guarantee. Not RSS, vendor WSI, network storage, GUI frame, hardware or biological acceptance. Output file is retained for inspection.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps(result,indent=2))


if __name__=='__main__':main()
