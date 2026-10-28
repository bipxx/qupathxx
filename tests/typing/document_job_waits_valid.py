import qupathxx as q


def same_thread_waits(doc: q.Document, selected_id: int) -> None:
    analysis=q.AnalysisJob()
    analysis_stats: q.AnalysisStats=analysis.wait(document=doc,selected_id=selected_id)
    result: q.AnalysisResult | None=analysis.result
    tissue=q.TissueJob()
    tissue_stats: q.TissueStats=tissue.wait(document=doc,selected_id=selected_id)
    mask: q.TissueResult | None=tissue.result
    cells=q.CellJob()
    cell_stats: q.CellStats=cells.wait(document=doc,selected_id=selected_id)
    detections: q.CellResult | None=cells.result
    stains=q.StainEstimateJob()
    stain_stats: q.AnalysisStats=stains.wait(document=doc,selected_id=selected_id)
    estimate: q.StainEstimateResult | None=stains.result
    objects=q.ObjectClassifierJob()
    object_stats: q.ObjectClassifierStats=objects.wait(document=doc)
    classes: q.ObjectClassificationResult | None=objects.result
    training=q.PixelTrainingJob()
    training_stats: q.PixelTrainingStats=training.wait(document=doc)
    classifier: q.PixelClassifier | None=training.model
    prediction=q.PixelPredictionJob()
    prediction_stats: q.PixelTrainingStats=prediction.wait(document=doc)
    pixels: q.PixelPredictionResult | None=prediction.result
    contour=q.PixelContourJob()
    contour_stats: q.PixelTrainingStats=contour.wait(document=doc)
    geometry: q.PixelContours | None=contour.result
    export=q.PixelStreamExportJob()
    export_stats: q.PixelStreamExportStats=export.wait(document=doc)
    for job in (analysis,tissue,cells,stains,objects,training,prediction,contour,export):
        job.cancel()
