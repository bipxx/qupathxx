"""Installed native raw-reader correctness, bounded cache and view reuse probe."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time

import numpy as np
import tifffile
import qupathxx as q
from qupathxx import _native


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--gui',action='store_true')
    parser.add_argument('--capture',default='')
    args=parser.parse_args()
    base=np.arange(1024*1024,dtype='uint32').reshape(1024,1024)
    values=np.stack([(base+12345*c)%65536 for c in range(4)]).astype('uint16')
    assert all(not np.array_equal(values[0],values[c]) for c in range(1,4))
    with tempfile.TemporaryDirectory(prefix='qpx-tiff-benchmark-') as root:
        path=Path(root)/'quantitative.ome.tif'
        tifffile.imwrite(path,values,ome=True,photometric='minisblack',tile=(256,256),compression='deflate',metadata={'axes':'CYX'})
        options=q.TiffOpenOptions();options.cache_bytes=16*1024*1024
        start=time.perf_counter();server=q.open_tiff(str(path),options);open_ms=(time.perf_counter()-start)*1000
        request=q.RawRegionRequest();request.region=q.RegionRequest(0,0,0,1024,1024);request.channels=4
        start=time.perf_counter();raw=server.read_raw(request);cold_ms=(time.perf_counter()-start)*1000
        np.testing.assert_array_equal(raw.numpy(),values.transpose(1,2,0))
        blocks=server.tiff_read_stats.decoded_blocks
        assert blocks==64
        warm_views_ms=0.
        for channel in range(4):
            start=time.perf_counter()
            view=server.tiff_view;view.channels=[channel,channel,channel];view.lower=[1000]*3;view.upper=[50000]*3
            projected=server.with_tiff_view(view)
            preview=projected.read_region(q.RegionRequest(0,0,0,1024,1024)).numpy()
            warm_views_ms+=(time.perf_counter()-start)*1000
            expected=np.floor(np.clip((values[channel].astype(float)-1000)/49000,0,1)*255+.5).astype('uint8')
            for color in range(3):np.testing.assert_array_equal(preview[...,color],expected)
        assert projected.tiff_read_stats.decoded_blocks==blocks
        start=time.perf_counter();statistics=q.raw_region_statistics(projected,request);statistics_ms=(time.perf_counter()-start)*1000
        np.testing.assert_allclose(statistics.means,values.mean(axis=(1,2)),rtol=1e-12)
        assert statistics.valid_pixels==1024*1024 and server.tiff_read_stats.decoded_blocks==blocks
        stats=server.tiff_read_stats
        assert stats.peak_cache_bytes<=options.cache_bytes
        server.close()
        np.testing.assert_array_equal(projected.read_raw(request).numpy(),values.transpose(1,2,0))
        gui=None
        if args.gui:
            ui=q.GuiOptions();ui.raw_open=True;ui.width=1280;ui.height=1000
            report=q.show(q.Document(projected),frame_limit=80,options=ui,capture_path=args.capture)
            assert report.tiff_state=='ready' and not report.raw_error and report.ui_overflow_items==0
            gui=dict(frames=report.frames,tiff_state=report.tiff_state,ui_overflow=report.ui_overflow_items,cpu_frame_p95_ms=report.cpu_frame_p95_ms)
        projected.close()
        environment=q.build_environment()
        result=dict(schema='qpx.tiff-reader-benchmark/1',shape=[1024,1024,4],dtype='uint16',open_ms=open_ms,
            cold_read_ms=cold_ms,four_warm_views_ms=warm_views_ms,statistics_ms=statistics_ms,
            decoded_blocks=blocks,cache_hits=stats.cache_hits,cache_peak_bytes=stats.peak_cache_bytes,
            cache_budget_bytes=options.cache_bytes,exact_raw_verified=True,preview_verified=True,
            view_cache_reuse_verified=True,distinct_channels_verified=True,independent_close_verified=True,gui=gui,
            native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),
            environment={key:getattr(environment,key) for key in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
            scope='Synthetic independent tifffile fixture on a shared workstation. Timings are observations, not hardware acceptance. Cache charge is native accounting, not process RSS. No biological validation.')
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(result,indent=2))


if __name__=='__main__':main()
