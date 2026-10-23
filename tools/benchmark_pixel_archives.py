"""Installed pixel training/mask archive, bounded save capture and reopen sample."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time
import numpy as np
import qupathxx as q
from qupathxx import _native


def timed(fn):
    start=time.perf_counter();value=fn();return value,(time.perf_counter()-start)*1000


def main():
    ap=argparse.ArgumentParser();ap.add_argument("--size",type=int,default=512);ap.add_argument("--output",type=Path,required=True);args=ap.parse_args();n=args.size
    if n<64 or n>1024 or n%2:ap.error("size must be an even integer in64..1024")
    y,x=np.indices((n,n));rgb=np.empty((n,n,3),dtype=np.uint8);rgb[:,:,0]=np.where(x<n//2,0,200);rgb[:,:,1]=y%256;rgb[:,:,2]=17
    doc=q.Document(q.Image.from_numpy(rgb));a=doc.add_rectangle(q.Rect(0,0,n//2,n));b=doc.add_rectangle(q.Rect(n//2,0,n//2,n));root=doc.add_rectangle(q.Rect(0,0,n,n));doc.classify_objects([a],"A");doc.classify_objects([b],"B")
    options=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1];options.features=features;options.max_samples=4000;options.max_samples_per_class=2000
    request=q.PixelTrainingRequest(doc,[a,b],options=options);table=q.sample_pixel_training(request);parameters=q.pixel_forest_parameters();parameters.trees=16;parameters.max_depth=4
    model=q.train_pixel_classifier(table,parameters)
    training,train_encode=timed(lambda:q.archive_pixel_training(table,model));loaded_training,train_decode=timed(lambda:q.PixelClassifierArchive.from_bytes(training.to_bytes()))
    assert loaded_training.sha256==training.sha256 and loaded_training.to_bytes()==training.to_bytes()
    np.testing.assert_array_equal(loaded_training.table.numpy(),table.numpy());np.testing.assert_array_equal(loaded_training.table.coordinates_numpy(),table.coordinates_numpy())
    training_id=doc.keep_pixel_training(request,training);prediction_request=q.PixelPredictionRequest(doc,root,model);result=q.predict_pixel_region(prediction_request)
    expected=np.where(x<n//2,model.classes.index("A")+1,model.classes.index("B")+1).astype(np.uint32);np.testing.assert_array_equal(result.labels_numpy(),expected)
    prediction,predict_encode=timed(lambda:q.archive_pixel_prediction(result));loaded_prediction,predict_decode=timed(lambda:q.PixelClassifierArchive.from_bytes(prediction.to_bytes()))
    np.testing.assert_array_equal(loaded_prediction.result.labels_numpy(),expected);np.testing.assert_array_equal(loaded_prediction.result.vote_fractions_numpy(),result.vote_fractions_numpy())
    assert loaded_prediction.sha256==prediction.sha256 and not loaded_prediction.result.matches(prediction_request)
    prediction_id=doc.keep_pixel_prediction(prediction_request,prediction)
    project=q.Project();image=project.add_document(doc);job=q.ProjectSaveJob()
    with tempfile.TemporaryDirectory(prefix="qpx-pixel-archive-") as directory:
        path=Path(directory)/"pixels.qpxproj";assert job.request(project,str(path));doc.remove_pixel_classifier_record(training_id)
        status=job.wait();assert status.state=="saved" and project.dirty
        reopened,open_ms=timed(lambda:q.Project.open(str(path)));stored=reopened.document(image)
        assert stored.pixel_classifier_record_count==2 and stored.pixel_classifier_record(training_id).archive.sha256==training.sha256 and stored.pixel_classifier_record(prediction_id).archive.sha256==prediction.sha256
        project_bytes=path.stat().st_size
    env=q.build_environment();out=dict(schema="qpx.pixel-archive-benchmark/1",size=n,rows=table.row_count,features=len(table.features),training_sha256=table.sha256,model_sha256=model.sha256,prediction_sha256=result.sha256,
        training_archive_sha256=training.sha256,prediction_archive_sha256=prediction.sha256,training_encoded_bytes=len(training.to_bytes()),prediction_encoded_bytes=len(prediction.to_bytes()),training_charged_payload_bytes=training.payload_bytes,prediction_charged_payload_bytes=prediction.payload_bytes,
        training_encode_ms=train_encode,training_decode_ms=train_decode,prediction_encode_ms=predict_encode,prediction_decode_ms=predict_decode,save_capture_ms=status.capture_ms,save_write_ms=status.write_ms,save_snapshot_bytes=status.snapshot_bytes,project_open_ms=open_ms,project_bytes=project_bytes,
        native_sha256=hashlib.sha256(Path(_native.__file__).read_bytes()).hexdigest(),source_tree_sha256=env.source_tree_sha256,
        verified=["sample values and coordinates","every mask label and winning vote fraction","model and archive hashes","no restored live request authority","immutable async snapshot despite live record removal"],
        limitations="Single synthetic shared-workstation CPU sample. Charged payload/work estimates are conservative quotas, not allocator/RSS measurements. Encoding/decoding and save writing run outside the GUI owner capture. No biological, hardware or frame-budget acceptance claim.")
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(out,indent=2)+"\n",encoding="utf-8");print(json.dumps(out,indent=2))

if __name__=="__main__":main()
