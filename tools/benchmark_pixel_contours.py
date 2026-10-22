"""Installed source prediction -> shared-edge contours -> atomic publication sample."""
import argparse
import hashlib
import json
from pathlib import Path
import time
import numpy as np
import qupathxx as q
import qupathxx._native as native


def main():
    ap=argparse.ArgumentParser();ap.add_argument("--size",type=int,default=1024);ap.add_argument("--output",type=Path,required=True);args=ap.parse_args()
    n=args.size
    if n<32 or n>2048 or n%16:raise ValueError("size must be a multiple of 16 in 32..2048")
    rgb=np.zeros((8,16,3),dtype=np.uint8);rgb[:,:8]=[240,20,10];rgb[:,8:]=[10,20,240]
    training=q.Document(q.Image.from_numpy(rgb));a=training.add_rectangle(q.Rect(0,0,8,8));b=training.add_rectangle(q.Rect(8,0,8,8))
    training.classify_objects([a],"red");training.classify_objects([b],"blue")
    options=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0];options.features=features;options.max_samples=64;options.max_samples_per_class=32
    parameters=q.pixel_forest_parameters();parameters.trees=8;parameters.max_depth=3
    model=q.train_pixel_classifier(q.sample_pixel_training(q.PixelTrainingRequest(training,[a,b],options=options)),parameters)
    y,x=np.indices((n,n));expected=((x//16+y//16)%2).astype(np.uint32)
    image=np.where(expected[:,:,None]==0,np.array([240,20,10],dtype=np.uint8),np.array([10,20,240],dtype=np.uint8))
    document=q.Document(q.Image.from_numpy(image));root=document.add_rectangle(q.Rect(0,0,n,n));document.clear_history()
    request=q.PixelPredictionRequest(document,root,model);prediction=q.predict_pixel_region(request)
    np.testing.assert_array_equal(prediction.labels_numpy(),np.where(expected==0,model.classes.index("red")+1,model.classes.index("blue")+1))
    contour_options=q.TissueContourOptions();contour_options.clip_to_roi=False
    start=time.perf_counter();traced=q.trace_pixel_prediction(prediction,contour_options);trace_ms=(time.perf_counter()-start)*1000
    contours=traced.contours
    assert contours.component_count==2 and sum(c.pixels for c in contours.components)==n*n
    assert all(c.geometry.area==c.pixels for c in contours.components)
    assert sum(len(c.geometry.polygon_parts) for c in contours.components)==(n//16)**2
    start=time.perf_counter();ids=q.publish_pixel_contours(document,request,traced);publish_ms=(time.perf_counter()-start)*1000
    assert len(ids)==2 and all(document.annotation(i).parent_id==0 for i in ids)
    history_bytes=document.history_bytes
    assert document.undo() and document.object_count==1 and document.redo() and document.object_count==3
    report={"schema":"qpx.pixel-contour-benchmark/1","size":n,"fixture":"16-pixel alternating opaque RGB squares; two classes; 8 trees/depth 3; sigma 0", "native_sha256":hashlib.sha256(Path(native.__file__).read_bytes()).hexdigest(),"source_tree_sha256":q.build_environment().source_tree_sha256,"model_sha256":model.sha256,"prediction_sha256":prediction.sha256,"class_counts":prediction.class_counts,"prediction_ms":prediction.elapsed_ms,"trace_ms":trace_ms,"publish_ms":publish_ms,"edge_count":contours.edge_count,"vertex_count":contours.vertex_count,"polygon_parts":sum(len(c.geometry.polygon_parts) for c in contours.components),"estimated_trace_work_bytes":contours.estimated_work_bytes,"retained_geometry_bytes":contours.retained_bytes,"history_bytes":history_bytes,"published_annotations":len(ids),"verified":["every predicted label","exact geometry areas and part count","atomic publication","undo/redo"],"limitations":"Single shared-workstation CPU sample; estimates exclude allocator/RSS and the existing prediction buffers. No hardware, UI-frame or biological acceptance claim."}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8");print(json.dumps(report))

if __name__=="__main__":main()
