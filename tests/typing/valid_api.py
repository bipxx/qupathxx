from pathlib import Path
import numpy as np
from numpy.typing import NDArray
import qupathxx as q


def physical_cell_request(document: q.Document, object_id: int, transform: q.StainTransform) -> q.CellRequest:
    request = q.CellRequest.with_nucleus_area_limits(document, object_id, transform, minimum_um2=10.0, maximum_um2=100.0)
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.CELLS
    options.cell_min_nucleus_area_um2 = 10.0
    options.cell_max_nucleus_area_um2 = None
    gui = q.GuiOptions()
    gui.cell_min_nucleus_area_um2 = 10.0
    gui.cell_max_nucleus_area_um2 = 100.0
    return request


def calibrated_cell_values(result: q.CellResult) -> q.CalibratedCellFeature:
    feature = result.calibrated_feature(1)
    area: float | None = feature.nucleus_sample_area_um2
    point: q.Point | None = feature.cell_centroid_um
    options = q.CellTableOptions()
    options.include_capture_calibration = True
    gui = q.GuiOptions()
    gui.cell_table_capture_calibration = True
    if area is not None and point is not None:
        assert area >= 0 and point.x >= 0
    return feature


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


def retain_pixel_cohort(document: q.Document, request: q.PixelTrainingRequest, table: q.PixelTrainingTable, model: q.PixelClassifier) -> q.PixelClassifierRecord:
    archive: q.PixelClassifierArchive=q.archive_pixel_training(table,model)
    decoded: q.PixelClassifierArchive=q.PixelClassifierArchive.from_bytes(archive.to_bytes())
    cohort: q.PixelTrainingTable | None=decoded.table
    prediction: q.PixelPredictionResult | None=decoded.result
    assert cohort is not None and prediction is None
    source: q.PixelAnnotationSnapshot=decoded.annotations[0]
    assert source.id
    identifier=document.keep_pixel_training(request,archive)
    return document.pixel_classifier_record(identifier)


def historical_pixel_preview(report: q.GuiReport) -> q.PixelPredictionResult | None:
    ignored: int=report.replay_physical_wheel_events_ignored
    assert ignored>=0
    state: str=report.pixel_archive_state
    selected: int=report.pixel_selected_record_id
    kept: int=report.pixel_record_id
    step: int=report.pixel_record_catalog_max_step
    complete: bool=report.pixel_record_catalog_complete
    assert state and selected>=0 and kept>=0 and step<=1024 and isinstance(complete,bool)
    return report.pixel_historical_result


def windowed_pixel_request(document: q.Document, target: int, model: q.PixelClassifier) -> q.PixelPredictionRequest:
    window: q.Rect=q.Rect(10,20,64,64)
    request=q.PixelPredictionRequest(document,target,model,window=window)
    assert request.windowed
    options=q.GuiOptions()
    options.pixel_viewport_only=True
    return request


def streaming_pixel_counts(document: q.Document, target: int, model: q.PixelClassifier) -> list[int]:
    options=q.PixelStreamOptions()
    options.block_size=128
    producer=q.PixelPredictionStream(document,target,model,options)
    first: q.PixelPredictionResult | None=producer.next()
    if first is not None:
        assert first.windowed
    for block in producer:
        assert block.width<=128
    return producer.class_counts


def write_stream_file(producer: q.PixelPredictionStream, path: str) -> q.PixelPredictionResult:
    summary: q.PixelStreamFileSummary=q.save_pixel_stream(producer,path)
    assert summary.bytes>0 and summary.blocks>0
    file=q.PixelStreamFile(path)
    file.verify()
    assert file.verified and not file.cancelled
    return file.read_block(0)


def export_stream_job(document: q.Document, target: int, model: q.PixelClassifier, path: str) -> q.PixelStreamExportStats:
    job=q.PixelStreamExportJob()
    assert job.request(document,target,model,path)
    result: q.PixelStreamExportStats=job.wait(document)
    if result.summary is not None:
        assert result.summary.blocks==result.total_blocks
    return result


def inspect_stream_file(path: str) -> q.PixelPredictionResult:
    job=q.PixelStreamReadJob()
    assert job.open(path)
    result: q.PixelStreamReadStats=job.wait()
    assert result.block is not None and result.declared_summary is not None
    assert job.verify()
    assert job.wait().verified
    assert job.close()
    return result.block


def keep_stream_link(document: q.Document, path: str) -> q.PixelStreamRecord:
    link: q.PixelStreamLink=q.prepare_pixel_stream_link(path)
    identity=document.keep_pixel_stream(link,"Historical stream")
    record: q.PixelStreamRecord=document.pixel_stream_record(identity)
    job=q.PixelStreamReadJob()
    assert job.open_link(record.link)
    result=job.wait()
    assert result.verified and result.link is not None
    return record


def pixel_input_hashes(pixels: q.PixelBuffer, request: q.PixelPredictionRequest,
                       stream: q.PixelPredictionStream, file: q.PixelStreamFile) -> list[str]:
    feature: str = q.pixel_feature_input_sha256(pixels)
    source: str = q.input_sha256(request)
    current: str = stream.input_sha256()
    file.verify()
    historical: str = file.input_sha256()
    return [feature, source, current, historical]


def project_pixel_batch(project: q.Project, model: q.PixelClassifier, directory: str) -> q.ProjectBatchReport:
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.PIXELS
    options.pixel_model = model
    options.pixels = q.PixelStreamOptions()
    options.pixel_total_limit = 1024*1024*1024
    request = q.ProjectBatchRequest(project, model.transform, options=options)
    result = q.run_project_batch(request, directory)
    for image in result.images:
        assert image.inferred_blocks >= 0 and image.hashed_blocks >= 0
    return result


