"""Installed annotated-ROI training and source-aware prediction; synthetic only."""
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
    ap=argparse.ArgumentParser();ap.add_argument('--size',type=int,default=512);ap.add_argument('--output',type=Path,required=True);args=ap.parse_args()
    if not 64<=args.size<=1024:ap.error('size must be64..1024')
    n=args.size;y,x=np.indices((n,n));rgb=np.empty((n,n,3),dtype=np.uint8)
    rgb[:,:,0]=np.where(x<n//2,220+x%16,10+x%16);rgb[:,:,1]=30+y%16;rgb[:,:,2]=np.where(x<n//2,10+y%16,220+y%16)
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(.5,.25)
    a=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n//2,n)));b=doc.add_roi(q.Roi.rectangle(q.Rect(n//2,0,n-n//2,n)))
    doc.classify_objects([a],'red');doc.classify_objects([b],'blue');target=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,n,n)))
    options=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1,2];features.gradient_magnitude=True;options.features=features
    options.max_samples=4000;options.max_samples_per_class=2000;options.tile_size=128;options.max_scanned_pixels=n*n
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    started=time.perf_counter();request=q.PixelTrainingRequest(doc,[a,b],options=options);capture=(time.perf_counter()-started)*1000
    job=q.PixelTrainingJob();started=time.perf_counter();assert job.train(request,parameters);stats=job.wait(doc);train_wall=(time.perf_counter()-started)*1000
    assert stats.state=='ready',stats.error
    table=job.table;model=job.model;assert table is not None and model is not None
    coords=table.coordinates_numpy();expected_labels=(coords[:,0]<n//2).astype(np.uint32) # sorted names: blue then red
    np.testing.assert_array_equal(table.labels_numpy(),expected_labels)
    np.testing.assert_array_equal(table.numpy()[:,0],rgb[coords[:,1],coords[:,0],0])
    assert table.sampled_class_counts==[2000,2000] and table.eligible_class_pixels==[n*(n-n//2),n*(n//2)]
    portable=q.PixelClassifier.from_bytes(model.to_bytes());assert portable.to_bytes()==model.to_bytes() and q.pixel_classifier_level(doc,portable)==0
    prediction_options=q.PixelPredictionOptions();prediction_options.tile_size=128
    started=time.perf_counter();prediction_request=q.PixelPredictionRequest(doc,target,portable,prediction_options);prediction_capture=(time.perf_counter()-started)*1000
    prediction=q.PixelPredictionJob();started=time.perf_counter();assert prediction.request(prediction_request);status=prediction.wait(doc);prediction_wall=(time.perf_counter()-started)*1000
    assert status.state=='ready',status.error
    result=prediction.result;assert result is not None
    expected=np.where(x<n//2,2,1).astype(np.uint32);np.testing.assert_array_equal(result.labels_numpy(),expected)
    rgba=doc.server.read_region(q.RegionRequest(0,0,0,n,n));started=time.perf_counter();direct=q.classify_pixels(rgba,portable);direct_wall=(time.perf_counter()-started)*1000
    np.testing.assert_array_equal(result.labels_numpy(),direct.labels_numpy());np.testing.assert_array_equal(result.vote_fractions_numpy(),direct.vote_fractions_numpy())
    revision=doc.revision;doc.rename_annotation(target,'Retired request');assert prediction.poll(doc).state=='stale' and prediction.result is None
    assert doc.revision==revision+1 and result.valid_pixels==n*n
    env=q.build_environment();report=dict(schema='qpx.pixel-workflow-benchmark/1',size=n,feature_count=features.feature_count,
        training_capture_wall_ms=capture,training_capture_native_ms=request.capture_ms,training_snapshot_bytes=request.snapshot_bytes,
        training_worker_wall_ms=train_wall,training_worker_native_ms=stats.elapsed_ms,sampling_native_ms=table.elapsed_ms,training_rows=table.row_count,
        sampled_class_counts=table.sampled_class_counts,eligible_class_pixels=table.eligible_class_pixels,scanned_pixels=table.scanned_pixels,training_tiles=table.tiles,
        sampling_estimated_work_bytes=table.estimated_work_bytes,table_retained_charged_bytes=table.retained_bytes,
        prediction_capture_wall_ms=prediction_capture,prediction_worker_wall_ms=prediction_wall,prediction_native_ms=result.elapsed_ms,
        prediction_tiles=prediction_request.total_tiles,prediction_estimated_work_bytes=result.estimated_work_bytes,
        direct_buffer_wall_ms=direct_wall,direct_buffer_estimated_work_bytes=direct.estimated_work_bytes,
        model_sha256=model.sha256,table_sha256=table.sha256,sampling_input_sha256=table.input_sha256,prediction_sha256=result.sha256,prediction_input_sha256=result.input_sha256,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),platform=platform.platform(),
        environment={name:getattr(env,name) for name in ['source_tree_sha256','git_revision','compiler','configuration','platform']},
        verified=['every sampled RGB and class','balanced 4000-row cohort','portable resolution-bound recipe roundtrip','every synthetic predicted class','source-tiled labels and vote fractions exactly equal direct buffer','document edit retires controller result'],
        scope='One observation per phase on a shared workstation, synthetic RGB data and two rectangular labels, level0 calibrated .5/.25um, 128px output tiles, 16 trees/depth4. Source prediction includes decoder calls, context, features, masks and worker reconciliation; direct buffer prediction excludes decoder/capture and uses an already owned full RGBA frame. Work estimates exclude original image, retained model, Python arrays and runtime; no RSS, paired implementation speedup, frame-budget, UI/project-publication or biological accuracy acceptance.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
