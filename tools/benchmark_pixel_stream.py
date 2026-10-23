"""Bounded native block production; no full output mask or retained block list."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=1024);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    if not 1024<=args.size<=4096:ap.error('size must be 1024..4096')
    n=args.size;rgb=np.empty((n,n,3),np.uint8);rgb[:,:n//2]=[220,30,10];rgb[:,n//2:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));del rgb
    a=doc.add_roi(q.Roi.rectangle(q.Rect(n//2-32,n//2-16,32,32)));b=doc.add_roi(q.Roi.rectangle(q.Rect(n//2,n//2-16,32,32)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');root=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n,n)))
    training=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1];features.gradient_magnitude=True;features.laplacian=True;training.features=features
    training.max_samples=128;training.max_samples_per_class=64
    parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(doc,[a,b],options=training)),parameters)
    options=q.PixelStreamOptions();options.block_size=128;p=q.PixelPredictionOptions();p.work_bytes=8*1024*1024;p.min_vote_fraction=.6;options.prediction=p
    try:q.PixelPredictionRequest(doc,root,model,p)
    except (ValueError,RuntimeError):pass
    else:raise AssertionError('full output must exceed the configured budget')
    start=time.perf_counter();stream=q.PixelPredictionStream(doc,root,model,options);capture_ms=(time.perf_counter()-start)*1000
    assert stream.estimated_work_bytes<=p.work_bytes and stream.matches(doc)
    times=[];maximum=0;digest=hashlib.sha256();revision=doc.revision;seen=0
    while True:
        start=time.perf_counter();block=stream.next();ms=(time.perf_counter()-start)*1000
        if block is None:break
        times.append(ms);seen+=1;maximum=max(maximum,block.width*block.height)
        oracle=q.predict_pixel_region(q.PixelPredictionRequest(doc,root,model,p,block.bounds))
        for method in ['labels_numpy','valid_numpy','vote_fractions_numpy']:np.testing.assert_array_equal(getattr(block,method)(),getattr(oracle,method)())
        assert block.windowed and block.roi.wkt==doc.annotation(root).geometry.wkt
        digest.update(block.sha256.encode());del block,oracle
    assert stream.state=='complete' and stream.completed_blocks==stream.total_blocks==seen
    assert stream.valid_pixels==n*n and sum(stream.class_counts)==stream.classified_pixels and doc.revision==revision
    env=q.build_environment();output=dict(schema='qpx.pixel-stream-benchmark/1',size=n,pixels=n*n,blocks=seen,block_size=options.block_size,
        budget_bytes=p.work_bytes,estimated_producer_work_bytes=stream.estimated_work_bytes,max_block_pixels=maximum,max_block_array_bytes=maximum*9,
        capture_ms=capture_ms,producer_ms=sum(times),block_p95_ms=float(np.percentile(times,95)),block_max_ms=max(times),
        class_counts=stream.class_counts,valid_pixels=stream.valid_pixels,classified_pixels=stream.classified_pixels,model_sha256=model.sha256,ordered_block_digest=digest.hexdigest(),
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        verified=['full target rejected under the same budget','all block labels/validity/votes equal independently captured window requests','complete counters and unchanged Document','no retained block list or full output array'],
        scope='Synthetic in-memory RGB source; input pixels and immutable model are caller-owned. Producer timings exclude oracle verification. Work/array sizes are allocation estimates, not process RSS. No file-backed WSI throughput, disk output, GUI frame, hardware or biological acceptance claim. Native virtual-slide tests separately exercise a32000-square target without a full source allocation.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output,indent=2))


if __name__=='__main__':main()
