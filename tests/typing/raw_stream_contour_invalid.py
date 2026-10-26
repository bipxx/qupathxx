import qupathxx as q

forged=q.RawPixelStreamContours()
job=q.RawPixelStreamContourJob()
samples: int=job.result.completed_samples

def wrong(doc: q.Document, request: q.RawPixelTiledPredictionRequest, result: q.RawPixelContours) -> None:
    q.publish_raw_pixel_stream_contours(doc,request,result)


def missing_guards(report: q.GuiRawPixelReport, event: q.ViewerEvent) -> None:
    a: int=report.stream_contours.completed_samples
    b: int=event.raw_pixel_stream_contours.completed_samples
