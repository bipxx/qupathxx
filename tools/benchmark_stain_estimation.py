"""Bounded native OD-plane estimation and exact model use in a selected viewer."""
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
    parser=argparse.ArgumentParser();parser.add_argument('--size',type=int,default=1024);parser.add_argument('--objects',type=int,default=100000);parser.add_argument('--gui',action='store_true');parser.add_argument('--output',type=Path,default=Path('build/stain-estimation.json'));args=parser.parse_args()
    if not 128<=args.size<=4096 or not 0<=args.objects<=100000:parser.error('size 128..4096, objects 0..100000')
    white=np.array([255.,250.,245.]);truth=np.array([[.65,.70,.29],[.07,.99,.11]]);truth/=np.linalg.norm(truth,axis=1)[:,None]
    y,x=np.indices((args.size,args.size));a=np.where(x%8==0,0,.1+.6*x/(args.size-1));b=np.where(x%8==1,0,.1+.7*y/(args.size-1));rgb=np.rint(white*10**(-a[...,None]*truth[0]-b[...,None]*truth[1])).clip(0,255).astype(np.uint8)
    doc=q.Document(q.Image(args.size,args.size,rgb.ravel().tolist()));doc.set_history_limits(0,0)
    if args.objects:doc.execute([q.ObjectEdit.create_geometry(q.Roi.rectangle(q.Rect(i%args.size,(i//args.size)%args.size,.5,.5))) for i in range(args.objects)])
    obj=doc.add_rectangle(q.Rect(0,0,args.size,args.size));p=q.StainEstimateParameters();request=q.StainEstimateRequest(doc,obj,p,white.tolist());q.estimate_stains(request)
    captures=[]
    for _ in range(100):
        start=time.perf_counter();q.StainEstimateRequest(doc,obj,p,white.tolist());captures.append((time.perf_counter()-start)*1000)
    job=q.StainEstimateJob();start=time.perf_counter();assert job.request(request);submit=(time.perf_counter()-start)*1000;max_poll=0
    while True:
        start=time.perf_counter();status=job.poll(doc,obj);max_poll=max(max_poll,(time.perf_counter()-start)*1000)
        if not job.busy:break
        time.sleep(.001)
    assert status.state=='ready',status.error
    result=job.result;assert result.sampled==min(result.eligible,p.sample_limit) and result.model_sha256==q.stain_model_sha256(result.transform)
    errors=np.degrees(np.arccos(np.clip(np.sum(np.array(result.stains[:2])*truth,axis=1),-1,1)));assert errors.max()<.5
    output=dict(platform=platform.platform(),native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=q.build_environment().source_tree_sha256,size=args.size,samples=result.samples,eligible=result.eligible,sampled=result.sampled,tiles=result.tiles,work_bytes=result.estimated_work_bytes,capture_p95_ms=sorted(captures)[94],request_ms=submit,max_poll_ms=max_poll,worker_ms=status.elapsed_ms,input_sha256=result.input_sha256,model_sha256=result.model_sha256,angle_error_degrees=errors.tolist(),residual_fraction=result.residual_fraction)
    if args.gui:
        def event(frame,kind,wait=q.GuiInputWait.IMMEDIATE):
            e=q.GuiInput();e.frame,e.kind,e.wait_for=frame,kind,wait;return e
        options=q.GuiOptions();options.analysis_white=white.tolist();options.stain_estimate_parameters=p
        options.inputs=[event(4,q.GuiInputKind.SELECT_ALL_KEY),event(20,q.GuiInputKind.ESTIMATE_STAINS_KEY),event(22,q.GuiInputKind.APPLY_STAIN_ESTIMATE_KEY,q.GuiInputWait.STAINS_ESTIMATED),event(24,q.GuiInputKind.ANALYZE_KEY,q.GuiInputWait.STAINS_APPLIED)]
        report=q.show(doc,frame_limit=180,options=options)
        assert report.stain_estimate_state=='ready' and report.stain_estimate_applied and report.stain_estimate_result.model_sha256==result.model_sha256
        assert report.analysis_state=='ready' and len(report.selected_ids)==args.objects+1
        direct=q.analyze(q.AnalysisRequest(doc,obj,result.transform));np.testing.assert_array_equal(report.analysis_means,direct.means)
        output.update(selected=len(report.selected_ids),visible=report.visible_objects,cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,gui_estimate_ms=report.stain_estimate_result.elapsed_ms,overlay_cache_bytes=report.overlay_cache_bytes,tile_gpu_bytes=report.peak_gpu_bytes,gl_renderer=report.gl_renderer)
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8');print(json.dumps(output,indent=2))

if __name__=='__main__':main()
