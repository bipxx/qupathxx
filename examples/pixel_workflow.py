"""Labelled ROI training, saved resolution-bound recipe and source-aware prediction."""
from pathlib import Path
import tempfile
import numpy as np
import qupathxx as q


def main():
    rgb=np.empty((64,96,3),dtype=np.uint8);rgb[:,:48]=[220,30,10];rgb[:,48:]=[10,30,220]
    doc=q.Document(q.Image.from_numpy(rgb));doc.set_calibration(.5,.5)
    left=doc.add_roi(q.Roi.rectangle(q.Rect(0,0,48,64)));right=doc.add_roi(q.Roi.rectangle(q.Rect(48,0,48,64)))
    doc.classify_objects([left],'red');doc.classify_objects([right],'blue')
    options=q.PixelTrainingOptions();features=q.PixelFeatureOptions();features.sigmas=[0,1];options.features=features
    parameters=q.ForestParameters();parameters.trees=16;parameters.max_depth=4
    job=q.PixelTrainingJob();assert job.train(q.PixelTrainingRequest(doc,[left,right],options=options),parameters)
    status=job.wait(doc)
    if status.state!='ready' or job.model is None:raise RuntimeError(status.error)
    with tempfile.TemporaryDirectory(prefix='qpx-pixel-workflow-') as folder:
        path=str(Path(folder)/'pixel.qpc');q.save_pixel_classifier(job.model,path);model=q.load_pixel_classifier(path)
    target=doc.add_roi(q.Roi.ellipse(q.Rect(4,4,88,56)))
    prediction=q.PixelPredictionJob();assert prediction.request(q.PixelPredictionRequest(doc,target,model))
    status=prediction.wait(doc);result=prediction.result
    if status.state!='ready' or result is None:raise RuntimeError(status.error)
    print(dict(classes=result.model.classes,counts=result.class_counts,valid_pixels=result.valid_pixels,sampling_um=model.resolution.pixel_width_um))
    # Read-only owned masks survive controller retirement and source closure.
    mask=result.labels_numpy();doc.rename_annotation(target,'Retire old prediction')
    assert prediction.poll(doc).state=='stale';assert mask.any()


if __name__=='__main__':main()
