"""Measure bounded overview budget changes on the pinned full Aperio slide."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time

import qupathxx as q
from qupathxx import _native


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--slide',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--capture',type=Path)
    args=parser.parse_args()
    with args.slide.open('rb') as stream:fixture=hashlib.file_digest(stream,'sha256').hexdigest()
    if fixture!='00a3d54482cd707abf254fe69dccc8d06b8ff757a1663f1290c23418c480eb30':
        parser.error('requires pinned full CMU-1.svs')
    results=[]
    with tempfile.TemporaryDirectory(prefix='qpx-overview-') as temporary:
        for name,initial,target,complete in [
            ('default_unavailable',4,None,False),
            ('explicit_complete',8,None,True),
            ('interactive_raise',4,q.GuiInputTarget.OVERVIEW_BUDGET_INCREASE,True),
            ('interactive_lower',8,q.GuiInputTarget.OVERVIEW_BUDGET_DECREASE,False),
        ]:
            source=q.open_slide(str(args.slide.resolve()))
            try:
                options=q.GuiOptions();options.settle_tiles=True;options.replay_timeout_seconds=15
                options.overview_source_pixel_limit=initial*1024*1024
                if target is not None:
                    down=q.GuiInput();down.frame=4;down.kind=q.GuiInputKind.LEFT_DOWN;down.target=target;down.wait_for=q.GuiInputWait.TARGET_READY
                    up=q.GuiInput();up.frame=5;up.kind=q.GuiInputKind.LEFT_UP;up.target=target
                    options.inputs=[down,up]
                capture=Path(temporary)/'overview.ppm' if args.capture and name=='explicit_complete' else None
                began=time.perf_counter()
                report=q.show(q.Document(source),frame_limit=30,capture_path=str(capture) if capture else '',options=options)
                wall_ms=(time.perf_counter()-began)*1000
                assert report.tiles_settled and report.pending_tiles==0 and report.uploaded_tiles>0
                assert report.ui_overflow_items==0 and report.peak_gpu_bytes<=64*1024*1024
                assert report.overview_state==('succeeded' if complete else 'failed')
                assert report.overview_source_pixel_limit==(8 if complete else 4)*1024*1024
                digest=None;source_pixels=0;worker_ms=None
                if complete:
                    preview=report.overview_result
                    assert preview.plan.level==2 and preview.plan.source_pixels==5913875
                    digest=hashlib.sha256(preview.pixels.data).hexdigest()
                    source_pixels=preview.plan.source_pixels;worker_ms=preview.elapsed_ms
                    assert 0<report.overview_gpu_bytes<=250*150*4 and not report.overview_error
                else:
                    assert 'source pixel limit' in report.overview_error
                    assert report.overview_result is None and report.overview_gpu_bytes==0
                if capture:
                    from PIL import Image
                    args.capture.parent.mkdir(parents=True,exist_ok=True)
                    with Image.open(capture) as image:image.save(args.capture)
                results.append(dict(case=name,frames=report.frames,wall_ms=wall_ms,
                    cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,
                    overview_worker_ms=worker_ms,source_pixels=source_pixels,source_pixel_limit=report.overview_source_pixel_limit,
                    overview_gpu_bytes=report.overview_gpu_bytes,overview_peak_gpu_bytes=report.overview_peak_gpu_bytes,
                    canvas_peak_gpu_bytes=report.peak_gpu_bytes,overview_rgba_sha256=digest,
                    renderer=report.gl_renderer,ui_overflow=report.ui_overflow_items))
            finally:source.close()
    assert results[1]['overview_rgba_sha256']==results[2]['overview_rgba_sha256']
    with Path(_native.__file__).open('rb') as stream:native_sha=hashlib.file_digest(stream,'sha256').hexdigest()
    receipt=dict(schema='qpx.gui-overview-benchmark/1',source_tree_sha256=q.build_environment().source_tree_sha256,
        native_sha256=native_sha,fixture_sha256=fixture,cases=results,
        scope='Local completed-frame software/hardware renderer as recorded; requested buffer budgets, not RSS, decoder-memory or hardware frame acceptance. Wall time includes window startup and fixed frames.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(receipt,indent=2))


if __name__=='__main__':main()
