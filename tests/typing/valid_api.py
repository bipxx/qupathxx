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


def matching_import(doc: q.Document, text: str) -> q.InterchangeReport:
    options=q.GuiOptions()
    options.interchange_assume_pixels=True
    options.interchange_allow_losses=True
    options.qupath_ellipse_precision=True
    return q.import_geojson(doc,text,assume_pixels=True,allow_losses=True,qupath_ellipse_precision=True)


def resize_after_minimize(doc: q.Document) -> tuple[int, float, bool]:
    minimize=q.GuiInput()
    minimize.frame,minimize.kind=0,q.GuiInputKind.MINIMIZE_WINDOW
    restore=q.GuiInput()
    restore.frame,restore.kind=2,q.GuiInputKind.RESTORE_WINDOW
    resize=q.GuiInput()
    resize.frame,resize.kind,resize.x,resize.y=3,q.GuiInputKind.RESIZE_WINDOW,1040,660
    ready=q.GuiInput()
    ready.frame,ready.wait_for=4,q.GuiInputWait.WINDOW_READY
    options=q.GuiOptions()
    options.inputs=[minimize,restore,resize,ready]
    report=q.show(doc,frame_limit=12,options=options)
    return report.suspended_iterations,report.suspended_work_ms,report.window_iconified


def thumbnail_workflow(server: q.ImageServer) -> tuple[q.ThumbnailPlan, q.PixelBuffer, q.ThumbnailStats]:
    options=q.ThumbnailOptions()
    options.max_width,options.max_height=250,150
    plan=q.plan_thumbnail(server.metadata,options)
    job=q.ThumbnailJob()
    job.request(server,options)
    stats=job.wait()
    result=job.result()
    assert result is not None
    return plan,result.pixels,stats


def docked_workspace(doc: q.Document) -> tuple[q.Rect, bool, str]:
    options=q.GuiOptions()
    options.docked_workspace=True
    options.layout_path=q.default_workspace_layout_path()
    options.reset_layout=False
    report=q.show(doc,frame_limit=10,options=options)
    return report.canvas_bounds,report.canvas_visible,report.layout_error


def scaled_workspace(doc: q.Document) -> tuple[float, float, float, int]:
    options=q.GuiOptions()
    options.ui_scale=1.5
    event=q.GuiInput()
    event.frame,event.kind,event.x=3,q.GuiInputKind.SET_UI_SCALE,1.25
    options.inputs=[event]
    report=q.show(doc,frame_limit=10,options=options)
    return report.monitor_scale,report.framebuffer_scale,report.ui_scale,report.font_rebuilds
