"""Read pixels, annotate, analyze, export and save a reopenable project.

Run with an installed arrays-enabled qupathxx:
  python region_workflow.py slide.svs output --show
PPM also works without OpenSlide. Existing outputs are never overwritten.
"""
import argparse
from pathlib import Path

import numpy as np
import qupathxx as q


def main() -> None:
    ap=argparse.ArgumentParser();ap.add_argument('image',type=Path);ap.add_argument('output',type=Path);ap.add_argument('--show',action='store_true');args=ap.parse_args()
    project_path=args.output/'study.qpxproj';table_path=args.output/'measurements.csv'
    if project_path.exists() or table_path.exists():ap.error('choose an output directory without study.qpxproj or measurements.csv')
    project=q.Project();image_id=project.add_image(str(args.image.resolve()));doc=project.document(image_id)
    width,height=min(doc.metadata.width,512),min(doc.metadata.height,512)
    obj=doc.add_rectangle(q.Rect(0,0,width,height),'Scripted ROI')
    pixels=doc.server.read_region(q.RegionRequest(0,0,0,width,height));rgba=pixels.numpy()
    assert rgba.dtype==np.uint8 and rgba.shape==(height,width,4) and not rgba.flags.writeable
    transform=q.StainTransform([[.65,.70,.29],[.07,.99,.11]])
    request=q.AnalysisRequest(doc,obj,transform,kind=q.AnalysisKind.OPTICAL_DENSITY_MEANS)
    result=q.analyze(request);doc.keep_analysis(request,result)
    # NumPy is a separate check of full rectangular RGB means. Native OD means
    # apply the application's OD convention, which is documented separately.
    print('RGB means:',rgba[:,:,:3].mean(axis=(0,1)).tolist(),'OD means:',result.means)
    args.output.mkdir(parents=True,exist_ok=True)
    q.save_measurements_csv(q.MeasurementTableRequest(project,q.MeasurementTableKind.ROI),str(table_path))
    project.save(str(project_path));reopened=q.Project.open(str(project_path))
    assert len(reopened.document(image_id).analysis_records)==1
    if args.show:q.show(reopened)
    print(project_path)


if __name__=='__main__':main()
