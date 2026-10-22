from pathlib import Path
import numpy as np
from numpy.typing import NDArray
import qupathxx as q


def workflow(source: NDArray[np.uint8], output: Path) -> tuple[NDArray[np.uint8], NDArray[np.float32], str]:
    image=q.Image.from_numpy(source)
    doc=q.Document(image);project=q.Project();image_id=project.add_document(doc)
    obj=doc.add_rectangle(q.Rect(0,0,image.width,image.height))
    pixels=doc.server.read_region(q.RegionRequest(0,0,0,image.width,image.height))
    transform=q.StainTransform([[1.,0.,0.],[0.,1.,0.]])
    rgb: NDArray[np.uint8]=image.numpy()
    floats: NDArray[np.float32]=transform.deconvolve(pixels).numpy()
    request=q.AnalysisRequest(doc,obj,transform);doc.keep_analysis(request,q.analyze(request))
    q.save_measurements_csv(q.MeasurementTableRequest(project,q.MeasurementTableKind.ROI),str(output))
    options=q.ProjectBatchOptions();options.kind=q.ProjectBatchKind.TISSUE
    selection=q.ProjectBatchSelection();selection.image_id=image_id;selection.object_ids=[obj]
    job=q.ProjectBatchJob();job.request(q.ProjectBatchRequest(project,transform,[selection],options),str(output.parent))
    status: q.ProjectBatchStats=job.wait();report=job.result
    if report is not None:
        return rgb,floats,q.project_batch_report_json(report)
    return rgb,floats,status.error


def managed(path: str) -> NDArray[np.uint8] | None:
    session=q.ViewerSession(path)
    session.execute(1,[q.ObjectEdit.create(q.Rect(0,0,4,4))],expected_revision=0)
    event=session.wait(session.read_region(1,q.RegionRequest(0,0,0,4,4)),0)
    if event.pixels is not None:
        return event.pixels.numpy()
    return None


def desktop_batch(project: q.Project, directory: str) -> q.ProjectBatchReport | None:
    options=q.GuiOptions()
    preset=q.ProjectBatchOptions()
    preset.kind=q.ProjectBatchKind.TISSUE
    options.batch_options=preset
    options.batch_options=None
    options.batch_output_directory=directory
    options.batch_selected_objects=True
    report=q.show(project,frame_limit=5,options=options)
    return report.batch_result
