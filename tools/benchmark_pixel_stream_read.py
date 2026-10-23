"""Read-only job latency, complete verification and bounded aggregate oracle."""
import argparse
from collections import deque
import hashlib
import itertools
import json
from pathlib import Path
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as file:
        for b in iter(lambda:file.read(65536),b''):h.update(b)
    return h.hexdigest()


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--file',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    job=q.PixelStreamReadJob();samples=deque(maxlen=256);polls=0;maximum=0.;request_max=0.;started=time.perf_counter()
    def collect():
        nonlocal polls,maximum
        while job.busy:
            began=time.perf_counter();s=job.poll();elapsed=(time.perf_counter()-began)*1000;samples.append(elapsed);maximum=max(maximum,elapsed);polls+=1
            if time.perf_counter()-started>90:job.cancel();job.wait();raise RuntimeError('reader job deadline exceeded')
            if job.busy:time.sleep(.001)
        assert s.state=='ready' and not s.error,s.error
        return s
    began=time.perf_counter();assert job.open(str(args.file));open_request_ms=(time.perf_counter()-began)*1000;first=collect();open_worker_ms=first.elapsed_ms
    assert first.block is not None and first.declared_summary and not first.verified
    began=time.perf_counter();assert job.verify();verified=collect();verify_wall_ms=(time.perf_counter()-began)*1000;assert verified.verified
    summary=verified.declared_summary;classes=len(first.block.model.classes);counts=np.zeros(classes,np.uint64);valid=0;maximum_arrays=0
    count=summary.blocks;last_odd=count-1 if count%2==0 else count-2
    began=time.perf_counter()
    for index in itertools.chain(range(0,count,2),range(last_odd,0,-2)):
        launch=time.perf_counter();assert job.read(index);request_max=max(request_max,(time.perf_counter()-launch)*1000);s=collect();block=s.block
        assert block is not None and s.block_index==index and s.verified
        labels=block.labels_numpy();assert not labels.flags.writeable
        counts+=np.bincount(labels.reshape(-1),minlength=classes+1)[1:].astype(np.uint64);valid+=int(np.count_nonzero(block.valid_numpy()));maximum_arrays=max(maximum_arrays,block.width*block.height*9)
        del block,labels
    indexed_wall_ms=(time.perf_counter()-began)*1000
    assert counts.tolist()==summary.class_counts and valid==summary.valid_pixels and int(counts.sum())==summary.classified_pixels
    assert job.close() and job.poll().block is None
    env=q.build_environment();out=dict(schema='qpx.pixel-stream-read-benchmark/1',file=args.file.name,file_bytes=args.file.stat().st_size,file_sha256=sha(args.file),content_sha256=summary.sha256,
        blocks=count,valid_pixels=valid,class_counts=counts.tolist(),max_block_array_bytes=maximum_arrays,open_request_ms=open_request_ms,open_worker_ms=open_worker_ms,
        verify_wall_ms=verify_wall_ms,indexed_wall_ms=indexed_wall_ms,read_request_max_ms=request_max,polls=polls,max_poll_ms=maximum,last_256_poll_p95_ms=float(np.percentile(samples,95)),retained_poll_samples=len(samples),
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        scope='Read-only synthetic/file observation. One native job operation at a time, no request queue, fixed256 poll-sample storage, out-of-order reads, independently summed block-array counts and full file verification. No full mask or block index list. File header and selected/verification blocks have separate bounded allowances. Not RSS, training, original-slide numerical/biological accuracy, hardware or GUI acceptance.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps(out,indent=2))


if __name__=='__main__':main()
