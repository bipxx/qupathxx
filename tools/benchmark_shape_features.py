"""Bounded native shape-cache reuse, pressure and optional selected-view replay."""
import argparse
import json
import math
from pathlib import Path
import platform
import time

import qupathxx as q


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--objects', type=int, default=100000)
    parser.add_argument('--hot', type=int, default=4096)
    parser.add_argument('--passes', type=int, default=16)
    parser.add_argument('--gui', action='store_true')
    parser.add_argument('--isotropic',action='store_true')
    parser.add_argument('--output', type=Path, default=Path('build/shape-features.json'))
    args = parser.parse_args()
    if not 1 <= args.objects <= 1000000 or not 1 <= args.hot <= min(args.objects, 65536) or not 1 <= args.passes <= 128:
        parser.error('require 1..1000000 objects, 1..min(objects,65536) hot entries and 1..128 passes')
    height = max(768, (args.objects+999)//1000)
    pixels = q.Image.demo() if height == 768 else q.Image(1024,height,[40,80,160]*1024*height)
    doc = q.Document(pixels)
    doc.set_history_limits(0,0)
    doc.execute([q.ObjectEdit.create_geometry(q.Roi.ellipse(q.Rect(i % 1000, i // 1000, .5+(i % 17)*.02, .4+(i % 13)*.04)), str(i))
                 for i in range(args.objects)])
    doc.set_calibration(.23,.23 if args.isotropic else .61)
    hot = range(args.objects-args.hot+1,args.objects+1)

    def run(ids, passes=1):
        checksum = 0.0
        start = time.perf_counter()
        for _ in range(passes):
            for obj in ids:
                value = doc.shape_measurements(obj).roi
                checksum += value.area_um2+value.length_um
        return (time.perf_counter()-start)*1000, checksum

    doc.set_feature_cache_limit(0)
    uncached_ms, reference = run(hot,args.passes)
    assert doc.feature_cache_stats().entries == 0
    doc.set_feature_cache_limit(args.hot)
    doc.clear_feature_cache()
    run(hot)
    cached_ms, actual = run(hot,args.passes)
    stats = doc.feature_cache_stats()
    assert math.isclose(actual,reference,rel_tol=1e-13) and stats.hits == args.hot*args.passes
    assert stats.entries == stats.misses == args.hot
    pressure_ms, _ = run(range(1,args.objects+1))
    assert doc.feature_cache_stats().entries <= args.hot
    result = dict(platform=platform.platform(), objects=args.objects, capacity=args.hot,isotropic=args.isotropic, calls_per_timed_pass=args.hot*args.passes,
                  uncached_ms=uncached_ms,cached_ms=cached_ms,pressure_ms=pressure_ms,checksum=actual,
                  resident_after_pressure=doc.feature_cache_stats().entries,
                  scope='Native/Python calls over anisotropically calibrated unit ellipses. Fixture construction and warm-fill excluded. '
                        'Cached and disabled-cache passes use identical objects/order and compare checksums. '
                        'Pressure traverses every object once with a bounded resident cache. Intensity reads and persisted feature tables excluded.')
    if args.isotropic:
        result['scope'] = result['scope'].replace('anisotropically calibrated','isotropically calibrated')
    if args.gui:
        before = doc.feature_cache_stats()
        options = q.GuiOptions()
        options.width, options.height = 2560,1440
        view = q.ProjectView()
        view.center,view.zoom = q.Point(512 if args.objects >= 1000 else max(.5,args.objects/2),
                                        64 if args.objects >= 64000 else max(.5,args.objects/2000)),64
        project = q.Project()
        image = project.add_document(doc)
        project.set_view(image,view)
        options.project,options.autosave_interval_seconds = project,0
        event = q.GuiInput()
        event.frame,event.kind = 4,q.GuiInputKind.SELECT_ALL_KEY
        options.inputs = [event]
        report = q.show(project,frame_limit=180,options=options)
        after = doc.feature_cache_stats()
        assert len(report.selected_ids) == args.objects and report.uploaded_tiles > 0
        assert after.hits-before.hits > 100 and after.entries <= args.hot
        result.update(cpu_frame_p95_ms=report.cpu_frame_p95_ms,max_cpu_frame_ms=report.max_cpu_frame_ms,
                      selected_objects=len(report.selected_ids),visible_objects=report.visible_objects,
                      peak_gpu_payload_bytes=report.peak_gpu_bytes,gui_cache_hits=after.hits-before.hits,
                      gui_cache_misses=after.misses-before.misses,gl_renderer=report.gl_renderer)
        result['scope'] += ' GUI selects all objects and shows the primary object measurements each frame in a narrow view; normal frame samples exclude first ten frames and swap wait.'
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,indent=2))


if __name__ == '__main__':
    main()
