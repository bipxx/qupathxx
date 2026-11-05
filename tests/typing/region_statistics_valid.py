from qupathxx import AnalysisRequest, AnalysisResult, CompartmentIntensityStatistics, Document, StainTransform, GuiOptions, GuiReport, ProjectBatchOptions, ViewerSession

def inspect(doc: Document, transform: StainTransform, result: AnalysisResult, report: GuiReport, session: ViewerSession) -> None:
    request = AnalysisRequest.with_intensity_statistics(doc, 1, transform)
    enabled: bool = request.measure_intensity
    statistics: CompartmentIntensityStatistics | None = result.intensity_statistics
    if statistics is not None:
        total: list[float] = statistics.rgb.sum
        sd: list[float] | None = statistics.channels.sample_standard_deviation
    shown: CompartmentIntensityStatistics | None = report.analysis_intensity_statistics
    options = GuiOptions()
    options.analysis_intensity_statistics = True
    batch = ProjectBatchOptions()
    batch.roi_intensity_statistics = True
    ticket: int = session.analyze_with_intensity_statistics(1, 1, transform)
