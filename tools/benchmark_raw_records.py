"""Measure native raw scans, bounded catalog pages, snapshots and CSV export."""
import argparse
import csv
import json
from pathlib import Path
import statistics
import tempfile
import time

import numpy as np
import qupathxx as q
import tifffile


def timed(function):
    start=time.perf_counter();value=function();return value,(time.perf_counter()-start)*1000


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--records',type=int,default=10000);args=parser.parse_args()
    if not 1<=args.records<=10000:parser.error('records must be 1..10000')
    with tempfile.TemporaryDirectory(prefix='qpx-raw-records-') as folder:
        root=Path(folder);source=root/'channels.ome.tif'
        base=np.arange(512*512,dtype='uint32').reshape(512,512)
        data=np.stack([(base+12345*c)%65536 for c in range(4)]).astype('uint16')
        assert all(not np.array_equal(data[0],data[c]) for c in range(1,4))
        tifffile.imwrite(source,data,ome=True,photometric='minisblack',tile=(128,128),compression='deflate',metadata={'axes':'CYX'})
        project=q.Project();image=project.add_tiff_image(str(source));doc=project.document(image)
        region=q.RawRegionRequest();region.region=q.RegionRequest(0,0,0,512,512);region.channels=4
        request=q.RawAnalysisRequest(doc,region,tile_size=256)
        result,cold_ms=timed(lambda:q.analyze_raw(request))
        expected=data.mean(axis=(1,2));np.testing.assert_allclose(result.statistics.means,expected,rtol=1e-12)
        blocks=doc.server.tiff_read_stats.decoded_blocks;scans=[];hashes=[]
        for _ in range(5):
            measured,elapsed=timed(lambda:q.analyze_raw(request));scans.append(elapsed)
            digest,elapsed=timed(lambda:q.raw_analysis_input_digest(request));hashes.append(elapsed)
            assert digest.input_sha256==result.input_sha256==measured.input_sha256
        assert doc.server.tiff_read_stats.decoded_blocks==blocks
        grid=q.TmaGridOptions();grid.rows=grid.columns=1;grid.origin=q.Point(256,256);grid.width=grid.height=384
        core=doc.create_tma_grid(grid)[0];doc.set_tma_case_ids({core:'Case A'})
        parent=doc.add_rectangle(q.Rect(240,240,32,32),'Parent');child=doc.add_rectangle(q.Rect(248,248,16,16),'Child')
        doc.reparent_annotation(parent,core);doc.reparent_annotation(child,parent)
        retained_request=q.RawAnalysisRequest(doc,region,child,tile_size=256)
        retained_result=q.analyze_raw(retained_request);assert retained_result.object_context.tma.case_id=='Case A'
        np.testing.assert_allclose(retained_result.statistics.means,data[:,248:264,248:264].mean(axis=(1,2)),rtol=1e-12)
        # Distinct immutable results exercise real archive payload ownership.
        # Snapshot charges remain logical payload accounting, not process RSS.
        results,generation_ms=timed(lambda:[q.analyze_raw(retained_request) for _ in range(args.records)])
        assert len({id(value) for value in results})==args.records
        ids,keep_ms=timed(lambda:[doc.keep_raw_analysis(retained_request,value) for value in results])
        del results
        if args.records==10000:
            try:doc.keep_raw_analysis(retained_request,retained_result)
            except (ValueError,RuntimeError):pass
            else:raise AssertionError('native catalog exceeded 10000 records')
        after=0;pages=[];seen=[]
        while True:
            page,elapsed=timed(lambda:doc.raw_analysis_page(after,64));pages.append(elapsed)
            if not page:break
            seen.extend(record.id for record in page);after=page[-1].id
        assert seen==ids
        snapshot,capture_ms=timed(lambda:q.RawTableRequest(doc))
        assert snapshot.records==args.records and snapshot.rows==4*args.records
        output=root/'raw.csv';written,csv_ms=timed(lambda:q.save_raw_analysis_csv(snapshot,str(output)))
        assert written==output.stat().st_size
        checked_rows=0
        with output.open(encoding='utf-8',newline='') as source_csv:
            for row in csv.DictReader(source_csv):
                assert row['object_name']=='Child' and row['tma_case_id']=='Case A' and row['status_at_capture']=='current'
                assert row['input_sha256']==retained_result.input_sha256
                checked_rows+=1
        assert checked_rows==4*args.records
        job=q.ProjectSaveJob();_,save_capture_ms=timed(lambda:job.request(project,str(root/'saved.qpxproj')))
        saved=job.wait();assert saved.state=='saved',saved.error
        restored=q.Project.open(str(root/'saved.qpxproj')).document(image)
        assert restored.raw_analysis_record_count==args.records
        receipt={'schema':'qpx.raw-records-benchmark/1','source_tree_sha256':q.build_environment().source_tree_sha256,
            'records':args.records,'record_context':'child -> parent -> TMA core','distinct_result_instances':args.records,'result_generation_ms':generation_ms,'retained_input_sha256':retained_result.input_sha256,'channels':4,'image_shape':[512,512],'tiles':result.tiles,'input_sha256':result.input_sha256,
            'decoded_blocks':blocks,'warm_scans_add_decoded_blocks':False,'cold_analysis_ms':cold_ms,
            'warm_analysis_median_ms':statistics.median(scans),'warm_input_digest_median_ms':statistics.median(hashes),
            'keep_all_ms':keep_ms,'page_size':64,'page_max_ms':max(pages),'page_median_ms':statistics.median(pages),
            'table_capture_ms':capture_ms,'table_snapshot_charged_bytes':snapshot.payload_bytes,
            'csv_rows':snapshot.rows,'csv_bytes':written,'csv_ms':csv_ms,'save_capture_ms':save_capture_ms,
            'save_snapshot_bytes':saved.snapshot_bytes,'save_write_ms':saved.write_ms,'reopened_records':restored.raw_analysis_record_count}
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
        print(json.dumps(receipt,indent=2))
        restored.server.close();doc.server.close()


if __name__=='__main__':main()