def queued_physical_cells(session: q.ViewerSession, transform: q.StainTransform) -> int:
    return session.cells_with_nucleus_area_limits(1, 1, transform, minimum_um2=10.1,
        maximum_um2=100., parameters=q.CellParameters(), retain_result=True)


def tissue_microarray(document: q.Document, session: q.ViewerSession) -> str:
    options = q.TmaGridOptions()
    options.rows, options.columns = 2, 3
    options.origin = q.Point(30, 30)
    ids: list[int] = document.create_tma_grid(options)
    document.set_tma_missing([ids[0]], True)
    document.set_tma_case_ids({ids[0]: 'case-001'})
    grid = document.tma_grid()
    assert grid is not None
    core = grid.core(0, 0)
    assert core is not None and core.tma_core is not None
    session.execute(1, [q.ObjectEdit.tma_metadata(core.id, core.tma_core.with_case_id('case-002'))])
    edits: list[q.ObjectEdit] = q.tma_grid_edits(options)
    session.execute(2, edits)
    return q.tma_metadata_csv(grid)


def quantitative_tiff(path: str, session: q.ViewerSession) -> q.RawRegionStatistics:
    options = q.TiffOpenOptions()
    options.cache_bytes = 8 * 1024 * 1024
    server = q.open_tiff(path, options)
    metadata = server.raw_metadata
    assert metadata is not None
    request = q.RawRegionRequest()
    request.region = q.RegionRequest(0, 0, 0, 16, 16)
    request.channels = min(2, metadata.channels)
    raw: q.RawPixelBuffer = server.read_raw(request)
    values: NDArray[np.generic] = raw.numpy()
    validity: NDArray[np.uint8] = raw.valid_numpy()
    assert values.shape[:2] == validity.shape
    view = server.tiff_view
    assert view is not None
    projected: q.ImageServer = server.with_tiff_view(view)
    session.inspect_raw(1)
    session.read_raw(1, request)
    session.set_tiff_view(1, view)
    return q.raw_region_statistics(projected, request)


def saved_raw_measurements(document: q.Document, region: q.RawRegionRequest,
                           session: q.ViewerSession) -> q.RawAnalysisResult:
    captured = q.RawAnalysisRequest(document, region)
    result: q.RawAnalysisResult = q.analyze_raw(captured)
    digest: q.RawInputDigest = q.raw_analysis_input_digest(captured)
    assert digest.input_sha256 == result.input_sha256
    record_id = document.keep_raw_analysis(captured, result)
    record: q.RawAnalysisRecord = document.raw_analysis_record(record_id)
    context: q.RawObjectContext = record.result.object_context
    if context.tma is not None:
        case_id: str = context.tma.case_id
    replay = q.RawAnalysisRequest.replay(document, record_id)
    job = q.RawAnalysisJob()
    job.request(replay)
    state: q.RawAnalysisStats = job.wait(document)
    if state.state == 'ready':
        job.verify_result(document, record_id)
    table = q.RawTableRequest(document, [record_id], require_current=True)
    csv: str = q.raw_analysis_csv(table)
    assert csv
    session.analyze_raw(1, region, retain_result=True)
    session.verify_raw_analysis(1, record_id)
    session.export_raw_measurements(1, 'measurements.csv', record_ids=[record_id])
    options = q.ProjectBatchOptions()
    options.kind = q.ProjectBatchKind.RAW
    options.raw = region
    options.raw_follow_display = False
    options.validate()
    return result


def polygon_sampling(document: q.Document, object_id: int, transform: q.StainTransform,
                     session: q.ViewerSession) -> q.RoiSamplingPolicy:
    policy = q.RoiSamplingPolicy.POLYGON_WINDING
    request = q.AnalysisRequest(document, object_id, transform, sampling_policy=policy)
    result = q.analyze(request)
    roi: q.Roi = request.roi
    member: bool = roi.contains_sample(q.Point(1, 1), policy)
    parameters = q.TissueParameters()
    parameters.sampling_policy = policy
    session.analyze_with_sampling_policy(1, object_id, transform, policy)
    options = q.GuiOptions()
    options.roi_sampling_policy = options.tissue_sampling_policy = policy
    batch = q.ProjectBatchOptions()
    batch.roi_sampling_policy = policy
    assert isinstance(member, bool)
    return result.sampling_policy


def log_cell_recipe() -> q.CellParameters:
    parameters = q.CellParameters()
    parameters.algorithm = q.CellAlgorithm.LOG_WATERSHED
    parameters.log_prominence = .5
    parameters.context_padding = 32
    return parameters


def rendered_loop_metrics(report: q.GuiReport) -> tuple[float, float, int]:
    return (report.rendered_frame_wall_p95_ms, report.max_rendered_frame_wall_ms,
            report.rendered_frame_wall_samples)


def watershed_body_recipe() -> q.CellParameters:
    parameters = q.CellParameters()
    parameters.algorithm = q.CellAlgorithm.LOG_WATERSHED_BODIES
    return parameters


def sampled_watershed_recipe() -> q.CellParameters:
    return q.CellParameters(algorithm=q.CellAlgorithm.SAMPLED_LOG_WATERSHED_BODIES)


def case_summary(snapshot: q.TmaGrid, table: str) -> str:
    options = q.GuiOptions()
    options.tma_case_summary = True
    options.tma_measurements_path = 'measurements.csv'
    options.tma_value_column = 'mean_red'
    options.tma_weight_column = 'pixels'
    options.tma_allow_unversioned = False
    return q.tma_case_summary_csv(snapshot, table, 'mean_red', 'pixels')
