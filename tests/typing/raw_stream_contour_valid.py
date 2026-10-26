import qupathxx as q

def streamed(doc: q.Document, request: q.RawPixelTiledPredictionRequest) -> list[int]:
    options=q.RawPixelStreamContourOptions()
    result: q.RawPixelStreamContours=q.trace_raw_pixel_stream(request,options)
    region: q.RawPixelStreamRegion=result.region
    counts: list[int]=result.class_counts
    job=q.RawPixelStreamContourJob()
    job.request(request,options)
    job.wait(doc)
    delivered: q.RawPixelStreamContours | None=job.result
    if delivered is not None:
        return q.publish_raw_pixel_stream_contours(doc,request,delivered)
    return []
