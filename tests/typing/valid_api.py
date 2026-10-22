from pathlib import Path
import numpy as np
from numpy.typing import NDArray
import qupathxx as q


def workflow(source: NDArray[np.uint8], output: Path) -> tuple[NDArray[np.uint8], NDArray[np.float32], str]:
    image=q.Image.from_numpy(source)
    doc=q.Document(image);project=q.Project();image_id=project.add_document(doc)
    obj=doc.add_rectangle(q.Rect(0,0,image.width,image.height))
    doc.apply_classifications([(obj, "Tissue")], label="Classify region")
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


def responsive_workspace(project: q.Project) -> tuple[int, int, int, float, str]:
    options=q.GuiOptions()
    options.docked_workspace=True
    event=q.GuiInput()
    event.frame,event.kind,event.target=3,q.GuiInputKind.LEFT_DOWN,q.GuiInputTarget.SAVE_PROJECT
    event.wait_for=q.GuiInputWait.TARGET_READY
    options.inputs=[event]
    report=q.show(project,frame_limit=10,options=options)
    return report.ui_stacked_fields,report.ui_wrapped_buttons,report.ui_overflow_items,report.ui_max_overflow,report.ui_overflow_label


def numeric_classifier(samples: NDArray[np.float32], labels: NDArray[np.uint32]) -> tuple[list[q.ClassifierPrediction], str]:
    available: bool=q.has_classifier_training
    if not available:
        raise RuntimeError('training backend unavailable')
    schema=[q.ClassifierFeature('area','um2')]
    model=q.train_forest(samples,labels,schema,['small','large'],q.ForestParameters())
    restored=q.ForestModel.from_bytes(model.to_bytes())
    return restored.predict(samples,schema),restored.sha256


def object_classifier(doc: q.Document, ids: list[int], path: Path) -> tuple[list[int], NDArray[np.float32]]:
    table=q.extract_object_features(q.ObjectFeatureRequest(doc,ids))
    model=q.train_object_forest(table,q.ForestParameters())
    q.save_forest_model(model,str(path))
    loaded=q.load_forest_model(str(path))
    request=q.ObjectFeatureRequest(doc,ids,q.ObjectFeatureOptions.from_model(loaded))
    result=q.classify_objects(request,loaded)
    return q.apply_object_classification(doc,request,result),table.numpy()


def object_classifier_job(doc: q.Document, ids: list[int]) -> str:
    job=q.ObjectClassifierJob()
    job.train(q.ObjectFeatureRequest(doc,ids))
    job.wait(doc)
    model=job.model
    if model is None:
        raise RuntimeError('training did not publish a model')
    options=q.GuiOptions()
    options.classifier_model=model
    options.classifier_features=q.ObjectFeatureOptions.from_model(model)
    return model.sha256


def classifier_audit(document: q.Document, job: q.ObjectClassifierJob) -> q.ForestModel | None:
    artifact: q.ObjectClassifierArchive | None = job.archive
    if artifact is None:
        return None
    record_id: int = job.retain_training(document, name="Reviewed training cohort")
    record: q.ObjectClassifierRecord = document.classifier_record(record_id)
    model: q.ForestModel = record.archive.model
    return model


def pixel_recipe(pixels: q.PixelBuffer, forest: q.ForestModel, options: q.PixelFeatureOptions) -> str:
    recipe: q.PixelClassifier = q.PixelClassifier(forest, options)
    result: q.PixelClassification = q.classify_pixels(pixels, recipe, work_bytes=32*1024*1024)
    result.labels_numpy(copy=True)
    result.vote_fractions_numpy()
    result.valid_numpy()
    portable: q.PixelClassifier = q.PixelClassifier.from_bytes(recipe.to_bytes())
    return portable.sha256 + result.sha256


def annotated_pixel_training(doc: q.Document, ids: list[int]) -> q.PixelClassifier | None:
    job=q.PixelTrainingJob()
    request=q.PixelTrainingRequest(doc, ids)
    parameters: q.ForestParameters=q.pixel_forest_parameters()
    job.train(request,parameters)
    job.wait(doc)
    table: q.PixelTrainingTable | None = job.table
    if table is not None:
        table.numpy()
        table.coordinates_numpy()
        table.annotation_ids_numpy()
    return job.model


def pixel_source_mask(doc: q.Document, object_id: int, model: q.PixelClassifier) -> str:
    resolution: q.PixelResolution | None = model.resolution
    request=q.PixelPredictionRequest(doc, object_id, model)
    result: q.PixelPredictionResult=q.predict_pixel_region(request)
    result.labels_numpy()
    result.valid_numpy()
    result.vote_fractions_numpy()
    return result.sha256 if resolution is not None else ''


def pixel_desktop_options(model: q.PixelClassifier, training: q.PixelTrainingOptions) -> q.GuiOptions:
    options=q.GuiOptions()
    options.pixel_model=model
    options.pixel_training_options=training
    options.pixel_parameters=q.pixel_forest_parameters()
    options.pixel_prediction_options=q.PixelPredictionOptions()
    options.pixel_open=True
    options.pixel_object_id=1
    return options


def pixel_desktop_result(report: q.GuiReport) -> q.PixelClassifier | None:
    bounds: q.Rect=report.canvas_content_bounds
    result: q.PixelPredictionResult | None=report.pixel_result
    if result is not None:
        result.labels_numpy()
    table: q.PixelTrainingTable | None=report.pixel_training_table
    if table is not None:
        table.coordinates_numpy()
    return report.pixel_model


def publish_pixel_mask(document: q.Document, request: q.PixelPredictionRequest, result: q.PixelPredictionResult) -> list[int]:
    if not result.matches(request):
        return []
    traced: q.PixelContours=q.trace_pixel_prediction(result)
    geometry: q.TissueContours=traced.contours
    assert geometry.input_sha256==traced.prediction_sha256
    return q.publish_pixel_contours(document,request,traced)


def trace_pixel_mask_job(document: q.Document, request: q.PixelPredictionRequest, result: q.PixelPredictionResult) -> q.PixelContours | None:
    job=q.PixelContourJob()
    options=q.TissueContourOptions()
    job.request(request,result,options)
    status: q.PixelTrainingStats=job.wait(document)
    assert status.phase=="finished"
    return job.result


def configure_pixel_contours(options: q.GuiOptions, report: q.GuiReport) -> list[int]:
    options.pixel_contour_options=q.TissueContourOptions()
    value: q.PixelContours | None=report.pixel_contours
    if value is not None:
        assert value.prediction_sha256
    return report.pixel_published_ids
