from qupathxx import AnalysisRequest, AnalysisResult, Document, StainTransform

def misuse(doc: Document, transform: StainTransform, result: AnalysisResult) -> None:
    AnalysisRequest.with_intensity_statistics(doc, "first", transform)
    result.intensity_statistics = None
    result.intensity_statistics.rgb.sum
